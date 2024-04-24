#include "interpreter.h"
// #include "lisp/library.cpp"
#include "lisp/configure.h"
#include "lisp/parser.h"
#include "lisp/value.h"
#include "utils/log.h"
#include <cmath>

// MACROS
#define BUILTINFUNC(__name__, __body__, __numArgs__)                                                         \
    Value __name__(std::vector<Value>& args, Environment& env)                                               \
    {                                                                                                        \
        eval_args(args, env);                                                                                \
        Value ret = Value();                                                                                 \
        if (args.size() != __numArgs__)                                                                      \
        {                                                                                                    \
            println(args.size() > __numArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS);                               \
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
            println(args.size() > __maxArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS);                               \
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
            println(args.size() > __numArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS);                               \
        else                                                                                                 \
        {                                                                                                    \
            __body__                                                                                         \
        }                                                                                                    \
        return ret;                                                                                          \
    }

// Create a lambda function (SPECIAL FORM)
Value lambda(std::vector<Value>& args, Environment& env)
{
    if (args.size() < 2)
        println(TOO_FEW_ARGS);

    // throw Error(Value("lambda", lambda), env, TOO_FEW_ARGS);

    if (args[0].get_type_name() != LIST_TYPE)
        println(INVALID_LAMBDA);

    // throw Error(Value("lambda", lambda), env, INVALID_LAMBDA);

    return Value(args[0].as_list(), args[1], env);
}

// if-else (SPECIAL FORM)
Value if_then_else(std::vector<Value>& args, Environment& env)
{
    if (args.size() != 3)
        println(args.size() > 3 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    if (args[0].eval(env).as_bool())
        return args[1].eval(env);
    else
        return args[2].eval(env);
}

// Define a variable with a value (SPECIAL FORM)
Value define(std::vector<Value>& args, Environment& env)
{
    if (args.size() != 2)
        println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);

    Value result = args[1].eval(env);
    env.set(args[0].display(), result);
    return result;
}

Value set(std::vector<Value>& args, Environment& env)
{
    if (args.size() != 2)
        println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);

    Value result = args[1].eval(env);
    env.set_global(args[0].display(), result);
    return result;
}

// Define a function with parameters and a result expression (SPECIAL FORM)
Value defun(std::vector<Value>& args, Environment& env)
{
    if (args.size() != 3)
        println(args.size() > 3 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("defun", defun), env, args.size() > 3? TOO_MANY_ARGS : TOO_FEW_ARGS);

    if (args[1].get_type_name() != LIST_TYPE)
        println(INVALID_LAMBDA);
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
    for (size_t i = 0; i < args.size(); i++)
    {
        args[i].eval(env);
    }

    return args[args.size() - 1];
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
        println(TOO_FEW_ARGS);
    // throw Error(Value("print", print), env, TOO_FEW_ARGS);

    Value acc;
    for (size_t i = 0; i < args.size(); i++)
    {
        acc = args[i];
        print(acc.display().c_str());
        // std::cout << acc.display();
        if (i < args.size() - 1)
            // std::cout << " ";
            print(" ");
    }
    // std::cout << std::endl;
    println();
    return acc;
}

// Get a random number between two numbers inclusively
Value gen_random(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
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
// TODO is this needed?
/* Value eval(std::vector<Value>& args, Environment& env) */
/* { */
/*     // Is not a special form, so we can evaluate our args. */
/*     eval_args(args, env); */
/*     if (ret == Value::error()) */
/*     { */
/*         println("eval err"); */
/*         return Value::error(); */
/*     } */
/*     if (args.size() != 1) */
/*     { */
/*         println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS); */
/*         return Value(); */
/*     } */
/*     // throw Error(Value("eval", eval), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS); */
/*     else */
/*     { */
/*         return args[0].eval(env); */
/*     } */

/*     return result; */
/* } */

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
        println(TOO_FEW_ARGS);
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
        println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("-", subtract), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
    return args[0] - args[1];
}

// Multiply several values
Value product(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() < 2)
        println(TOO_FEW_ARGS);
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
        println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("/", divide), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
    auto result = args[0] / args[1];
    return result;
}

// Get the remainder of values
Value my_remainder(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
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
        println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("=", eq), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
    return Value(int(args[0] == args[1]));
}

// Are two values not equal?
Value neq(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("!=", neq), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
    return Value(int(args[0] != args[1]));
}

// Is one number greater than another?
Value greater(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value(">", greater), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
    return Value(int(args[0] > args[1]));
}

// Is one number less than another?
Value less(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("<", less), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
    return Value(int(args[0] < args[1]));
}

