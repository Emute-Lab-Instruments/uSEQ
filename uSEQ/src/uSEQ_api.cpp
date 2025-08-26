#include "uSEQ.h"
#include "utils.h"

// Creates a Lisp Value of type BUILTIN_METHOD,
// which requires
#define INSERT_BUILTINDEF(__name__, __func_name__)                                  \
    Environment::builtindefs[__name__] =                                            \
        Value((String)__name__, &uSEQ::__func_name__);

void uSEQ::init_builtinfuncs()
{
    DBG("uSEQ::init_builtinfuncs");

    INSERT_BUILTINDEF("eval-at-time", useq_eval_at_time);
    INSERT_BUILTINDEF("useq-rewind", useq_rewind_logical_time);

    // a
    INSERT_BUILTINDEF("a1", useq_a1);
    INSERT_BUILTINDEF("a2", useq_a2);
    INSERT_BUILTINDEF("a3", useq_a3);
    INSERT_BUILTINDEF("a4", useq_a4);
    INSERT_BUILTINDEF("a5", useq_a5);
    INSERT_BUILTINDEF("a6", useq_a6);
    INSERT_BUILTINDEF("a7", useq_a7);
    INSERT_BUILTINDEF("a8", useq_a8);
    // d
    INSERT_BUILTINDEF("d1", useq_d1);
    INSERT_BUILTINDEF("d2", useq_d2);
    INSERT_BUILTINDEF("d3", useq_d3);
    INSERT_BUILTINDEF("d4", useq_d4);
    INSERT_BUILTINDEF("d5", useq_d5);
    INSERT_BUILTINDEF("d6", useq_d6);
    INSERT_BUILTINDEF("d7", useq_d7);
    INSERT_BUILTINDEF("d8", useq_d8);
    // s
    INSERT_BUILTINDEF("s1", useq_s1);
    INSERT_BUILTINDEF("s2", useq_s2);
    INSERT_BUILTINDEF("s3", useq_s3);
    INSERT_BUILTINDEF("s4", useq_s4);
    INSERT_BUILTINDEF("s5", useq_s5);
    INSERT_BUILTINDEF("s6", useq_s6);
    INSERT_BUILTINDEF("s7", useq_s7);
    INSERT_BUILTINDEF("s8", useq_s8);

    INSERT_BUILTINDEF("q0", useq_q0);

    // These are not class methods, so they can be inserted normally
    INSERT_BUILTINDEF("useqaw", ard_useqaw);
    INSERT_BUILTINDEF("useqdw", ard_useqdw);

    // These are all class methods
    INSERT_BUILTINDEF("toggle-pick", useq_toggle_pick);
    INSERT_BUILTINDEF("swm", useq_swm);
    INSERT_BUILTINDEF("swt", useq_swt);
    INSERT_BUILTINDEF("swr", useq_swr);
    INSERT_BUILTINDEF("rot", useq_rot);
    INSERT_BUILTINDEF("ssin", useq_ssin);

    INSERT_BUILTINDEF("in1", useq_in1);
    INSERT_BUILTINDEF("in2", useq_in2);
    INSERT_BUILTINDEF("gin1", useq_in1);
    INSERT_BUILTINDEF("gin2", useq_in2);
    INSERT_BUILTINDEF("ain1", useq_ain1);
    INSERT_BUILTINDEF("ain2", useq_ain2);

    INSERT_BUILTINDEF("get-a1", useq_get_a1);
    INSERT_BUILTINDEF("get-a2", useq_get_a2);
    INSERT_BUILTINDEF("get-a3", useq_get_a3);
    INSERT_BUILTINDEF("get-a4", useq_get_a4);
    INSERT_BUILTINDEF("get-a5", useq_get_a5);
    INSERT_BUILTINDEF("get-a6", useq_get_a6);
    INSERT_BUILTINDEF("get-a7", useq_get_a7);
    INSERT_BUILTINDEF("get-a8", useq_get_a8);

    INSERT_BUILTINDEF("get-d1", useq_get_d1);
    INSERT_BUILTINDEF("get-d2", useq_get_d2);
    INSERT_BUILTINDEF("get-d3", useq_get_d3);
    INSERT_BUILTINDEF("get-d4", useq_get_d4);
    INSERT_BUILTINDEF("get-d5", useq_get_d5);
    INSERT_BUILTINDEF("get-d6", useq_get_d6);
    INSERT_BUILTINDEF("get-d7", useq_get_d7);
    INSERT_BUILTINDEF("get-d8", useq_get_d8);

    // INSERT_BUILTINDEF("get-s1", useq_get_s1);
    // INSERT_BUILTINDEF("get-s2", useq_get_s2);
    // INSERT_BUILTINDEF("get-s3", useq_get_s3);
    // INSERT_BUILTINDEF("get-s4", useq_get_s4);
    // INSERT_BUILTINDEF("get-s5", useq_get_s5);
    // INSERT_BUILTINDEF("get-s6", useq_get_s6);
    // INSERT_BUILTINDEF("get-s7", useq_get_s7);
    // INSERT_BUILTINDEF("get-s8", useq_get_s8);

    INSERT_BUILTINDEF("slow", useq_slow);
    INSERT_BUILTINDEF("fast", useq_fast);
    INSERT_BUILTINDEF("offset", useq_offset_time);

    INSERT_BUILTINDEF("set-bpm", useq_setbpm);
    INSERT_BUILTINDEF("get-input-bpm", useq_get_input_bpm);
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

    // clock sources and management
    INSERT_BUILTINDEF("reset-clock-ext", useq_reset_external_clock_tracking);
    INSERT_BUILTINDEF("reset-clock-int", useq_reset_internal_clock);
    INSERT_BUILTINDEF("get-clock-source", useq_get_clock_source);
    INSERT_BUILTINDEF("set-clock-int", useq_set_clock_internal);
    INSERT_BUILTINDEF("set-clock-ext", useq_set_clock_external);

    INSERT_BUILTINDEF("random", useq_random);
    INSERT_BUILTINDEF("index-rand", useq_index_rand);
    INSERT_BUILTINDEF("loop-at", useq_loop_at_time);

    // TODO
#ifdef MUSICTHING
    INSERT_BUILTINDEF("knob", useq_mt_knob);
    INSERT_BUILTINDEF("knobx", useq_mt_knobx);
    INSERT_BUILTINDEF("knoby", useq_mt_knoby);
    INSERT_BUILTINDEF("swz", useq_mt_swz);
#endif

#ifdef MIDIOUT
    INSERT_BUILTINDEF("mdo", useq_mdo);
#endif

#ifdef ARDUINO
    // SYSTEM API
    INSERT_BUILTINDEF("useq-reboot", useq_reboot);

    // FLASH API
    // NOTE: These are meant to be the main user interface
    // ID
    INSERT_BUILTINDEF("useq-set-id", useq_set_my_id);
    INSERT_BUILTINDEF("useq-get-id", useq_get_my_id);

    // MEMORY
    INSERT_BUILTINDEF("useq-memory-save", useq_memory_save);
    INSERT_BUILTINDEF("useq-memory-restore", useq_memory_restore);
    INSERT_BUILTINDEF("useq-memory-erase", useq_memory_erase);
    // NOTE: aliases
    INSERT_BUILTINDEF("useq-memory-load", useq_memory_restore);
    INSERT_BUILTINDEF("useq-memory-clear", useq_memory_erase);
#endif

    // NOTE: these are NOT meant to be user interface, just for
    // more granular dev tests
#ifdef ARDUINO
    INSERT_BUILTINDEF("write-flash-info", useq_write_flash_info);
    INSERT_BUILTINDEF("load-flash-info", useq_load_flash_info);

    INSERT_BUILTINDEF("write-flash-env", useq_write_flash_env);
    INSERT_BUILTINDEF("load-flash-env", useq_load_flash_env);

    INSERT_BUILTINDEF("useq-autoload-flash", useq_autoload_flash);
#endif
    INSERT_BUILTINDEF("useq-stop-all", useq_stop_all);

    // FIRMWARE
    // NOTE: for user interface only
    INSERT_BUILTINDEF("useq-firmware-info", useq_firmware_info);
    // NOTE: for editor use only
    INSERT_BUILTINDEF("useq-report-firmware-info", useq_report_firmware_info);
    // INSERT_BUILTINDEF("useq-clear-flash", useq_clear_non_program_flash);

    INSERT_BUILTINDEF("send-to", useq_i2c_send_to);
    INSERT_BUILTINDEF("i2c-host-start", useq_i2c_host_start);

#ifdef ARDUINO
    INSERT_BUILTINDEF("useq-enter-bootloader-mode", useq_enter_bootloader_mode);
#endif

    // Transport offsets
    INSERT_BUILTINDEF("useq-set-time-offset", useq_set_time_offset);
    INSERT_BUILTINDEF("useq-nudge-time", useq_nudge_time);

    // Sync functions
    INSERT_BUILTINDEF("useq-enter-sync-mode", useq_enter_sync_mode);
    INSERT_BUILTINDEF("useq-send-sync-trigger", useq_send_sync_trigger);
    // INSERT_BUILTINDEF("useq-send-sync-trigger-i2c", useq_send_sync_trigger_i2c);

    // DSP engine
    #ifdef ARDUINO
    INSERT_BUILTINDEF("ppp-go", useq_dsp_start);
    INSERT_BUILTINDEF("ppp-stop", useq_dsp_stop);
    INSERT_BUILTINDEF("ppp-mount", useq_dsp_create);
    INSERT_BUILTINDEF("ppp-unmount", useq_dsp_kill);
    INSERT_BUILTINDEF("ppp-patch", useq_dsp_connect);
    INSERT_BUILTINDEF("ppp-unpatch", useq_dsp_disconnect);
    INSERT_BUILTINDEF("ppp-getugens", useq_dsp_getugens);
    INSERT_BUILTINDEF("ppp-get", useq_dsp_qget);
    INSERT_BUILTINDEF("ppp-set", useq_dsp_qset);
    INSERT_BUILTINDEF("ppp-reset", useq_dsp_reset);
    INSERT_BUILTINDEF("ppp-msg", useq_dsp_message);
    INSERT_BUILTINDEF("ppp-qlist", useq_dsp_listqueues);
    #endif
}

