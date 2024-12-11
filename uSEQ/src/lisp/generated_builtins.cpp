#include "generated_builtins.h"
#include "../utils.h"
#include "../utils/log.h"
#include "environment.h"
#include "interpreter.h"
#include "value.h"

namespace builtin
{

Value tail(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "tail";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    std::vector<Value> acc, list = args[0].as_list();
    for (size_t i = 1; i < list.size(); i++)
        acc.push_back(list[i]);
    result = Value(acc);
    return result;
}

Value vec(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "vec";

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    result       = Value::vector(args);
    return result;
}

Value zeros(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "zeros";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    int length   = args[0].as_int();
    std::vector<Value> zeroList(length, Value(0));
    result = Value(zeroList);
    return result;
}

Value defn(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "defn";

    // Checking number of args
    if (!(args.size() == 3))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 3, -1);
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_symbol()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a symbol",
                                         args[0].to_lisp_src());
        return Value::error();
    }
    if (!(args[1].is_sequential()))
    {
        report_error_wrong_specific_pred(
            user_facing_name, 2, "a sequential structure (e.g. a list or a vector)",
            args[1].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result              = Value::nil();
    String f_name             = args[0].display();
    std::vector<Value> params = args[1].as_sequential();
    Value body                = args[2];
    result                    = Value(params, body, env);
    env.set(f_name, result);
    return result;
}

Value list(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "list";

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    result       = Value(args);
    return result;
}

Value ard_floor(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "floor";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = Value(floor(args[0].as_float()));
    return result;
}

Value ard_ceil(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "ceil";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = Value(ceil(args[0].as_float()));
    return result;
}

Value do_block(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "do";

    // BODY
    Value result = Value::nil();
    Value acc;
    for (size_t i = 0; i < args.size(); i++)
        acc = args[i].eval(env);
    result = acc;
    return result;
}

Value neq(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "!=";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    result       = Value(int(args[0] != args[1]));
    return result;
}

Value ard_usin(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "usin";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = Value(0.5 + 0.5 * sin(args[0].as_float()));
    return result;
}

Value index(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "index";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(
            user_facing_name, 1, "a sequential structure (e.g. a list or a vector)",
            args[0].to_lisp_src());
        return Value::error();
    }
    if (!(!args[0].is_empty()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a non-empty list",
                                         args[0].to_lisp_src());
        return Value::error();
    }
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result            = Value::nil();
    std::vector<Value> list = args[0].as_vector();
    int i                   = args[1].as_int();
    if (i < list.size())
    {
        result = list[i];
    }
    else
    {
        report_custom_function_error(
            "index", "Index should be smaller than the size of the list.");
        return Value::error();
    }
    return result;
}

Value ard_cos(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "cos";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = Value(cos(args[0].as_float()));
    return result;
}

