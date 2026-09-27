#include "serial_protocol.h"
#include "build_info.h"
#include "../devtools/devtools.h"
#include "../utils/json_builder.h"

#include <cstdio>
#include <cstring>

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace firmware {

void SerialProtocol::send_eval_response(const sig::EvalResult& result)
{
    JsonBuilder b;
    b.object_begin()
        .field("type", "response")
        .field("success", result.kind != sig::EvalResult::Error);

    // Build console/text output
    if (result.text && result.text_length > 0)
    {
        // Text result — make a null-terminated copy
        char text_copy[512];
        size_t copy_len = result.text_length < sizeof(text_copy) - 1
                              ? result.text_length
                              : sizeof(text_copy) - 1;
        memcpy(text_copy, result.text, copy_len);
        text_copy[copy_len] = '\0';

        b.field("console", text_copy)
         .field("text", text_copy);
    }
    else if (result.kind == sig::EvalResult::Number)
    {
        char num_str[32];
        snprintf(num_str, sizeof(num_str), "%g", result.number);
        b.field("console", num_str)
         .field("text", num_str);
    }
    else
    {
        b.field("console", "")
         .field("text", "");
    }

    b.field_null("meta");

    // Diagnostics array
    if (result.diagnostic_count > 0)
    {
        JsonBuilder diag_arr;
        diag_arr.array_begin_unkeyed();
        for (uint8_t i = 0; i < result.diagnostic_count; ++i)
        {
            const auto& d = result.diagnostics[i];
            diag_arr.object_begin()
                .field("severity", sig::severity_to_cstr(d.severity))
                .field("category", sig::category_to_cstr(d.category))
                .field("start", static_cast<int>(d.span_start))
                .field("end", static_cast<int>(d.span_start + d.span_len));
            if (d.message)
                diag_arr.field("message", d.message);
            if (d.suggestion)
                diag_arr.field("suggestion", d.suggestion);
            diag_arr.object_end();
        }
        diag_arr.array_end();
        b.field_raw("diagnostics", diag_arr.build());
    }

    b.field("requestId", m_request_id)
        .object_end();

    write_json_str(b.build().c_str());
}

// ══════════════════════════════════════════════════════════════════════════
// Send — opportunistic (may be dropped if TX full)
// ══════════════════════════════════════════════════════════════════════════

void SerialProtocol::send_diagnostics(const sig::Diagnostic* diags, uint8_t count)
{
    if (count == 0 || !can_write()) return;

    JsonBuilder b;
    b.object_begin()
        .field("type", "diagnostics");

    JsonBuilder arr;
    arr.array_begin_unkeyed();
    for (uint8_t i = 0; i < count; ++i)
    {
        const auto& d = diags[i];
        arr.object_begin()
            .field("severity", sig::severity_to_cstr(d.severity))
            .field("category", sig::category_to_cstr(d.category))
            .field("start", static_cast<int>(d.span_start))
            .field("end", static_cast<int>(d.span_start + d.span_len));
        if (d.message)
            arr.field("message", d.message);
        if (d.suggestion)
            arr.field("suggestion", d.suggestion);
        arr.object_end();
    }
    arr.array_end();
    b.field_raw("diagnostics", arr.build())
        .object_end();

    write_json_str(b.build().c_str());
}

void SerialProtocol::send_stream_data(const double* output_values, size_t output_count,
                                      const double* input_values, size_t input_count)
{
#ifdef ARDUINO
    if (!m_json_mode) return;
    if (!Serial) return;

    unsigned long now = micros();
    if (stream_rate_limit_us > 0) {
        if (now - m_last_stream_us < stream_rate_limit_us) return;
    }
    m_last_stream_us = now;

    static constexpr size_t FRAME_SIZE = 11;

    for (uint8_t ch = 0; ch < num_stream_channels; ++ch) {
        StreamChannel& sc = stream_channels[ch];
        if (!sc.enabled) continue;

        double val;
        switch (sc.source) {
            case StreamSource::Time:
                val = m_current_time;
                break;
            case StreamSource::Output:
                val = (sc.source_idx < output_count) ? output_values[sc.source_idx] : 0.0;
                break;
            case StreamSource::Input:
                val = (input_values && sc.source_idx < input_count) ? input_values[sc.source_idx] : 0.0;
                break;
            default:
                val = 0.0;
                break;
        }

        if (sc.on_change_only && val == sc.last_sent) continue;

        if (Serial.availableForWrite() < static_cast<int>(FRAME_SIZE)) {
            dt::count("stream_drop");
            return;
        }

        Serial.write(SerialMsg::message_begin_marker);
        Serial.write(static_cast<uint8_t>(SerialMsg::serial_message_types::STREAM));
        Serial.write(static_cast<uint8_t>(ch + 1)); // 1-indexed on wire
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&val);
        for (size_t b = 0; b < 8; ++b)
            Serial.write(bytes[b]);

        sc.last_sent = val;
    }
#else
    (void)output_values;
    (void)output_count;
    (void)input_values;
    (void)input_count;
#endif
}

