#include "interpreter.h"
// #include "lisp/library.cpp"
#include "../utils.h"
#include "../utils/log.h"
#include "configure.h"
#include "environment.h"
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
    Value ret = Value();
    for (size_t i = 0; i < args.size(); i++)
    {
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            Serial.println("eval args error");
            ret = Value::error();
        }
    }
    return ret;
}

// Create a lambda function (SPECIAL FORM)
Value lambda(std::vector<Value>& args, Environment& env)
{
    if (args.size() < 2)
        Serial.println(TOO_FEW_ARGS);

    // throw Error(Value("lambda", lambda), env, TOO_FEW_ARGS);

    if (args[0].get_type_name() != LIST_TYPE)
        Serial.println(INVALID_LAMBDA);

    // throw Error(Value("lambda", lambda), env, INVALID_LAMBDA);

    return Value(args[0].as_list(), args[1], env);
}

// if-else (SPECIAL FORM)
Value if_then_else(std::vector<Value>& args, Environment& env)
{
    if (args.size() != 3)
        Serial.println(args.size() > 3 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    if (args[0].eval(env).as_bool())
        return args[1].eval(env);
    else
        return args[2].eval(env);
}

// Define a variable with a value (SPECIAL FORM)
Value define(std::vector<Value>& args, Environment& env)
{
    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);

    Value result = args[1].eval(env);
    env.set(args[0].display(), result);
    return result;
}

Value set(std::vector<Value>& args, Environment& env)
{
    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);

    Value result = args[1].eval(env);
    env.set_global(args[0].display(), result);
    return result;
}

// Define a function with parameters and a result expression (SPECIAL FORM)
Value defun(std::vector<Value>& args, Environment& env)
{
    if (args.size() != 3)
        Serial.println(args.size() > 3 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("defun", defun), env, args.size() > 3? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);

    if (args[1].get_type_name() != LIST_TYPE)
        Serial.println(INVALID_LAMBDA);
    // throw Error(Value("defun", defun), env, INVALID_LAMBDA);

    String f_name             = args[0].display();
    std::vector<Value> params = args[1].as_list();
    Value body                = args[2];
    Value f                   = Value(params, body, env);
    env.set(f_name, f);
    return f;
}

// Loop over a list of expressions with a condition (SPECIAL FORM)
Value while_loop(std::vector<Value>& args, Environment& env)
{
    Value acc;
    while (args[0].eval(env).as_bool())
    {
        for (size_t i = 1; i < args.size() - 1; i++)
            args[i].eval(env);
        acc = args[args.size() - 1].eval(env);
    }
    return acc;
}

// Iterate through a list of values in a list (SPECIAL FORM)
Value for_loop(std::vector<Value>& args, Environment& env)
{
    Value acc;
    std::vector<Value> list = args[1].eval(env).as_list();

    for (size_t i = 0; i < list.size(); i++)
    {
        env.set(args[0].as_atom(), list[i]);

        for (size_t j = 1; j < args.size() - 1; j++)
            args[j].eval(env);
        acc = args[args.size() - 1].eval(env);
    }

    return acc;
}

// Evaluate a block of expressions in the current environment (SPECIAL FORM)
Value do_block(std::vector<Value>& args, Environment& env)
{
    Value acc;
    for (size_t i = 0; i < args.size(); i++)
        acc = args[i].eval(env);
    return acc;
}

// Evaluate a block of expressions in a new environment (SPECIAL FORM)
Value scope(std::vector<Value>& args, Environment& env)
{
    Environment e = env;
    Value acc;
    for (size_t i = 0; i < args.size(); i++)
        acc = args[i].eval(e);
    return acc;
}

// Quote an expression (SPECIAL FORM)
Value quote(std::vector<Value>& args, Environment&)
{
    // std::vector<Value> v(args);
    // for (size_t i = 0; i < args.size(); i++)
    //   v.push_back(args[i]);
    return Value(args);
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

    int low = args[0].as_int(), high = args[1].as_int();
    return Value((int)random(low, high));
}

// // Get the contents of a file
// Value read_file(std::vector<Value> &args, Environment &env) {
//     // Is not a special form, so we can evaluate our args.
//     eval_args(args, env);

//     if (args.size() != 1)
//         throw Error(Value("read-file", read_file), env, args.size() > 1?
//         TOO_MANY_ARGS : TOO_FEW_ARGS);

//     // return Value::string(content);
//     return Value::string(read_file_contents(args[0].as_string()));
// }

// // Write a string to a file
// Value write_file(std::vector<Value> &args, Environment &env) {
//     // Is not a special form, so we can evaluate our args.
//     eval_args(args, env);

//     if (args.size() != 2)
//         throw Error(Value("write-file", write_file), env, args.size() > 1?
//         TOO_MANY_ARGS : TOO_FEW_ARGS);

//     std::ofstream f;
//     // The first argument is the file name
//     f.open(args[0].as_string().c_str());
//     // The second argument is the contents of the file to write
//     Value result = Value((f << args[1].as_string())? 1 : 0);
//     f.close();
//     return result;
// }