// Is one number greater than or equal to another?
Value greater_eq(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value(">=", greater_eq), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
    return Value(int(args[0] >= args[1]));
}

// Is one number less than or equal to another?
Value less_eq(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("<=", less_eq), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
    return Value(int(args[0] <= args[1]));
}

// Get the type name of a value
Value get_type_name(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("type", get_type_name), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

    return Value::string(args[0].get_type_name());
}

// Cast an item to a float
Value cast_to_float(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value(FLOAT_TYPE, cast_to_float), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
    return args[0].cast_to_float();
}

// Cast an item to an int
Value cast_to_int(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value(INT_TYPE, cast_to_int), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
    return args[0].cast_to_int();
}

// Index a list
Value my_index(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("index", index), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);

    std::vector<Value> list = args[0].as_list();
    int i                   = args[1].as_int();
    if (list.empty() || i >= (int)list.size())
    {
        println(INDEX_OUT_OF_RANGE);
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
        println(args.size() > 3 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("insert", insert), env, args.size() > 3? TOO_MANY_ARGS : TOO_FEW_ARGS);

    std::vector<Value> list = args[0].as_list();
    int i                   = args[1].as_int();
    if (i > (int)list.size())
        println(INDEX_OUT_OF_RANGE);
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
        println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("remove", remove), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);

    std::vector<Value> list = args[0].as_list();
    int i                   = args[1].as_int();
    if (list.empty() || i >= (int)list.size())
        println(INDEX_OUT_OF_RANGE);
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
        println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
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
        println(TOO_FEW_ARGS);
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
        println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("pop", pop), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
    return args[0].pop();
}

Value head(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("head", head), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
    std::vector<Value> list = args[0].as_list();
    if (list.empty())
        println(INDEX_OUT_OF_RANGE);
    // throw Error(Value("head", head), env, INDEX_OUT_OF_RANGE);

    return list[0];
}

Value tail(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("tail", tail), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

    std::vector<Value> result, list = args[0].as_list();

    for (size_t i = 1; i < list.size(); i++)
        result.push_back(list[i]);

    return Value(result);
}

/* Value parse(std::vector<Value>& args, Environment& env) */
/* { */
/*     // Is not a special form, so we can evaluate our args. */
/*     eval_args(args, env); */

/*     if (args.size() != 1) */
/*         println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS); */
/*     // throw Error(Value("parse", parse), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS); */
/*     if (args[0].get_type_name() != STRING_TYPE) */
/*         println(INVALID_ARGUMENT); */
/*     // throw Error(args[0], env, INVALID_ARGUMENT); */
/*     std::vector<Value> parsed = parse(args[0].as_string()); */

/*     // if (parsed.size() == 1) */
/*     //     return parsed[0]; */
/*     // else return Value(parsed); */
/*     return Value(parsed); */
/* } */

Value replace(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 3)
        println(args.size() > 3 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
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
        println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("display", display), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

    return Value::string(args[0].display());
}

