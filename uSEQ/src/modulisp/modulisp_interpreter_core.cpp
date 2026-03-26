#include "../utils.h"
#include "../utils/log.h"
#include "bytecode_vm.h"
#include "lisp/builtins.h"
#include "lisp/configure.h"
#include "lisp/environment.h"
#include "lisp/macros.h"
#include "lisp/value.h"
#include "lisp/symbol_intern.h"
#include "modulisp_interpreter.h"
#include "temporal_context.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <vector>

#ifdef ARDUINO
#define INTERP_MEM __not_in_flash("interp")
#else
#define INTERP_MEM // Empty for desktop builds
#endif

bool INTERP_MEM user_interaction = false;

bool INTERP_MEM ModuLispInterpreter::m_attempt_expr_eval_first          = false;
bool INTERP_MEM ModuLispInterpreter::m_eval_expr_if_def_not_found       = true;
bool INTERP_MEM ModuLispInterpreter::m_update_loop_evaluation           = false;
String INTERP_MEM ModuLispInterpreter::m_atom_currently_being_evaluated = "";

namespace
{
bool is_output_assignment_symbol(const String& symbol)
{
    if (symbol.length() < 2)
    {
        return false;
    }

    const char prefix = symbol[0];
    if (prefix != 'a' && prefix != 'd' && prefix != 's')
    {
        return false;
    }

    for (unsigned int i = 1; i < symbol.length(); ++i)
    {
        if (!std::isdigit(static_cast<unsigned char>(symbol[i])))
        {
            return false;
        }
    }

    return true;
}

bool requires_tree_walk_eval(const Value& expr)
{
    if (expr.type != Value::LIST || expr.list.empty() || !expr.list[0].is_symbol())
    {
        return false;
    }

    const String& head = expr.list[0].as_atom();
    if (is_output_assignment_symbol(head))
    {
        return true;
    }

    return head == "define" || head == "def" || head == "defn" ||
           head == "defun" || head == "defs" || head == "set" ||
           head == "schedule" || head == "unschedule" ||
           head == "useq-play" || head == "useq-pause" ||
           head == "useq-stop" || head == "useq-rewind" ||
           head == "useq-clear" || head == "set-bpm" ||
           head == "set-time-sig" || head == "useq-set-time-offset" ||
           head == "useq-nudge-time";
}
} // namespace

// Constructors
ModuLispInterpreter::ModuLispInterpreter(IClock* clk, ILogger* log,
                                         IRandomGenerator* rng,
                                         size_t num_analog_outs,
                                         size_t num_digital_outs,
                                         size_t num_serial_outs)
    : clock(clk), logger(log),
      m_num_analog_outs(num_analog_outs),
      m_num_digital_outs(num_digital_outs),
      m_num_serial_outs(num_serial_outs),
      m_diagnostics(),
      m_environment(),
      m_parser(&m_diagnostics)
{
    // Managers
    m_time_manager = std::make_unique<TimeManager>(clk);
    m_scheduler    = std::make_unique<Scheduler>();
    if (rng)
    {
        m_random_generator.reset(rng);
    }
    else
    {
        m_random_generator = std::make_unique<SimpleRandomGenerator>();
    }

    set_bpm(130.0, 0.0);
    if (get_parser())
    {
        m_scheduler->set_cqp_ast(get_parser()->parse("bar"));
    }
    else
    {
        m_scheduler->set_cqp_ast(Value::atom("bar"));
    }

    // Initialize output storage
    m_analog_outputs.resize(num_analog_outs);
    m_digital_outputs.resize(num_digital_outs);
    m_serial_outputs.resize(num_serial_outs);

    for (auto& slot : m_analog_outputs)
    {
        reset_output_slot(slot, OutputType::ANALOG);
    }
    for (auto& slot : m_digital_outputs)
    {
        reset_output_slot(slot, OutputType::DIGITAL);
    }
    for (auto& slot : m_serial_outputs)
    {
        reset_output_slot(slot, OutputType::SERIAL);
    }

    // Ensure temporal lookup table is initialized (idempotent)
    TemporalContext::initLookupTable();

    // Point the global environment at our temporal context struct
    m_environment.set_temporal_context(&m_global_temporal_ctx);

    // Initialize time variables with default values (0.0)
    // This ensures they always exist in the environment
    m_environment.set("t", Value(0.0));
    m_environment.set("time", Value(0.0));
    m_environment.set("beat", Value(0.0));
    m_environment.set("bar", Value(0.0));
    m_environment.set("phrase", Value(0.0));
    m_environment.set("section", Value(0.0));
}