////////////////////
// USEQ API
Value uSEQ::useq_set_time_offset(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "useq-set-time-offset";

    if (!(args.size() == 1))
    {
        // error_wrong_num_args(user_facing_name, args.size(),
        //                      NumArgsComparison::Between, 2, 3);
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }

        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    // BODY
    m_transport_time_offset = args[0].as_float();
    set("useq-time-offset", m_transport_time_offset);

    return args[0];
}

Value uSEQ::useq_nudge_time(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "useq-nudge-time";

    if (!(args.size() == 1))
    {
        // error_wrong_num_args(user_facing_name, args.size(),
        //                      NumArgsComparison::Between, 2, 3);
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }

        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    // BODY
    m_transport_time_offset += args[0].as_float();
    set("useq-time-offset", m_transport_time_offset);

    return args[0];
}

////////////////////
// USEQ API
Value uSEQ::ard_useqdw(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "useqdw";

    if (!(args.size() == 2))
    {
        // error_wrong_num_args(user_facing_name, args.size(),
        //                      NumArgsComparison::Between, 2, 3);
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }

        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    // BODY
    int out = args[0].as_int();
    int val = args[1].as_int();
    digital_write_with_led(out, val);

    return Value::nil();
}

Value uSEQ::ard_useqaw(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "useqaw";

    if (!(args.size() == 2))
    {
        // error_wrong_num_args(user_facing_name, args.size(),
        //                      NumArgsComparison::Between, 2, 3);
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }

        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    // BODY
    analog_write_with_led(args[0].as_int(), args[1].as_float());

    return Value::nil();
}

Value uSEQ::useq_fast(std::vector<Value>& args, Environment& env)
{
    DBG("uSEQ::fast");
    constexpr const char* user_facing_name = "fast";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, 0);
        return Value::error();
    }

    // Eval first arg
    Value pre_eval = args[0];
    args[0]        = args[0].eval(env);
    if (args[0].is_error())
    {
        report_error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }
    // Checking first arg
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    }

    // BODY
    Value result           = Value::nil();
    double current_time_s  = env.get("t").value().as_float();
    double factor          = args[0].as_float();
    double new_time_micros = (current_time_s * factor) * 1e+6;

    // Make an env with just the updated beat-dur and bar-dur
    Environment env_with_updated_durs =
        make_env_with_updated_time_durs(env, 1.0 / factor);
    env_with_updated_durs.set_parent_scope(&env);

    result = eval_at_time(args[1], env_with_updated_durs, new_time_micros);
    return result;
}

Value uSEQ::useq_slow(std::vector<Value>& args, Environment& env)
{
    DBG("uSEQ::useq_slow");
    constexpr const char* user_facing_name = "slow";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, 0);
        return Value::error();
    }

    // Eval first arg only
    Value pre_eval = args[0];
    args[0]        = args[0].eval(env);
    if (args[0].is_error())
    {
        report_error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }
    // Checking first arg
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result           = Value::nil();
    double current_time_s  = env.get("t").value().as_float();
    double factor          = args[0].as_float();
    double new_time_micros = (current_time_s / factor) * 1e+6;

    // Make an env with just the updated beat-dur and bar-dur
    Environment env_with_updated_durs = make_env_with_updated_time_durs(env, factor);
    env_with_updated_durs.set_parent_scope(&env);

    result = eval_at_time(args[1], env_with_updated_durs, new_time_micros);
    return result;
}

Value uSEQ::useq_offset_time(std::vector<Value>& args, Environment& env)
{
    DBG("uSEQ::offset");
    constexpr const char* user_facing_name = "offset";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, 0);
        return Value::error();
    }

    // Eval first arg
    Value pre_eval = args[0];
    args[0]        = args[0].eval(env);
    if (args[0].is_error())
    {
        report_error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }
    // Checking first arg
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    }

    // BODY
    Value result           = Value::nil();
    double current_time_s  = env.get("t").value().as_float();
    double amt             = args[0].as_float();
    double new_time_micros = (current_time_s + amt) * 1e+6;
    result                 = eval_at_time(args[1], env, new_time_micros);
    return result;
}

