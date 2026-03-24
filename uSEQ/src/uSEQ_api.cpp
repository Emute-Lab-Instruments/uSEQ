#include "uSEQ.h"
#include "uSEQ/io_manager.h"
#include "uSEQ/output_manager.h"
#include "utils.h"
#include "utils/log.h"
#ifdef ARDUINO
#include "dsp/uSeqGens/uSeqGen_Sampler.h"
#endif

template <Value (uSEQ::*Method)(std::vector<Value>&, Environment&)>
static Value method_trampoline(void* ctx, std::vector<Value>& args, Environment& env)
{
    return (static_cast<uSEQ*>(ctx)->*Method)(args, env);
}

#define INSERT_BUILTINDEF(__name__, __func_name__)                                  \
    m_interpreter.register_plugin_builtin(                                          \
        __name__, &method_trampoline<&uSEQ::__func_name__>, this);

const std::array<uSEQ::OutputBuiltinEntry, uSEQ::kOutputBuiltinCount>
    uSEQ::s_output_builtin_entries = {{
        {"a1", 1, OutputManager::OutputType::CONTINUOUS, false},
        {"a2", 2, OutputManager::OutputType::CONTINUOUS, false},
        {"a3", 3, OutputManager::OutputType::CONTINUOUS, false},
        {"a4", 4, OutputManager::OutputType::CONTINUOUS, false},
        {"a5", 5, OutputManager::OutputType::CONTINUOUS, false},
        {"a6", 6, OutputManager::OutputType::CONTINUOUS, false},
        {"a7", 7, OutputManager::OutputType::CONTINUOUS, false},
        {"a8", 8, OutputManager::OutputType::CONTINUOUS, false},
        {"d1", 1, OutputManager::OutputType::BINARY, false},
        {"d2", 2, OutputManager::OutputType::BINARY, false},
        {"d3", 3, OutputManager::OutputType::BINARY, false},
        {"d4", 4, OutputManager::OutputType::BINARY, false},
        {"d5", 5, OutputManager::OutputType::BINARY, false},
        {"d6", 6, OutputManager::OutputType::BINARY, false},
        {"d7", 7, OutputManager::OutputType::BINARY, false},
        {"d8", 8, OutputManager::OutputType::BINARY, false},
        {"s1", 1, OutputManager::OutputType::SERIAL_OUT, false},
        {"s2", 2, OutputManager::OutputType::SERIAL_OUT, false},
        {"s3", 3, OutputManager::OutputType::SERIAL_OUT, false},
        {"s4", 4, OutputManager::OutputType::SERIAL_OUT, false},
        {"s5", 5, OutputManager::OutputType::SERIAL_OUT, false},
        {"s6", 6, OutputManager::OutputType::SERIAL_OUT, false},
        {"s7", 7, OutputManager::OutputType::SERIAL_OUT, false},
        {"s8", 8, OutputManager::OutputType::SERIAL_OUT, false},
        {"get-a1", 1, OutputManager::OutputType::CONTINUOUS, true},
        {"get-a2", 2, OutputManager::OutputType::CONTINUOUS, true},
        {"get-a3", 3, OutputManager::OutputType::CONTINUOUS, true},
        {"get-a4", 4, OutputManager::OutputType::CONTINUOUS, true},
        {"get-a5", 5, OutputManager::OutputType::CONTINUOUS, true},
        {"get-a6", 6, OutputManager::OutputType::CONTINUOUS, true},
        {"get-a7", 7, OutputManager::OutputType::CONTINUOUS, true},
        {"get-a8", 8, OutputManager::OutputType::CONTINUOUS, true},
        {"get-d1", 1, OutputManager::OutputType::BINARY, true},
        {"get-d2", 2, OutputManager::OutputType::BINARY, true},
        {"get-d3", 3, OutputManager::OutputType::BINARY, true},
        {"get-d4", 4, OutputManager::OutputType::BINARY, true},
        {"get-d5", 5, OutputManager::OutputType::BINARY, true},
        {"get-d6", 6, OutputManager::OutputType::BINARY, true},
        {"get-d7", 7, OutputManager::OutputType::BINARY, true},
        {"get-d8", 8, OutputManager::OutputType::BINARY, true},
    }};