Value debug(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
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
        println(MISMATCHED_TYPES);
    // throw Error(low, env, MISMATCHED_TYPES);
    if (high.get_type_name() != INT_TYPE && high.get_type_name() != FLOAT_TYPE)
        println(MISMATCHED_TYPES);
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

// extra arduino api functions

/* BUILTINFUNC_NOEVAL(useq_q0, env.set_global("q-form", args[0]); useq::q0AST = { args[0] };, 1) */

// ANALOG OUTS

#ifdef MIDIOUT
// midi drum out
BUILTINFUNC(
    useq_mdo, int midiNote = args[0].as_int();
    if (args[1] != 0) { useqMDOMap[midiNote] = args[1]; } else { useqMDOMap.erase(midiNote); }, 2)
#endif

#if defined(USE_ARDUINO_PIO)
BUILTINFUNC(ard_pinMode, int pinNumber = args[0].as_int(); int onOff = args[1].as_int();
            pinMode(pinNumber, onOff);, 2)

BUILTINFUNC(
    ard_useqdw,
    if (args[1] == Value::error()) {
        println("useqdw arg err");
        ret = args[1];
    } else {
        int pin     = digital_out_pin(args[0].as_int());
        int led_pin = digital_out_LED_pin(args[0].as_int());
        int val     = args[1].as_int();
#ifdef DIGI_OUT_INVERT
        digitalWrite(pin, 1 - val);
#else
        digitalWrite(pin, val);
#endif
        digitalWrite(led_pin, val);
    },
    2)

BUILTINFUNC(
    ard_useqaw,
    if (args[1] == Value::error()) {
        println("useqaw arg err");
        ret = args[1];
    } else {
        int pin     = analog_out_pin(args[0].as_int());
        int led_pin = analog_out_LED_pin(args[0].as_int());
        int val     = args[1].as_float() * 2047.0;
        analogWrite(pin, val);
        analogWrite(led_pin, val);
    },
    2)

BUILTINFUNC(ard_digitalWrite, int pinNumber = args[0].as_int(); int onOff = args[1].as_int();
            digitalWrite(pinNumber, onOff); ret = args[0];, 2)

BUILTINFUNC(ard_digitalRead, int pinNumber = args[0].as_int(); int val = digitalRead(pinNumber);
            ret = Value(val);, 1)
#endif // if defined(USE_ARDUINO_PIO)

#ifdef ARDUINO
// (timeit <function>) - returns time take to run in microseconds
BUILTINFUNC_NOEVAL(timeit, unsigned long ts = micros(); args[0].eval(env); ts = micros() - ts;
                   ret = Value(static_cast<int>(ts));, 1)

BUILTINFUNC(ard_delay, int delaytime = args[0].as_int(); delay(delaytime); ret = args[0];, 1)

BUILTINFUNC(ard_delaymicros, int delaytime = args[0].as_int(); delayMicroseconds(delaytime); ret = args[0];
            , 1)

BUILTINFUNC(ard_millis, int m = millis(); ret = Value(m);, 0)

BUILTINFUNC(ard_micros, int m = micros(); ret = Value(m);, 0)

BUILTINFUNC(ard_sin, float m = sin(args[0].as_float()); ret = Value(m);, 1)
BUILTINFUNC(ard_cos, float m = cos(args[0].as_float()); ret = Value(m);, 1)
BUILTINFUNC(ard_tan, float m = tan(args[0].as_float()); ret = Value(m);, 1)

BUILTINFUNC(ard_abs, float m = abs(args[0].as_float()); ret = Value(m);, 1)
BUILTINFUNC(ard_min, float m = min(args[0].as_float(), args[1].as_float()); ret = Value(m);, 2)
BUILTINFUNC(ard_max, float m = max(args[0].as_float(), args[1].as_float()); ret = Value(m);, 2)
BUILTINFUNC(ard_pow, float m = pow(args[0].as_float(), args[1].as_float()); ret = Value(m);, 2)
BUILTINFUNC(ard_sqrt, float m = sqrt(args[0].as_float()); ret = Value(m);, 1)
BUILTINFUNC(ard_map, float m = map(args[0].as_float(), args[1].as_float(), args[2].as_float(),
                                   args[3].as_float(), args[4].as_float());
            ret = Value(m);, 5)
BUILTINFUNC(ard_floor, double m = floor(args[0].as_float()); ret = Value(m);, 1)
BUILTINFUNC(ard_ceil, double m = ceil(args[0].as_float()); ret = Value(m);, 1)

BUILTINFUNC(ard_pinMode, int pinNumber = args[0].as_int(); int onOff = args[1].as_int();
            pinMode(pinNumber, onOff);, 2)

BUILTINFUNC(perf,

            String report = "fps0: ";
            report += env.get("fps").as_float();
            // report += ", fps1: ";
            // report += env.get("perf_fps1").as_int();
            report += ", qt: ";
            report += env.get("qt").as_float(); report += ", in: "; report += env.get("perf_in").as_int();
            report += ", upd_tm: "; report += env.get("perf_time").as_int(); report += ", out: ";
            report += env.get("perf_out").as_int(); report += ", get: ";
            report += env.get("perf_get").as_float(); report += ", parse: ";
            report += env.get("perf_parse").as_float(); report += ", run: ";
            report += env.get("perf_run").as_float(); report += ", ts1: ";
            report += env.get("perf_ts1").as_float(); report += ", heap free: ";
            report += rp2040.getFreeHeap() / 1024; println(report); ret = Value();, 0)
#endif // ARDUINO

BUILTINFUNC(useq_pulse,
            // args: pulse width, phasor
            double pulseWidth = args[0].as_float();
            double phasor = args[1].as_float(); ret = Value(pulseWidth < phasor ? 1.0 : 0.0);, 2)
BUILTINFUNC(useq_sqr, ret = Value(args[0].as_float() < 0.5 ? 1.0 : 0.0);, 1)

// TODO D-R-Y
// this version doesn't work properly with phasors - freezes at >2x speedup
// BUILTINFUNC_NOEVAL(useq_fast,
//             double factor = args[0].eval(env).as_float();
//             Value expr = args[1];
//             // store the current time to reset later
//             double currentTime = env.get("time").as_float();
//             // update the interpreter's time just for this expr
//             double newTime = currentTime * factor;
//             useq::setTime((size_t)newTime);
//             double evaled_expr = expr.eval(env).as_float();
//             ret = Value(evaled_expr);
//             // restore the interpreter's time
//             useq::setTime((size_t)currentTime);
//             , 2)

BUILTINFUNC(zeros, int length = args[0].as_int(); std::vector<Value> zeroList(length, Value(0));
            ret = Value(zeroList);, 1)

void eval_args(std::vector<Value>& args, Environment& env)
{
    for (size_t i = 0; i < args.size(); i++)
    {
        args[i] = args[i].eval(env);
        if (args[i] == Value::error())
        {
            error("eval args error");
        }
    }
}

// Static Class-wide flag
bool Interpreter::m_builtindefs_init = false;

Interpreter::Interpreter()
{
    if (!m_builtindefs_init)
    {
        loadBuiltinDefs();
    }
}

// Allow external code to add definitions to m_builtindefs
// void Interpreter::insert_external_builtins(ExternalBuiltinInserter f) { f(builtindefs); }

Value parse(const String& code);
void set(String, Value);

String Interpreter::eval(const String& code) { return eval_in(code, m_environment); }

String Interpreter::eval_in(const String& code, Environment env)
{
    Value tree   = parse(code);
    Value result = eval_in(tree, env);
    return result.display();
};

Value Interpreter::eval(Value v) { return eval_in(v, m_environment); }
Value Interpreter::eval_in(Value v, Environment env) { return v.eval(env); }

void Interpreter::set(String name, Value val) { m_environment.set(name, val); }

Value builtin_set(std::vector<Value>& args, Environment& env)
{
    if (args.size() != 2)
        println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);

    Value result = args[1].eval(env);
    // TODO should this be set_global? it bubbles up mutatively
    env.set_global(args[0].display(), result);
    return result;
}

