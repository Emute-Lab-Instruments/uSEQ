// Suppress all warnings for this file
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wpedantic"

#include "../utils.h"
#include "modulisp.h"

// Creates a Lisp Value of type BUILTIN_METHOD,
// which requires
#define INSERT_BUILTINDEF(__name__, __func_name__)                             \
    Environment::builtindefs()[__name__] =                                       \
        Value((String)__name__, &ModuLispInterpreter::__func_name__);

void ModuLispInterpreter::init_builtinfuncs() {
    DBG("ModuLispInterpreter::init_builtinfuncs");

    INSERT_BUILTINDEF("eval-at-time", useq_eval_at_time);
    INSERT_BUILTINDEF("useq-rewind", useq_rewind_logical_time);

    // These are not class methods, so they can be inserted normally

    // These are all class methods

    INSERT_BUILTINDEF("slow", useq_slow);
    INSERT_BUILTINDEF("fast", useq_fast);
    INSERT_BUILTINDEF("offset", useq_offset_time);

    INSERT_BUILTINDEF("set-bpm", useq_setbpm);
    INSERT_BUILTINDEF("set-time-sig", useq_set_time_sig);
    INSERT_BUILTINDEF("schedule", useq_schedule);
    INSERT_BUILTINDEF("unschedule", useq_unschedule);

    INSERT_BUILTINDEF("tri", useq_tri);

    // INSERT_BUILTINDEF("looph", useq_loopPhasor);
    INSERT_BUILTINDEF("dm", useq_dm);
    INSERT_BUILTINDEF("gates", useq_gates);
    INSERT_BUILTINDEF("gatesw", useq_gatesw);
    INSERT_BUILTINDEF("trigs", useq_trigs);
    INSERT_BUILTINDEF("euclid", useq_euclidean);
    INSERT_BUILTINDEF("eu", useq_eu);
    INSERT_BUILTINDEF("rpulse", useq_ratiotrig);
    INSERT_BUILTINDEF("rstep", useq_ratiostep);
    INSERT_BUILTINDEF("ridx", useq_ratioindex);
    INSERT_BUILTINDEF("rwarp", useq_ratiowarp);
    INSERT_BUILTINDEF("shift", useq_phasor_offset);

    // NOTE: different names for the same function
    INSERT_BUILTINDEF("from-list", useq_fromList);
    INSERT_BUILTINDEF("seq", useq_seq);
    INSERT_BUILTINDEF("flatseq", useq_flatseq);
    //
    INSERT_BUILTINDEF("from-flattened-list", useq_fromFlattenedList);
    INSERT_BUILTINDEF("flatten", useq_flatten);
    INSERT_BUILTINDEF("interp", useq_interpolate);
    INSERT_BUILTINDEF("step", useq_step);

    INSERT_BUILTINDEF("random", useq_random);
    INSERT_BUILTINDEF("index-rand", useq_index_rand);
    INSERT_BUILTINDEF("loop-at", useq_loop_at_time);

    // Transport offsets
    INSERT_BUILTINDEF("useq-set-time-offset", useq_set_time_offset);
    INSERT_BUILTINDEF("useq-nudge-time", useq_nudge_time);

    // Sync functions
    // INSERT_BUILTINDEF("useq-send-sync-trigger-i2c",
    // useq_send_sync_trigger_i2c);
}