const std::array<uSEQ::InputBuiltinEntry, uSEQ::kInputBuiltinCount>
    uSEQ::s_input_builtin_entries = {{
        {"in1", USEQI1},
        {"in2", USEQI2},
        {"gin1", USEQI1},
        {"gin2", USEQI2},
        {"ain1", USEQAI1},
        {"ain2", USEQAI2},
    }};

Value uSEQ::dispatch_output_builtin(void* ctx, std::vector<Value>& args,
                                    Environment& env)
{
    const auto& output_ctx = *static_cast<OutputContext*>(ctx);
    auto& output_manager   = output_ctx.useq->get_output_manager();
    if (output_ctx.is_getter)
    {
        return output_manager.handle_output_getter(output_ctx.index,
                                                   output_ctx.type,
                                                   args, env);
    }

    return output_manager.handle_output_setter(output_ctx.index,
                                               output_ctx.type,
                                               args, env);
}

Value uSEQ::dispatch_input_builtin(void* ctx, std::vector<Value>& args,
                                   Environment& env)
{
    (void)args;
    (void)env;

    const auto& input_ctx = *static_cast<InputContext*>(ctx);
    if (!input_ctx.useq->get_io_manager())
    {
        return Value::nil();
    }

    return Value(input_ctx.useq->get_io_manager()->get_input_value(
        input_ctx.input_id));
}