// Read a file and execute its code
// Value include(std::vector<Value> &args, Environment &env) {
//     // Import is technically not a special form, it's more of a macro.
//     // We can evaluate our arguments.
//     eval_args(args, env);

//     if (args.size() != 1)
//         throw Error(Value("include", include), env, args.size() > 1?
//         TOO_MANY_ARGS : TOO_FEW_ARGS);

//     Environment e;
//     Value result = run(read_file_contents(args[0].as_string()), e);
//     env.combine(e);
//     return result;
// }
#endif

// Evaluate a value as code
Value eval(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    Value ret = eval_args(args, env);
    if (ret.is_error())
    {
        Serial.println("eval err");
        return Value::error();
    }
    if (args.size() != 1)
    {
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return Value();
    }
    // throw Error(Value("eval", eval), env, args.size() > 1? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    else
    {
        return args[0].eval(env);
    }
}

// Create a list of values
Value list(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    return Value(args);
}

// Sum multiple values
Value sum(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() < 2)
        Serial.println(TOO_FEW_ARGS);
    // throw Error(Value("+", sum), env, TOO_FEW_ARGS);

    Value acc = args[0];
    for (size_t i = 1; i < args.size(); i++)
        acc = acc + args[i];
    return acc;
}

// Subtract two values
Value subtract(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("-", subtract), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    return args[0] - args[1];
}

// Multiply several values
Value product(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() < 2)
        Serial.println(TOO_FEW_ARGS);
    // throw Error(Value("*", product), env, TOO_FEW_ARGS);

    Value acc = args[0];
    for (size_t i = 1; i < args.size(); i++)
        acc = acc * args[i];
    return acc;
}

// Divide two values
Value divide(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("/", divide), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    auto result = args[0] / args[1];
    return result;
}

// Get the remainder of values
Value remainder(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("%", remainder), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    auto result = args[0] % args[1];
    return result;
}

// Are two values equal?
Value eq(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("=", eq), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    return Value(int(args[0] == args[1]));
}

// Are two values not equal?
Value neq(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("!=", neq), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    return Value(int(args[0] != args[1]));
}

// Is one number greater than another?
Value greater(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value(">", greater), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    return Value(int(args[0] > args[1]));
}

// Is one number less than another?
Value less(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("<", less), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    return Value(int(args[0] < args[1]));
}

// Is one number greater than or equal to another?
Value greater_eq(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value(">=", greater_eq), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    return Value(int(args[0] >= args[1]));
}

// Is one number less than or equal to another?
Value less_eq(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("<=", less_eq), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    return Value(int(args[0] <= args[1]));
}

// Get the type name of a value
Value get_type_name(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("type", get_type_name), env, args.size() > 1?
    // TOO_MANY_ARGS : TOO_FEW_ARGS);

    return Value::string(args[0].get_type_name());
}

// Cast an item to a float
Value cast_to_float(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value(FLOAT_TYPE, cast_to_float), env, args.size() > 1?
    // TOO_MANY_ARGS : TOO_FEW_ARGS);
    return args[0].cast_to_float();
}

// Cast an item to an int
Value cast_to_int(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value(INT_TYPE, cast_to_int), env, args.size() > 1?
    // TOO_MANY_ARGS : TOO_FEW_ARGS);
    return args[0].cast_to_int();
}

// Index a list
Value index(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("index", index), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);

    std::vector<Value> list = args[0].as_list();
    int i                   = args[1].as_int();
    if (list.empty() || i >= (int)list.size())
    {
        Serial.println(INDEX_OUT_OF_RANGE);
        return Value::error();
    }
    // throw Error(list, env, INDEX_OUT_OF_RANGE);

    return list[i];
}

// Insert a value into a list
Value insert(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 3)
        Serial.println(args.size() > 3 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("insert", insert), env, args.size() > 3? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);

    std::vector<Value> list = args[0].as_list();
    int i                   = args[1].as_int();
    if (i > (int)list.size())
        Serial.println(INDEX_OUT_OF_RANGE);
    else
        list.insert(list.begin() + args[1].as_int(), args[2].as_int());
    return Value(list);
}

// Remove a value at an index from a list
Value remove(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("remove", remove), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);

    std::vector<Value> list = args[0].as_list();
    int i                   = args[1].as_int();
    if (list.empty() || i >= (int)list.size())
        Serial.println(INDEX_OUT_OF_RANGE);
    else
        list.erase(list.begin() + i);
    return Value(list);
}

// Get the length of a list
Value len(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("len", len), env, args.size() > 1?
    // TOO_MANY_ARGS : TOO_FEW_ARGS
    // );

    return Value(int(args[0].as_list().size()));
}

// Add an item to the end of a list
Value push(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() == 0)
        Serial.println(TOO_FEW_ARGS);
    // throw Error(Value("push", push), env, TOO_FEW_ARGS);
    for (size_t i = 1; i < args.size(); i++)
        args[0].push(args[i]);
    return args[0];
}

Value pop(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("pop", pop), env, args.size() > 1? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    return args[0].pop();
}

Value head(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("head", head), env, args.size() > 1? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    std::vector<Value> list = args[0].as_list();
    if (list.empty())
        Serial.println(INDEX_OUT_OF_RANGE);
    // throw Error(Value("head", head), env, INDEX_OUT_OF_RANGE);

    return list[0];
}