////////////////////
// USEQ API
Value ModuLispInterpreter::useq_set_time_offset(std::vector<Value> &args,
                                                Environment &env) {
    constexpr const char *user_facing_name = "useq-set-time-offset";

    if (!(args.size() == 1)) {
        // error_wrong_num_args(user_facing_name, static_cast<int>(args.size()),
        //                      NumArgsComparison::Between, 2, 3);
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }

        if (!(args[i].is_number())) {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    // BODY
    m_time_manager->set_transport_offset(args[0].as_float());
    set("useq-time-offset", args[0].as_float());

    return args[0];
}

Value ModuLispInterpreter::useq_nudge_time(std::vector<Value> &args,
                                           Environment &env) {
    constexpr const char *user_facing_name = "useq-nudge-time";

    if (!(args.size() == 1)) {
        // error_wrong_num_args(user_facing_name, static_cast<int>(args.size()),
        //                      NumArgsComparison::Between, 2, 3);
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }

        if (!(args[i].is_number())) {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    // BODY
    double new_offset = m_time_manager->get_transport_offset() + args[0].as_float();
    m_time_manager->set_transport_offset(new_offset);
    set("useq-time-offset", new_offset);

    return args[0];
}

////////////////////
// USEQ API

Value ModuLispInterpreter::useq_fast(std::vector<Value> &args,
                                     Environment &env) {
    DBG("ModuLispInterpreter::fast");
    constexpr const char *user_facing_name = "fast";

    // Checking number of args
    if (!(args.size() == 2)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 2, 0);
        return Value::error();
    }

    // Eval first arg
    Value pre_eval = args[0];
    args[0] = args[0].eval(env);
    if (args[0].is_error()) {
        report_error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }
    // Checking first arg
    if (!(args[0].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    double current_time_s = env.get("t").value().as_float();
    double factor = args[0].as_float();
    double new_time_micros = (current_time_s * factor) * 1e+6;

    // Make an env with just the updated beat-dur and bar-dur
    Environment env_with_updated_durs =
        make_env_with_updated_time_durs(env, 1.0 / factor);
    env_with_updated_durs.set_parent_scope(&env);

    result = eval_at_time(args[1], env_with_updated_durs, new_time_micros);
    return result;
}

Value ModuLispInterpreter::useq_slow(std::vector<Value> &args,
                                     Environment &env) {
    DBG("ModuLispInterpreter::useq_slow");
    constexpr const char *user_facing_name = "slow";

    // Checking number of args
    if (!(args.size() == 2)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 2, 0);
        return Value::error();
    }

    // Eval first arg only
    Value pre_eval = args[0];
    args[0] = args[0].eval(env);
    if (args[0].is_error()) {
        report_error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }
    // Checking first arg
    if (!(args[0].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    double current_time_s = env.get("t").value().as_float();
    double factor = args[0].as_float();
    double new_time_micros = (current_time_s / factor) * 1e+6;

    // Make an env with just the updated beat-dur and bar-dur
    Environment env_with_updated_durs =
        make_env_with_updated_time_durs(env, factor);
    env_with_updated_durs.set_parent_scope(&env);

    result = eval_at_time(args[1], env_with_updated_durs, new_time_micros);
    return result;
}

Value ModuLispInterpreter::useq_offset_time(std::vector<Value> &args,
                                            Environment &env) {
    DBG("ModuLispInterpreter::offset");
    constexpr const char *user_facing_name = "offset";

    // Checking number of args
    if (!(args.size() == 2)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 2, 0);
        return Value::error();
    }

    // Eval first arg
    Value pre_eval = args[0];
    args[0] = args[0].eval(env);
    if (args[0].is_error()) {
        report_error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }
    // Checking first arg
    if (!(args[0].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    double current_time_s = env.get("t").value().as_float();
    double amt = args[0].as_float();
    double new_time_micros = (current_time_s + amt) * 1e+6;
    result = eval_at_time(args[1], env, new_time_micros);
    return result;
}

Value ModuLispInterpreter::useq_setbpm(std::vector<Value> &args,
                                       Environment &env) {
    DBG("ModuLispInterpreter::useq_setbpm");
    constexpr const char *user_facing_name = "set-bpm";

    // Checking number of args
    if (!(1 <= args.size() <= 2)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::Between, 1, 2);
        return Value::error();
    }

    // Eval args
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    }

    double thresh = 0.0;
    double newBpm = args[0].as_float();

    if (args.size() == 2) {
        if (!(args[1].is_number())) {
            report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                             args[1].display());
            return Value::error();
        } else {
            thresh = args[1].as_float();
        }
    }

    set_bpm(newBpm, thresh);
    return args[0];
}

Value ModuLispInterpreter::useq_set_time_sig(std::vector<Value> &args,
                                             Environment &env) {
    DBG("ModuLispInterpreter::useq_set_time_sig");
    constexpr const char *user_facing_name = "set-time-sig";

    // Checking number of args
    if (!(args.size() == 2)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 2, 0);
        return Value::error();
    }

    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    }
    if (!(args[1].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    set_time_sig(args[0].as_float(), args[1].as_float());
    return Value::nil();
}

Value ModuLispInterpreter::useq_tri(std::vector<Value> &args,
                                    Environment &env) {
    constexpr const char *user_facing_name = "tri";
    if (!(args.size() == 2)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 2, 0);
        return Value::error();
    }
    if (!(args[0].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    }
    if (!(args[1].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }
    double duty = args[0].as_float();
    if (duty < 0.01) {
        duty = 0.01;
    }
    if (duty > 0.99) {
        duty = 0.99;
    }
    double phase = args[1].as_float();
    // w - ((p-w) * (w/(1-w)))
    if (phase > duty) {
        phase = (duty - ((phase - duty) * (duty / (1 - duty))));
    }
    return Value(phase / duty);
}



// NOTE: doesn't eval its arguments until they're selected by the phasor
Value fromList(std::vector<Value> &lst, double phasor, Environment &env) {
    if (phasor < 0.0) {
        phasor = 0;
    } else if (phasor > 1.0) {
        phasor = 1.0;
    }
    double scaled_phasor = lst.size() * phasor;
    size_t idx = floor(scaled_phasor);
    // keep index in bounds
    if (idx == lst.size())
        idx--;
    return Interpreter::eval_in(lst[idx], env);
}



Value ModuLispInterpreter::useq_dm(std::vector<Value> &args, Environment &env) {
    constexpr const char *user_facing_name = "dm";

    // Checking number of args
    if (!(args.size() == 3)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 3, -1);
        return Value::error();
    }

    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }
        // Check all-pred(s)

        if (!(args[i].is_number())) {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();

    int index = args[0].as_int();
    double v1 = args[1].as_float();
    double v2 = args[2].as_float();
    result = Value(index > 0 ? v2 : v1);

    return result;
}

// FIXME shouldn't the pulsewidth be compared to the
// 1/nth phase of the phasor, where n is the number of gates?
// the way it is now it defaults to muting the first half of the gates
Value ModuLispInterpreter::useq_gates(std::vector<Value> &args,
                                      Environment &env) {
    constexpr const char *user_facing_name = "gates";

    // Checking number of args
    if (!(2 <= args.size() <= 3)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::Between, 2, 3);
        return Value::error();
    }

    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }
        // Check all-pred(s)

        if (i == 0) {
            if (!(args[i].is_sequential())) {
                report_error_wrong_specific_pred(user_facing_name, i + 1,
                                                 "a vector or list",
                                                 args[i].display());
                return Value::error();
            }
        } else {
            if (!(args[i].is_number())) {
                report_error_wrong_specific_pred(user_facing_name, i + 1,
                                                 "a number", args[i].display());
                return Value::error();
            }
        }
    }

    // BODY
    Value result = Value::nil();

    bool pulse_width_specified = args.size() == 3;
    auto gates_vec = args[0].as_sequential();

    double pulseWidth;
    double phasor;
    if (pulse_width_specified) {
        pulseWidth = args[1].as_float();
        phasor = args[2].as_float();
    } else {
        pulseWidth = 0.5;
        phasor = args[1].as_float();
    }

    const double val = fromList(gates_vec, phasor, env).as_int();
    const double gates =
        (fmod(phasor * gates_vec.size(), 1.0)) < pulseWidth ? 1.0 : 0.0;
    result = Value(val * gates);

    return result;
}

