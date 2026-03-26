#include "log.h"
#include "json_builder.h"
#include "logger_bridge.h"

#include <cstdio>
#include <iostream>

namespace
{
struct JsonProtocolState
{
    bool json_mode_enabled = false;
    bool request_active    = false;
    String request_id      = "";
    String text_buffer     = "";
};

JsonProtocolState& state()
{
    static JsonProtocolState s;
    return s;
}

#ifdef ARDUINO
void write_serial_json(const String& payload)
{
    if (!Serial.availableForWrite())
    {
        return;
    }

    Serial.write(SerialMsg::message_begin_marker);
    Serial.write((u_int8_t)SerialMsg::serial_message_types::JSON);
    Serial.println(payload);
}
#else
void write_serial_json(const String&)
{
    // Desktop build has no serial transport; JSON responses are ignored.
}
#endif
} // namespace

namespace Protocol
{
void enable_json_mode() { state().json_mode_enabled = true; }

void disable_json_mode()
{
    state().json_mode_enabled = false;
    state().request_active    = false;
    state().text_buffer       = "";
    state().request_id        = "";
}

bool json_mode_enabled() { return state().json_mode_enabled; }

void begin_request(const String& request_id)
{
    state().request_active = true;
    state().request_id     = request_id;
    state().text_buffer    = "";
}

void finish_request()
{
    state().request_active = false;
    state().request_id     = "";
    state().text_buffer    = "";
}

bool request_active() { return state().request_active; }

void append_request_text(const String& line)
{
    auto& json_state = state();
    if (!json_state.request_active)
    {
        return;
    }

    if (json_state.text_buffer.length() > 0)
    {
        json_state.text_buffer += "\n";
    }
    json_state.text_buffer += line;
}

String consume_request_text()
{
    String copy = state().text_buffer;
    state().text_buffer = "";
    return copy;
}

void send_json_response(bool success, const String& text,
                        const std::optional<String>& meta,
                        const String& request_id,
                        const std::optional<String>& diagnostics_json)
{
    JsonBuilder b;
    b.object_begin()
        .field("type", "response")
        .field("success", success)
        .field("console", text)
        .field("text", text);

    if (meta && meta->length() > 0)
    {
        b.field_raw("meta", *meta);
    }
    else
    {
        b.field_null("meta");
    }

    if (diagnostics_json && diagnostics_json->length() > 0)
    {
        b.field_raw("diagnostics", *diagnostics_json);
    }

    b.field("requestId", request_id)
        .object_end();

    write_serial_json(b.build());
}

void send_json_error(const String& request_id, const String& message)
{
    send_json_response(false, message, std::nullopt, request_id);
}

void send_raw_json(const String& payload) { write_serial_json(payload); }
} // namespace Protocol

void message_editor(const String& s)
{
#ifdef ARDUINO
    if (Protocol::request_active())
    {
        Protocol::append_request_text(s);
        return;
    }

    if (Protocol::json_mode_enabled())
    {
        Protocol::send_json_response(true, s, std::nullopt, String());
        return;
    }

    if (Serial.availableForWrite())
    {
        Serial.write(SerialMsg::message_begin_marker);
        Serial.write((u_int8_t)SerialMsg::serial_message_types::MSG_TO_EDITOR);
        Serial.println(s);
        // Serial.write(SerialMsg::message_end_marker);
    }
#else
    std::cout << "[EDITOR] " << s.c_str() << std::endl;
#endif
}

void println(const String& s)
{
    if (auto L = get_global_logger())
    {
        L->info(s);
    }

#ifdef ARDUINO
    if (Protocol::request_active())
    {
        Protocol::append_request_text(s);
        return;
    }

    if (Protocol::json_mode_enabled())
    {
        Protocol::send_json_response(true, s, std::nullopt, String());
        return;
    }

    if (Serial.availableForWrite())
    {
        Serial.write(SerialMsg::message_begin_marker);
        Serial.write((u_int8_t)SerialMsg::serial_message_types::TEXT);
        Serial.println(s);
        // Serial.write(SerialMsg::message_end_marker);
    }
#else
    std::cout << s.c_str() << std::endl;
#endif
}

// ERRORS
std::vector<String> error_msg_q = {};
void report_error(const String& s) { error_msg_q.push_back(s); }

void report_generic_error(const String& s)
{
    report_error(String("**Error**: ") + s);
}

void report_runtime_error(const String& s)
{
    report_error((String) "**Runtime Error**: " + s);
}

void report_user_warning(const String& s) { report_error("**Warning**: " + s); }

void report_evaluation_error(const String& error_msg, String atom)
{
    String msg = "**Evaluation Error**: While evaluating atom " + atom +
                 ", the following error ocurred:\n    " + error_msg;
    report_error(msg);
}