Value let_block(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "let";

    // Checking number of args
    if (!(args.size() >= 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::AtLeast, 2, -1);
        return Value::error();
    }

    // BODY
    Value result   = Value::nil();
    Value bindings = args[0];
    // Make sure bindings arg is a valid vector/list
    if (!(bindings.is_sequential()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a list or a vector",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    std::vector<Value> bindings_vec = bindings.as_vector();

    if (bindings_vec.size() % 2 != 0)
    {
        report_custom_function_error(
            user_facing_name, "Number of elements in the bindings vector should be "
                              "even, forming pairs of <name> <value>.");
        return Value::error();
    }

    // iterate bindings by twos and make sure all even elements are symbols
    for (size_t i = 0; i < bindings_vec.size() - 1; i += 2)
    {
        Value item = bindings_vec[i];
        if (!(item.is_symbol()))
        {
            report_custom_function_error(
                user_facing_name, "All even elements in the bindings vector are "
                                  "expected to be symbols, but element #" +
                                      String((i - 1) / 2) +
                                      " is not: " + item.to_lisp_src());
            return Value::error();
        }
    }

    // Make local env to store local bindings, deferring to the
    // linked parent scope for everything else
    Environment local_env;
    local_env.set_parent_scope(&env);

    // iterate bindings by twos and insert defs in local env
    for (size_t i = 0; i < bindings_vec.size() - 1; i += 2)
    {
        String name       = bindings_vec[i].str;
        Value body        = bindings_vec[i + 1];
        Value evaled_body = body.eval(local_env);
        if (evaled_body.is_error())
        {
            report_custom_function_error(
                user_facing_name,
                "The definition for entry #" + String((i - 1) / 2) +
                    " evaluates to an error: " + body.to_lisp_src());
            // println("- " + body.display());
        }
        local_env.set(name, evaled_body);
    }

    // Eval the remaining body exprs in that env
    // NOTE: it starts at 1
    for (int i = 1; i < args.size(); i++)
    {
        Value expr = args[i];
        result     = expr.eval(local_env);
    }
    return result;
}

Value eq(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "=";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    result       = Value(int(args[0] == args[1]));
    return result;
}

Value ard_abs(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "abs";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = Value(abs(args[0].as_float()));
    return result;
}

Value ard_millis(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "ard_millis";

    // Checking number of args
    if (!(args.size() == 0))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 0, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    int m        = millis();
    result       = Value(m);
    return result;
}

Value sum(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "+";

    // Checking number of args
    if (!(args.size() >= 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::AtLeast, 1, -1);
        return Value::error();
    }

    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
        // Check all-pred(s)

        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    Value acc    = args[0];
    for (size_t i = 1; i < args.size(); i++)
    {
        acc = acc + args[i];
    }
    result = acc;
    return result;
}

Value for_loop(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "for";

    // Checking number of args
    if (!(args.size() >= 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::AtLeast, 2, -1);
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_symbol()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a symbol",
                                         args[0].to_lisp_src());
        return Value::error();
    }
    if (!(args[1].is_list()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a list",
                                         args[1].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    Value acc;
    std::vector<Value> list = args[1].eval(env).as_list();
    for (size_t i = 0; i < list.size(); i++)
    {
        env.set(args[0].as_atom(), list[i]);
        for (size_t j = 1; j < args.size() - 1; j++)
            args[j].eval(env);
        acc = args[args.size() - 1].eval(env);
    }
    result = acc;
    return result;
}

Value pop(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "last";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    result       = args[0].pop();
    return result;
}

Value ard_min(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "min";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
        // Check all-pred(s)

        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    result       = Value(std::min(args[0].as_float(), args[1].as_float()));
    return result;
}

Value push(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "push";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    for (size_t i = 1; i < args.size(); i++)
        args[0].push(args[i]);
    result = args[0];
    return result;
}

Value greater(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = ">";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    result       = Value(int(args[0] > args[1]));
    return result;
}

Value product(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "*";

    // Checking number of args
    if (!(args.size() >= 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::AtLeast, 1, -1);
        return Value::error();
    }

    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
        // Check all-pred(s)

        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    Value acc    = args[0];
    for (size_t i = 1; i < args.size(); i++)
    {
        Value num = args[i];
        if (num == Value(0))
        {
            return Value(0);
        }
        else
        {
            acc = acc * args[i];
        }
    }
    result = acc;
    return result;
}

Value replace(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "replace";

    // Checking number of args
    if (!(args.size() == 3))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 3, -1);
        return Value::error();
    }

    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
        // Check all-pred(s)

        if (!(args[i].is_string()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a string",
                                        args[i].to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    String src   = args[0].as_string();
    src.replace(args[1].as_string(), args[2].as_string());
    return Value::string(src);
    return result;
}

Value ard_sin(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "sin";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = Value(sin(args[0].as_float()));
    return result;
}

Value ard_ucos(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "ucos";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = Value(0.5 + 0.5 * cos(args[0].as_float()));
    return result;
}

Value eval(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "eval";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = args[0].eval(env);
    return result;
}

Value println(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "println";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    String s;
    if (args[0].is_string())
    {
        s = args[0].as_string();
    }
    else
    {
        s = args[0].display();
    }
    message_editor(s);
    return result;
}

Value cast_to_int(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "int";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = args[0].cast_to_int();
    return result;
}

Value remainder(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "%";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
        // Check all-pred(s)

        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    result       = args[0] % args[1];
    return result;
}

Value subtract(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "-";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
        // Check all-pred(s)

        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    result       = args[0] - args[1];
    return result;
}

Value define(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "define";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_symbol()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a symbol",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    String name  = args[0].display();
    // NOTE: body is unevalled still
    Value body = args[1];
    env.set_expr(name, body);
    env.set(name, body.eval(env));
    result = Value::atom(name);
    return result;
}

Value ard_pow(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "pow";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
        // Check all-pred(s)

        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result   = Value::nil();
    float exponent = args[0].as_float();
    float base     = args[1].as_float();
    result         = Value(pow(base, exponent));
    return result;
}

Value while_loop(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "while";

    // Checking number of args
    if (!(args.size() >= 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::AtLeast, 1, -1);
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    Value acc;
    while (args[0].eval(env).as_bool())
    {
        for (size_t i = 1; i < args.size() - 1; i++)
            args[i].eval(env);
        acc = args[args.size() - 1].eval(env);
    }
    result = acc;
    return result;
}

Value remove(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "remove";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_list()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a list",
                                         args[0].to_lisp_src());
        return Value::error();
    }
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result            = Value::nil();
    std::vector<Value> list = args[0].as_list();
    int i                   = args[1].as_int();
    if (list.empty() || i >= (int)list.size())
        report_error(INDEX_OUT_OF_RANGE);
    else
        list.erase(list.begin() + i);
    result = Value(list);
    return result;
}

Value scope(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "scope";

    // BODY
    Value result  = Value::nil();
    Environment e = env;
    Value acc;
    for (size_t i = 0; i < args.size(); i++)
        acc = args[i].eval(e);
    result = acc;
    return result;
}

Value debug(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "debug";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    result       = Value::string(args[0].to_lisp_src());
    return result;
}

Value less_eq(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "<=";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    result       = Value(int(args[0] <= args[1]));
    return result;
}

Value ard_max(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "max";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
        // Check all-pred(s)

        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    result       = Value(std::max(args[0].as_float(), args[1].as_float()));
    return result;
}

Value ard_delay(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "ard_delay";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result  = Value::nil();
    int delaytime = args[0].as_int();
    delay(delaytime);
    result = args[0];
    return result;
}

Value timeit(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "timeit";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result     = Value::nil();
    unsigned long ts = micros();
    args[0].eval(env);
    ts     = micros() - ts;
    result = Value(static_cast<int>(ts));
    return result;
}

Value ard_map(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "scale";

    // Checking number of args
    if (!(3 <= args.size() <= 5))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::Between, 3, 5);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    float phasor = args.back().as_float();

    float in_min, in_max, out_min, out_max;

    if (args.size() == 5)
    {
        in_min = args[0].as_float();
        in_max = args[1].as_float();

        out_min = args[2].as_float();
        out_max = args[3].as_float();
    }
    else
    {
        in_min = 0.0;
        in_max = 1.0;

        out_min = args[0].as_float();
        out_max = args[1].as_float();
    }

    return Value(scale_value(phasor, in_min, in_max, out_min, out_max));
}

Value head(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "first";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_list()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a list",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result            = Value::nil();
    std::vector<Value> list = args[0].as_list();
    if (list.empty())
    {
        report_error(INDEX_OUT_OF_RANGE);
    }
    else
    {
        result = list[0];
    }
    return result;
}

Value get_type_name(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "type";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    result       = Value::string(args[0].get_type_name());
    return result;
}

Value b_to_u(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "b->u";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = args[0].as_float() * 0.5 + 0.5;
    return result;
}

Value less(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "<";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    result       = Value(int(args[0] < args[1]));
    return result;
}

Value ard_micros(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "ard_micros";

    // Checking number of args
    if (!(args.size() == 0))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 0, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    int m        = micros();
    result       = Value(m);
    return result;
}

Value divide(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "/";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
        // Check all-pred(s)

        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    result       = args[0] / args[1];
    return result;
}

Value ard_delaymicros(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "ard_delaymicros";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result  = Value::nil();
    int delaytime = args[0].as_int();
#ifdef ARDUINO
    delayMicroseconds(delaytime);
#endif // ARDUINO
    result = args[0];
    return result;
}

Value insert(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "insert";

    // Checking number of args
    if (!(args.size() == 3))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 3, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_list()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a list",
                                         args[0].to_lisp_src());
        return Value::error();
    }
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result            = Value::nil();
    std::vector<Value> list = args[0].as_list();
    int i                   = args[1].as_int();
    if (i < list.size())
        ::println(INDEX_OUT_OF_RANGE);
    else
        list.insert(list.begin() + args[1].as_int(), args[2].as_int());
    result = Value(list);
    return result;
}

