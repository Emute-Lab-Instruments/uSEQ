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
using serial_protocol_detail::validate_protocol_request;

bool SerialProtocol::dispatch_message(const char* payload, size_t len,
                                      char* buf, size_t buf_size)
{
    dt::count("msg_in");

    auto reject_request = [this](const char* message) {
        JsonBuilder response;
        response.object_begin()
            .field("type", "response")
            .field("success", false)
            .field("text", message)
            .field("requestId", m_request_id)
            .object_end();
        write_json_str(response.build().c_str());
    };

    // Parse and validate the JSON message against the schema-defined route.
    m_request_id[0] = '\0';
    extract_string(payload, len, "requestId",
                   m_request_id, sizeof(m_request_id));

    char validation_error[128] = {};
    const auto* request = validate_protocol_request(
        payload, len, validation_error, sizeof(validation_error));
    if (!request)
    {
        reject_request(validation_error);
        return false;
    }

    using useq::protocol::Route;
    switch (request->route)
    {
        case Route::Hello:
            handle_hello(payload, len);
            return false;
        case Route::Ping:
            handle_ping(payload, len);
            return false;
        case Route::StreamConfig:
            handle_stream_config(payload, len);
            return false;
        case Route::SetLiveInputs:
            handle_set_live_inputs(payload, len);
            return false;
        case Route::GetState:
            handle_get_state(payload, len);
            return false;
        case Route::SetFailureMode:
            handle_set_failure_mode(payload, len);
            return false;
        case Route::RescanModules:
#ifdef ENABLE_I2C_NETWORKING
            handle_rescan_modules(payload, len);
#else
            reject_request("rescan-modules is unavailable in this build");
#endif
            return false;
        case Route::Debug:
            // Release builds compile this call to an unhandled no-op.
            dt::handle_debug_message(payload, len, write_debug_json);
            return false;
        case Route::Unsupported: {
            char error[96] = {};
            std::snprintf(error, sizeof(error),
                          "%s is not supported by this firmware", request->type);
            reject_request(error);
            return false;
        }
        case Route::Eval:
            break;
    }

    // Eval route — extract the schema-validated code field.
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