// Destructor
ModuLispInterpreter::~ModuLispInterpreter() = default;

// Builtins initialization
void ModuLispInterpreter::init_builtin_functions()
{
    DBG("ModuLispInterpreter::init_builtin_functions");
    static bool initialized = false;
    if (!initialized)
    {
        // Pre-intern common symbols for better performance
        SymbolIntern::getInstance().preinternCommonSymbols();

        // Initialize temporal context lookup table (must happen after SymbolIntern)
        TemporalContext::initLookupTable();

        ModuLispInterpreter temp;
        temp.loadBuiltinDefs();
        initialized = true;
    }
}

std::unique_ptr<ModuLispInterpreter> ModuLispInterpreter::create_fresh_interpreter()
{
    init_builtin_functions();
    return std::make_unique<ModuLispInterpreter>();
}

Value ModuLispInterpreter::eval_form_with_vm_at_time(const Value& expr,
                                                     Environment& env,
                                                     double time_seconds,
                                                     bool* used_vm)
{
    if (used_vm)
    {
        *used_vm = false;
    }

    const NumericVmCompileResult compiled = compile_numeric_program(expr, env);
    if (!compiled.ok)
    {
        return Value::error();
    }

    const TaggedVmExecutionResult executed =
        execute_tagged_program(compiled.program, make_temporal_context(time_seconds));
    if (!executed.ok)
    {
        return Value::error();
    }

    if (used_vm)
    {
        *used_vm = true;
    }

    return executed.value;
}

Value ModuLispInterpreter::eval_form_with_vm(Value expr)
{
    if (requires_tree_walk_eval(expr))
    {
        return eval_in(expr, m_environment);
    }

    if (expr.type == Value::LIST && !expr.list.empty() && expr.list[0].is_symbol() &&
        expr.list[0].as_atom() == "do")
    {
        Value result = Value::nil();
        for (size_t i = 1; i < expr.list.size(); ++i)
        {
            result = eval_form_with_vm(expr.list[i]);
        }
        return result;
    }

    bool used_vm = false;
    const double time_seconds =
        m_time_manager ? m_time_manager->get_transport_time() / 1e6 : 0.0;
    Value result = eval_form_with_vm_at_time(expr, m_environment, time_seconds, &used_vm);
    if (used_vm)
    {
        return result;
    }

    return eval_in(expr, m_environment);
}

// Instance eval wrappers
String ModuLispInterpreter::eval(const String& code)
{
    return eval_v(code).display();
}
Value ModuLispInterpreter::eval(Value v) { return eval_form_with_vm(v); }
Value ModuLispInterpreter::eval_v(const String& code)
{
    return eval_form_with_vm(m_parser.parse(code));
}

// Static helpers (ported)
String ModuLispInterpreter::eval_in(const String& code, Environment& env)
{
    Value tree   = uLispParser::parse_static(code);
    Value result = eval_in(tree, env);
    return result.display();
}