// (schedule <name> <period> <expr>)
Value uSEQ::useq_schedule(std::vector<Value>& args, Environment& env)
{
    DBG("uSEQ::lisp_schedule");
    constexpr const char* user_facing_name = "schedule";

    // Checking number of args
    if (!(args.size() == 3))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 3, 0);
        return Value::error();
    }

    // Evaluating ONLY first 2 args & checking for errors
    for (size_t i = 0; i < 2; i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_string()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a string",
                                         args[0].display());
        return Value::error();
    }
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }
    // BODY
    const auto itemName = args[0].as_string();
    const auto period   = args[1].as_float();
    const auto ast      = args[2];
    scheduledItem v;
    v.id      = itemName;
    v.period  = period;
    v.lastRun = 0;
    v.ast     = ast;
    // remove if exists
    const String searchId = args[0].as_string();

    auto is_item = [searchId](scheduledItem& candidate)
    { return candidate.id == searchId; };

    if (auto it = std::find_if(std::begin(m_scheduledItems),
                               std::end(m_scheduledItems), is_item);
        it != std::end(m_scheduledItems))
    {
        m_scheduledItems.erase(it);
    }
    // add to scheduler list
    m_scheduledItems.push_back(v);
    return Value::nil();
}

// (schedule <name> <period> <expr>)
Value uSEQ::useq_unschedule(std::vector<Value>& args, Environment& env)
{
    DBG("uSEQ::useq_unschedule");
    constexpr const char* user_facing_name = "unschedule";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, 0);
        return Value::error();
    }

    // Evaluating ONLY first arg & checking for errors
    Value pre_eval = args[0];
    args[0]        = args[0].eval(env);
    if (args[0].is_error())
    {
        report_error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_string()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a string",
                                         args[0].display());
        return Value::error();
    }

    const String id = args[0].as_string();
    auto is_item    = [id](scheduledItem& v) { return v.id == id; };

    if (auto it = std::find_if(std::begin(m_scheduledItems),
                               std::end(m_scheduledItems), is_item);
        it != std::end(m_scheduledItems))
    {
        m_scheduledItems.erase(it);
        println("- (unschedule) Item " + args[0].str + " removed successfully.");
    }
    else
    {
        println("- (unschedule) Item " + args[0].str + " not found; ignoring.");
    }
    return Value::nil();
}

Value uSEQ::useq_setbpm(std::vector<Value>& args, Environment& env)
{
    DBG("uSEQ::useq_setbpm");
    constexpr const char* user_facing_name = "set-bpm";

    // Checking number of args
    if (!(1 <= args.size() <= 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::Between, 1, 2);
        return Value::error();
    }

    // Eval args
    for (size_t i = 0; i < args.size(); i++)
    {
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    }

    double thresh = 0.0;
    double newBpm = args[0].as_float();

    if (args.size() == 2)
    {
        if (!(args[1].is_number()))
        {
            report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                             args[1].display());
            return Value::error();
        }
        else
        {
            thresh = args[1].as_float();
        }
    }

    set_bpm(newBpm, thresh);
    return args[0];
}

Value uSEQ::useq_get_input_bpm(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "get-input-bpm";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, 0);
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    }

    Value result;

    int index = args[0].as_int();
    if (index == 1)
    {
        result = tempoI1.avgBPM;
    }
    else if (index == 1)
    {
        result = tempoI2.avgBPM;
    }
    else
    {
        result = 0;
    }
    return result;
}

Value uSEQ::useq_set_time_sig(std::vector<Value>& args, Environment& env)
{
    DBG("uSEQ::useq_set_time_sig");
    constexpr const char* user_facing_name = "set-time-sig";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, 0);
        return Value::error();
    }

    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    }
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    set_time_sig(args[0].as_float(), args[1].as_float());
    return Value::nil();
}

Value uSEQ::useq_in1(std::vector<Value>& args, Environment& env)
{
    return Value(m_input_vals[USEQI1]);
}

Value uSEQ::useq_in2(std::vector<Value>& args, Environment& env)
{
    return Value(m_input_vals[USEQI2]);
}

Value uSEQ::useq_ain1(std::vector<Value>& args, Environment& env)
{
    return Value(m_input_vals[USEQAI1]);
}

Value uSEQ::useq_ain2(std::vector<Value>& args, Environment& env)
{
    return Value(m_input_vals[USEQAI2]);
}

Value uSEQ::useq_get_a1(std::vector<Value>& args, Environment& env)
{
    return Value(m_continuous_vals[0]);
}
Value uSEQ::useq_get_a2(std::vector<Value>& args, Environment& env)
{
    return Value(m_continuous_vals[1]);
}
Value uSEQ::useq_get_a3(std::vector<Value>& args, Environment& env)
{
    return Value(m_continuous_vals[2]);
}
Value uSEQ::useq_get_a4(std::vector<Value>& args, Environment& env)
{
    return Value(m_continuous_vals[3]);
}
Value uSEQ::useq_get_a5(std::vector<Value>& args, Environment& env)
{
    return Value(m_continuous_vals[4]);
}
Value uSEQ::useq_get_a6(std::vector<Value>& args, Environment& env)
{
    return Value(m_continuous_vals[5]);
}
Value uSEQ::useq_get_a7(std::vector<Value>& args, Environment& env)
{
    return Value(m_continuous_vals[6]);
}
Value uSEQ::useq_get_a8(std::vector<Value>& args, Environment& env)
{
    return Value(m_continuous_vals[7]);
}

Value uSEQ::useq_get_d1(std::vector<Value>& args, Environment& env)
{
    return Value(m_binary_vals[0]);
}
Value uSEQ::useq_get_d2(std::vector<Value>& args, Environment& env)
{
    return Value(m_binary_vals[1]);
}
Value uSEQ::useq_get_d3(std::vector<Value>& args, Environment& env)
{
    return Value(m_binary_vals[2]);
}
Value uSEQ::useq_get_d4(std::vector<Value>& args, Environment& env)
{
    return Value(m_binary_vals[3]);
}
Value uSEQ::useq_get_d5(std::vector<Value>& args, Environment& env)
{
    return Value(m_binary_vals[4]);
}
Value uSEQ::useq_get_d6(std::vector<Value>& args, Environment& env)
{
    return Value(m_binary_vals[5]);
}
Value uSEQ::useq_get_d7(std::vector<Value>& args, Environment& env)
{
    return Value(m_binary_vals[6]);
}
Value uSEQ::useq_get_d8(std::vector<Value>& args, Environment& env)
{
    return Value(m_binary_vals[7]);
}