String comp_to_string(NumArgsComparison comp)
{
    switch (comp)
    {
    case NumArgsComparison::EqualTo:
    {
        return "exactly";
    }
    case NumArgsComparison::AtLeast:
    {
        return "at least";
    }
    case NumArgsComparison::AtMost:
    {
        return "at most";
    }
    case NumArgsComparison::Between:
    {
        return "between";
    }
    default:
        return "<comparison not found>";
    }

    return "";
}

void report_error_wrong_num_args(const String& function_name, int num_received,
                                 NumArgsComparison expected_comp, int num,
                                 int num2 = 0)
{
    String expected_comparison_str = comp_to_string(expected_comp);
    String received_num_str        = String(num_received);

    expected_comparison_str += " " + String(num);

    if (expected_comp == NumArgsComparison::Between)
    {
        expected_comparison_str += " and " + String(num2);
    }

    report_error("(`" + function_name + "`) Wrong number of arguments: expected " +
                 expected_comparison_str + " but received " + received_num_str +
                 " instead.");
}

void report_error_arg_is_error(const String& function_name, int num,
                               const String& received_val_str)
{
    String msg = "(`" + function_name + "`) Argument #" + String(num) +
                 " evaluates to an error:";
    msg += "\n    " + received_val_str;
    report_error(msg);
}

void report_error_wrong_all_pred(const String& function_name, int num,
                                 const String& expected_str,
                                 const String& received_val_str)
{
    String msg = "(`" + function_name + "`) All arguments should evaluate to " +
                 expected_str + ", but argument #" + String(num) + " does not:";
    msg += "\n    " + received_val_str;
    report_error(msg);
}

void report_error_wrong_specific_pred(const String& function_name, int num,
                                      const String& expected_str,
                                      const String& received_val_str)
{
    String msg = "(`" + function_name + "`) Argument #" + String(num) +
                 " should evaluate to " + expected_str + ", but instead it is:";
    msg += "\n    " + received_val_str;
    report_error(msg);
}

void report_error_atom_not_defined(const String& atom)
{
    report_error("Atom **" + atom + "** not defined.");
}

void report_custom_function_error(const String& function_name, const String& msg)
{
    report_error("(`" + function_name + "`) " + msg);
}

int free_heap() { 
    #ifdef ARDUINO
    return rp2040.getFreeHeap() / 1024; 
    #else
    return 1000000; // Dummy value for non-Arduino environments
    #endif
}

// DebugLogger

#if USEQ_DEBUG

std::unordered_set<String> DebugLogger::mutes = {};
std::unordered_set<String> DebugLogger::solos = { "uSEQ::eval_at_time" };

bool DebugLogger::print_free_heap = false;

int DebugLogger::m_level = -1;

String DebugLogger::m_spaces = "";

void DebugLogger::inc_level()
{
    m_level += 1;
    update_spaces();
}

void DebugLogger::dec_level()
{
    m_level -= 1;
    update_spaces();
}

DebugLogger::DebugLogger(String s) : m_name(s)
{
    start_heap = free_heap();
    inc_level();

    debug_print("[DBG] " + m_name + " START...");
    inc_level();
    if (print_free_heap)
    {
        debug_print("free heap (start): " + String(start_heap) + "KB");
    }
}

DebugLogger::~DebugLogger()
{
    int end_heap = free_heap();

    dec_level();

    debug_print("[DBG] " + m_name + " END.");
    if (print_free_heap)
    {
        debug_print(m_spaces + "free heap (end): " + String(end_heap) + "KB");
        debug_print(m_spaces + "heap difference: " + String(end_heap - start_heap) +
                    "KB");
    }

    dec_level();
}

void DebugLogger::filtered_log(const String& message)
{
    bool should_print = true;

    // If solos have been specified and this isn't in them, don't print
    if (solos.size() > 0 && !solos.count(m_name))
    {
        should_print = false;
    }
    // Otherwise, print as long as it's not in the mutes
    else if (mutes.count(m_name))
    {
        should_print = false;
    }

    if (should_print)
    {
        debug_print("[" + m_name + "] " + addIndentAfterNewlines(message));
    }
}

void DebugLogger::operator()(const String& message) { filtered_log(message); }

void DebugLogger::debug_print(const String& s)
{
    print(m_spaces + s);
    print("\n");
}

String DebugLogger::addIndentAfterNewlines(const String& input)
{
    String result;
    int lastIndex    = 0;
    int currentIndex = 0;

    // Search for newline characters and modify the string
    while ((currentIndex = input.indexOf('\n', lastIndex)) != -1)
    {
        // Add the current segment plus newline
        result += input.substring(lastIndex, currentIndex + 1);
        // Add spaces after the newline
        result += m_spaces;
        // Move past the newline character
        lastIndex = currentIndex + 1;
    }

    // Add the last part of the string if any
    result += input.substring(lastIndex);

    return result;
}

void DebugLogger::update_spaces()
{
    String s = "";

    if (m_level > 0)
    {
        for (int i = 0; i < m_level; i++)
        {
            s += "  ";
        }

        m_spaces = s;
    }
    else
    {
        m_spaces = "";
    }
}

#endif // USEQ_DEBUG
