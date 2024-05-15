#include "interpreter.h"
// #include "lisp/library.cpp"
#include "../utils.h"
#include "../utils/log.h"
#include "configure.h"
#include "environment.h"
#include "generated_builtins.h"
#include "macros.h"
#include "parser.h"
#include "value.h"
#include <cmath>

// Static Class-wide flag
// bool Interpreter::m_builtindefs_init = false;

uSEQ* Interpreter::useq_instance_ptr;

Interpreter::Interpreter() {}

void Interpreter::init()
{
    DBG("Interpreter::init");
    // if (!m_builtindefs_init)
    // {
    loadBuiltinDefs();
    // m_builtindefs_init = true;
    // }
}

bool Interpreter::evalled_args_contain_errors(std::vector<Value>& args)
{
    // NOTE: false means "no errors"
    bool has_errors = false;
    for (const Value& v : args)
    {
        if (v.is_error())
        {
            has_errors = true;
            break;
        }
    }
    return has_errors;
}
// #ifdef OEUOEU

// MACROS

namespace builtin
{
// This function is NOT a builtin function, but it is used
// by almost all of them.
//
// Special forms are just builtin functions that don't evaluate
// their arguments. To make a regular builtin that evaluates its
// arguments, we just call this function in our builtin definition.
Value eval_args(std::vector<Value>& args, Environment& env)
{
    Value result = Value::nil();

    for (size_t i = 0; i < args.size(); i++)
    {
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("eval args error");
            result = Value::error();
        }
    }
    return result;
}

#ifdef USE_STD
// Exit the program with an integer code
// Value exit(std::vector<Value> &args, Environment &env) {
//   // Is not a special form, so we can evaluate our args.
//   eval_args(args, env);

//   std::exit(args.size() < 1 ? 0 : args[0].cast_to_int().as_int());
//   return Value();
// }

// Print several values and return the last one
Value print(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() < 1)
        Serial.println(TOO_FEW_ARGS);
    // throw Error(Value("print", print), env, TOO_FEW_ARGS);

    Value acc;
    for (size_t i = 0; i < args.size(); i++)
    {
        acc = args[i];
        Serial.print(acc.display().c_str());
        // std::cout << acc.display();
        if (i < args.size() - 1)
            // std::cout << " ";
            Serial.print(" ");
    }
    // std::cout << std::endl;
    Serial.println();
    return acc;
}

// Get a random number between two numbers inclusively
Value gen_random(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("random", random), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);

    if (args[0].is_number() && args[1].is_number())
    {

        int low = args[0].as_int(), high = args[1].as_int();
        return Value((int)random(low, high));
    }
    else
    {
        error("(gen_random) Both arguments should evaluate to numbers, received "
              "this instead:");
        error(args[0].display() + args[1].display());
    }
}

#endif // USE_STD

Value flatten(Value& val, Environment& env)
{
    std::vector<Value> flattened;
    if (!val.is_list())
    {
        flattened.push_back(val);
    }
    else
    {
        auto valList = val.as_list();
        for (size_t i = 0; i < valList.size(); i++)
        {
            Value evaluatedElement = valList[i].eval(env);
            if (evaluatedElement.is_list())
            {
                auto flattenedElement = flatten(evaluatedElement, env).as_list();
                flattened.insert(flattened.end(), flattenedElement.begin(),
                                 flattenedElement.end());
            }
            else
            {
                flattened.push_back(evaluatedElement);
            }
        }
    }
    return Value(flattened);
}

Value map_list(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    std::vector<Value> result, l = args[1].as_list(), tmp;
    for (size_t i = 0; i < l.size(); i++)
    {
        tmp.push_back(l[i]);
        result.push_back(args[0].apply(tmp, env));
        tmp.clear();
    }
    return Value(result);
}