// Value uSEQ::useq_get_s1(std::vector<Value>& args, Environment& env)
// {
//     return Value(m_serial_vals[0]);
// }
// Value uSEQ::useq_get_s2(std::vector<Value>& args, Environment& env)
// {
//     return Value(m_serial_vals[1]);
// }
// Value uSEQ::useq_get_s3(std::vector<Value>& args, Environment& env)
// {
//     return Value(m_serial_vals[2]);
// }
// Value uSEQ::useq_get_s4(std::vector<Value>& args, Environment& env)
// {
//     return Value(m_serial_vals[3]);
// }
// Value uSEQ::useq_get_s5(std::vector<Value>& args, Environment& env)
// {
//     return Value(m_serial_vals[4]);
// }
// Value uSEQ::useq_get_s6(std::vector<Value>& args, Environment& env)
// {
//     return Value(m_serial_vals[5]);
// }
// Value uSEQ::useq_get_s7(std::vector<Value>& args, Environment& env)
// {
//     return Value(m_serial_vals[6]);
// }
// Value uSEQ::useq_get_s8(std::vector<Value>& args, Environment& env)
// {
//     return Value(m_serial_vals[7]);
// }

#ifdef MUSICTHING

Value uSEQ::useq_mt_knob(std::vector<Value>& args, Environment& env)
{
    return Value(m_input_vals[MTMAINKNOB]);
}

Value uSEQ::useq_mt_knobx(std::vector<Value>& args, Environment& env)
{
    return Value(m_input_vals[MTXKNOB]);
}

Value uSEQ::useq_mt_knoby(std::vector<Value>& args, Environment& env)
{
    return Value(m_input_vals[MTYKNOB]);
}

Value uSEQ::useq_mt_swz(std::vector<Value>& args, Environment& env)
{
    return Value(m_input_vals[MTZSWITCH]);
}
#endif
// clock sources

// BUILTINFUNC_MEMBER(
//     useq_reset_internal_clock,
//     constexpr const char* user_facing_name = "reset-clock-int";
//     if (!(args.size() == 0)) {
//         report_error_wrong_num_args(user_facing_name, args.size(),
//                                     NumArgsComparison::EqualTo, 0, 0);
//         return Value::error();
//     } reset_logical_time();
//     return Value::nil();, 0)

// BUILTINFUNC_MEMBER(
//     useq_get_clock_source,
//     constexpr const char* user_facing_name = "get-clock-source";
//     if (!(args.size() == 0)) {
//         report_error_wrong_num_args(user_facing_name, args.size(),
//                                     NumArgsComparison::EqualTo, 0, 0);
//         return Value::error();
//     }

//     switch (useq_clock_source) {
// case uSEQ::CLOCK_SOURCES::INTERNAL:
//     println("Internal");
//     break;
// case uSEQ::CLOCK_SOURCES::EXTERNAL_I1:
//     println("External 1");
//     break;
// case uSEQ::CLOCK_SOURCES::EXTERNAL_I2:
//     println("External 2");
//     break;
//     } return Value((int)useq_clock_source);
//     , 0)

// BUILTINFUNC_MEMBER(useq_set_clock_internal,
//                    useq_clock_source = uSEQ::CLOCK_SOURCES::INTERNAL;
//                    println("Clock source set to internal"); return Value::nil();,
//                    0)

// BUILTINFUNC_MEMBER(
//     useq_set_clock_external,
//     constexpr const char* user_facing_name = "set-clock-ext";

//     // Checking number of args
//     if (!(args.size() == 2)) {
//         report_error_wrong_num_args(user_facing_name, args.size(),
//                                     NumArgsComparison::EqualTo, 1, 0);
//         return Value::error();
//     } if (!(args[0].is_number())) {
//         report_error_wrong_specific_pred(user_facing_name, 1, "a number",
//                                          args[0].display());
//         return Value::error();
//     } else if (args[0] < 1 || args[0] > 2) {
//         report_custom_function_error(user_facing_name,
//                                      "The clock source can be either input 1 or
//                                      2");
//     } if (!(args[1].is_number())) {
//         report_error_wrong_specific_pred(user_facing_name, 1, "a number",
//                                          args[0].display());
//         return Value::error();
//     } else if (args[1] <= 0) {
//         report_custom_function_error(user_facing_name,
//                                      "The clock divisor must be more than 0");
//     }

//     // update settings
//     if (args[0] == 1) {
//         useq_clock_source = uSEQ::CLOCK_SOURCES::EXTERNAL_I1;
//     } else if (args[0] == 2) {
//         useq_clock_source = uSEQ::CLOCK_SOURCES::EXTERNAL_I2;
//     }

//     set_ext_clock_div(args[1].as_int());

//     // notify player
//     println("Clock source set to external: " + String(args[0].as_int()));
//     println("Clock divisor: " + String(args[1].as_int()));

//     return Value::nil();, 2)

// clock sources

BUILTINFUNC_MEMBER(
    useq_reset_external_clock_tracking,
    constexpr const char* user_facing_name = "reset-clock-ext";
    if (!(args.size() == 0)) {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 0, 0);
        return Value::error();
    } reset_ext_tracking();
    return Value::nil();, 0)

BUILTINFUNC_MEMBER(
    useq_reset_internal_clock,
    constexpr const char* user_facing_name = "reset-clock-int";
    if (!(args.size() == 0)) {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 0, 0);
        return Value::error();
    } reset_logical_time();
    return Value::nil();, 0)

BUILTINFUNC_MEMBER(
    useq_get_clock_source,
    constexpr const char* user_facing_name = "get-clock-source";
    if (!(args.size() == 0)) {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 0, 0);
        return Value::error();
    }

    switch (useq_clock_source) {
case uSEQ::CLOCK_SOURCES::INTERNAL:
    println("Internal");
    break;
case uSEQ::CLOCK_SOURCES::EXTERNAL_I1:
    println("External 1");
    break;
case uSEQ::CLOCK_SOURCES::EXTERNAL_I2:
    println("External 2");
    break;
    } return Value((int)useq_clock_source);
    , 0)

BUILTINFUNC_MEMBER(useq_set_clock_internal,
                   useq_clock_source = uSEQ::CLOCK_SOURCES::INTERNAL;
                   println("Clock source set to internal"); return Value::nil();, 0)

