#include "modulisp.h"

Value ModuLispInterpreter::useq_eval_at_time(std::vector<Value> &args,
                                             Environment &env) {
    constexpr const char *user_facing_name = "eval-at-time";

    // Checking number of args
    // if (!(2 <= args.size() <= 3))
    if (!(args.size() == 2)) {
        // error_wrong_num_args(user_facing_name, static_cast<int>(args.size()),
        //                      NumArgsComparison::Between, 2, 3);
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
    args[0] = args[0].eval(env);
    if (args[0].is_error()) {
        report_error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    }

    // NOTE: go from seconds to micros for internal calculations
    double time = args[0].as_float() * 1e+6;

    // BODY
    return eval_at_time(args[1], env, time);
}

Value ModuLispInterpreter::eval_at_time(Value &expr, Environment &env,
                                        TimeValue time_micros) {

    // Prepare new env with appropriate time vars
    // and current env as parent

    Environment new_env = this->make_env_for_time(time_micros);

    new_env.set_parent_scope(&env);
    // Eval in new env
    Value result = Interpreter::eval_in(expr, new_env);
    return result;
}

// make_env_for_time and make_env_with_updated_time_durs are now defined in modulisp_time.cpp