Value ModuLispInterpreter::useq_gatesw(std::vector<Value> &args,
                                       Environment &env) {
    constexpr const char *user_facing_name = "gatesw";

    // Checking number of args
    if (!(args.size() == 2)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }
    }

    // Check specific preds
    if (!(args[0].is_sequential())) {
        report_error_wrong_specific_pred(user_facing_name, 1,
                                         "a vector or list", args[0].display());
        return Value::error();
    }

    if (!(args[1].is_number())) {
        report_error_wrong_all_pred(user_facing_name, 2, "a number",
                                    args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto gates_vec = args[0].as_sequential();
    const double phasor = args[1].as_float();
    const double val = fromList(gates_vec, phasor, env).as_int();
    const double pulseWidth = val / 9.0;
    const double relative_phasor = fmod(phasor * gates_vec.size(), 1.0);
    const double gate = relative_phasor < pulseWidth ? 1.0 : 0.0;

    result = Value((val > 0 ? 1.0 : 0.0) * gate);

    return result;
}

Value ModuLispInterpreter::useq_trigs(std::vector<Value> &args,
                                      Environment &env) {
    constexpr const char *user_facing_name = "trigs";

    // Checking number of args
    if (!(2 <= args.size() <= 3)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::Between, 2, 3);
        return Value::error();
    }

    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }
    }

    // Check specific preds
    if (!(args[0].is_sequential())) {
        report_error_wrong_specific_pred(user_facing_name, 1,
                                         "a vector or list", args[0].display());
        return Value::error();
    }

    if (!(args.back().is_number())) {
        report_error_wrong_all_pred(user_facing_name, args.size() + 1,
                                    "a number", args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto lst = args[0].as_sequential();
    // NOTE: phasor at the end
    const double phasor = args.back().as_float();
    const double val = fromList(lst, phasor, env).as_int();
    const double amp = std::clamp(val / 9.0, 0.0, 1.0);
    const double pulseWidth = args.size() == 3 ? args[1].as_float() : 0.1;
    const double gate = fmod(phasor * lst.size(), 1.0) < pulseWidth ? 1.0 : 0.0;
    result = Value((val > 0 ? 1.0 : 0.0) * gate * amp);

    return result;
}

Value ModuLispInterpreter::useq_euclidean(std::vector<Value> &args,
                                          Environment &env) {
    constexpr const char *user_facing_name = "euclid";

    // Checking number of args
    if (!(3 <= args.size() <= 5)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::Between, 3, 5);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }

        // Check all-pred(s)
        if (!(args[i].is_number())) {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();

    // NOTE: Phasor is the last arg
    const double phasor = args.back().as_float();
    const int n = args[0].as_int();
    const int k = args[1].as_int();
    const int offset = (args.size() >= 4) ? args[2].as_int() : 0;
    const float pulseWidth = (args.size() == 5) ? args[3].as_float() : 0.5;
    const float fi = phasor * n;
    int i = static_cast<int>(fi);
    const float rem = fi - i;
    if (i == n) {
        i--;
    }
    const int idx = ((i + n - offset) * k) % n;
    result = Value(idx < k && rem < pulseWidth ? 1 : 0);

    return result;
}

Value ModuLispInterpreter::useq_eu(std::vector<Value> &args, Environment &env) {
    constexpr const char *user_facing_name = "eu";

    // Checking number of args
    if (!(3 <= args.size() <= 5)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::Between, 3, 4);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }

        // Check all-pred(s)
        if (!(args[i].is_number())) {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();

    // NOTE: Phasor is the last arg
    const double phasor = args.back().as_float();
    const int n = args[0].as_int();
    const int k = args[1].as_int();
    const float pulseWidth = (args.size() == 4) ? args[2].as_float() : 0.5;
    const float fi = phasor * n;
    int i = static_cast<int>(fi);
    const float rem = fi - i;
    if (i == n) {
        i--;
    }
    // NOTE: original, testing
    const int idx = ((i + n) * k) % n;
    result = Value(idx < k && rem < pulseWidth ? 1 : 0);

    // NOTE: working one
    // const int idx             = ((i + n) * k) % n;
    // float effectivePulseWidth = (idx < k) ? pulseWidth : 0;
    // result                    = Value(rem < effectivePulseWidth ? 1 : 0);

    return result;
}

Value ModuLispInterpreter::useq_ratiotrig(std::vector<Value> &args,
                                          Environment &env) {
    constexpr const char *user_facing_name = "rpulse";

    // Checking number of args
    if (!(3 == args.size())) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 3, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }
    }
    if (!(args[0].is_sequential())) {
        report_error_wrong_specific_pred(
            user_facing_name, 1,
            "a sequential structure (e.g. a list or a vector)",
            args[0].display());
        return Value::error();
    }
    // Checking individual args
    if (!(args[1].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }
    if (!(args[2].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto ratios = args[0].as_sequential();
    const auto pulseWidth = args[1].as_float();
    const auto phase = args[2].as_float();

    double trig = 0;
    double ratioSum = 0;
    for (Value v : ratios) {
        ratioSum += v.as_float();
    }
    double phaseAdj = ratioSum * phase;
    double accumulatedSum = 0;
    double lastAccumulatedSum = 0;
    for (Value v : ratios) {
        accumulatedSum += v.as_float();
        if (phaseAdj <= accumulatedSum) {
            // check pulse width
            double beatPhase = (phaseAdj - lastAccumulatedSum) /
                               (accumulatedSum - lastAccumulatedSum);
            trig = beatPhase <= pulseWidth;
            break;
        }
        lastAccumulatedSum = accumulatedSum;
    }
    result = Value(trig);

    return result;
}

Value ModuLispInterpreter::useq_ratiostep(std::vector<Value> &args,
                                          Environment &env) {
    constexpr const char *user_facing_name = "rstep";

    // Checking number of args
    if (!(2 == args.size())) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            Value pre_eval = args[i];
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }
    }
    if (!(args[0].is_sequential())) {
        report_error_wrong_specific_pred(
            user_facing_name, 1,
            "a sequential structure (e.g. a list or a vector)",
            args[0].display());
        return Value::error();
    }
    // Checking individual args
    if (!(args[1].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto ratios = args[0].as_sequential();
    const auto phase = args[1].as_float();

    double phaseOut = 0;
    double ratioSum = 0;
    for (Value v : ratios) {
        ratioSum += v.as_float();
    }
    double phaseAdj = ratioSum * phase;
    double accumulatedSum = 0;
    double lastAccumulatedSum = 0;
    for (Value v : ratios) {
        accumulatedSum += v.as_float();
        if (phaseAdj <= accumulatedSum) {
            phaseOut = lastAccumulatedSum;
            break;
        }
        lastAccumulatedSum = accumulatedSum;
    }
    result = Value(phaseOut / ratioSum);
    return result;
}

LISP_FUNC_DECL(ModuLispInterpreter::useq_ratioindex)
// Value ModuLispInterpreter::useq_ratioindex(std::vector<Value>& args,
// Environment& env)
{
    constexpr const char *user_facing_name = "ridx";

    // Checking number of args
    if (!(2 == args.size())) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            Value pre_eval = args[i];
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }
    }
    if (!(args[0].is_sequential())) {
        report_error_wrong_specific_pred(
            user_facing_name, 1,
            "a sequential structure (e.g. a list or a vector)",
            args[0].display());
        return Value::error();
    }
    // Checking individual args
    if (!(args[1].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto ratios = args[0].as_sequential();
    const auto phase = args[1].as_float();

    double index = 0;
    double ratioSum = 0;
    for (Value v : ratios) {
        ratioSum += v.as_float();
    }
    double phaseAdj = ratioSum * phase;
    double accumulatedSum = 0;
    for (Value v : ratios) {
        accumulatedSum += v.as_float();
        if (phaseAdj <= accumulatedSum) {
            break;
        }
        index++;
    }
    index /= static_cast<double>(ratios.size());
    result = Value(index);
    return result;
}

LISP_FUNC_DECL(ModuLispInterpreter::useq_ratiowarp)
// Value ModuLispInterpreter::useq_ratiowarp(std::vector<Value>& args,
// Environment& env)
{
    constexpr const char *user_facing_name = "rwarp";

    // Checking number of args
    if (!(2 == args.size())) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            Value pre_eval = args[i];
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }
    }
    if (!(args[0].is_sequential())) {
        report_error_wrong_specific_pred(
            user_facing_name, 1,
            "a sequential structure (e.g. a list or a vector)",
            args[0].display());
        return Value::error();
    }
    // Checking individual args
    if (!(args[1].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto ratios = args[0].as_sequential();
    const auto phase = args[1].as_float();

    double output = 0;

    if (ratios.size() > 0) {
        double index = 0;
        double indexWidth = 1.0 / ratios.size();

        double ratioSum = 0;
        for (Value v : ratios) {
            ratioSum += v.as_float();
        }

        double phaseAdj = ratioSum * phase;
        double accumulatedSum = 0;
        double lastAccumulatedSum = 0;
        for (Value v : ratios) {
            accumulatedSum += v.as_float();
            if (phaseAdj <= accumulatedSum) {
                double beatPhase = (phaseAdj - lastAccumulatedSum) /
                                   (accumulatedSum - lastAccumulatedSum);
                output = (index * indexWidth) + (beatPhase * indexWidth);
                break;
            }
            lastAccumulatedSum = accumulatedSum;
            index++;
        }
    }

    result = Value(output);
    return result;
}

Value ModuLispInterpreter::useq_phasor_offset(std::vector<Value> &args,
                                              Environment &env) {
    constexpr const char *user_facing_name = "shift";

    // Checking number of args
    if (!(2 == args.size())) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 3, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }
    }
    // Checking individual args
    if (!(args[0].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }
    if (!(args[1].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    const auto offset = args[0].as_float();
    auto phase = args[1].as_float();

    phase = std::fmod(phase + offset, 1.0);
    result = Value(phase);

    return result;
}

// NOTE: doesn't eval its arguments until they're selected by the phasor
Value ModuLispInterpreter::useq_fromList(std::vector<Value> &args,
                                         Environment &env) {
    constexpr const char *user_facing_name = "from-list";

    // Checking number of args
    // if (!(2 <= args.size() <= 3))
    if (!(args.size() == 2)) {
        // error_wrong_num_args(user_facing_name, static_cast<int>(args.size()),
        //                      NumArgsComparison::Between, 2, 3);
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // NOTE: This needs to eval both of its args, including the list,
    // to cover for cases where the user passes anything other than a
    // list literal (e.g. a symbol that points to a list)
    //
    // Evaluating & checking args for errors
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_sequential())) {
        report_error_wrong_specific_pred(
            user_facing_name, 1,
            "a sequential structure (e.g. a list or a vector)",
            args[0].display());
        return Value::error();
    }
    // Checking individual args
    if (!(args[1].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    auto lst = args[0].as_sequential();
    const double phasor = args[1].as_float();
    return fromList(lst, phasor, env);
}

// NOTE: duplicate of fromList, FIXME
Value ModuLispInterpreter::useq_seq(std::vector<Value> &args,
                                    Environment &env) {
    constexpr const char *user_facing_name = "seq";

    // Checking number of args
    // if (!(2 <= args.size() <= 3))
    if (!(args.size() == 2)) {
        // error_wrong_num_args(user_facing_name, static_cast<int>(args.size()),
        //                      NumArgsComparison::Between, 2, 3);
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // NOTE: This needs to eval both of its args, including the list,
    // to cover for cases where the user passes anything other than a
    // list literal (e.g. a symbol that points to a list)
    //
    // Evaluating & checking args for errors
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_sequential())) {
        report_error_wrong_specific_pred(
            user_facing_name, 1,
            "a sequential structure (e.g. a list or a vector)",
            args[0].display());
        return Value::error();
    }
    // Checking individual args
    if (!(args[1].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    auto lst = args[0].as_sequential();
    const double phasor = args[1].as_float();
    return fromList(lst, phasor, env);
}

Value flatten_impl(const Value &val, Environment &env) {
    std::vector<Value> flattened;

    // int original_type = val.type;

    if (!val.is_sequential()) {
        flattened.push_back(val);
    } else {
        auto valList = val.as_sequential();
        for (size_t i = 0; static_cast<size_t>(i) < valList.size(); i++) {
            Value evaluatedElement = Interpreter::eval_in(valList[i], env);
            if (evaluatedElement.is_sequential()) {
                auto flattenedElement =
                    flatten_impl(evaluatedElement, env).as_list();
                flattened.insert(flattened.end(), flattenedElement.begin(),
                                 flattenedElement.end());
            } else {
                flattened.push_back(evaluatedElement);
            }
        }
    }

    Value result;

    // Only return a list if the input was a list,
    // otherwise a vec
    // if (original_type == Value::LIST)
    // {
    //     result = Value(flattened);
    // }
    // else
    // {
    //     result = Value::vector(flattened);
    // }

    result = val.is_vector() ? Value::vector(flattened) : Value(flattened);
    return result;
}

// simple_hashing_function is now delegated to RandomGenerator via inline function in header

Value ModuLispInterpreter::useq_index_rand(std::vector<Value> &args,
                                           Environment &env) {
    constexpr const char *user_facing_name = "index-rand";

    // Checking number of args
    if (!(1 <= args.size() <= 3)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::Between, 0, 2);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }

        // Check all-pred(s)
        if (!(args[i].is_number())) {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    bool scale = args.size() > 1;
    bool lower_bound_provided = args.size() == 3;

    // BODY
    Value result = Value::nil();

    double index = args[args.size() - 1].as_float();
    double lo = (scale && lower_bound_provided) ? args[0].as_float() : 0.0;
    double hi = scale ? args[args.size() - 1].as_float() : 1.0;

    double rand_val = simple_hashing_function(index);
    rand_val = lo + (rand_val * (hi - lo));
    // TODO
    result = Value(rand_val);

    return result;
}

Value ModuLispInterpreter::useq_random(std::vector<Value> &args,
                                       Environment &env) {
    constexpr const char *user_facing_name = "random";

    // Checking number of args
    if (!(0 <= args.size() <= 2)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::Between, 0, 2);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }

        // Check all-pred(s)
        if (!(args[i].is_number())) {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    bool scale = args.size() > 0;
    bool lower_bound_provided = args.size() == 2;

    // BODY
    Value result = Value::nil();

    uint32_t current_beat_num = static_cast<uint32_t>(
        env.get("beat-num")
            .value_or(Value(static_cast<int>(m_current_phasor_state.current_beat_num)))
            .as_int());

    double rand_val = simple_hashing_function(current_beat_num);

    if (scale) {
        double low = lower_bound_provided ? args[0].as_float() : 0.0;
        double high = args[1].as_float();
        rand_val = low + (rand_val * (high - low));
    }

    // TODO
    result = Value(rand_val);

    return result;
}

Value ModuLispInterpreter::useq_loop_at_time(std::vector<Value> &args,
                                             Environment &env) {
    constexpr const char *user_facing_name = "loop-at-time";

    // Checking number of args
    if (!(args.size() == 2)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 2, 2);
        return Value::error();
    }

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

    // BODY
    Value result = Value::nil();
    double current_time_s = env.get("t").value().as_float();
    double modulo_time_s = args[0].as_float();
    double new_time_seconds = fmod(current_time_s, modulo_time_s);
    double new_time_micros = new_time_seconds * 1e+6;

    result = eval_at_time(args[1], env, new_time_micros);

    return result;
}

// TODO test
Value ModuLispInterpreter::useq_flatten(std::vector<Value> &args,
                                        Environment &env) {
    constexpr const char *user_facing_name = "flatten";

    if (!(args.size() == 1)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Check list for error
    Value pre_eval = args[0];
    args[0] = args[0].eval(env);
    if (args[0].is_error()) {
        report_error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result = flatten_impl(args[0], env);

    return result;
}

// NOTE: duplicate of fromFlattenedList, FIXME
Value ModuLispInterpreter::useq_flatseq(std::vector<Value> &args,
                                        Environment &env) {
    constexpr const char *user_facing_name = "flatseq";

    if (!(args.size() == 2)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // NOTE: This needs to eval both of its args, including the list,
    // to cover for cases where the user passes anything other than a
    // list literal (e.g. a symbol that points to a list)
    //
    // Evaluating & checking args for errors
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_sequential())) {
        report_error_wrong_specific_pred(
            user_facing_name, 0, "a list or a vector", args[0].display());
        return Value::error();
    }

    if (!(args[1].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto lst = flatten_impl(args[0], env).as_sequential();
    double phasor = args[1].as_float();
    result = fromList(lst, phasor, env);

    return result;
}

Value ModuLispInterpreter::useq_fromFlattenedList(std::vector<Value> &args,
                                                  Environment &env) {
    constexpr const char *user_facing_name = "from-flat-list";

    if (!(args.size() == 2)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // NOTE: This needs to eval both of its args, including the list,
    // to cover for cases where the user passes anything other than a
    // list literal (e.g. a symbol that points to a list)
    //
    // Evaluating & checking args for errors
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_sequential())) {
        report_error_wrong_specific_pred(
            user_facing_name, 0,
            "a sequential structure (e.g. a list or a vector)",
            args[0].display());
        return Value::error();
    }

    if (!(args[1].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto lst = flatten_impl(args[0], env).as_sequential();
    double phasor = args[1].as_float();
    result = fromList(lst, phasor, env);

    return result;
}

Value ModuLispInterpreter::useq_interpolate(std::vector<Value> &args,
                                            Environment &env) {
    constexpr const char *user_facing_name = "interp";

    // Check num arguments
    if (!(args.size() == 2)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_sequential())) {
        report_error_wrong_specific_pred(
            user_facing_name, 0, "a list or a vector", args[0].display());
        return Value::error();
    }

    if (!(args[1].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    auto lst = args[0].as_list();
    double phasor = args[1].as_float();
    if (phasor < 0.0) {
        phasor = 0;
    } else if (phasor > 1) {
        phasor = 1;
    }
    float a;
    double index = phasor * (lst.size() - 1);
    size_t pos0 = static_cast<size_t>(index);
    if (pos0 == (lst.size() - 1))
        pos0--;
    a = (index - pos0);
    double v2 = lst[pos0 + 1].as_float();
    double v1 = lst[pos0].as_float();
    result = Value(((v2 - v1) * a) + v1);

    return result;
}

Value ModuLispInterpreter::useq_step(std::vector<Value> &args,
                                     Environment &env) {
    constexpr const char *user_facing_name = "step";

    // Check num arguments
    if (!(2 <= args.size() <= 3)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::Between, 2, 3);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }

        // Check all-pred(s)
        if (!(args[i].is_number())) {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();

    const int count = args[0].as_int();
    bool offset_provided = args.size() == 3;
    double phasor, offset;

    if (offset_provided) {
        offset = args[1].as_float();
        phasor = args[2].as_float();
    } else {
        offset = 0;
        phasor = args[1].as_float();
    }

    double val = static_cast<int>(phasor * abs(count));
    if (val == count)
        val--;
    result = Value((count > 0 ? val : count - 1 - val) + offset);
    return result;
}

Value ModuLispInterpreter::useq_rewind_logical_time(std::vector<Value> &args,
                                     Environment &env) {
    constexpr const char *user_facing_name = "step";

    // BODY
                          reset_logical_time();
    Value result = Value::nil();
    return result;
}


// (schedule <name> <period> <expr>)
Value ModuLispInterpreter::useq_schedule(std::vector<Value> &args, Environment &env) {
    DBG("uSEQ::lisp_schedule");
    constexpr const char *user_facing_name = "schedule";

    // Checking number of args
    if (!(args.size() == 3)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 3, 0);
        return Value::error();
    }

    // Evaluating ONLY first 2 args & checking for errors
    for (size_t i = 0; i < 2; i++) {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_string())) {
        report_error_wrong_specific_pred(user_facing_name, 1, "a string",
                                         args[0].display());
        return Value::error();
    }
    if (!(args[1].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }
    // BODY
    const auto itemName = args[0].as_string();
    const auto period = static_cast<size_t>(args[1].as_float());
    const auto ast = args[2];
    
    // Use Scheduler to manage scheduled items
    m_scheduler->schedule(itemName, ast, period);
    return Value::nil();
}

// (schedule <name> <period> <expr>)
Value ModuLispInterpreter::useq_unschedule(std::vector<Value> &args, Environment &env) {
    DBG("ModuLispInterpreter::useq_unschedule");
    constexpr const char *user_facing_name = "unschedule";

    // Checking number of args
    if (!(args.size() == 1)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 1, 0);
        return Value::error();
    }

    // Evaluating ONLY first arg & checking for errors
    Value pre_eval = args[0];
    args[0] = args[0].eval(env);
    if (args[0].is_error()) {
        report_error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_string())) {
        report_error_wrong_specific_pred(user_facing_name, 1, "a string",
                                         args[0].display());
        return Value::error();
    }

    const String id = args[0].as_string();
    
    // Use Scheduler to unschedule items
    if (m_scheduler->unschedule(id)) {
        println("- (unschedule) Item " + args[0].str +
                " removed successfully.");
    } else {
        println("- (unschedule) Item " + args[0].str + " not found; ignoring.");
    }
    return Value::nil();
}

#pragma GCC diagnostic pop