BUILTINFUNC_MEMBER(
    useq_set_clock_external,
    constexpr const char* user_facing_name = "set-clock-ext";

    // Checking number of args
    if (!(args.size() == 2)) {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, 0);
        return Value::error();
    } if (!(args[0].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    } else if (args[0] < 1 || args[0] > 2) {
        report_custom_function_error(user_facing_name,
                                     "The clock source can be either input 1 or 2");
    } if (!(args[1].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    } else if (args[1] <= 0) {
        report_custom_function_error(user_facing_name,
                                     "The clock divisor must be more than 0");
    }

    // update settings
    if (args[0] == 1) {
        useq_clock_source = uSEQ::CLOCK_SOURCES::EXTERNAL_I1;
    } else if (args[0] == 2) {
        useq_clock_source = uSEQ::CLOCK_SOURCES::EXTERNAL_I2;
    }

    set_ext_clock_div(args[1].as_int());

    // notify player
    println("Clock source set to external: " + String(args[0].as_int()));
    println("Clock divisor: " + String(args[1].as_int()));

    return Value::nil();, 2)

BUILTINFUNC_MEMBER(useq_swr, ret = Value(m_input_vals[USEQRS1]);, 0)

BUILTINFUNC_MEMBER(useq_rot, ret = Value(m_input_vals[USEQR1]);, 0)

BUILTINFUNC_MEMBER(
    useq_tri, constexpr const char* user_facing_name = "tri";
    if (!(args.size() == 2)) {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, 0);
        return Value::error();
    } if (!(args[0].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    } if (!(args[1].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    } double duty = args[0].as_float();
    if (duty < 0.01) { duty = 0.01; } if (duty > 0.99) {
        duty = 0.99;
    } double phase = args[1].as_float();
    // w - ((p-w) * (w/(1-w)))
    if (phase > duty) {
        phase = (duty - ((phase - duty) * (duty / (1 - duty))));
    } return phase /
    duty;
    , 2)

Value uSEQ::useq_ssin(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "ssin";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, 0);
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    }

    int index    = args[0].as_int();
    Value result = Value::nil();
    if (index > 0 && index <= m_num_serial_ins)
    {
        result = Value(m_serial_input_streams[index - 1]);
    }
    else
    {
        report_user_warning("(ssin) Received request for index " + String(index) +
                            ", which is out of bounds; returning nil.");
    }

    return result;
}

Value uSEQ::useq_swm(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "swm";

    // Checking number of args
    if (!(args.size() == 0))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 0, 0);
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = Value(m_input_vals[USEQM1]);
    return result;
}

Value uSEQ::useq_swt(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "swt";

    // Checking number of args
    if (!(args.size() == 0))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 0, 0);
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = Value(m_input_vals[USEQT1]);
    return result;
}

// NOTE: doesn't eval its arguments until they're selected by the phasor
Value fromList(std::vector<Value>& lst, double phasor, Environment& env)
{
    if (phasor < 0.0)
    {
        phasor = 0;
    }
    else if (phasor > 1.0)
    {
        phasor = 1.0;
    }
    double scaled_phasor = lst.size() * phasor;
    size_t idx           = floor(scaled_phasor);
    // keep index in bounds
    if (idx == lst.size())
        idx--;
    return Interpreter::eval_in(lst[idx], env);
}

// FIXME
Value uSEQ::useq_toggle_pick(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "toggle-pick";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, 0);
        return Value::error();
    }

    // // Evaluating & checking args for errors
    // for (size_t i = 0; i < args.size(); i++)
    // {
    //     // Eval
    //     Value pre_eval = args[i];
    //     args[i]        = args[i].eval(env);
    //     if (args[i].is_error())
    //     {
    //         report_error_arg_is_error(user_facing_name, i + 1,
    //                                   pre_eval.to_lisp_src());
    //         return Value::error();
    //     }
    // }

    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a vector or list",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY
    Value result     = Value::nil();
    std::vector list = args[0].as_sequential();
    float phasor     = args[1].as_float();

    // FIXME should this be evalled here?
    result = list[m_input_vals[USEQT1]].eval(env);

    return result;
}

Value uSEQ::useq_dm(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "dm";

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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
        // Check all-pred(s)

        if (!(args[i].is_number()))
        {
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
    result    = Value(index > 0 ? v2 : v1);

    return result;
}

// FIXME shouldn't the pulsewidth be compared to the
// 1/nth phase of the phasor, where n is the number of gates?
// the way it is now it defaults to muting the first half of the gates
Value uSEQ::useq_gates(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "gates";

    // Checking number of args
    if (!(2 <= args.size() <= 3))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::Between, 2, 3);
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
        // Check all-pred(s)

        if (i == 0)
        {
            if (!(args[i].is_sequential()))
            {
                report_error_wrong_specific_pred(
                    user_facing_name, i + 1, "a vector or list", args[i].display());
                return Value::error();
            }
        }
        else
        {
            if (!(args[i].is_number()))
            {
                report_error_wrong_specific_pred(user_facing_name, i + 1, "a number",
                                                 args[i].display());
                return Value::error();
            }
        }
    }

    // BODY
    Value result = Value::nil();

    bool pulse_width_specified = args.size() == 3;
    auto gates_vec             = args[0].as_sequential();

    double pulseWidth;
    double phasor;
    if (pulse_width_specified)
    {
        pulseWidth = args[1].as_float();
        phasor     = args[2].as_float();
    }
    else
    {
        pulseWidth = 0.5;
        phasor     = args[1].as_float();
    }

    const double val = fromList(gates_vec, phasor, env).as_int();
    const double gates =
        (fmod(phasor * gates_vec.size(), 1.0)) < pulseWidth ? 1.0 : 0.0;
    result = Value(val * gates);

    return result;
}

Value uSEQ::useq_gatesw(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "gatesw";

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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Check specific preds
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a vector or list",
                                         args[0].display());
        return Value::error();
    }

    if (!(args[1].is_number()))
    {
        report_error_wrong_all_pred(user_facing_name, 2, "a number",
                                    args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto gates_vec               = args[0].as_sequential();
    const double phasor          = args[1].as_float();
    const double val             = fromList(gates_vec, phasor, env).as_int();
    const double pulseWidth      = val / 9.0;
    const double relative_phasor = fmod(phasor * gates_vec.size(), 1.0);
    const double gate            = relative_phasor < pulseWidth ? 1.0 : 0.0;

    result = Value((val > 0 ? 1.0 : 0.0) * gate);

    return result;
}

Value uSEQ::useq_trigs(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "trigs";

    // Checking number of args
    if (!(2 <= args.size() <= 3))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::Between, 2, 3);
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Check specific preds
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a vector or list",
                                         args[0].display());
        return Value::error();
    }

    if (!(args.back().is_number()))
    {
        report_error_wrong_all_pred(user_facing_name, args.size() + 1, "a number",
                                    args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto lst = args[0].as_sequential();
    // NOTE: phasor at the end
    const double phasor     = args.back().as_float();
    const double val        = fromList(lst, phasor, env).as_int();
    const double amp        = std::clamp(val / 9.0, 0.0, 1.0);
    const double pulseWidth = args.size() == 3 ? args[1].as_float() : 0.1;
    const double gate = fmod(phasor * lst.size(), 1.0) < pulseWidth ? 1.0 : 0.0;
    result            = Value((val > 0 ? 1.0 : 0.0) * gate * amp);

    return result;
}

Value uSEQ::useq_euclidean(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "euclid";

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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }

        // Check all-pred(s)
        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();

    // NOTE: Phasor is the last arg
    const double phasor    = args.back().as_float();
    const int n            = args[0].as_int();
    const int k            = args[1].as_int();
    const int offset       = (args.size() >= 4) ? args[2].as_int() : 0;
    const float pulseWidth = (args.size() == 5) ? args[3].as_float() : 0.5;
    const float fi         = phasor * n;
    int i                  = static_cast<int>(fi);
    const float rem        = fi - i;
    if (i == n)
    {
        i--;
    }
    const int idx = ((i + n - offset) * k) % n;
    result        = Value(idx < k && rem < pulseWidth ? 1 : 0);

    return result;
}

