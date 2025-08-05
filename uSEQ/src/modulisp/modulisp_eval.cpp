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

Environment ModuLispInterpreter::make_env_for_time(TimeValue t_micros) {
    Environment env;

    // TimeValue time_s = m_time_since_boot * 1e-6;
    TimeValue t_s = t_micros * 1e-6;

    // env.set("time", Value(time_s));
    env.set("t", Value(t_s));
    env.set("beat", Value(beat_at_time(t_micros)));
    env.set("beat-num", Value(static_cast<int>(beat_num_at_time(t_micros))));
    env.set("bar", Value(bar_at_time(t_micros)));
    env.set("bar-num", Value(static_cast<int>(bar_num_at_time(t_micros))));
    env.set("phrase", Value(phrase_at_time(t_micros)));
    env.set("section", Value(section_at_time(t_micros)));

    return env;
}

Environment ModuLispInterpreter::make_env_with_updated_time_durs(
    const Environment &parent_env, TimeValue factor) {
    Environment env;

    TimeValue current_beat_dur = parent_env.get("beat-dur").value().as_float();
    env.set("beat-dur", Value(current_beat_dur * factor));

    TimeValue current_bar_dur = parent_env.get("bar-dur").value().as_float();
    env.set("bar-dur", Value(current_bar_dur * factor));

    return env;
}