Value get_expr(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "get-expr";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_symbol()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a symbol",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result              = Value::nil();
    String name               = args[0].display();
    std::optional<Value> expr = env.get_expr(name);
    if (expr)
    {
        result = expr.value();
    }
    else
    {
        report_user_warning("(**get-expr**) Expression for " + name +
                            " wasn't found.");
    }
    return result;
}

Value ard_sqrt(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "sqrt";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = Value(sqrt(args[0].as_float()));
    return result;
}

Value display(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "display";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    result       = Value::string(args[0].display());
    return result;
}

Value greater_eq(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = ">=";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    result       = Value(int(args[0] >= args[1]));
    return result;
}

Value ard_lerp(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "lerp";

    // Checking number of args
    if (!(args.size() == 5))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 5, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();
    result = Value(lerp(args[0].as_float(), args[1].as_float(), args[0].as_float()));
    return result;
}

Value quote(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "quote";

    // BODY
    Value result = Value::nil();
    result       = Value(args);
    return result;
}

Value cast_to_float(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "float";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = Value(args[0].as_float());
    return result;
}

Value len(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "len";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_list()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a list",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = Value((int)args[0].as_list().size());
    return result;
}

Value set(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "set";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_symbol()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a symbol",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    String name  = args[0].display();
    // NOTE: body is unevalled still
    Value val = args[1].eval(env);
    env.set(name, val);
    env.unset_expr(name);
    result = val;
    return result;
}