#if defined(USE_NOT_IN_FLASH)
Value __not_in_flash_func(ModuLispInterpreter::eval_in)(Value& v, Environment& env)
#else
Value ModuLispInterpreter::eval_in(Value& v, Environment& env)
#endif
{
    DBG("ModuLispInterpreter::eval_in");

    Value result = Value::error();
    switch (v.type)
    {
    case Value::QUOTE:
        result = v.list[0];
        break;
    case Value::ATOM:
    {
        const String& symbol_name = v.str;

        auto try_eval_expr_binding = [&](const String& name) -> std::optional<Value> {
            std::optional<Value> binding_expr = env.get_expr(name);
            if (!binding_expr)
            {
                return std::nullopt;
            }

            static std::vector<String> recursion_guard;
            if (std::find(recursion_guard.begin(), recursion_guard.end(), name) !=
                recursion_guard.end())
            {
                report_runtime_error("Expression for '" + name +
                                     "' references itself");
                return Value::error();
            }

            recursion_guard.push_back(name);
            Value evaluated = eval_in(*binding_expr, env);
            recursion_guard.pop_back();

            return evaluated;
        };

        if (m_attempt_expr_eval_first)
        {
            if (auto evaluated = try_eval_expr_binding(symbol_name))
            {
                result = *evaluated;
                break;
            }
        }

        bool has_binding = env.has(symbol_name) ||
                           Environment::builtindefs().has(symbol_name);
        if (has_binding)
        {
            auto tmp = env.get(symbol_name);
            if (tmp.has_value())
            {
                result = tmp.value();
                break;
            }
        }

        if (!m_attempt_expr_eval_first && m_eval_expr_if_def_not_found)
        {
            if (auto evaluated = try_eval_expr_binding(symbol_name))
            {
                result = *evaluated;
                break;
            }
        }

        report_generic_error("Variable '" + symbol_name + "' is not defined");
        result = Value::error();
        break;
    }
    case Value::INT:
    case Value::FLOAT:
    case Value::STRING:
    case Value::UNIT:
    case Value::NIL:
        result = v;
        break;
    case Value::LIST:
    {
        if (v.list.size() == 0)
        {
            result = Value::nil();
            break;
        }

        std::vector<Value> args =
            std::vector<Value>(v.list.begin() + 1, v.list.end());
        Value function = eval_in(v.list[0], env);
        if (function.is_error())
        {
            if (v.list[0].is_symbol())
            {
                report_generic_error("Function '" + v.list[0].as_atom() +
                                     "' is not defined");
            }
            else
            {
                report_runtime_error(
                    "Trying to evaluate the function " + v.list[0].display() +
                    " results in an error. This could either mean that it hasn't "
                    "been defined, or that it's not valid.");
            }
            result = Value::error();
        }
        else
        {
            bool evalError = false;
            if (!function.is_builtin())
            {
                dbg("(list) NOT builtin, evalling args...");
                for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++)
                {
                    Value expr       = args[i];
                    Value evaled_arg = eval_in(expr, env);
                    if (evaled_arg.is_error())
                    {
                        evalError = true;
                        report_generic_error("Arg number " + String(i) +
                                             " is error:\n" + expr.display());
                        break;
                    }
                    args[i] = evaled_arg;
                }
            }

            if (evalError)
            {
                result = Value::error();
            }
            else
            {
                result = apply(function, args, env);
            }
        }
        break;
    }
    case Value::VECTOR:
    {
        std::vector<Value> result_vec;
        for (auto& val : v.list)
        {
            Value evalled = val.eval(env);
            if (evalled.is_error())
                return Value::error();
            result_vec.push_back(evalled);
        }
        return Value::vector(result_vec);
    }
    default:
        dbg("default case");
        result = v;
        break;
    }

    dbg("Result is: " + result.display());
    return result;
}

#if defined(USE_NOT_IN_FLASH)
Value __not_in_flash_func(ModuLispInterpreter::apply)(Value& f,
                                                      LispFuncArgsVec& args,
                                                      Environment& env)
#else
Value ModuLispInterpreter::apply(Value& f, LispFuncArgsVec& args, Environment& env)
#endif
{
    DBG("ModuLispInterpreter::apply");
    dbg(f.str);
    switch (f.type)
    {
    case Value::LAMBDA:
    {
        dbg("lambda");
        Environment e;
        std::vector<Value>* params;
        params = &f.list[0].list;
        if (params->size() != args.size())
        {
            ::println(args.size() > params->size() ? TOO_MANY_ARGS : TOO_FEW_ARGS);
            return Value::error();
        }
        e = *f.lambda_scope;
        e.set_parent_scope(&env);
        for (size_t i = 0; i < params->size(); i++)
        {
            if ((*params)[i].type != Value::ATOM)
            {
                ::println(INVALID_LAMBDA);
            }
            else
            {
                e.set((*params)[i].str, args[i]);
            }
        }
        auto result = eval_in(f.list[1], e);
        return result;
    }
    case Value::BUILTIN:
    {
        dbg("builtin");
        if (f.stack_data.builtin != NULL)
        {
            auto callable = *f.stack_data.builtin;
            Value result  = callable(args, env);
            return result;
        }
        report_generic_error("EMPTY BUILTIN POINTER");
        return Value::error();
    }
    case Value::BUILTIN_PLUGIN:
    {
        dbg("builtin PLUGIN!");
        if (f.stack_data.plugin_builtin != NULL && f.plugin_context != NULL)
        {
            Value result =
                f.stack_data.plugin_builtin(f.plugin_context, args, env);
            return result;
        }
        if (f.stack_data.plugin_builtin == NULL)
        {
            report_generic_error("EMPTY PLUGIN BUILTIN POINTER for function with name " +
                                 f.str);
        }
        else if (f.plugin_context == NULL)
        {
            report_generic_error("PLUGIN CONTEXT IS NULL for function with name " +
                                 f.str);
        }
        return Value::error();
    }
    case Value::VECTOR:
    {
        size_t size = f.list.size();
        if (args.size() == 1 && args[0].is_number())
        {
            float phasor = std::clamp(fmod(args[0].as_float(), 1.0), 0.0, 1.0);
            size_t idx   = floor(args[0].as_float() * size);
            if (idx >= size)
            {
                idx = size - 1;
            }
            return f.list[idx];
        }
        report_custom_function_error("[]",
                                     "Expected a phasor (0..1) to index a vector");
        return Value::error();
    }
    default:
        break;
    }
    return Value::error();
}