Value filter_list(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    std::vector<Value> result, l = args[1].as_list(), tmp;
    for (size_t i = 0; i < l.size(); i++)
    {
        tmp.push_back(l[i]);
        if (args[0].apply(tmp, env).as_bool())
            result.push_back(l[i]);
        tmp.clear();
    }
    return Value(result);
}

Value reduce_list(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    std::vector<Value> l = args[2].as_list(), tmp;
    Value acc            = args[1];
    for (size_t i = 0; i < l.size(); i++)
    {
        tmp.push_back(acc);
        tmp.push_back(l[i]);
        acc = args[0].apply(tmp, env);
        tmp.clear();
    }
    return acc;
}

Value range(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    std::vector<Value> result;
    Value low = args[0], high = args[1];
    if (low.get_type_name() != INT_TYPE && low.get_type_name() != FLOAT_TYPE)
        Serial.println(MISMATCHED_TYPES);
    // throw Error(low, env, MISMATCHED_TYPES);
    if (high.get_type_name() != INT_TYPE && high.get_type_name() != FLOAT_TYPE)
        Serial.println(MISMATCHED_TYPES);
    // throw Error(high, env, MISMATCHED_TYPES);

    if (low >= high)
        return Value(result);

    while (low < high)
    {
        result.push_back(low);
        low = low + Value(1);
    }
    return Value(result);
}

BUILTINFUNC(ard_digitalWrite, int pinNumber = args[0].as_int();
            int onOff = args[1].as_int(); digitalWrite(pinNumber, onOff);
            ret       = args[0];, 2)

BUILTINFUNC(ard_digitalRead, int pinNumber = args[0].as_int();
            int val = digitalRead(pinNumber); ret = Value(val);, 1)

BUILTINFUNC(useq_perf, String report = "fps0: "; report += env.get("fps").as_float();
            // report += ", fps1: ";
            // report += env.get("perf_fps1").as_int();
            report += ", qt: ";
            report += env.get("qt").as_float();
            // report += ", in: ";
            // report += env.get("perf_in").as_int();
            // report += ", upd_tm: ";
            // report += env.get("perf_time").as_int();
            // report += ", out: ";
            // report += env.get("perf_out").as_int();
            // report += ", get: ";
            // report += env.get("perf_get").as_float();
            // report += ", parse: ";
            // report += env.get("perf_parse").as_float();
            // report += ", run: ";
            // report += env.get("perf_run").as_float();
            // report += ", ts1: ";
            // report += env.get("perf_ts1").as_float();
            report += ", heap free: ";
            report += rp2040.getFreeHeap() / 1024; Serial.println(report);
            ret = Value();, 0)

double fast(double speed, double phasor)
{
    phasor *= speed;
    double phase = fmod(phasor, 1.0);
    return phase;
}

// uSEQ-specific builtins

} // namespace builtin

// extra arduino api functions

/* BUILTINFUNC_NOEVAL(useq_q0, env.set_global("q-form", args[0]); useq::q0AST =
 * { args[0] };, 1) */

// ANALOG OUTS

#ifdef MIDIOUT
// midi drum out
BUILTINFUNC(
    useq_mdo, int midiNote = args[0].as_int(); if (args[1] != 0) {
        useqMDOMap[midiNote] = args[1];
    } else { useqMDOMap.erase(midiNote); },
                                               2)
#endif

void Interpreter::eval_args(std::vector<Value>& args, Environment& env)
{
    for (size_t i = 0; i < args.size(); i++)
    {
        args[i] = Interpreter::eval_in(args[i], env);
        if (args[i].is_error())
        {
            error("eval args error");
        }
    }
}

// Allow external code to add definitions to m_builtindefs
// void Interpreter::insert_external_builtins(ExternalBuiltinInserter f) {
// f(builtindefs); }

Value builtin_set(std::vector<Value>& args, Environment& env)
{
    if (args.size() != 2)
        println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);

    Value result = Interpreter::eval_in(args[1], env);
    // TODO should this be set_global? it bubbles up mutatively
    env.set_global(args[0].display(), result);
    return result;
}