Value tail(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("tail", tail), env, args.size() > 1? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);

    std::vector<Value> result, list = args[0].as_list();

    for (size_t i = 1; i < list.size(); i++)
        result.push_back(list[i]);

    return Value(result);
}

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

// Value parse(std::vector<Value> &args, Environment &env) {
//   // Is not a special form, so we can evaluate our args.
//   eval_args(args, env);

//   if (args.size() != 1)
//     Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
//   // throw Error(Value("parse", parse), env, args.size() > 1? TOO_MANY_ARGS :
//   // TOO_FEW_ARGS);
//   if (args[0].get_type_name() != STRING_TYPE)
//     Serial.println(INVALID_ARGUMENT);
//   // throw Error(args[0], env, INVALID_ARGUMENT);
//   std::vector<Value> parsed = parse(args[0].as_string());

//   // if (parsed.size() == 1)
//   //     return parsed[0];
//   // else return Value(parsed);
//   return Value(parsed);
// }

Value replace(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 3)
        Serial.println(args.size() > 3 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("replace", replace), env, args.size() > 3? TOO_MANY_ARGS
    // : TOO_FEW_ARGS);

    String src = args[0].as_string();
    src.replace(args[1].as_string(), args[2].as_string());
    return Value::string(src);
}

Value display(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("display", display), env, args.size() > 1? TOO_MANY_ARGS
    // : TOO_FEW_ARGS);

    return Value::string(args[0].display());
}

Value debug(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("debug", debug), env, args.size() > 1? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);

    return Value::string(args[0].debug());
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

BUILTINFUNC(ard_delay, int delaytime = args[0].as_int(); delay(delaytime);
            ret = args[0];, 1)

BUILTINFUNC(ard_delaymicros, int delaytime = args[0].as_int();
            delayMicroseconds(delaytime); ret = args[0];, 1)

BUILTINFUNC(ard_millis, int m = millis(); ret = Value(m);, 0)

BUILTINFUNC(ard_micros, int m = micros(); ret = Value(m);, 0)

BUILTINFUNC(useq_pulse,
            // args: pulse width, phasor
            double pulseWidth = args[0].as_float();
            double phasor     = args[1].as_float();
            ret               = Value(pulseWidth < phasor ? 1.0 : 0.0);, 2)
// NOTE: this applies an fmod 1 to its input
BUILTINFUNC(useq_sqr, ret = Value(fmod(args[0].as_float(), 1.0) < 0.5 ? 1.0 : 0.0);
            , 1)

BUILTINFUNC(ard_sin, float m = sin(args[0].as_float()); ret = Value(m);, 1)
BUILTINFUNC(ard_cos, float m = cos(args[0].as_float()); ret = Value(m);, 1)
BUILTINFUNC(ard_tan, float m = tan(args[0].as_float()); ret = Value(m);, 1)

BUILTINFUNC(ard_abs, float m = abs(args[0].as_float()); ret = Value(m);, 1)
BUILTINFUNC(ard_min, float m = min(args[0].as_float(), args[1].as_float());
            ret = Value(m);, 2)
BUILTINFUNC(ard_max, float m = max(args[0].as_float(), args[1].as_float());
            ret = Value(m);, 2)
BUILTINFUNC(ard_pow, float m = pow(args[0].as_float(), args[1].as_float());
            ret = Value(m);, 2)
BUILTINFUNC(ard_sqrt, float m = sqrt(args[0].as_float()); ret = Value(m);, 1)
BUILTINFUNC(ard_map,
            float m = map(args[0].as_float(), args[1].as_float(), args[2].as_float(),
                          args[3].as_float(), args[4].as_float());
            ret     = Value(m);, 5)
BUILTINFUNC(ard_floor, double m = floor(args[0].as_float()); ret = Value(m);, 1)
BUILTINFUNC(ard_ceil, double m = ceil(args[0].as_float()); ret = Value(m);, 1)

BUILTINFUNC(zeros, int length = args[0].as_int();
            std::vector<Value> zeroList(length, Value(0)); ret = Value(zeroList);, 1)

// (timeit <function>) - returns time take to run in microseconds
BUILTINFUNC_NOEVAL(timeit, unsigned long ts = micros();
                   Interpreter::eval_in(args[0], env); ts = micros() - ts;
                   ret = Value(static_cast<int>(ts));, 1)

BUILTINFUNC(ard_digitalWrite, int pinNumber = args[0].as_int();
            int onOff = args[1].as_int(); digitalWrite(pinNumber, onOff);
            ret       = args[0];, 2)

BUILTINFUNC(ard_digitalRead, int pinNumber = args[0].as_int();
            int val = digitalRead(pinNumber); ret = Value(val);, 1)

BUILTINFUNC(
    useq_perf,
    String report = "fps0: ";
    report += env.get("fps").as_float();
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
    report += rp2040.getFreeHeap() / 1024;
    Serial.println(report);
    ret = Value();
    , 0)


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
    Environment::builtindefs["perf"] = Value("perf", builtin::useq_perf);

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