void ModuLispInterpreter::eval_args(std::vector<Value>& args, Environment& env)
{
    Value result = Value::nil();
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++)
    {
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            report_generic_error("eval args error");
            result = Value::error();
        }
    }
    (void)result;
}

void ModuLispInterpreter::loadBuiltinDefs()
{
    DBG("ModuLispInterpreter::loadBuiltinDefs");
    // Register core builtins into the static builtin map
    // Collections
    Environment::builtindefs()["list"]   = Value("list", builtin::list);
    Environment::builtindefs()["vec"]    = Value("vec", builtin::vec);
    Environment::builtindefs()["head"]   = Value("head", builtin::head);
    Environment::builtindefs()["tail"]   = Value("tail", builtin::tail);
    Environment::builtindefs()["first"]  = Value("first", builtin::head);
    Environment::builtindefs()["last"]   = Value("last", builtin::pop);
    Environment::builtindefs()["len"]    = Value("len", builtin::len);
    Environment::builtindefs()["index"]  = Value("index", builtin::index);
    Environment::builtindefs()["nth"]    = Value("nth", builtin::index);
    Environment::builtindefs()["insert"] = Value("insert", builtin::insert);
    Environment::builtindefs()["remove"] = Value("remove", builtin::remove);
    Environment::builtindefs()["slice"]  = Value("slice", builtin::slice);
    // Functional ops
    Environment::builtindefs()["map"]    = Value("map", builtin::map_list);
    Environment::builtindefs()["filter"] = Value("filter", builtin::filter_list);
    Environment::builtindefs()["reduce"] = Value("reduce", builtin::reduce_list);

    // Comparison
    Environment::builtindefs()["="]  = Value("=", builtin::eq);
    Environment::builtindefs()["!="] = Value("!=", builtin::neq);
    Environment::builtindefs()[">"]  = Value(">", builtin::greater);
    Environment::builtindefs()["<"]  = Value("<", builtin::less);
    Environment::builtindefs()[">="] = Value(">=", builtin::greater_eq);
    Environment::builtindefs()["<="] = Value("<=", builtin::less_eq);

    // Arithmetic
    Environment::builtindefs()["+"]     = Value("+", builtin::sum);
    Environment::builtindefs()["-"]     = Value("-", builtin::subtract);
    Environment::builtindefs()["*"]     = Value("*", builtin::product);
    Environment::builtindefs()["/"]     = Value("/", builtin::divide);
    Environment::builtindefs()["%"]     = Value("%", builtin::remainder);
    Environment::builtindefs()["floor"] = Value("floor", builtin::ard_floor);
    Environment::builtindefs()["ceil"]  = Value("ceil", builtin::ard_ceil);

    // Meta
    Environment::builtindefs()["type"]     = Value("type", builtin::get_type_name);
    Environment::builtindefs()["eval"]     = Value("eval", builtin::eval);
    Environment::builtindefs()["defn"]     = Value("defn", builtin::defn);
    Environment::builtindefs()["lambda"]   = Value("lambda", builtin::lambda);
    Environment::builtindefs()["def"]      = Value("def", builtin::def);
    Environment::builtindefs()["defun"]    = Value("defun", builtin::defun);
    Environment::builtindefs()["defs"]     = Value("defs", builtin::defs);
    Environment::builtindefs()["set"]      = Value("set", builtin::set);
    Environment::builtindefs()["get-expr"] = Value("get-expr", builtin::get_expr);
    Environment::builtindefs()["do"]       = Value("do", builtin::do_block);
    Environment::builtindefs()["let"]      = Value("let", builtin::let_block);
    Environment::builtindefs()["for"]      = Value("for", builtin::for_loop);
    Environment::builtindefs()["while"]    = Value("while", builtin::while_loop);
    Environment::builtindefs()["if"]       = Value("if", builtin::if_then_else);

#ifndef ARDUINO
    Environment::builtindefs()["print"]   = Value("print", builtin::print);
    Environment::builtindefs()["println"] = Value("println", builtin::println);
#else
    Environment::builtindefs()["print"]   = Value("print", builtin::print);
    Environment::builtindefs()["println"] = Value("println", builtin::println);
#endif
    Environment::builtindefs()["display"] = Value("display", builtin::display);
    Environment::builtindefs()["replace"] = Value("replace", builtin::replace);
    Environment::builtindefs()["debug"]   = Value("debug", builtin::debug);
    Environment::builtindefs()["int"]     = Value("int", builtin::cast_to_int);
    Environment::builtindefs()["float"]   = Value("float", builtin::cast_to_float);
    Environment::builtindefs()["quote"]   = Value("quote", builtin::quote);

    // Math / trig and helpers
    Environment::builtindefs()["sin"]     = Value("sin", builtin::ard_sin);
    Environment::builtindefs()["sine"]    = Value("sine", builtin::ard_sin);
    Environment::builtindefs()["usin"]    = Value("usin", builtin::ard_usin);
    Environment::builtindefs()["usine"]   = Value("usine", builtin::ard_usin);
    Environment::builtindefs()["cos"]     = Value("cos", builtin::ard_cos);
    Environment::builtindefs()["cosine"]  = Value("cosine", builtin::ard_cos);
    Environment::builtindefs()["ucos"]    = Value("ucos", builtin::ard_ucos);
    Environment::builtindefs()["ucosine"] = Value("ucosine", builtin::ard_ucos);
    Environment::builtindefs()["tan"]     = Value("tan", builtin::ard_tan);
    Environment::builtindefs()["abs"]     = Value("abs", builtin::ard_abs);
    Environment::builtindefs()["min"]     = Value("min", builtin::ard_min);
    Environment::builtindefs()["max"]     = Value("max", builtin::ard_max);
    Environment::builtindefs()["pow"]     = Value("pow", builtin::ard_pow);
    Environment::builtindefs()["sqrt"]    = Value("sqrt", builtin::ard_sqrt);
    Environment::builtindefs()["scale"]   = Value("scale", builtin::ard_map);
    Environment::builtindefs()["lerp"]    = Value("lerp", builtin::ard_lerp);

    // Conversions
    Environment::builtindefs()["b->u"]    = Value("b->u", builtin::b_to_u);
    Environment::builtindefs()["bi->uni"] = Value("bi->uni", builtin::b_to_u);
    Environment::builtindefs()["u->b"]    = Value("u->b", builtin::u_to_b);
    Environment::builtindefs()["uni->bi"] = Value("uni->bi", builtin::u_to_b);

    // Timing/utility
    Environment::builtindefs()["timeit"] = Value("timeit", builtin::timeit);
    Environment::builtindefs()["millis"] = Value("millis", builtin::ard_millis);
    Environment::builtindefs()["micros"] = Value("micros", builtin::ard_micros);
    Environment::builtindefs()["delay"]  = Value("delay", builtin::ard_delay);
    Environment::builtindefs()["delayus"] =
        Value("delayus", builtin::ard_delaymicros);
    Environment::builtindefs()["zeros"] = Value("zeros", builtin::zeros);

    // Previously unregistered functions
    Environment::builtindefs()["define"] = Value("define", builtin::define);
    Environment::builtindefs()["push"]   = Value("push", builtin::push);
    Environment::builtindefs()["scope"]  = Value("scope", builtin::scope);
    Environment::builtindefs()["pulse"]  = Value("pulse", builtin::useq_pulse);
    Environment::builtindefs()["sqr"]    = Value("sqr", builtin::useq_sqr);
}