Value uSEQ::useq_eu(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "eu";

    // Checking number of args
    if (!(3 <= args.size() <= 5))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::Between, 3, 4);
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }

        // Check all-pred(s)
        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();

    // NOTE: Phasor is the last arg
    const double phasor    = args.back().as_float();
    const int n            = args[0].as_int();
    const int k            = args[1].as_int();
    const float pulseWidth = (args.size() == 4) ? args[2].as_float() : 0.5;
    const float fi         = phasor * n;
    int i                  = static_cast<int>(fi);
    const float rem        = fi - i;
    if (i == n)
    {
        i--;
    }
    // NOTE: original, testing
    const int idx = ((i + n) * k) % n;
    result        = Value(idx < k && rem < pulseWidth ? 1 : 0);

    // NOTE: working one
    // const int idx             = ((i + n) * k) % n;
    // float effectivePulseWidth = (idx < k) ? pulseWidth : 0;
    // result                    = Value(rem < effectivePulseWidth ? 1 : 0);

    return result;
}

Value uSEQ::useq_ratiotrig(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "rpulse";

    // Checking number of args
    if (!(3 == args.size()))
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(
            user_facing_name, 1, "a sequential structure (e.g. a list or a vector)",
            args[0].display());
        return Value::error();
    }
    // Checking individual args
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }
    if (!(args[2].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto ratios           = args[0].as_sequential();
    const auto pulseWidth = args[1].as_float();
    const auto phase      = args[2].as_float();

    double trig     = 0;
    double ratioSum = 0;
    for (Value v : ratios)
    {
        ratioSum += v.as_float();
    }
    double phaseAdj           = ratioSum * phase;
    double accumulatedSum     = 0;
    double lastAccumulatedSum = 0;
    for (Value v : ratios)
    {
        accumulatedSum += v.as_float();
        if (phaseAdj <= accumulatedSum)
        {
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

Value uSEQ::useq_ratiostep(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "rstep";

    // Checking number of args
    if (!(2 == args.size()))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            Value pre_eval = args[i];
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(
            user_facing_name, 1, "a sequential structure (e.g. a list or a vector)",
            args[0].display());
        return Value::error();
    }
    // Checking individual args
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto ratios      = args[0].as_sequential();
    const auto phase = args[1].as_float();

    double phaseOut = 0;
    double ratioSum = 0;
    for (Value v : ratios)
    {
        ratioSum += v.as_float();
    }
    double phaseAdj           = ratioSum * phase;
    double accumulatedSum     = 0;
    double lastAccumulatedSum = 0;
    for (Value v : ratios)
    {
        accumulatedSum += v.as_float();
        if (phaseAdj <= accumulatedSum)
        {
            phaseOut = lastAccumulatedSum;
            break;
        }
        lastAccumulatedSum = accumulatedSum;
    }
    result = Value(phaseOut / ratioSum);
    return result;
}

LISP_FUNC_DECL(uSEQ::useq_ratioindex)
// Value uSEQ::useq_ratioindex(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "ridx";

    // Checking number of args
    if (!(2 == args.size()))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            Value pre_eval = args[i];
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(
            user_facing_name, 1, "a sequential structure (e.g. a list or a vector)",
            args[0].display());
        return Value::error();
    }
    // Checking individual args
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto ratios      = args[0].as_sequential();
    const auto phase = args[1].as_float();

    double index    = 0;
    double ratioSum = 0;
    for (Value v : ratios)
    {
        ratioSum += v.as_float();
    }
    double phaseAdj       = ratioSum * phase;
    double accumulatedSum = 0;
    for (Value v : ratios)
    {
        accumulatedSum += v.as_float();
        if (phaseAdj <= accumulatedSum)
        {
            break;
        }
        index++;
    }
    index /= static_cast<double>(ratios.size());
    result = Value(index);
    return result;
}

LISP_FUNC_DECL(uSEQ::useq_ratiowarp)
// Value uSEQ::useq_ratiowarp(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "rwarp";

    // Checking number of args
    if (!(2 == args.size()))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            Value pre_eval = args[i];
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(
            user_facing_name, 1, "a sequential structure (e.g. a list or a vector)",
            args[0].display());
        return Value::error();
    }
    // Checking individual args
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto ratios      = args[0].as_sequential();
    const auto phase = args[1].as_float();

    double output = 0;

    if (ratios.size() > 0)
    {
        double index      = 0;
        double indexWidth = 1.0 / ratios.size();

        double ratioSum = 0;
        for (Value v : ratios)
        {
            ratioSum += v.as_float();
        }

        double phaseAdj           = ratioSum * phase;
        double accumulatedSum     = 0;
        double lastAccumulatedSum = 0;
        for (Value v : ratios)
        {
            accumulatedSum += v.as_float();
            if (phaseAdj <= accumulatedSum)
            {
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

Value uSEQ::useq_phasor_offset(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "shift";

    // Checking number of args
    if (!(2 == args.size()))
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }
    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    const auto offset = args[0].as_float();
    auto phase        = args[1].as_float();

    phase  = std::fmod(phase + offset, 1.0);
    result = Value(phase);

    return result;
}

// NOTE: doesn't eval its arguments until they're selected by the phasor
Value uSEQ::useq_fromList(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "from-list";

    // Checking number of args
    // if (!(2 <= args.size() <= 3))
    if (!(args.size() == 2))
    {
        // error_wrong_num_args(user_facing_name, args.size(),
        //                      NumArgsComparison::Between, 2, 3);
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // NOTE: This needs to eval both of its args, including the list,
    // to cover for cases where the user passes anything other than a
    // list literal (e.g. a symbol that points to a list)
    //
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(
            user_facing_name, 1, "a sequential structure (e.g. a list or a vector)",
            args[0].display());
        return Value::error();
    }
    // Checking individual args
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    auto lst            = args[0].as_sequential();
    const double phasor = args[1].as_float();
    return fromList(lst, phasor, env);
}

// NOTE: duplicate of fromList, FIXME
Value uSEQ::useq_seq(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "seq";

    // Checking number of args
    // if (!(2 <= args.size() <= 3))
    if (!(args.size() == 2))
    {
        // error_wrong_num_args(user_facing_name, args.size(),
        //                      NumArgsComparison::Between, 2, 3);
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // NOTE: This needs to eval both of its args, including the list,
    // to cover for cases where the user passes anything other than a
    // list literal (e.g. a symbol that points to a list)
    //
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(
            user_facing_name, 1, "a sequential structure (e.g. a list or a vector)",
            args[0].display());
        return Value::error();
    }
    // Checking individual args
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    auto lst            = args[0].as_sequential();
    const double phasor = args[1].as_float();
    return fromList(lst, phasor, env);
}

Value flatten_impl(const Value& val, Environment& env)
{
    std::vector<Value> flattened;

    // int original_type = val.type;

    if (!val.is_sequential())
    {
        flattened.push_back(val);
    }
    else
    {
        auto valList = val.as_sequential();
        for (size_t i = 0; i < valList.size(); i++)
        {
            Value evaluatedElement = Interpreter::eval_in(valList[i], env);
            if (evaluatedElement.is_sequential())
            {
                auto flattenedElement =
                    flatten_impl(evaluatedElement, env).as_list();
                flattened.insert(flattened.end(), flattenedElement.begin(),
                                 flattenedElement.end());
            }
            else
            {
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

double uSEQ::simple_hashing_function(uint32_t input)
{
    // Combine input with seed using a fast mixing technique
    input ^= m_random_seed;
    input = ((input >> 16) ^ input) * 0x45d9f3b;
    input = ((input >> 16) ^ input) * 0x45d9f3b;
    input = (input >> 16) ^ input;

    // Convert to float in range [0, 1)
    // Uses bit manipulation to avoid floating-point division
    union
    {
        uint32_t i;
        float f;
    } convert;

    convert.i = (input & 0x007fffff) | 0x3f800000;
    return convert.f - 1.0f;
}

Value uSEQ::useq_index_rand(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "index-rand";

    // Checking number of args
    if (!(1 <= args.size() <= 3))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::Between, 0, 2);
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }

        // Check all-pred(s)
        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    bool scale                = args.size() > 1;
    bool lower_bound_provided = args.size() == 3;

    // BODY
    Value result = Value::nil();

    double index = args[args.size() - 1].as_float();
    double lo    = (scale && lower_bound_provided) ? args[0].as_float() : 0.0;
    double hi    = scale ? args[args.size() - 1].as_float() : 1.0;

    double rand_val = simple_hashing_function(index);
    rand_val        = lo + (rand_val * (hi - lo));
    // TODO
    result = Value(rand_val);

    return result;
}

Value uSEQ::useq_random(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "random";

    // Checking number of args
    if (!(0 <= args.size() <= 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::Between, 0, 2);
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }

        // Check all-pred(s)
        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    bool scale                = args.size() > 0;
    bool lower_bound_provided = args.size() == 2;

    // BODY
    Value result = Value::nil();

    uint32_t current_beat_num = static_cast<uint32_t>(
        env.get("beat-num")
            .value_or(Value(static_cast<int>(m_current_beat_num)))
            .as_int());

    double rand_val = simple_hashing_function(current_beat_num);

    if (scale)
    {
        double low  = lower_bound_provided ? args[0].as_float() : 0.0;
        double high = args[1].as_float();
        rand_val    = low + (rand_val * (high - low));
    }

    // TODO
    result = Value(rand_val);

    return result;
}

Value uSEQ::useq_loop_at_time(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "loop-at-time";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, 2);
        return Value::error();
    }

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

    // BODY
    Value result            = Value::nil();
    double current_time_s   = env.get("t").value().as_float();
    double modulo_time_s    = args[0].as_float();
    double new_time_seconds = fmod(current_time_s, modulo_time_s);
    double new_time_micros  = new_time_seconds * 1e+6;

    result = eval_at_time(args[1], env, new_time_micros);

    return result;
}

// TODO test
Value uSEQ::useq_flatten(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "flatten";

    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Check list for error
    Value pre_eval = args[0];
    args[0]        = args[0].eval(env);
    if (args[0].is_error())
    {
        report_error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = flatten_impl(args[0], env);

    return result;
}

// NOTE: duplicate of fromFlattenedList, FIXME
Value uSEQ::useq_flatseq(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "flatseq";

    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // NOTE: This needs to eval both of its args, including the list,
    // to cover for cases where the user passes anything other than a
    // list literal (e.g. a symbol that points to a list)
    //
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(user_facing_name, 0, "a list or a vector",
                                         args[0].display());
        return Value::error();
    }

    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto lst      = flatten_impl(args[0], env).as_sequential();
    double phasor = args[1].as_float();
    result        = fromList(lst, phasor, env);

    return result;
}

Value uSEQ::useq_fromFlattenedList(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "from-flat-list";

    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // NOTE: This needs to eval both of its args, including the list,
    // to cover for cases where the user passes anything other than a
    // list literal (e.g. a symbol that points to a list)
    //
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(
            user_facing_name, 0, "a sequential structure (e.g. a list or a vector)",
            args[0].display());
        return Value::error();
    }

    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto lst      = flatten_impl(args[0], env).as_sequential();
    double phasor = args[1].as_float();
    result        = fromList(lst, phasor, env);

    return result;
}

Value uSEQ::useq_interpolate(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "interp";

    // Check num arguments
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(user_facing_name, 0, "a list or a vector",
                                         args[0].display());
        return Value::error();
    }

    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result  = Value::nil();
    auto lst      = args[0].as_list();
    double phasor = args[1].as_float();
    if (phasor < 0.0)
    {
        phasor = 0;
    }
    else if (phasor > 1)
    {
        phasor = 1;
    }
    float a;
    double index = phasor * (lst.size() - 1);
    size_t pos0  = static_cast<size_t>(index);
    if (pos0 == (lst.size() - 1))
        pos0--;
    a         = (index - pos0);
    double v2 = lst[pos0 + 1].as_float();
    double v1 = lst[pos0].as_float();
    result    = Value(((v2 - v1) * a) + v1);

    return result;
}

