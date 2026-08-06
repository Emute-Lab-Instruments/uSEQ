#include "serial_protocol.h"
#include "serial_protocol_json.h"
#include "../devtools/devtools.h"
#include "../utils/json_builder.h"

#include <cstdio>
#include <cstring>

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace {

void write_debug_json(const char* payload, size_t len)
{
    dt::count("msg_out");
#ifdef ARDUINO
    Serial.write(reinterpret_cast<const uint8_t*>(payload), len);
    Serial.write('\n');
#else
    fwrite(payload, 1, len, stdout);
    putchar('\n');
    fflush(stdout);
#endif
}

} // namespace

namespace firmware {

using serial_protocol_detail::extract_string;

bool SerialProtocol::dispatch_message(const char* payload, size_t len,
                                      char* buf, size_t buf_size)
{
    dt::count("msg_in");

    // Parse the JSON message to determine type
    char type_buf[32] = {};
    extract_string(payload, len, "type", type_buf, sizeof(type_buf));

    // Extract request ID for response correlation
    extract_string(payload, len, "requestId",
                   m_request_id, sizeof(m_request_id));

    // Handle system messages internally
    if (strcmp(type_buf, "hello") == 0)
    {
        handle_hello(payload, len);
        return false; // not a user command
    }
    if (strcmp(type_buf, "ping") == 0)
    {
        handle_ping(payload, len);
        return false;
    }
    if (strcmp(type_buf, "stream-config") == 0)
    {
        handle_stream_config(payload, len);
        return false;
    }
    if (strcmp(type_buf, "set-live-inputs") == 0)
    {
        handle_set_live_inputs(payload, len);
        return false;
    }
    if (strcmp(type_buf, "get-state") == 0)
    {
        handle_get_state(payload, len);
        return false;
    }
#ifdef ENABLE_I2C_NETWORKING
    if (strcmp(type_buf, "rescan-modules") == 0)
    {
        handle_rescan_modules(payload, len);
        return false;
    }
#endif
    if (strcmp(type_buf, "set-failure-mode") == 0)
    {
        handle_set_failure_mode(payload, len);
        return false;
    }
    if (strcmp(type_buf, "debug") == 0)
    {
        // Release builds compile this call to an unhandled no-op. Observation
        // builds route the request to the fixed-capacity telemetry subsystem.
        // In neither case may a debug envelope fall through to eval parsing.
        dt::handle_debug_message(payload, len, write_debug_json);
        return false;
    }

    // Eval message — extract code field
    char code_buf[2048] = {};
    bool code_truncated = false;
    size_t code_len = extract_string(payload, len, "code",
                                     code_buf, sizeof(code_buf),
                                     &code_truncated);

    if (code_truncated)
    {
        // Refuse to evaluate a silently-truncated program — that would run
        // arbitrary half-of-a-form code. Emit a clear diagnostic instead.
        JsonBuilder b;
        b.object_begin()
            .field("type", "response")
            .field("success", false)
            .field("console", "")
            .field("text", "Program too large (exceeds receive buffer)")
            .field_null("meta")
            .field("requestId", m_request_id)
            .object_end();
        write_json_str(b.build().c_str());
        return false;
    }

    if (code_len == 0)
    {
        // Empty code — no-op success
        JsonBuilder b;
        b.object_begin()
            .field("type", "response")
            .field("success", true)
            .field("console", "")
            .field("text", "")
            .field_null("meta")
            .field("requestId", m_request_id)
            .object_end();
        write_json_str(b.build().c_str());
        return false;
    }

    // Copy code into caller's buffer
    size_t copy_len = code_len < buf_size - 1 ? code_len : buf_size - 1;
    memcpy(buf, code_buf, copy_len);
    buf[copy_len] = '\0';

    return true;
}

bool SerialProtocol::read_command(char* buf, size_t buf_size)
{
    if (!m_message_ready) return false;

    m_message_ready = false;

    return dispatch_message(m_msg_buf, m_msg_len, buf, buf_size);
}

// ══════════════════════════════════════════════════════════════════════════
// Send — must-deliver
// ══════════════════════════════════════════════════════════════════════════

} // namespace firmware