// ===== Plugin builtin registration =====

void ModuLispInterpreter::register_plugin_builtin(const String& name,
                                                   PluginBuiltinFunc func,
                                                   void* ctx)
{
    Environment::builtindefs()[name] = Value(name, func, ctx);
}

// ===== eval_at_time (merged from modulisp_eval.cpp) =====

Value ModuLispInterpreter::useq_eval_at_time(std::vector<Value>& args,
                                             Environment& env)
{
    constexpr const char* user_facing_name = "eval-at-time";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // NOTE: This needs to eval both of its args, including the list,
    // to cover for cases where the user passes anything other than a
    // list literal (e.g. a symbol that points to a list)
    //
    // Evaluating & checking args for errors
    Value pre_eval = args[0];
    args[0]        = args[0].eval(env);
    if (args[0].is_error())
    {
        report_error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    }

    // NOTE: go from seconds to micros for internal calculations
    double time = args[0].as_float() * 1e+6;

    // BODY
    return eval_at_time(args[1], env, time);
}

Value ModuLispInterpreter::eval_at_time(Value& expr, Environment& env,
                                        TimeValue time_micros)
{
    bool used_vm = false;
    Value compiled_result =
        eval_form_with_vm_at_time(expr, env, time_micros * 1e-6, &used_vm);
    if (used_vm)
    {
        return compiled_result;
    }

    TemporalContext ctx = make_temporal_context(time_micros * 1e-6);

    Environment new_env;  // empty, no map allocations
    new_env.set_temporal_context(&ctx);
    new_env.set_parent_scope(&env);
    return eval_in(expr, new_env);
}