Value uSEQ::useq_step(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "step";

    // Check num arguments
    if (!(2 <= args.size() <= 3))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::Between, 2, 3);
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }

        // Check all-pred(s)
        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();

    const int count      = args[0].as_int();
    bool offset_provided = args.size() == 3;
    double phasor, offset;

    if (offset_provided)
    {
        offset = args[1].as_float();
        phasor = args[2].as_float();
    }
    else
    {
        offset = 0;
        phasor = args[1].as_float();
    }

    double val = static_cast<int>(phasor * abs(count));
    if (val == count)
        val--;
    result = Value((count > 0 ? val : count - 1 - val) + offset);
    return result;
}

BUILTINFUNC_NOEVAL_MEMBER(useq_firmware_info, //
                                              // println(USEQ_FIRMWARE_VERSION);
                          String msg = "uSEQ Firmware Version: " +
                                       String(USEQ_FIRMWARE_VERSION);
#ifdef MUSICTHING
                          msg += " (Music Thing Workshop Computer Edition)";
#endif
                          ret = Value::string(msg);, 0)

BUILTINFUNC_NOEVAL_MEMBER(useq_report_firmware_info, //
                          message_editor((String)USEQ_FIRMWARE_VERSION);
                          // String msg = "{";
                          // msg += "\"release_date\": \"";
                          // msg += String((String)USEQ_FIRMWARE_RELEASE_DATE);
                          // msg += "\", {\"version\": \"";
                          // msg += String((String)USEQ_FIRMWARE_VERSION); //
                          // msg += "\"}";                                 //
                          // println(msg);
                          , 0)

#ifdef MIDIOUT
// midi drum out
BUILTINFUNC_MEMBER(
    useq_mdo, int midiNote = args[0].as_int(); if (args[1] != 0) {
        useqMDOMap[midiNote] = args[1];
    } else { useqMDOMap.erase(midiNote); },
                                               2)
#endif