Value defun(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "defun";

    // Checking number of args
    if (!(args.size() == 3))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 3, -1);
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_symbol()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a symbol",
                                         args[0].to_lisp_src());
        return Value::error();
    }
    if (!(args[1].is_sequential()))
    {
        report_error_wrong_specific_pred(
            user_facing_name, 2, "a sequential structure (e.g. a list or a vector)",
            args[1].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result              = Value::nil();
    String f_name             = args[0].display();
    std::vector<Value> params = args[1].as_sequential();
    Value body                = args[2];
    result                    = Value(params, body, env);
    env.set(f_name, result);
    return result;
}

Value def(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "def";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_symbol()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a symbol",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    String name  = args[0].display();
    // NOTE: body is unevalled still
    Value body = args[1];
    env.set_expr(name, body);
    env.set(name, body.eval(env));
    result = Value::atom(name);
    return result;
}

Value useq_sqr(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "sqr";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = Value(fmod(args[0].as_float(), 1.0) < 0.5 ? 1.0 : 0.0);
    return result;
}

Value defs(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "defs";

    // Checking number of args
    if (!(args.size() >= 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::AtLeast, 2, -1);
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    if (args.size() % 2 != 0)
    {
        report_custom_function_error(
            user_facing_name,
            "Number of arguments should be even, forming pairs of <name> <value>.");
    }
    // iterate by twos and make sure all even args are symbols
    for (size_t i = 0; i < args.size() - 1; i += 2)
    {
        if (!(args[i].is_symbol()))
        {
            report_error_wrong_specific_pred(user_facing_name, i + 1, "a symbol ",
                                             args[i].to_lisp_src());
            return Value::error();
        }
    }
    std::vector<Value> names;
    for (size_t i = 0; i < args.size() - 1; i += 2)
    {
        String name = args[i].display();
        // NOTE: body is unevalled still
        Value body = args[i + 1];
        env.set_expr(name, body);
        env.set(name, body.eval(env));
        names.push_back(Value::atom(name));
    }
    result = Value::vector(names);
    return result;
}

Value lambda(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "lambda";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_vector()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a vector",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = Value(args[0].as_vector(), args[1], env);
    return result;
}

Value useq_pulse(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "pulse";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].to_lisp_src());
        return Value::error();
    }
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    // args: pulse width, phasor
    double pulseWidth = args[0].as_float();
    double phasor     = args[1].as_float();
    result            = Value(phasor < pulseWidth ? 1.0 : 0.0);
    return result;
}

Value u_to_b(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "u->b";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = args[0].as_float() * 2 - 1;
    return result;
}

Value ard_tan(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "tan";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = Value(tan(args[0].as_float()));
    return result;
}

Value if_then_else(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "if";

    // Checking number of args
    if (!(args.size() == 3))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 3, -1);
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    if (args[0].eval(env).as_bool())
        result = args[1].eval(env);
    else
        result = args[2].eval(env);
    return result;
}

} // namespace builtin