// make_env_for_time and make_env_with_updated_time_durs are now defined in
// modulisp_time.cpp

// ===== Execution API (merged from modulisp_interpreter.cpp) =====

ExecutionResult ModuLispInterpreter::execute_now(const String& code)
{
    // Clear any previous errors before execution
    // This ensures we only capture errors from THIS execution
    error_msg_q.clear();

    // Execute the code synchronously and capture the result
    String result = eval(code);

    // Build the execution result structure
    ExecutionResult exec_result;
    exec_result.result_text = result;
    exec_result.had_errors = !error_msg_q.empty();
    exec_result.errors = error_msg_q;  // Copy error queue to result
    exec_result.printed = true;         // Immediate execution always prints

    return exec_result;
}

ExecutionResult ModuLispInterpreter::schedule_code(const String& code)
{
    // Parse the code into an AST Value
    Value expr = get_parser()->parse(code);

    // Add the parsed expression to the scheduler's run queue
    get_scheduler()->add_to_run_queue(expr);

    // Build the execution result structure
    ExecutionResult exec_result;
    exec_result.result_text = code;     // Echo the scheduled code
    exec_result.had_errors = false;     // Scheduling itself doesn't produce errors
    exec_result.errors.clear();         // No errors
    exec_result.printed = true;         // Echo confirms scheduling

    return exec_result;
}

void ModuLispInterpreter::update_stream_value(size_t channel, double value)
{
    (void)channel;  // Suppress unused parameter warning
    (void)value;    // Suppress unused parameter warning
}

void ModuLispInterpreter::run_scheduled_items()
{
    DBG("ModuLispInterpreter::runScheduledItems");

    size_t current_time = static_cast<size_t>(m_time_manager->get_transport_time());
    auto items_to_run   = m_scheduler->get_items_to_run(current_time);

    for (auto* item : items_to_run)
    {
        if (item && !item->ast.is_nil())
        {
            Value result = eval(item->ast);
            (void)result; // Suppress unused variable warning
        }
    }
}

void ModuLispInterpreter::check_code_quant_phasor()
{
    DBG("ModuLispInterpreter::check_code_quant_phasor");

    double newCqpVal = eval(m_scheduler->get_cqp_ast()).as_float();
    double last_cqp  = m_scheduler->get_last_cqp();

    if (newCqpVal < last_cqp)
    {
        // Phasor wrapped around, execute queued items
        update_Q0();
        auto& run_queue = m_scheduler->get_run_queue();
        for (const auto& item : run_queue)
        {
            Value res;
            int cmdts = micros();
            res       = eval(item);
            cmdts     = micros() - cmdts;
            println(res.to_lisp_src());
        }
        m_scheduler->clear_run_queue();
    }
    m_scheduler->set_last_cqp(newCqpVal);
}

void ModuLispInterpreter::update_Q0()
{
    const Value& q0_ast = m_scheduler->get_q0_ast();
    if (!q0_ast.is_nil())
    {
        Value result = eval(q0_ast);
        if (result.is_error())
        {
            println("Error in q0 output function, clearing");
            m_scheduler->set_q0_ast(Value());
        }
    }
}
