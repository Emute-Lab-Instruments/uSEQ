#include "uSEQ.h"
#include "uSEQ/output_manager.h"
#include "uSEQ/io_manager.h"
#include "utils.h"

// Creates a Lisp Value of type BUILTIN_METHOD,
// which requires
#define INSERT_BUILTINDEF(__name__, __func_name__)                                  \
    Environment::builtindefs()[__name__] =                                            \
        Value((String)__name__, &uSEQ::__func_name__);

void uSEQ::init_builtinfuncs()
{
    DBG("uSEQ::init_builtinfuncs");
    
    // Call parent class init_builtinfuncs first
    ModuLispInterpreter::init_builtinfuncs();
    
    // Add uSEQ-specific hardware functions
    
    // Unified output system - individual functions forward to OutputManager
    // Analog outputs (a1-a8)
    INSERT_BUILTINDEF("a1", useq_a1);
    INSERT_BUILTINDEF("a2", useq_a2);
    INSERT_BUILTINDEF("a3", useq_a3);
    INSERT_BUILTINDEF("a4", useq_a4);
    INSERT_BUILTINDEF("a5", useq_a5);
    INSERT_BUILTINDEF("a6", useq_a6);
    INSERT_BUILTINDEF("a7", useq_a7);
    INSERT_BUILTINDEF("a8", useq_a8);
    
    // Digital outputs (d1-d8)
    INSERT_BUILTINDEF("d1", useq_d1);
    INSERT_BUILTINDEF("d2", useq_d2);
    INSERT_BUILTINDEF("d3", useq_d3);
    INSERT_BUILTINDEF("d4", useq_d4);
    INSERT_BUILTINDEF("d5", useq_d5);
    INSERT_BUILTINDEF("d6", useq_d6);
    INSERT_BUILTINDEF("d7", useq_d7);
    INSERT_BUILTINDEF("d8", useq_d8);
    
    // Serial outputs (s1-s8)
    INSERT_BUILTINDEF("s1", useq_s1);
    INSERT_BUILTINDEF("s2", useq_s2);
    INSERT_BUILTINDEF("s3", useq_s3);
    INSERT_BUILTINDEF("s4", useq_s4);
    INSERT_BUILTINDEF("s5", useq_s5);
    INSERT_BUILTINDEF("s6", useq_s6);
    INSERT_BUILTINDEF("s7", useq_s7);
    INSERT_BUILTINDEF("s8", useq_s8);

    INSERT_BUILTINDEF("q0", useq_q0);

    // Hardware control functions  
    INSERT_BUILTINDEF("swr", useq_swr);
    INSERT_BUILTINDEF("rot", useq_rot);
    
#ifdef ARDUINO
    INSERT_BUILTINDEF("toggle-pick", useq_toggle_pick);
    INSERT_BUILTINDEF("swm", useq_swm);
    INSERT_BUILTINDEF("swt", useq_swt);
    INSERT_BUILTINDEF("ssin", useq_ssin);
#endif

    // Input functions
    INSERT_BUILTINDEF("in1", useq_in1);
    INSERT_BUILTINDEF("in2", useq_in2);
    INSERT_BUILTINDEF("gin1", useq_in1);  // Alias for in1
    INSERT_BUILTINDEF("gin2", useq_in2);  // Alias for in2
    INSERT_BUILTINDEF("ain1", useq_ain1);
    INSERT_BUILTINDEF("ain2", useq_ain2);

    // Output getter functions
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

    // Hardware IO functions - TODO: Implement
#ifdef ARDUINO
    INSERT_BUILTINDEF("useqaw", ard_useqaw);
    INSERT_BUILTINDEF("useqdw", ard_useqdw);
#endif
    // INSERT_BUILTINDEF("useqdr", ard_useqdr);  // Function doesn't exist
    // INSERT_BUILTINDEF("useqar", ard_useqar);  // Function doesn't exist
    
    // Flash/persistence
    // Flash/persistence functions - Not yet implemented
    // INSERT_BUILTINDEF("flash-save", useq_flash_save);
    // INSERT_BUILTINDEF("flash-save-named", useq_flash_save_named);
    // INSERT_BUILTINDEF("flash-load", useq_flash_load);
    // INSERT_BUILTINDEF("flash-list", useq_flash_list);
    // INSERT_BUILTINDEF("flash-erase", useq_flash_erase_all);
    // INSERT_BUILTINDEF("flash-print", useq_flash_print);
    
    // System functions
    // INSERT_BUILTINDEF("useq-bpm", useq_bpm);  // Function doesn't exist
    INSERT_BUILTINDEF("fw-info", useq_firmware_info);
    INSERT_BUILTINDEF("fw-version", useq_firmware_info);
    // Control and state functions - Not yet implemented
    // INSERT_BUILTINDEF("useq-set-sync-led", useq_set_sync_led);
    // INSERT_BUILTINDEF("useq-sync-led", useq_set_sync_led);
    // INSERT_BUILTINDEF("trigger-out", useq_sync_led_off);
    // INSERT_BUILTINDEF("useq-stop", useq_stop);
    // INSERT_BUILTINDEF("quit", useq_stop);
    // INSERT_BUILTINDEF("useq-save", useq_save);
    // INSERT_BUILTINDEF("spit", useq_save);
    // INSERT_BUILTINDEF("save", useq_save);
    // INSERT_BUILTINDEF("useq-save-form", useq_save_form);
    // INSERT_BUILTINDEF("useq-load", useq_load);
    // INSERT_BUILTINDEF("slurp", useq_load);
    // INSERT_BUILTINDEF("load", useq_load);
    // INSERT_BUILTINDEF("print-schedule", useq_print_schedule);
    
    // I2C/sync functions - TODO: Implement these
    // I2C functions - Not yet implemented
    // INSERT_BUILTINDEF("useq-send-trigger-i2c", useq_send_trigger_i2c);
    // INSERT_BUILTINDEF("useq-send-clock-i2c", useq_send_clock_i2c);
    // INSERT_BUILTINDEF("useq-send-note-i2c", useq_send_note_i2c);
    // INSERT_BUILTINDEF("wait-trigger", useq_wait_trigger);
    // INSERT_BUILTINDEF("useq-wait-trigger-from-i2c", useq_wait_trigger_from_i2c);
    // INSERT_BUILTINDEF("sync", useq_wait_for_sync_trigger);
    // INSERT_BUILTINDEF("wait-clock", useq_wait_for_clock_trigger);
    // INSERT_BUILTINDEF("useq-get-i2c-device-count", useq_i2c_device_count);
    // INSERT_BUILTINDEF("print-i2c-devices", useq_print_i2c_device_addresses);
    // INSERT_BUILTINDEF("i2c-leader", useq_i2c_set_as_leader);
    // INSERT_BUILTINDEF("i2c-follower", useq_i2c_set_as_follower);
    // INSERT_BUILTINDEF("i2c-receive", useq_i2c_recieve_notenum);
    
    // DSP engine functions (including the new ones)
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

BUILTINFUNC_NOEVAL_MEMBER(useq_firmware_info, //
                                              // println(USEQ_FIRMWARE_VERSION);
                          String msg = "uSEQ Firmware Version: " +
                                       String(USEQ_FIRMWARE_VERSION);
#ifdef MUSICTHING
                          msg += " (Music Thing Workshop Computer Edition)";
#endif
                          println(msg); ret = Value();
                          , 0)

BUILTINFUNC_NOEVAL_MEMBER(useq_q0, set("q-expr", args[0]); m_q0AST = args[0]; ret = Value::atom("q0");
                          , 1)

// TODO: there is potentially a lot of duplicated/wasted memory by storing
// the exprs in both the environment and the class member vectors
// especially once the exprs get more and more complex

// Unified analog output functions - forward to OutputManager
Value uSEQ::useq_a1(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(1, OutputManager::OutputType::CONTINUOUS, args, env);
}

Value uSEQ::useq_a2(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(2, OutputManager::OutputType::CONTINUOUS, args, env);
}
Value uSEQ::useq_a3(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(3, OutputManager::OutputType::CONTINUOUS, args, env);
}
Value uSEQ::useq_a4(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(4, OutputManager::OutputType::CONTINUOUS, args, env);
}
Value uSEQ::useq_a5(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(5, OutputManager::OutputType::CONTINUOUS, args, env);
}
Value uSEQ::useq_a6(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(6, OutputManager::OutputType::CONTINUOUS, args, env);
}
Value uSEQ::useq_a7(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(7, OutputManager::OutputType::CONTINUOUS, args, env);
}
Value uSEQ::useq_a8(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(8, OutputManager::OutputType::CONTINUOUS, args, env);
}

// Unified digital output functions - forward to OutputManager
Value uSEQ::useq_d1(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(1, OutputManager::OutputType::BINARY, args, env);
}
Value uSEQ::useq_d2(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(2, OutputManager::OutputType::BINARY, args, env);
}
Value uSEQ::useq_d3(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(3, OutputManager::OutputType::BINARY, args, env);
}
Value uSEQ::useq_d4(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(4, OutputManager::OutputType::BINARY, args, env);
}
Value uSEQ::useq_d5(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(5, OutputManager::OutputType::BINARY, args, env);
}
Value uSEQ::useq_d6(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(6, OutputManager::OutputType::BINARY, args, env);
}
Value uSEQ::useq_d7(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(7, OutputManager::OutputType::BINARY, args, env);
}
Value uSEQ::useq_d8(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(8, OutputManager::OutputType::BINARY, args, env);
}

// Unified serial output functions - forward to OutputManager
Value uSEQ::useq_s1(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(1, OutputManager::OutputType::SERIAL, args, env);
}
Value uSEQ::useq_s2(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(2, OutputManager::OutputType::SERIAL, args, env);
}
Value uSEQ::useq_s3(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(3, OutputManager::OutputType::SERIAL, args, env);
}
Value uSEQ::useq_s4(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(4, OutputManager::OutputType::SERIAL, args, env);
}
Value uSEQ::useq_s5(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(5, OutputManager::OutputType::SERIAL, args, env);
}
Value uSEQ::useq_s6(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(6, OutputManager::OutputType::SERIAL, args, env);
}
Value uSEQ::useq_s7(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(7, OutputManager::OutputType::SERIAL, args, env);
}
Value uSEQ::useq_s8(std::vector<Value>& args, Environment& env) {
    return m_output_manager->handle_output_setter(8, OutputManager::OutputType::SERIAL, args, env);
}

// Function that should be available in all builds (not just Arduino)
void uSEQ::clear_all_outputs() {
    for (int i = 0; static_cast<size_t>(i) < m_continuous_ASTs.size(); i++) {
        String name = "a" + String(i + 1);
        m_continuous_ASTs[i] = default_continuous_expr;
        m_def_exprs.erase(name);
    }

    for (int i = 0; static_cast<size_t>(i) < m_binary_ASTs.size(); i++) {
        String name = "d" + String(i + 1);
        m_binary_ASTs[i] = default_binary_expr;
        m_def_exprs.erase(name);
    }

    for (int i = 0; static_cast<size_t>(i) < m_serial_ASTs.size(); i++) {
        String name = "s" + String(i + 1);
        m_serial_ASTs[i] = default_serial_expr;
        m_def_exprs.erase(name);
    }
}

BUILTINFUNC_NOEVAL_MEMBER(useq_stop_all, clear_all_outputs();
                          println("All outputs cleared.");, 0)

Value uSEQ::useq_in1(std::vector<Value> &args, Environment &env) {
    return m_io_manager ? Value(m_io_manager->get_input_value(USEQI1)) : Value::nil();
}

Value uSEQ::useq_in2(std::vector<Value> &args, Environment &env) {
    return m_io_manager ? Value(m_io_manager->get_input_value(USEQI2)) : Value::nil();
}

Value uSEQ::useq_ain1(std::vector<Value> &args, Environment &env) {
    return m_io_manager ? Value(m_io_manager->get_input_value(USEQAI1)) : Value::nil();
}

Value uSEQ::useq_ain2(std::vector<Value> &args, Environment &env) {
    return m_io_manager ? Value(m_io_manager->get_input_value(USEQAI2)) : Value::nil();
}

// Unified analog output getter functions - forward to OutputManager
Value uSEQ::useq_get_a1(std::vector<Value> &args, Environment &env) {
    return m_output_manager->handle_output_getter(1, OutputManager::OutputType::CONTINUOUS, args, env);
}
Value uSEQ::useq_get_a2(std::vector<Value> &args, Environment &env) {
    return m_output_manager->handle_output_getter(2, OutputManager::OutputType::CONTINUOUS, args, env);
}
Value uSEQ::useq_get_a3(std::vector<Value> &args, Environment &env) {
    return m_output_manager->handle_output_getter(3, OutputManager::OutputType::CONTINUOUS, args, env);
}
Value uSEQ::useq_get_a4(std::vector<Value> &args, Environment &env) {
    return m_output_manager->handle_output_getter(4, OutputManager::OutputType::CONTINUOUS, args, env);
}
Value uSEQ::useq_get_a5(std::vector<Value> &args, Environment &env) {
    return m_output_manager->handle_output_getter(5, OutputManager::OutputType::CONTINUOUS, args, env);
}
Value uSEQ::useq_get_a6(std::vector<Value> &args, Environment &env) {
    return m_output_manager->handle_output_getter(6, OutputManager::OutputType::CONTINUOUS, args, env);
}
Value uSEQ::useq_get_a7(std::vector<Value> &args, Environment &env) {
    return m_output_manager->handle_output_getter(7, OutputManager::OutputType::CONTINUOUS, args, env);
}
Value uSEQ::useq_get_a8(std::vector<Value> &args, Environment &env) {
    return m_output_manager->handle_output_getter(8, OutputManager::OutputType::CONTINUOUS, args, env);
}

// Unified digital output getter functions - forward to OutputManager
Value uSEQ::useq_get_d1(std::vector<Value> &args, Environment &env) {
    return m_output_manager->handle_output_getter(1, OutputManager::OutputType::BINARY, args, env);
}
Value uSEQ::useq_get_d2(std::vector<Value> &args, Environment &env) {
    return m_output_manager->handle_output_getter(2, OutputManager::OutputType::BINARY, args, env);
}
Value uSEQ::useq_get_d3(std::vector<Value> &args, Environment &env) {
    return m_output_manager->handle_output_getter(3, OutputManager::OutputType::BINARY, args, env);
}
Value uSEQ::useq_get_d4(std::vector<Value> &args, Environment &env) {
    return m_output_manager->handle_output_getter(4, OutputManager::OutputType::BINARY, args, env);
}
Value uSEQ::useq_get_d5(std::vector<Value> &args, Environment &env) {
    return m_output_manager->handle_output_getter(5, OutputManager::OutputType::BINARY, args, env);
}
Value uSEQ::useq_get_d6(std::vector<Value> &args, Environment &env) {
    return m_output_manager->handle_output_getter(6, OutputManager::OutputType::BINARY, args, env);
}
Value uSEQ::useq_get_d7(std::vector<Value> &args, Environment &env) {
    return m_output_manager->handle_output_getter(7, OutputManager::OutputType::BINARY, args, env);
}
Value uSEQ::useq_get_d8(std::vector<Value> &args, Environment &env) {
    return m_output_manager->handle_output_getter(8, OutputManager::OutputType::BINARY, args, env);
}

// Value uSEQ::useq_get_s1(std::vector<Value>& args, Environment&
// env)
// {
//     return Value(m_serial_vals[0]);
// }
// Value uSEQ::useq_get_s2(std::vector<Value>& args, Environment&
// env)
// {
//     return Value(m_serial_vals[1]);
// }
// Value uSEQ::useq_get_s3(std::vector<Value>& args, Environment&
// env)
// {
//     return Value(m_serial_vals[2]);
// }
// Value uSEQ::useq_get_s4(std::vector<Value>& args, Environment&
// env)
// {
//     return Value(m_serial_vals[3]);
// }
// Value uSEQ::useq_get_s5(std::vector<Value>& args, Environment&
// env)
// {
//     return Value(m_serial_vals[4]);
// }
// Value uSEQ::useq_get_s6(std::vector<Value>& args, Environment&
// env)
// {
//     return Value(m_serial_vals[5]);
// }
// Value uSEQ::useq_get_s7(std::vector<Value>& args, Environment&
// env)
// {
//     return Value(m_serial_vals[6]);
// }
// Value uSEQ::useq_get_s8(std::vector<Value>& args, Environment&
// env)
// {
//     return Value(m_serial_vals[7]);
// }

#ifdef MUSICTHING

Value uSEQ::useq_mt_knob(std::vector<Value> &args, Environment &env) {
    return m_io_manager ? Value(m_io_manager->get_input_value(MTMAINKNOB)) : Value::nil();
}

Value uSEQ::useq_mt_knobx(std::vector<Value> &args, Environment &env) {
    return m_io_manager ? Value(m_io_manager->get_input_value(MTXKNOB)) : Value::nil();
}

Value uSEQ::useq_mt_knoby(std::vector<Value> &args, Environment &env) {
    return m_io_manager ? Value(m_io_manager->get_input_value(MTYKNOB)) : Value::nil();
}

Value uSEQ::useq_mt_swz(std::vector<Value> &args, Environment &env) {
    return m_io_manager ? Value(m_io_manager->get_input_value(MTZSWITCH)) : Value::nil();
}
#endif
// clock sources

// BUILTINFUNC_MEMBER(
//     useq_reset_internal_clock,
//     constexpr const char* user_facing_name = "reset-clock-int";
//     if (!(args.size() == 0)) {
//         report_error_wrong_num_args(user_facing_name,
//         static_cast<int>(args.size()),
//                                     NumArgsComparison::EqualTo, 0, 0);
//         return Value::error();
//     } reset_logical_time();
//     return Value::nil();, 0)

// BUILTINFUNC_MEMBER(
//     useq_get_clock_source,
//     constexpr const char* user_facing_name = "get-clock-source";
//     if (!(args.size() == 0)) {
//         report_error_wrong_num_args(user_facing_name,
//         static_cast<int>(args.size()),
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
//                    useq_clock_source =
//                    uSEQ::CLOCK_SOURCES::INTERNAL;
//                    println("Clock source set to internal"); return
//                    Value::nil();, 0)

// BUILTINFUNC_MEMBER(
//     useq_set_clock_external,
//     constexpr const char* user_facing_name = "set-clock-ext";

//     // Checking number of args
//     if (!(args.size() == 2)) {
//         report_error_wrong_num_args(user_facing_name,
//         static_cast<int>(args.size()),
//                                     NumArgsComparison::EqualTo, 1, 0);
//         return Value::error();
//     } if (!(args[0].is_number())) {
//         report_error_wrong_specific_pred(user_facing_name, 1, "a number",
//                                          args[0].display());
//         return Value::error();
//     } else if (args[0] < 1 || args[0] > 2) {
//         report_custom_function_error(user_facing_name,
//                                      "The clock source can be either input 1
//                                      or 2");
//     } if (!(args[1].is_number())) {
//         report_error_wrong_specific_pred(user_facing_name, 1, "a number",
//                                          args[0].display());
//         return Value::error();
//     } else if (args[1] <= 0) {
//         report_custom_function_error(user_facing_name,
//                                      "The clock divisor must be more than
//                                      0");
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
    constexpr const char *user_facing_name = "reset-clock-ext";
    if (!(args.size() == 0)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 0, 0);
        return Value::error();
    } reset_ext_tracking();
    return Value::nil();, 0)

BUILTINFUNC_MEMBER(
    useq_reset_internal_clock,
    constexpr const char *user_facing_name = "reset-clock-int";
    if (!(args.size() == 0)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 0, 0);
        return Value::error();
    } reset_logical_time();
    return Value::nil();, 0)

BUILTINFUNC_MEMBER(
    useq_get_clock_source,
    constexpr const char *user_facing_name = "get-clock-source";
    if (!(args.size() == 0)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
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
                   println("Clock source set to internal"); return Value::nil();
                   , 0)

BUILTINFUNC_MEMBER(
    useq_set_clock_external,
    constexpr const char *user_facing_name = "set-clock-ext";

    // Checking number of args
    if (!(args.size() == 2)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 1, 0);
        return Value::error();
    } if (!(args[0].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    } else if (args[0] < 1 || args[0] > 2) {
        report_custom_function_error(
            user_facing_name, "The clock source can be either input 1 or 2");
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

BUILTINFUNC_MEMBER(useq_swr, ret = m_io_manager ? Value(m_io_manager->get_input_value(USEQRS1)) : Value::nil();, 0)

BUILTINFUNC_MEMBER(useq_rot, ret = m_io_manager ? Value(m_io_manager->get_input_value(USEQR1)) : Value::nil();, 0)