// ══════════════════════════════════════════════════════════════════════════
// Control messages
// ══════════════════════════════════════════════════════════════════════════

void SerialProtocol::handle_handshake()
{
    // Public API for explicit handshake re-negotiation.
    // Re-sends the hello response with current hardware config.
    handle_hello(nullptr, 0);
}

void SerialProtocol::send_ready()
{
    JsonBuilder b;
    b.object_begin()
        .field("type", "ready")
        .field("version", build_info::VERSION)
        .field("protocol", build_info::PROTOCOL_VERSION)
        .field("target", build_info::HARDWARE_TARGET)
        .field_raw("capabilities", build_info::CAPABILITIES_JSON)
        .object_end();

    write_json_str(b.build().c_str());
}

// ── Log envelope (§5.6) ───────────────────────────────────────────────────
// Replaces legacy TEXT (0x20) and MSG_TO_EDITOR (0x64) framed-byte paths.
// level: one of "debug", "info", "notice", "warn", "error"

void SerialProtocol::send_log(const char* level, const char* text)
{
    JsonBuilder b;
    b.object_begin()
        .field("type", "log")
        .field("level", level)
        .field("text", text)
        .object_end();

    write_json_str(b.build().c_str());
}

void SerialProtocol::send_status(const char* status)
{
    JsonBuilder b;
    b.object_begin().field("type", "status").field("status", status).object_end();
    write_json_str(b.build().c_str());
#ifdef ARDUINO
    Serial.flush();
#endif
}

void SerialProtocol::drain_pending_input()
{
    drain_serial_into_rx_buf();
    m_rx_head = 0;
    m_rx_len = 0;
    m_message_ready = false;
    m_msg_len = 0;
}

void SerialProtocol::send_hw_input(const char* kind, const char* id,
                                   const char* state, uint32_t timestamp_ms)
{
    JsonBuilder b;
    b.object_begin()
        .field("type", "hw-input")
        .field("kind", kind)
        .field("id", id)
        .field("state", state)
        .field("ts", static_cast<int>(timestamp_ms))
        .object_end();
    write_json_str(b.build().c_str());
}

void SerialProtocol::send_hw_toggle(const char* id, bool state,
                                    uint32_t timestamp_ms)
{
    JsonBuilder b;
    b.object_begin()
        .field("type", "hw-input")
        .field("kind", "toggle")
        .field("id", id)
        .field("state", state)
        .field("ts", static_cast<int>(timestamp_ms))
        .object_end();
    write_json_str(b.build().c_str());
}

// ══════════════════════════════════════════════════════════════════════════
// Internal: wire-level I/O
// ══════════════════════════════════════════════════════════════════════════

bool SerialProtocol::can_write()
{
#ifdef ARDUINO
    return Serial.availableForWrite() > 0;
#else
    return true;
#endif
}

void SerialProtocol::write_json(const char* payload, size_t len)
{
    dt::count("msg_out");
#ifdef ARDUINO
    // Spec §3.3: JSON messages are bare `{...}\n` — no framed-byte prefix.
    Serial.write(reinterpret_cast<const uint8_t*>(payload), len);
    Serial.write('\n');
#else
    fwrite(payload, 1, len, stdout);
    putchar('\n');
    fflush(stdout);
#endif
}

void SerialProtocol::write_json_str(const char* payload)
{
    write_json(payload, strlen(payload));
}

} // namespace firmware