BUILTINFUNC_NOEVAL_MEMBER(useq_q0, 
        std::vector<Value> new_form;
        set("q-expr", args[0]); 
        m_q0AST = args[0];
        ret                  = Value::atom("q0");
, 1)

// TODO: there is potentially a lot of duplicated/wasted memory by storing
// the exprs in both the environment and the class member vectors
// especially once the exprs get more and more complex

BUILTINFUNC_NOEVAL_MEMBER(
    useq_a1,
    if (NUM_CONTINUOUS_OUTS >= 1) {
        std::vector<Value> new_form;
        new_form.push_back(Value::atom("lambda"));
        new_form.push_back(args[0]);
        set_expr("a1", Value(new_form));
        m_continuous_ASTs[0] = args[0];
        ret                  = Value::atom("a1");
    },
    1)

BUILTINFUNC_NOEVAL_MEMBER(
    useq_a2,
    if (NUM_CONTINUOUS_OUTS >= 2) {
        set_expr("a2", args[0]);
        m_continuous_ASTs[1] = { args[0] };
        ret                  = Value::atom("a2");
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_a3,
    if (NUM_CONTINUOUS_OUTS >= 3) {
        set_expr("a3", args[0]);
        m_continuous_ASTs[2] = { args[0] };
        ret                  = Value::atom("a3");
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_a4,
    if (NUM_CONTINUOUS_OUTS >= 4) {
        set_expr("a4", args[0]);
        m_continuous_ASTs[3] = { args[0] };
        ret                  = Value::atom("a4");
    },
    1)

BUILTINFUNC_NOEVAL_MEMBER(
    useq_a5,
    if (NUM_CONTINUOUS_OUTS >= 5) {
        set_expr("a5", args[0]);
        m_continuous_ASTs[4] = { args[0] };
        ret                  = Value::atom("a5");
    },
    1)

BUILTINFUNC_NOEVAL_MEMBER(
    useq_a6,
    if (NUM_CONTINUOUS_OUTS >= 6) {
        set_expr("a6", args[0]);
        m_continuous_ASTs[5] = { args[0] };
        ret                  = Value::atom("a6");
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_a7,
    if (NUM_CONTINUOUS_OUTS >= 7) {
        set_expr("a7", args[0]);
        m_continuous_ASTs[6] = { args[0] };
        ret                  = Value::atom("a7");
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_a8,
    if (NUM_CONTINUOUS_OUTS >= 8) {
        set_expr("a6", args[0]);
        m_continuous_ASTs[7] = { args[0] };
        ret                  = Value::atom("a8");
    },
    1)

// DIGITAL OUTS
BUILTINFUNC_NOEVAL_MEMBER(
    useq_d1,
    if (NUM_BINARY_OUTS >= 1) {
        set_expr("d1", args[0]);
        m_binary_ASTs[0] = { args[0] };
        ret              = Value::atom("d1");
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_d2,
    if (NUM_BINARY_OUTS >= 2) {
        set_expr("d2", args[0]);
        m_binary_ASTs[1] = { args[0] };
        ret              = Value::atom("d2");
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_d3,
    if (NUM_BINARY_OUTS >= 3) {
        set_expr("d3", args[0]);
        m_binary_ASTs[2] = { args[0] };
        ret              = Value::atom("d3");
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_d4,
    if (NUM_BINARY_OUTS >= 4) {
        set_expr("d4", args[0]);
        m_binary_ASTs[3] = { args[0] };
        ret              = Value::atom("d4");
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_d5,
    if (NUM_BINARY_OUTS >= 5) {
        set_expr("d5", args[0]);
        m_binary_ASTs[4] = { args[0] };
        ret              = Value::atom("d5");
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_d6,
    if (NUM_BINARY_OUTS >= 6) {
        set_expr("d6", args[0]);
        m_binary_ASTs[5] = { args[0] };
        ret              = Value::atom("d6");
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_d7,
    if (NUM_BINARY_OUTS >= 7) {
        set_expr("d7", args[0]);
        m_binary_ASTs[5] = { args[0] };
        ret              = Value::atom("d7");
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_d8,
    if (NUM_BINARY_OUTS >= 8) {
        set_expr("d8", args[0]);
        m_binary_ASTs[5] = { args[0] };
        ret              = Value::atom("d8");
    },
    1)

BUILTINFUNC_NOEVAL_MEMBER(useq_s1, set_expr("s1", args[0]);
                          m_serial_ASTs[0] = { args[0] }; ret = Value::atom("s1");
                          ,
                          // println(m_serial_ASTs.size());,
                          1)

BUILTINFUNC_NOEVAL_MEMBER(useq_s2, set_expr("s2", args[0]);
                          m_serial_ASTs[1] = { args[0] }; ret = Value::atom("s2");
                          , 1)
BUILTINFUNC_NOEVAL_MEMBER(useq_s3, set_expr("s3", args[0]);
                          m_serial_ASTs[2] = { args[0] }; ret = Value::atom("s3");
                          , 1)
BUILTINFUNC_NOEVAL_MEMBER(useq_s4, set_expr("s4", args[0]);
                          m_serial_ASTs[3] = { args[0] }; ret = Value::atom("s5");
                          , 1)
BUILTINFUNC_NOEVAL_MEMBER(useq_s5, set_expr("s5", args[0]);
                          m_serial_ASTs[4] = { args[0] }; ret = Value::atom("s5");
                          , 1)
BUILTINFUNC_NOEVAL_MEMBER(useq_s6, set_expr("s6", args[0]);
                          m_serial_ASTs[5] = { args[0] }; ret = Value::atom("s6");
                          , 1)
BUILTINFUNC_NOEVAL_MEMBER(useq_s7, set_expr("s7", args[0]);
                          m_serial_ASTs[6] = { args[0] }; ret = Value::atom("s7");
                          , 1)
BUILTINFUNC_NOEVAL_MEMBER(useq_s8, set_expr("s8", args[0]);
                          m_serial_ASTs[7] = { args[0] }; ret = Value::atom("s8");
                          , 1)

// Functions that should be available in all builds (not just Arduino)
BUILTINFUNC_NOEVAL_MEMBER(useq_rewind_logical_time,
                          reset_logical_time();
                          , 0)

// Function that should be available in all builds (not just Arduino)
void uSEQ::clear_all_outputs()
{
    for (int i = 0; i < m_continuous_ASTs.size(); i++)
    {
        String name          = "a" + String(i + 1);
        m_continuous_ASTs[i] = default_continuous_expr;
        m_def_exprs.erase(name);
    }

    for (int i = 0; i < m_binary_ASTs.size(); i++)
    {
        String name      = "d" + String(i + 1);
        m_binary_ASTs[i] = default_binary_expr;
        m_def_exprs.erase(name);
    }

    for (int i = 0; i < m_serial_ASTs.size(); i++)
    {
        String name      = "s" + String(i + 1);
        m_serial_ASTs[i] = default_serial_expr;
        m_def_exprs.erase(name);
    }
}

BUILTINFUNC_NOEVAL_MEMBER(useq_stop_all,
                          clear_all_outputs();
                          println("All outputs cleared.");, 0)
