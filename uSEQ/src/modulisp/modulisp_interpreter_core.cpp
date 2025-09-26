#include "../utils.h"
#include "../utils/log.h"
#include "lisp/builtins.h"
#include "lisp/configure.h"
#include "lisp/environment.h"
#include "lisp/macros.h"
#include "lisp/value.h"
#include "lisp/symbol_intern.h"
#include "modulisp_interpreter.h"
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
bool INTERP_MEM ModuLispInterpreter::m_manual_evaluation                = false;
bool INTERP_MEM ModuLispInterpreter::m_update_loop_evaluation           = false;
String INTERP_MEM ModuLispInterpreter::m_atom_currently_being_evaluated = "";

uSEQ* INTERP_MEM ModuLispInterpreter::useq_instance_ptr;
ModuLispInterpreter* INTERP_MEM ModuLispInterpreter::modulisp_instance_ptr;

// Constructors
ModuLispInterpreter::ModuLispInterpreter(ErrorManager* error_mgr, Environment* env,
                                         uLispParser* parser, IClock* clk,
                                         ILogger* log, IRandomGenerator* rng)
    : clock(clk), logger(log)
{
    if (env == nullptr)
    {
        m_fallback_environment = std::make_unique<Environment>();
        m_environment          = m_fallback_environment.get();
    }
    else
    {
        m_environment = env;
    }

    if (error_mgr == nullptr)
    {
        m_fallback_error_manager = std::make_unique<ErrorManager>();
        m_error_manager          = m_fallback_error_manager.get();
    }
    else
    {
        m_error_manager = error_mgr;
    }

    if (parser == nullptr)
    {
        m_fallback_parser = std::make_unique<uLispParser>(m_error_manager);
        m_parser          = m_fallback_parser.get();
    }
    else
    {
        m_parser = parser;
    }

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

#ifdef WASM_BUILD
    auto resetWasmOutputs = [](auto& container) {
        for (auto& slot : container)
        {
            slot.expr             = Value::nil();
            slot.lastTimeSeconds  = std::numeric_limits<double>::quiet_NaN();
            slot.lastValue        = 0.0;
            slot.hasExpr          = false;
        }
    };

    resetWasmOutputs(m_wasm_continuous_outputs);
    resetWasmOutputs(m_wasm_binary_outputs);
    resetWasmOutputs(m_wasm_serial_outputs);
#endif
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

        ModuLispInterpreter temp(nullptr);
        temp.loadBuiltinDefs();
        initialized = true;
    }
}

std::unique_ptr<ModuLispInterpreter> ModuLispInterpreter::create_fresh_interpreter()
{
    init_builtin_functions();
    return std::make_unique<ModuLispInterpreter>(nullptr);
}

// Instance eval wrappers
String ModuLispInterpreter::eval(const String& code)
{
    return eval_in(code, *m_environment);
}
Value ModuLispInterpreter::eval(Value v) { return eval_in(v, *m_environment); }
Value ModuLispInterpreter::eval_v(const String& code)
{
    return eval(m_parser->parse(code));
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
    case Value::BUILTIN_METHOD:
    {
        dbg("builtin METHOD!");
        if (f.stack_data.builtin_method != NULL &&
            ModuLispInterpreter::useq_instance_ptr != NULL)
        {
            Value result =
                (*useq_instance_ptr.*f.stack_data.builtin_method)(args, env);
            return result;
        }
        if (f.stack_data.builtin_method == NULL)
        {
            report_generic_error("EMPTY BUILTIN POINTER for method with name " +
                                 f.str);
        }
        else if (ModuLispInterpreter::useq_instance_ptr == NULL)
        {
            report_generic_error("uSEQ POINTER INSTANCE IS NULL");
        }
        return Value::error();
    }
    case Value::BUILTIN_MODULISP_METHOD:
    {
        dbg("builtin MODULISP METHOD!");
        if (f.stack_data.builtin_modulisp_method != NULL &&
            ModuLispInterpreter::modulisp_instance_ptr != NULL)
        {
            Value result = (*modulisp_instance_ptr.*
                            f.stack_data.builtin_modulisp_method)(args, env);
            return result;
        }
        if (f.stack_data.builtin_modulisp_method == NULL)
        {
            report_generic_error(
                "EMPTY BUILTIN MODULISP POINTER for method with name " + f.str);
        }
        else if (ModuLispInterpreter::modulisp_instance_ptr == NULL)
        {
            report_generic_error("ModuLispInterpreter POINTER INSTANCE IS NULL");
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
