#include "uSEQ.h"



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
    // d
    INSERT_BUILTINDEF("d1", useq_d1);
    INSERT_BUILTINDEF("d2", useq_d2);
    INSERT_BUILTINDEF("d3", useq_d3);
    INSERT_BUILTINDEF("d4", useq_d4);
    INSERT_BUILTINDEF("d5", useq_d5);
    INSERT_BUILTINDEF("d6", useq_d6);
    // s
    INSERT_BUILTINDEF("s1", useq_s1);
    INSERT_BUILTINDEF("s2", useq_s2);
    INSERT_BUILTINDEF("s3", useq_s3);
    INSERT_BUILTINDEF("s4", useq_s4);
    INSERT_BUILTINDEF("s5", useq_s5);
    INSERT_BUILTINDEF("s6", useq_s6);
    INSERT_BUILTINDEF("s7", useq_s7);
    INSERT_BUILTINDEF("s8", useq_s8);

    // These are not class methods, so they can be inserted normally
    INSERT_BUILTINDEF("useqaw", ard_useqaw);
    INSERT_BUILTINDEF("useqdw", ard_useqdw);

    // These are all class methods
    INSERT_BUILTINDEF("toggle-sel", useq_toggle_select);
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

    // TODO
#ifdef MUSICTHING
    Environment::builtindefs["knob"]  = Value("knob", builtin::useq_mt_knob);
    Environment::builtindefs["knobx"] = Value("knobx", builtin::useq_mt_knobx);
    Environment::builtindefs["knoby"] = Value("knoby", builtin::useq_mt_knoby);
    Environment::builtindefs["swz"]   = Value("swz", builtin::useq_mt_swz);
#endif

#ifdef MIDIOUT
    INSERT_BUILTINDEF("mdo", useq_mdo);
#endif

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

    // NOTE: these are NOT meant to be user interface, just for
    // more granular dev tests
    INSERT_BUILTINDEF("write-flash-info", useq_write_flash_info);
    INSERT_BUILTINDEF("load-flash-info", useq_load_flash_info);

    INSERT_BUILTINDEF("write-flash-env", useq_write_flash_env);
    INSERT_BUILTINDEF("load-flash-env", useq_load_flash_env);

    INSERT_BUILTINDEF("useq-autoload-flash", useq_autoload_flash);
    INSERT_BUILTINDEF("useq-stop-all", useq_stop_all);

    // FIRMWARE
    // NOTE: for user interface only
    INSERT_BUILTINDEF("useq-firmware-info", useq_firmware_info);
    // NOTE: for editor use only
    INSERT_BUILTINDEF("useq-report-firmware-info", useq_report_firmware_info);
    // INSERT_BUILTINDEF("useq-clear-flash", useq_clear_non_program_flash);
}

BUILTINFUNC_NOEVAL_MEMBER(useq_firmware_info, //
                                              // println(USEQ_FIRMWARE_VERSION);
                          String msg = "uSEQ Firmware Version: " +
                                       String(USEQ_FIRMWARE_VERSION);
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

// Lisp interfaces
BUILTINFUNC_NOEVAL_MEMBER(useq_reboot,
                          //
                          reboot();
                          , 0)

BUILTINFUNC_MEMBER(useq_write_flash_info,
                   //
                   write_flash_info();
                   , 0)

BUILTINFUNC_MEMBER(useq_load_flash_info,
                   //
                   // print_flash_vars();
                   load_flash_info();
                   , 0)

BUILTINFUNC_MEMBER(useq_write_flash_env,
                   //
                   write_flash_env();
                   , 0)

BUILTINFUNC_MEMBER(useq_load_flash_env,
                   //
                   // print_flash_vars();
                   // println("Free heap: " + String(free_heap()));
                   load_flash_env();
                   , 0)

BUILTINFUNC_NOEVAL_MEMBER(useq_get_my_id,
                          //
                          ret = Value(m_my_id);
                          , 0)

BUILTINFUNC_NOEVAL_MEMBER(useq_autoload_flash,
                          //
                          autoload_flash();
                          , 0)

BUILTINFUNC_NOEVAL_MEMBER(useq_rewind_logical_time,
                          //
                          reset_logical_time();
                          , 0)

// NOTE: only these are meant for user interface
BUILTINFUNC_MEMBER(
    useq_set_my_id,                  //
    if (args[0].is_number())         //
    { set_my_id(args[0].as_int()); } //
    else {
        report_error_wrong_specific_pred("useq-set-id", 1, "a number",
                                         args[0].display());
    } //
    ,
    1)

BUILTINFUNC_NOEVAL_MEMBER(useq_memory_save, write_flash_env();, 0)
BUILTINFUNC_NOEVAL_MEMBER(useq_memory_restore, //
                          load_flash_info();
                          load_flash_env(); //
                          , 0);
BUILTINFUNC_NOEVAL_MEMBER(useq_memory_erase, //
                          reset_flash_env_var_info();
                          , 0)

BUILTINFUNC_NOEVAL_MEMBER(useq_stop_all, //
                          clear_all_outputs();
                          println("All outputs cleared.");, 0)

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

void uSEQ::set_my_id(int num)
{
    m_my_id = num;
    write_flash_info();
}