void uSEQ::init_builtinfuncs()
{
    DBG("uSEQ::init_builtinfuncs");

    // Call parent class init_builtinfuncs first
    m_interpreter.init_builtinfuncs();

    // Add uSEQ-specific hardware functions

    INSERT_BUILTINDEF("print-led-info", useq_print_led_info);

    for (size_t i = 0; i < s_output_builtin_entries.size(); ++i)
    {
        const auto& entry = s_output_builtin_entries[i];
        m_output_contexts[i] = {this, entry.index, entry.type, entry.is_getter};
        m_interpreter.register_plugin_builtin(entry.name,
                                              &uSEQ::dispatch_output_builtin,
                                              &m_output_contexts[i]);
    }

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

    for (size_t i = 0; i < s_input_builtin_entries.size(); ++i)
    {
        const auto& entry = s_input_builtin_entries[i];
        m_input_contexts[i] = {this, entry.input_id};
        m_interpreter.register_plugin_builtin(entry.name,
                                              &uSEQ::dispatch_input_builtin,
                                              &m_input_contexts[i]);
    }

    INSERT_BUILTINDEF("useq-report-firmware-info", useq_report_firmware_info);
    INSERT_BUILTINDEF("useq-firmware-info", useq_firmware_info);
    INSERT_BUILTINDEF("useq-talk-in-json", useq_talk_in_json);

    // Playback/transport control
    INSERT_BUILTINDEF("useq-play", useq_play);
    INSERT_BUILTINDEF("useq-pause", useq_pause);
    INSERT_BUILTINDEF("useq-stop", useq_stop);
    INSERT_BUILTINDEF("useq-rewind", useq_rewind);
    INSERT_BUILTINDEF("useq-clear", useq_clear);
    INSERT_BUILTINDEF("useq-get-transport-state", useq_get_transport_state);

    // Sample management functions
    INSERT_BUILTINDEF("list-samples", useq_list_samples);

#ifdef ARDUINO
    INSERT_BUILTINDEF("useqaw", ard_useqaw);
    INSERT_BUILTINDEF("useqdw", ard_useqdw);
    INSERT_BUILTINDEF("aw", ard_aw);
    INSERT_BUILTINDEF("dw", ard_dw);
    INSERT_BUILTINDEF("useq-enter-bootloader-mode", useq_enter_bootloader_mode);
    INSERT_BUILTINDEF("useq-reboot", useq_reboot);
#else
    // Desktop build versions
    INSERT_BUILTINDEF("useqaw", ard_useqaw);
    INSERT_BUILTINDEF("useqdw", ard_useqdw);
    INSERT_BUILTINDEF("aw", ard_aw);
    INSERT_BUILTINDEF("dw", ard_dw);
#endif

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

BUILTINFUNC_NOEVAL_MEMBER(
    useq_firmware_info,
    {
        String msg = "uSEQ Firmware Version: " + String(USEQ_FIRMWARE_VERSION);
#ifdef MUSICTHING
        msg += " (Music Thing Workshop Computer Edition)";
#endif
        println(msg);
        ret = Value();
    },
    0)

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

BUILTINFUNC_NOEVAL_MEMBER(useq_talk_in_json, Protocol::enable_json_mode();
                          ret = Value::string("json-mode-enabled");, 0)

BUILTINFUNC_NOEVAL_MEMBER(useq_q0, get_environment()->set("q-expr", args[0]);
                          get_scheduler()->set_q0_ast(args[0]);
                          ret = Value::atom("q0");, 1)

// Playback/transport control implementations (0-arg)
BUILTINFUNC_MEMBER(useq_play,
    m_is_playing = true;
    m_pending_transport_meta = "{\"transport\":\"playing\"}";
    return Value::nil();
, 0)

BUILTINFUNC_MEMBER(useq_pause,
    m_is_playing = false;
    m_pending_transport_meta = "{\"transport\":\"paused\"}";
    return Value::nil();
, 0)

BUILTINFUNC_MEMBER(useq_stop,
    m_is_playing = false;
    reset_logical_time();
    m_pending_transport_meta = "{\"transport\":\"stopped\"}";
    return Value::nil();
, 0)

BUILTINFUNC_MEMBER(useq_rewind,
    reset_logical_time();
    m_pending_transport_meta = "{\"transport\":\"stopped\"}";
    return Value::nil();
, 0)

// Transport state query
BUILTINFUNC_MEMBER(useq_get_transport_state,
    if (m_is_playing) return Value::string("playing");
    if (m_interpreter.get_time_manager()->get_transport_time() == 0.0)
        return Value::string("stopped");
    return Value::string("paused");
, 0)

// Set output expressions to defaults (0.5 for continuous, 0 for binary)
BUILTINFUNC_MEMBER(
    useq_clear,
    for (int i = 0; i < m_num_continuous_outs; i++) {
        String name = "a" + String(i + 1);
        Value v     = Value(0.5);
        get_environment()->set_expr(name, v);
        m_continuous_ASTs[i] = v;
    } for (int i = 0; i < m_num_binary_outs; i++) {
        String name = "d" + String(i + 1);
        Value v     = Value(0);
        get_environment()->set_expr(name, v);
        m_binary_ASTs[i] = v;
    } return Value::nil();
    , 0)

// TODO: there is potentially a lot of duplicated/wasted memory by storing
// the exprs in both the environment and the class member vectors
// especially once the exprs get more and more complex

// Function that should be available in all builds (not just Arduino)
void uSEQ::clear_all_outputs()
{
    for (int i = 0; static_cast<size_t>(i) < m_continuous_ASTs.size(); i++)
    {
        String name          = "a" + String(i + 1);
        m_continuous_ASTs[i] = default_continuous_expr;
        get_environment()->get_def_exprs().erase(name);
    }

    for (int i = 0; static_cast<size_t>(i) < m_binary_ASTs.size(); i++)
    {
        String name      = "d" + String(i + 1);
        m_binary_ASTs[i] = default_binary_expr;
        get_environment()->get_def_exprs().erase(name);
    }

    if (!m_serial_ASTs.empty())
    {
        m_serial_ASTs[0] = default_serial_expr;
        if (!m_serial_vals.empty())
        {
            m_serial_vals[0] = std::nullopt;
        }
    }

    for (int i = 1; static_cast<size_t>(i) < m_serial_ASTs.size(); i++)
    {
        String name      = "s" + String(i);
        m_serial_ASTs[i] = default_serial_expr;
        if (static_cast<size_t>(i) < m_serial_vals.size())
        {
            m_serial_vals[i] = std::nullopt;
        }
        get_environment()->get_def_exprs().erase(name);
    }
}

BUILTINFUNC_NOEVAL_MEMBER(useq_stop_all, clear_all_outputs();
                          println("All outputs cleared.");, 0)

BUILTINFUNC_MEMBER(
    useq_reset_external_clock_tracking,
    constexpr const char* user_facing_name = "reset-clock-ext";
    if (!(args.size() == 0)) {
        report_error_wrong_num_args(user_facing_name, static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 0, 0);
        return Value::error();
    } reset_ext_tracking();
    return Value::nil();, 0)

BUILTINFUNC_MEMBER(
    useq_reset_internal_clock,
    constexpr const char* user_facing_name = "reset-clock-int";
    if (!(args.size() == 0)) {
        report_error_wrong_num_args(user_facing_name, static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 0, 0);
        return Value::error();
    } reset_logical_time();
    return Value::nil();, 0)

BUILTINFUNC_MEMBER(
    useq_get_clock_source,
    constexpr const char* user_facing_name = "get-clock-source";
    if (!(args.size() == 0)) {
        report_error_wrong_num_args(user_facing_name, static_cast<int>(args.size()),
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
        report_error_wrong_num_args(user_facing_name, static_cast<int>(args.size()),
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

BUILTINFUNC_MEMBER(useq_swr,
                   ret = m_io_manager ? Value(m_io_manager->get_input_value(USEQRS1))
                                      : Value::nil();
                   , 0)

BUILTINFUNC_MEMBER(useq_rot, ret = m_io_manager
                                       ? Value(m_io_manager->get_input_value(USEQR1))
                                       : Value::nil();
                   , 0)

BUILTINFUNC_MEMBER(
    useq_print_led_info,
    println("num continuous outs: " + String(m_num_continuous_outs));
    println("num binary outs: " + String(m_num_binary_outs));

    for (int i = 0; i < m_num_continuous_outs; i++) {
        String name = "a" + String(i + 1);
        int led_pin = get_io_manager()->get_analog_out_led_pin(i + 1);
        println(name + ": " + led_pin);
    }

    for (int i = 0; i < m_num_binary_outs; i++) {
        String name = "d" + String(i + 1);
        int led_pin = get_io_manager()->get_digital_out_led_pin(i + 1);
        println(name + ": " + led_pin);
    },
    0)

// Sample management functions
BUILTINFUNC_NOEVAL_MEMBER(
    useq_list_samples,
#ifdef ARDUINO
    // Read pointers from memory based on flash address
    const uint8_t* binary_data           = (const uint8_t*)AUDIO_FLASH_ADDRESS;
    const audio_header_t* header         = (const audio_header_t*)binary_data;
    const audio_file_entry_t* file_table = (const audio_file_entry_t*)(binary_data +
                                                                       16);

    // Verify binary is valid
    if (header->magic != AUDIO_MAGIC) {
        println("Error: Invalid audio binary at 0x" +
                String(AUDIO_FLASH_ADDRESS, 16));
        ret = Value::error();
    } else if (header->file_count == 0) {
        println("No samples found.");
        ret = Value::nil();
    } else {
        // Print each sample name on a new line
        for (uint32_t i = 0; i < header->file_count; i++)
        {
            println(String(file_table[i].name));
        }
        ret = Value::nil();
    }
#else
    println("Sample listing not available on desktop build.");
    ret = Value::nil();
#endif
    ,
    0)