String Interpreter::eval(const String& code) { return eval_in(code, *this); }
Value Interpreter::eval(Value v) { return eval_in(v, *this); }
Value Interpreter::eval_v(const String& code) { return eval(parse(code)); }

String Interpreter::eval_in(const String& code, Environment& env)
{
    Value tree   = parse(code);
    Value result = eval_in(tree, env);
    return result.display();
}

Value Interpreter::eval_in(Value& v, Environment& env)
{
    DBG("Interpreter::eval_in");
    dbg("Value: " + v.display());

    Value result;

    switch (v.type)
    {
    case Value::NIL:
    {
        // NIL evaluates to itself
        result = v;
        break;
    }
    case Value::QUOTE:
    {
        dbg("quote");
        result = v.list[0];
        break;
    }
    case Value::ATOM:
    {
        dbg("atom");
        // ts_get = micros();
        auto atomdata = env.get(v.str);
        if (atomdata.is_error())
        {
            error("Get error: ");
            println(v.str);
        }
        // ts_get = micros() - ts_get;
        // get_time += ts_get;
        result = atomdata;
        break;
    }
    case Value::LIST:
    {
        dbg("list");
        if (v.list.size() < 1)
            println(EVAL_EMPTY_LIST);
        // throw Error(*this, env, EVAL_EMPTY_LIST);
        // note: this needs to be a copy?  so original remains unevaluated?  or if
        // not, use std::span to avoid the copy?
        std::vector<Value> args =
            std::vector<Value>(v.list.begin() + 1, v.list.end());
        // Only evaluate our arguments if it's not builtin!
        // Builtin functions can be special forms, so we
        // leave them to evaluate their arguments.
        Value function = eval_in(v.list[0], env);
        // Make sure we can find the function
        if (function.is_error())
        {
            error("function is error");
            result = Value::error();
        }
        else
        {
            bool evalError = false;
            // Evaluate all args
            if (!function.is_builtin())
            {
                dbg("(list) NOT builtin, evalling args...");
                for (size_t i = 0; i < args.size(); i++)
                {
                    Value expr       = args[i];
                    Value evaled_arg = eval_in(expr, env);
                    if (evaled_arg.is_error())
                    {
                        evalError = true;
                        error("Arg number " + String(i) + " is error:\n" +
                              expr.display());
                        break;
                    }
                }
            }

            if (evalError)
            {
                result = Value::error();
            }
            else
            {
                // If no args errored out, apply function
                result = apply(function, args, env);
            }
        }
        break;
    }

    default:
        dbg("default case");
        result = v;
        break;
    }

    dbg("Result is: " + result.display());
    return result;
}

#include <iostream>