void Interpreter::loadBuiltinDefs()
{
    if (!m_builtindefs_init)
    {
        // String operations
        builtindefs["debug"]   = Value("debug", debug);
        builtindefs["replace"] = Value("replace", replace);
        builtindefs["display"] = Value("display", display);

        // Casting operations
        builtindefs["int"]   = Value("int", cast_to_int);
        builtindefs["float"] = Value("float", cast_to_float);

        // Constants
        builtindefs["endl"] = Value::string("\n");

        // FIXME delete
        // builtindefs["HiTEST123DELETEME"] = Value(1);

        // sequencing

        builtindefs["pulse"] = Value("pulse", useq_pulse);
        builtindefs["sqr"]   = Value("sqr", useq_sqr);

        // Meta operations
        // builtindefs["eval"]  = Value("eval", eval);
        builtindefs["type"] = Value("type", get_type_name);
        // builtindefs["parse"] = Value("parse", parse);

        // Special forms
        builtindefs["do"]     = Value("do", do_block);
        builtindefs["if"]     = Value("if", if_then_else);
        builtindefs["for"]    = Value("for", for_loop);
        builtindefs["while"]  = Value("while", while_loop);
        builtindefs["scope"]  = Value("scope", scope);
        builtindefs["quote"]  = Value("quote", quote);
        builtindefs["defun"]  = Value("defun", defun);
        builtindefs["define"] = Value("define", define);
        builtindefs["lambda"] = Value("lambda", lambda);

        // Comparison operations
        builtindefs["="]  = Value("=", eq);
        builtindefs["!="] = Value("!=", neq);
        builtindefs[">"]  = Value(">", greater);
        builtindefs["<"]  = Value("<", less);
        builtindefs[">="] = Value(">=", greater_eq);
        builtindefs["<="] = Value("<=", less_eq);

        // Arithmetic operations
        builtindefs["+"] = Value("+", sum);
        builtindefs["-"] = Value("-", subtract);
        builtindefs["*"] = Value("*", product);
        builtindefs["/"] = Value("/", divide);
        // NOTE `remainder` was overloaded otherwise
        builtindefs["%"] = Value("%", my_remainder);

        // List operations
        builtindefs["list"]   = Value("list", list);
        builtindefs["insert"] = Value("insert", insert);
        builtindefs["index"]  = Value("index", my_index);
        builtindefs["remove"] = Value("remove", remove);

        builtindefs["len"] = Value("len", len);

        builtindefs["push"]  = Value("push", push);
        builtindefs["pop"]   = Value("pop", pop);
        builtindefs["head"]  = Value("head", head);
        builtindefs["tail"]  = Value("tail", tail);
        builtindefs["first"] = Value("first", head);
        builtindefs["last"]  = Value("last", pop);
        builtindefs["range"] = Value("range", range);

        // List generators

        builtindefs["zeros"] = Value("zeros", zeros);

        // Functional operations
        builtindefs["map"]    = Value("map", map_list);
        builtindefs["filter"] = Value("filter", filter_list);
        builtindefs["reduce"] = Value("reduce", reduce_list);

// IO operations
#if defined(USE_STD)
        builtindefs["print"]  = Value("print", print);
        builtindefs["random"] = Value("random", gen_random);
#endif

#ifdef MIDIOUT
        builtindefs["mdo"] = Value("mdo", useq_mdo);
#endif

#ifdef MUSICTHING
        builtindefs["knob"]  = Value("knob", useq_mt_knob);
        builtindefs["knobx"] = Value("knobx", useq_mt_knobx);
        builtindefs["knoby"] = Value("knoby", useq_mt_knoby);
        builtindefs["swz"]   = Value("swz", useq_mt_swz);
#endif

#if defined(ARDUINO)
        // utility
        builtindefs["timeit"] = Value("timeit", timeit);

        builtindefs["pm"]     = Value("pm", ard_pinMode);
        builtindefs["delay"]  = Value("delay", ard_delay);
        builtindefs["delaym"] = Value("delaym", ard_delaymicros);
        builtindefs["millis"] = Value("millis", ard_millis);
        builtindefs["micros"] = Value("micros", ard_micros);
        builtindefs["perf"]   = Value("perf", perf);
        builtindefs["in1"]    = Value("in1", useq_in1);
        builtindefs["in2"]    = Value("in2", useq_in2);
        builtindefs["gin1"]   = Value("gin1", useq_in1);
        builtindefs["gin2"]   = Value("gin2", useq_in2);
        builtindefs["ain1"]   = Value("ain1", useq_ain1);
        builtindefs["ain2"]   = Value("ain2", useq_ain2);
        builtindefs["swm"]    = Value("swm", useq_swm);
        builtindefs["swt"]    = Value("swt", useq_swt);
        builtindefs["swr"]    = Value("swr", useq_swr);
        builtindefs["rot"]    = Value("rot", useq_rot);
        builtindefs["ssin"]   = Value("ssin", useq_ssin);
        // arduino math
        builtindefs["sin"] = Value("sin", ard_sin);
        builtindefs["cos"] = Value("cos", ard_cos);
        builtindefs["tan"] = Value("tan", ard_tan);
        builtindefs["abs"] = Value("abs", ard_abs);

        builtindefs["min"]   = Value("min", ard_min);
        builtindefs["max"]   = Value("max", ard_max);
        builtindefs["pow"]   = Value("pow", ard_pow);
        builtindefs["sqrt"]  = Value("sqrt", ard_sqrt);
        builtindefs["scale"] = Value("scale", ard_map);

        builtindefs["floor"] = Value("floor", ard_floor);
        builtindefs["ceil"]  = Value("ceil", ard_ceil);
#endif

#if defined(USE_ARDUINO_PIO)
        builtindefs["set"]    = Value("set", builtin_set);
        builtindefs["useqdw"] = Value("useqdw", ard_useqdw);
        builtindefs["useqaw"] = Value("useqaw", ard_useqaw);
        builtindefs["dw"]     = Value("dw", ard_digitalWrite);
        builtindefs["dr"]     = Value("dr", ard_digitalRead);

        builtindefs["a1"] = Value("a1", a1);
        builtindefs["a2"] = Value("a2", a2);
        builtindefs["a3"] = Value("a3", a3);
        builtindefs["a4"] = Value("a4", a4);
        builtindefs["a5"] = Value("a5", a5);
        builtindefs["a6"] = Value("a6", a6);
        builtindefs["d1"] = Value("d1", d1);
        builtindefs["d2"] = Value("d2", d2);
        builtindefs["d3"] = Value("d3", d3);
        builtindefs["d4"] = Value("d4", d4);
        builtindefs["d5"] = Value("d5", d5);
        builtindefs["d6"] = Value("d6", d6);
        builtindefs["q0"] = Value("q0", useq_q0);
        builtindefs["s1"] = Value("s1", s1);
        builtindefs["s2"] = Value("s2", s2);
        builtindefs["s3"] = Value("s3", s3);
        builtindefs["s4"] = Value("s4", s4);
        builtindefs["s5"] = Value("s5", s5);
        builtindefs["s6"] = Value("s6", s6);
        builtindefs["s7"] = Value("s7", s7);
        builtindefs["s8"] = Value("s8", s8);
#endif

        // Set flag so that this doesn't trigger again
        m_builtindefs_init = true;
    }
}
