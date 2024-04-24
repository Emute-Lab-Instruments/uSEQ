#ifndef BUILTIN_H_
#define BUILTIN_H_

#include "lisp/value.h"

// The type for a builtin function, which takes a list of values,
// and the environment to run the function in.
typedef Value (*Builtin)(std::vector<Value>&, Environment&);

#define BUILTINFUNC(__name__, __body__, __numArgs__)                                                         \
    Value __name__(std::vector<Value>& args, Environment& env)                                               \
    {                                                                                                        \
        eval_args(args, env);                                                                                \
        Value ret = Value();                                                                                 \
        if (args.size() != __numArgs__)                                                                      \
        {                                                                                                    \
            Serial.println(args.size() > __numArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS);                        \
            ret = Value::error();                                                                            \
        }                                                                                                    \
        else                                                                                                 \
        {                                                                                                    \
            __body__                                                                                         \
        }                                                                                                    \
        return ret;                                                                                          \
    }

#define BUILTINFUNC_VARGS(__name__, __body__, __minArgs__, __maxArgs__)                                      \
    Value __name__(std::vector<Value>& args, Environment& env)                                               \
    {                                                                                                        \
        eval_args(args, env);                                                                                \
        Value ret = Value();                                                                                 \
        if (args.size() < __minArgs__ || args.size() > __maxArgs__)                                          \
            Serial.println(args.size() > __maxArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS);                        \
        else                                                                                                 \
        {                                                                                                    \
            __body__                                                                                         \
        }                                                                                                    \
        return ret;                                                                                          \
    }

#define BUILTINFUNC_NOEVAL(__name__, __body__, __numArgs__)                                                  \
    Value __name__(std::vector<Value>& args, Environment& env)                                               \
    {                                                                                                        \
        Value ret = Value();                                                                                 \
        if (args.size() != __numArgs__)                                                                      \
            Serial.println(args.size() > __numArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS);                        \
        else                                                                                                 \
        {                                                                                                    \
            __body__                                                                                         \
        }                                                                                                    \
        return ret;                                                                                          \
    }

// This namespace contains all the definitions of builtin functions
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
        if (args[i] == Value::error())
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
    // throw Error(Value("defun", defun), env, args.size() > 3? TOO_MANY_ARGS : TOO_FEW_ARGS);

    if (args[1].get_type_name() != LIST_TYPE)
        Serial.println(INVALID_LAMBDA);
    // throw Error(Value("defun", defun), env, INVALID_LAMBDA);

    Value f = Value(args[1].as_list(), args[2], env);
    env.set(args[0].display(), f);
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
    // throw Error(Value("random", random), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);

    int low = args[0].as_int(), high = args[1].as_int();
    return Value((int)random(low, high));
}

// // Get the contents of a file
// Value read_file(std::vector<Value> &args, Environment &env) {
//     // Is not a special form, so we can evaluate our args.
//     eval_args(args, env);

//     if (args.size() != 1)
//         throw Error(Value("read-file", read_file), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

//     // return Value::string(content);
//     return Value::string(read_file_contents(args[0].as_string()));
// }

// // Write a string to a file
// Value write_file(std::vector<Value> &args, Environment &env) {
//     // Is not a special form, so we can evaluate our args.
//     eval_args(args, env);

//     if (args.size() != 2)
//         throw Error(Value("write-file", write_file), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

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
//         throw Error(Value("include", include), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

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
    if (ret == Value::error())
    {
        Serial.println("eval err");
        return Value::error();
    }
    if (args.size() != 1)
    {
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return Value();
    }
    // throw Error(Value("eval", eval), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
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
    // throw Error(Value("-", subtract), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
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
    // throw Error(Value("/", divide), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
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
    // throw Error(Value("%", remainder), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
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
    // throw Error(Value("=", eq), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
    return Value(int(args[0] == args[1]));
}

// Are two values not equal?
Value neq(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("!=", neq), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
    return Value(int(args[0] != args[1]));
}

// Is one number greater than another?
Value greater(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value(">", greater), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
    return Value(int(args[0] > args[1]));
}

// Is one number less than another?
Value less(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("<", less), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
    return Value(int(args[0] < args[1]));
}

// Is one number greater than or equal to another?
Value greater_eq(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value(">=", greater_eq), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
    return Value(int(args[0] >= args[1]));
}

// Is one number less than or equal to another?
Value less_eq(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("<=", less_eq), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
    return Value(int(args[0] <= args[1]));
}

// Get the type name of a value
Value get_type_name(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("type", get_type_name), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

    return Value::string(args[0].get_type_name());
}

// Cast an item to a float
Value cast_to_float(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value(FLOAT_TYPE, cast_to_float), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
    return args[0].cast_to_float();
}

// Cast an item to an int
Value cast_to_int(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value(INT_TYPE, cast_to_int), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
    return args[0].cast_to_int();
}

// Index a list
Value index(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("index", index), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);

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
    // throw Error(Value("insert", insert), env, args.size() > 3? TOO_MANY_ARGS : TOO_FEW_ARGS);

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
    // throw Error(Value("remove", remove), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);

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
    // throw Error(Value("pop", pop), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
    return args[0].pop();
}

Value head(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("head", head), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
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
    // throw Error(Value("tail", tail), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

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
                flattened.insert(flattened.end(), flattenedElement.begin(), flattenedElement.end());
            }
            else
            {
                flattened.push_back(evaluatedElement);
            }
        }
    }
    return Value(flattened);
}

Value parse(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("parse", parse), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
    if (args[0].get_type_name() != STRING_TYPE)
        Serial.println(INVALID_ARGUMENT);
    // throw Error(args[0], env, INVALID_ARGUMENT);
    std::vector<Value> parsed = ::parse(args[0].as_string());

    // if (parsed.size() == 1)
    //     return parsed[0];
    // else return Value(parsed);
    return Value(parsed);
}

Value replace(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 3)
        Serial.println(args.size() > 3 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("replace", replace), env, args.size() > 3? TOO_MANY_ARGS : TOO_FEW_ARGS);

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
    // throw Error(Value("display", display), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

    return Value::string(args[0].display());
}

Value debug(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("debug", debug), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

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

// uSEQ-specific builtins

} // namespace builtin
// end of builtin namespace

#endif // BUILTIN_H_