// NOTE: should be const?
Value Interpreter::apply(Value& f, LispFuncArgsVec& args, Environment& env)
{
    DBG("Interpreter::apply");

    dbg(f.str);

    switch (f.type)
    {
    case Value::LAMBDA:
    {
        dbg("lambda");
        Environment e;
        std::vector<Value>* params;

        // Get the list of parameter atoms
        params = &f.list[0].list;
        if (params->size() != args.size())
        {
            println(args.size() > params->size() ? TOO_MANY_ARGS : TOO_FEW_ARGS);
            return Value::error();
        }

        // throw Error(Value(args), env, args.size() > params.size()?
        //     TOO_MANY_ARGS : TOO_FEW_ARGS
        // );

        // Get the captured scope from the lambda
        e = *f.lambda_scope;
        // And make this scope the parent scope
        e.set_parent_scope(&env);

        // Iterate through the list of parameters and
        // insert the arguments into the scope.
        for (size_t i = 0; i < params->size(); i++)
        {
            if ((*params)[i].type != Value::ATOM)
            {
                println(INVALID_LAMBDA);
            }
            else
            {
                // throw Error(*this, env, INVALID_LAMBDA);
                // Set the parameter name into the scope.
                e.set((*params)[i].str, args[i]);
            }
        }
        // Evaluate the function body with the function scope
        auto result = eval_in(f.list[1], e);
        return result;
    }
    case Value::BUILTIN:
    {
        dbg("builtin");
        // Here, we call the builtin function with the current scope.
        // This allows us to write special forms without syntactic sugar.
        // For functions that are not special forms, we just evaluate
        // the arguments before we run the function.
        if (f.stack_data.builtin != NULL)
        {
            auto callable = *f.stack_data.builtin;
            Value result  = callable(args, env);
            return result;
        }
        else
        {
            error("EMPTY BUILTIN POINTER");
            return Value::error();
        }
    }
    case Value::BUILTIN_METHOD:
    {
        dbg("builtin METHOD!");

        if (f.stack_data.builtin_method != NULL &&
            Interpreter::useq_instance_ptr != NULL)
        {
            // 1. Deref the uSEQ instance pointer
            // 2. Find its concrete method using the builtin_method ptr
            // 3. Call it with the args
            Value result =
                (*useq_instance_ptr.*f.stack_data.builtin_method)(args, env);
            return result;
        }
        else
        {
            if (f.stack_data.builtin_method == NULL)
            {
                error("EMPTY BUILTIN POINTER for method with name " + f.str);
            }
            else if (Interpreter::useq_instance_ptr == NULL)
            {
                error("uSEQ POINTER INSTANCE IS NULL");
            }
            return Value::error();
        }
    }
    default:
    {
        error("ERROR APPLY NOTHING, DEFAULT");
        // We can only call lambdas and builtins
        // print(CALL_NON_FUNCTION);
        // println(str);
        return Value::error();
    }
    }
}

void insert_builtindef(BuiltinMap& map, const String& name, BuiltinFuncRawPtr f_ptr)
{
    map[name] = Value(name, f_ptr);
}

void insert_builtindef(BuiltinMap& map, const String& name, Value v)
{
    map[name] = v;
}

void Interpreter::loadBuiltinDefs()
{
    DBG("Interpreter::loadBuiltinDefs");

    //// LISP
    // Special forms
    Environment::builtindefs["do"]     = Value("do", builtin::do_block);
    Environment::builtindefs["if"]     = Value("if", builtin::if_then_else);
    Environment::builtindefs["for"]    = Value("for", builtin::for_loop);
    Environment::builtindefs["while"]  = Value("while", builtin::while_loop);
    Environment::builtindefs["scope"]  = Value("scope", builtin::scope);
    Environment::builtindefs["quote"]  = Value("quote", builtin::quote);
    Environment::builtindefs["defun"]  = Value("defun", builtin::defun);
    Environment::builtindefs["define"] = Value("define", builtin::define);
    Environment::builtindefs["set"]    = Value("set", builtin::set);
    Environment::builtindefs["lambda"] = Value("lambda", builtin::lambda);

    // List operations
    Environment::builtindefs["list"]   = Value("list", builtin::list);
    Environment::builtindefs["insert"] = Value("insert", builtin::insert);
    Environment::builtindefs["index"]  = Value("index", builtin::index);
    Environment::builtindefs["remove"] = Value("remove", builtin::remove);
    Environment::builtindefs["len"]    = Value("len", builtin::len);
    Environment::builtindefs["push"]   = Value("push", builtin::push);
    Environment::builtindefs["pop"]    = Value("pop", builtin::pop);
    Environment::builtindefs["head"]   = Value("head", builtin::head);
    Environment::builtindefs["tail"]   = Value("tail", builtin::tail);
    Environment::builtindefs["first"]  = Value("first", builtin::head);
    Environment::builtindefs["last"]   = Value("last", builtin::pop);
    Environment::builtindefs["range"]  = Value("range", builtin::range);

    // Functional operations
    Environment::builtindefs["map"]    = Value("map", builtin::map_list);
    Environment::builtindefs["filter"] = Value("filter", builtin::filter_list);
    Environment::builtindefs["reduce"] = Value("reduce", builtin::reduce_list);

    // (arithmetic) Comparison operations
    Environment::builtindefs["="]  = Value("=", builtin::eq);
    Environment::builtindefs["!="] = Value("!=", builtin::neq);
    Environment::builtindefs[">"]  = Value(">", builtin::greater);
    Environment::builtindefs["<"]  = Value("<", builtin::less);
    Environment::builtindefs[">="] = Value(">=", builtin::greater_eq);
    Environment::builtindefs["<="] = Value("<=", builtin::less_eq);

    // Arithmetic operations
    Environment::builtindefs["+"]     = Value("+", builtin::sum);
    Environment::builtindefs["-"]     = Value("-", builtin::subtract);
    Environment::builtindefs["*"]     = Value("*", builtin::product);
    Environment::builtindefs["/"]     = Value("/", builtin::divide);
    Environment::builtindefs["%"]     = Value("%", builtin::remainder);
    Environment::builtindefs["floor"] = Value("floor", builtin::ard_floor);
    Environment::builtindefs["ceil"]  = Value("ceil", builtin::ard_ceil);

    // phasors etc
    // NOTE: duplicates
    Environment::builtindefs["square"] = Value("square", builtin::useq_sqr);
    Environment::builtindefs["sqr"]    = Value("sqr", builtin::useq_sqr);

    // arduino math
    // NOTE: duplicates
    Environment::builtindefs["sin"]  = Value("sin", builtin::ard_sin);
    Environment::builtindefs["sine"] = Value("sine", builtin::ard_sin);
    Environment::builtindefs["cos"]  = Value("cos", builtin::ard_cos);
    Environment::builtindefs["tan"]  = Value("tan", builtin::ard_tan);
    Environment::builtindefs["abs"]  = Value("abs", builtin::ard_abs);

    Environment::builtindefs["min"]   = Value("min", builtin::ard_min);
    Environment::builtindefs["max"]   = Value("max", builtin::ard_max);
    Environment::builtindefs["pow"]   = Value("pow", builtin::ard_pow);
    Environment::builtindefs["sqrt"]  = Value("sqrt", builtin::ard_sqrt);
    Environment::builtindefs["scale"] = Value("scale", builtin::ard_map);

    // Meta operations
    Environment::builtindefs["eval"] = Value("eval", builtin::eval);
    Environment::builtindefs["type"] = Value("type", builtin::get_type_name);
    // Environment::builtindefs["parse"] = Value("parse", builtin::parse);

    // List generators
    Environment::builtindefs["zeros"] = Value("zeros", builtin::zeros);

    // utility
    Environment::builtindefs["timeit"] = Value("timeit", builtin::timeit);
    Environment::builtindefs["perf"]   = Value("perf", builtin::useq_perf);

// IO operations
#ifdef USE_STD
    // if (name == "exit") return Value("exit", builtin::exit);
    // if (name == "quit") return Value("quit", builtin::exit);
    Environment::builtindefs["print"] = Value("print", builtin::print);
    // if (name == "input") return Value("input", builtin::input);
    Environment::builtindefs["random"] = Value("random", builtin::gen_random);
#endif

    // String operations
    Environment::builtindefs["debug"]   = Value("debug", builtin::debug);
    Environment::builtindefs["replace"] = Value("replace", builtin::replace);
    Environment::builtindefs["display"] = Value("display", builtin::display);

    // Casting operations
    Environment::builtindefs["int"]   = Value("int", builtin::cast_to_int);
    Environment::builtindefs["float"] = Value("float", builtin::cast_to_float);

    // Constants
    Environment::builtindefs["endl"] = Value::string("\n");

    //// IO
    Environment::builtindefs["dw"] = Value("dw", builtin::ard_digitalWrite);
    Environment::builtindefs["dr"] = Value("dr", builtin::ard_digitalRead);
}

// #endif
