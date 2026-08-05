#include "serial_protocol.h"
#include "build_info.h"
#include "i2c_network.h"
#include "../devtools/devtools.h"
#include "../signal_engine/executor.h"
#include "../utils/json_builder.h"
#include "../utils/json_cursor.h"
#include "../utils/serial_message.h"
#include "../modulisp/lisp/symbol_intern.h"

#include <cstdio>
#include <cstring>
#include <cmath>

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <iostream>
#endif

// ── Minimal JSON field extraction ──────────────────────────────────────────
// Operates on raw char*/len to avoid String allocation on the hot path.
// Only extracts what the protocol layer needs; full parsing is not required.

namespace {

// Devtools uses a small transport-neutral callback so its serializers remain
// independent of the firmware protocol object. Keep this bridge identical to
// SerialProtocol::write_json(): debug responses are ordinary JSONL messages.
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

// Extract a string value (without quotes) into dst. Returns length or 0.
// If `truncated` is non-null, it is set true when the value did not fit in dst
// (so callers can emit a diagnostic instead of silently truncating).
size_t extract_string(const char* json, size_t json_len,
                      const char* field_name,
                      char* dst, size_t dst_size,
                      bool* truncated = nullptr)
{
    useq::json::ObjectCursor object(json, json_len);
    useq::json::Value value;
    if (!object.find(field_name, value) ||
        !useq::json::copy_string(value, dst, dst_size, truncated)) {
        return 0;
    }
    return strlen(dst);
}

// Extract an integer value. Returns the value or default_val if not found.
int extract_int(const char* json, size_t json_len,
                const char* field_name, int default_val = 0)
{
    useq::json::ObjectCursor object(json, json_len);
    useq::json::Value value;
    int parsed = default_val;
    return object.find(field_name, value) && useq::json::integer(value, parsed)
        ? parsed
        : default_val;
}

} // anonymous namespace

namespace firmware {

// ══════════════════════════════════════════════════════════════════════════
// Lifecycle
// ══════════════════════════════════════════════════════════════════════════

void SerialProtocol::init(unsigned long baud_rate)
{
#ifdef ARDUINO
    Serial.begin(baud_rate);
    // Wait briefly for USB CDC to enumerate
    delay(100);
#else
    (void)baud_rate;
    // Desktop: nothing to initialise for stdout-based transport
#endif

    m_rx_head       = 0;
    m_rx_len        = 0;
    m_message_ready = false;
    m_msg_len       = 0;
    m_json_mode     = false;
    m_request_id[0] = '\0';

    // Default: only channel 0 (time) enabled at 100Hz
    for (size_t i = 0; i < MAX_STREAM_CHANNELS; ++i)
        stream_channels[i] = StreamChannel{};
    stream_channels[0].enabled = true;
    stream_channels[0].source = StreamSource::Time;
    num_stream_channels = 1;
}

// ══════════════════════════════════════════════════════════════════════════
// Receive
// ══════════════════════════════════════════════════════════════════════════

void SerialProtocol::drain_serial_into_rx_buf()
{
#ifdef ARDUINO
    while (Serial.available() && m_rx_len < RX_BUF_SIZE)
    {
        size_t write_pos = (m_rx_head + m_rx_len) % RX_BUF_SIZE;
        m_rx_buf[write_pos] = static_cast<char>(Serial.read());
        ++m_rx_len;
    }
#endif
    // Desktop: no serial hardware — input comes through stdin or test harness
}

size_t SerialProtocol::rx_inject(const uint8_t* bytes, size_t count)
{
    // Test/host seam mirroring drain_serial_into_rx_buf(): copy up to the
    // ring's free capacity, dropping any overflow (as the UART FIFO would).
    size_t written = 0;
    while (written < count && m_rx_len < RX_BUF_SIZE)
    {
        size_t write_pos = (m_rx_head + m_rx_len) % RX_BUF_SIZE;
        m_rx_buf[write_pos] = static_cast<char>(bytes[written]);
        ++m_rx_len;
        ++written;
    }
    return written;
}

uint8_t SerialProtocol::rx_byte_at(size_t offset) const
{
    return static_cast<uint8_t>(m_rx_buf[(m_rx_head + offset) % RX_BUF_SIZE]);
}

SerialProtocol::BinaryFrameResult SerialProtocol::try_consume_binary_frame()
{
    // Front byte is the binary frame marker (0x1F). Inspect the type byte and
    // attempt to consume a complete, fixed-shape frame. Runs allocation-free on
    // the serial RX hot path.
    //
    // Returns:
    //   Consumed   — a complete frame was parsed and applied; bytes consumed.
    //   Incomplete — a known frame type, but not all bytes have arrived yet;
    //                leave the buffer untouched and wait for more input.
    //   Skip       — unknown/unsupported type byte; caller drops the 0x1F and
    //                re-discriminates (per spec §3.2).

    // Need at least marker + type byte.
    if (m_rx_len < 2) return BinaryFrameResult::Incomplete;

    uint8_t type = rx_byte_at(1);

    if (type == static_cast<uint8_t>(SerialMsg::serial_message_types::INPUT_SET))
    {
        // §6.5: [0x1F][0x01][count:u16-LE][(slot_index:u16-LE, value:f64-LE)×count]
        // Header is 4 bytes; each entry is 10 bytes.
        if (m_rx_len < 4) return BinaryFrameResult::Incomplete;

        uint16_t count = static_cast<uint16_t>(rx_byte_at(2)) |
                         (static_cast<uint16_t>(rx_byte_at(3)) << 8);

        size_t frame_len = 4 + static_cast<size_t>(count) * 10;

        // A frame that can never fit in the RX ring can never be completed by
        // waiting — returning Incomplete here would deadlock all serial input
        // forever (a single corrupted count byte would brick the link). Treat
        // it as a protocol error and Skip so the caller drops the 0x1F marker
        // and re-discriminates. count is also bounded by the live-slot cap;
        // anything larger is garbage regardless of how many bytes arrive.
        if (frame_len > RX_BUF_SIZE ||
            static_cast<size_t>(count) > sig::MAX_LIVE_SLOTS)
            return BinaryFrameResult::Skip;

        if (m_rx_len < frame_len) return BinaryFrameResult::Incomplete;

        // Apply each (slot_index, value) entry directly to the live slots.
        if (engine)
        {
            for (uint16_t e = 0; e < count; ++e)
            {
                size_t base = 4 + static_cast<size_t>(e) * 10;

                uint16_t slot_index = static_cast<uint16_t>(rx_byte_at(base)) |
                                      (static_cast<uint16_t>(rx_byte_at(base + 1)) << 8);

                // Reassemble the f64-LE value byte-by-byte (ring buffer may wrap,
                // so we cannot memcpy a contiguous region).
                uint8_t vb[8];
                for (size_t b = 0; b < 8; ++b)
                    vb[b] = rx_byte_at(base + 2 + b);
                double value;
                memcpy(&value, vb, sizeof(value));

                // Out-of-range slot_index is bounds-checked + dropped inside.
                engine->pool.set_live_slot_value_by_index(slot_index, value);
            }
        }

        // Consume the whole frame regardless of engine presence.
        m_rx_head = (m_rx_head + frame_len) % RX_BUF_SIZE;
        m_rx_len -= frame_len;
        return BinaryFrameResult::Consumed;
    }

    // Unknown/unsupported binary type — caller drops the marker byte.
    return BinaryFrameResult::Skip;
}

bool SerialProtocol::try_extract_message()
{
    // Scan for a complete JSON message (starts with '{', ends with '\n').
    // The editor sends bare JSON objects, one per line. Inbound binary frames
    // (0x1F marker, §3.1/§6.5) are demuxed and applied here too.

    // Skip leading whitespace / garbage, handling inbound binary frames inline.
    while (m_rx_len > 0)
    {
        char front = m_rx_buf[m_rx_head];
        if (front == '{') break;

        if (static_cast<uint8_t>(front) == SerialMsg::message_begin_marker)
        {
            BinaryFrameResult r = try_consume_binary_frame();
            if (r == BinaryFrameResult::Incomplete)
                return false;        // wait for the rest of the frame
            if (r == BinaryFrameResult::Consumed)
                continue;            // frame applied; re-discriminate
            // Skip: fall through to drop the marker byte below.
        }

        // Skip non-JSON / unknown-frame byte
        m_rx_head = (m_rx_head + 1) % RX_BUF_SIZE;
        --m_rx_len;
    }

    if (m_rx_len == 0) return false;

    // Look for newline (message terminator)
    size_t newline_offset = SIZE_MAX;
    for (size_t i = 0; i < m_rx_len; ++i)
    {
        char c = m_rx_buf[(m_rx_head + i) % RX_BUF_SIZE];
        if (c == '\n' || c == '\r')
        {
            newline_offset = i;
            break;
        }
    }

    if (newline_offset == SIZE_MAX)
    {
        // No terminator yet. Normally we wait for more bytes — but if the ring
        // is completely full, drain_serial_into_rx_buf() can no longer accept
        // input, so waiting would deadlock ALL serial RX permanently. The
        // front byte is already a message start ('{' or 0x1F, ensured by the
        // resync loop above), so a full ring with no newline means a single
        // logical message overflowed the buffer. Resync: drop bytes up to the
        // NEXT message-start marker (or clear entirely if none), and emit a
        // "line too long" diagnostic so the editor knows its send was dropped.
        if (m_rx_len == RX_BUF_SIZE)
        {
            dt::count("rx_overflow");
            dt::event("protocol", "rx_overflow");
            size_t drop = 1; // skip the current (overflowed) message start
            while (drop < m_rx_len)
            {
                char c = m_rx_buf[(m_rx_head + drop) % RX_BUF_SIZE];
                if (c == '{' ||
                    static_cast<uint8_t>(c) == SerialMsg::message_begin_marker)
                    break;
                ++drop;
            }
            m_rx_head = (m_rx_head + drop) % RX_BUF_SIZE;
            m_rx_len -= drop;

            send_log("error",
                     "Message too long (exceeds receive buffer); dropped");
        }
        return false; // incomplete message
    }

    // Copy message into linear buffer
    size_t msg_len = newline_offset;
    if (msg_len >= sizeof(m_msg_buf)) msg_len = sizeof(m_msg_buf) - 1;

    for (size_t i = 0; i < msg_len; ++i)
        m_msg_buf[i] = m_rx_buf[(m_rx_head + i) % RX_BUF_SIZE];
    m_msg_buf[msg_len] = '\0';
    m_msg_len = msg_len;

    // Consume the message + newline from ring buffer
    size_t consume = newline_offset + 1;
    m_rx_head = (m_rx_head + consume) % RX_BUF_SIZE;
    m_rx_len -= consume;

    return true;
}

bool SerialProtocol::has_incoming()
{
    if (m_message_ready) return true;

    drain_serial_into_rx_buf();

    if (try_extract_message())
    {
        m_message_ready = true;
        return true;
    }
    return false;
}

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

// ══════════════════════════════════════════════════════════════════════════
// Internal: system message handlers
// ══════════════════════════════════════════════════════════════════════════

void SerialProtocol::handle_hello(const char* /*payload*/, size_t /*len*/)
{
    m_json_mode = true;

    // Build IO config — mirrors useq::protocol::build_hello_io_config_json
    JsonBuilder inputs_arr;
    inputs_arr.array_begin_unkeyed();
    for (int i = 0; i < num_serial_ins; ++i)
    {
        char name[16];
        snprintf(name, sizeof(name), "ssin%d", i + 1);
        inputs_arr.object_begin()
            .field("index", i + 1)
            .field("name", name)
            .object_end();
    }
    inputs_arr.array_end();

    JsonBuilder outputs_arr;
    outputs_arr.array_begin_unkeyed();
    for (int i = 0; i < num_serial_outs; ++i)
    {
        char name[16];
        if (i == 0)
            snprintf(name, sizeof(name), "time");
        else
            snprintf(name, sizeof(name), "s%d", i);

        outputs_arr.object_begin()
            .field("index", i + 1)
            .field("name", name)
            .object_end();
    }
    outputs_arr.array_end();

    JsonBuilder config;
    config.object_begin()
        .field_raw("inputs", inputs_arr.build())
        .field_raw("outputs", outputs_arr.build())
        .object_end();

    JsonBuilder response;
    response.object_begin()
        .field("type", "response")
        .field("success", true)
        .field("mode", "json")
        .field("fw", build_info::VERSION)
        .field("protocol", build_info::PROTOCOL_VERSION)
        .field("target", build_info::HARDWARE_TARGET)
        .field_raw("capabilities", build_info::CAPABILITIES_JSON)
#ifdef ENABLE_I2C_NETWORKING
        .field_raw("modules", build_modules_json())
#endif
        .field_raw("config", config.build())
        .field("requestId", m_request_id)
        .object_end();

    write_json_str(response.build().c_str());
}

#ifdef ENABLE_I2C_NETWORKING
String SerialProtocol::build_modules_json() const
{
    JsonBuilder modules;
    modules.array_begin_unkeyed();
    if (i2c_network != nullptr && i2c_network->host_mode)
    {
        for (uint8_t index = 0; index < i2c_network->expander_count; ++index)
        {
            const ExpanderDescriptor& descriptor = i2c_network->expanders[index];
            modules.object_begin()
                .field("kind", "output-expander")
                .field("address", static_cast<int>(descriptor.address))
                .field("identityStatus", descriptor.factory_identity_valid
                                             ? "verified"
                                             : "unidentified-prototype")
                .field("firmware", descriptor.firmware_version)
                .field("target", descriptor.firmware_target)
                .field("connectedVia", "i2c")
                .field("protocol", static_cast<int>(descriptor.protocol_version));
            if (descriptor.factory_identity_valid)
            {
                const FactoryIdentity& identity = descriptor.factory_identity;
                modules.field("product", identity.product)
                    .field("hardwareRevision", identity.hardware_revision)
                    .field("assemblyVariant", identity.assembly_variant)
                    .field("batch", identity.batch)
                    .field("serial", identity.serial)
                    .field("manufactured", identity.manufacture_date)
                    .field("mcu", mcu_family_name(identity.mcu_family))
                    .field("updateTransport",
                           (identity.feature_bits & factory_feature::USB_UPDATE) != 0
                               ? "usb"
                               : "i2c-only")
                    .field("autoUpdateSafe",
                           (identity.feature_bits & factory_feature::USB_UPDATE) != 0);
            }
            else
            {
                modules.field("product", "unidentified-prototype")
                    .field("updateTransport", "i2c-only")
                    .field("autoUpdateSafe", false);
            }
            modules.object_end();
        }
    }
    modules.array_end();
    return modules.build();
}

void SerialProtocol::handle_rescan_modules(const char* /*payload*/, size_t /*len*/)
{
    if (i2c_network != nullptr && i2c_network->host_mode)
        i2c_network->scan_for_expanders();

    JsonBuilder response;
    response.object_begin()
        .field("type", "response")
        .field("success", i2c_network != nullptr && i2c_network->host_mode)
        .field_raw("modules", build_modules_json())
        .field("requestId", m_request_id)
        .object_end();
    write_json_str(response.build().c_str());
}
#endif

void SerialProtocol::handle_ping(const char* /*payload*/, size_t /*len*/)
{
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
}

void SerialProtocol::handle_stream_config(const char* payload, size_t len)
{
    // Parse maxRateHz (cap at 100Hz)
    int max_rate = extract_int(payload, len, "maxRateHz", 0);
    if (max_rate > 100) max_rate = 100;
    if (max_rate > 0)
    {
        stream_rate_limit_us =
            static_cast<unsigned long>(1000000UL / static_cast<unsigned long>(max_rate));
        if (stream_rate_limit_us == 0) stream_rate_limit_us = 1;
    }

    // Reset all channels, then re-populate from the config
    // Channel 0 (time) is always present
    for (size_t i = 0; i < MAX_STREAM_CHANNELS; ++i)
        stream_channels[i] = StreamChannel{};
    stream_channels[0].enabled = true;
    stream_channels[0].source = StreamSource::Time;
    num_stream_channels = 1;

    useq::json::ObjectCursor request(payload, len);
    useq::json::Value channels;
    if (request.find("channels", channels) &&
        channels.kind == useq::json::Kind::Array) {
        useq::json::ArrayCursor entries(channels);
        useq::json::Value entry;
        while (entries.next(entry) && num_stream_channels < MAX_STREAM_CHANNELS) {
            if (entry.kind != useq::json::Kind::Object) continue;
            useq::json::ObjectCursor channel(entry);
            useq::json::Value field;
            bool enabled = true;
            if (channel.find("enabled", field)) {
                useq::json::boolean(field, enabled);
            }
            if (!enabled) continue;

            char name[16] = {};
            char direction[8] = {};
            if (channel.find("name", field))
                useq::json::copy_string(field, name, sizeof(name));
            if (channel.find("direction", field))
                useq::json::copy_string(field, direction, sizeof(direction));
            if (strcmp(name, "time") == 0) continue;

            StreamChannel& stream = stream_channels[num_stream_channels];
            stream.enabled = true;
            if (strncmp(direction, "input", 5) == 0) {
                stream.source = StreamSource::Input;
                if (strcmp(name, "ssin1") == 0) stream.source_idx = 8;
                else if (strcmp(name, "ssin2") == 0) stream.source_idx = 9;
                stream.on_change_only = true;
            } else {
                stream.source = StreamSource::Output;
                if (name[0] == 's' && name[1] >= '1' && name[1] <= '8' &&
                    name[2] == '\0') {
                    stream.source_idx = 16 + (name[1] - '1');
                }
            }
            ++num_stream_channels;
        }
    }

    // Acknowledge
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
}

void SerialProtocol::handle_set_failure_mode(const char* payload, size_t len)
{
    // §5.10: configure the runtime non-finite failure policy
    // (failure-model.md §3). `mode` is "lkg" (default — non-finite at an
    // output root falls back to the last-known-good value) or "zero"
    // (legacy — every non-finite node result clamps to 0.0).
    char mode_buf[8] = {};
    extract_string(payload, len, "mode", mode_buf, sizeof(mode_buf));

    bool ok = true;
    if (strcmp(mode_buf, "lkg") == 0)
        sig::set_failure_mode(sig::FailureMode::LkgFallback);
    else if (strcmp(mode_buf, "zero") == 0)
        sig::set_failure_mode(sig::FailureMode::ZeroSquash);
    else
        ok = false;

    JsonBuilder b;
    b.object_begin()
        .field("type", "response")
        .field("success", ok)
        .field("console", "")
        .field("text", ok ? "" : "set-failure-mode: mode must be \"lkg\" or \"zero\"")
        .field_null("meta")
        .field("requestId", m_request_id)
        .object_end();
    write_json_str(b.build().c_str());
}

void SerialProtocol::handle_set_live_inputs(const char* payload, size_t len)
{
    int applied = 0;
    useq::json::ObjectCursor request(payload, len);
    useq::json::Value slots_value;
    if (engine && request.find("slots", slots_value) &&
        slots_value.kind == useq::json::Kind::Object) {
        useq::json::ObjectCursor slots(slots_value);
        useq::json::Value key;
        useq::json::Value encoded;
        while (slots.next(key, encoded)) {
            char id[sig::MAX_LIVE_SLOT_ID] = {};
            bool id_truncated = false;
            if (!useq::json::copy_string(
                    key, id, sizeof(id), &id_truncated) || id_truncated) {
                continue;
            }
            const int16_t slot_index = engine->pool.find_live_slot(id);
            if (slot_index < 0) continue; // Unknown IDs are dropped per §5.8.

            double value = 0.0;
            bool value_valid = false;
            if (encoded.kind == useq::json::Kind::Number) {
                value_valid = useq::json::number(encoded, value) &&
                              std::isfinite(value);
            } else if (encoded.kind == useq::json::Kind::Boolean) {
                bool flag = false;
                value_valid = useq::json::boolean(encoded, flag);
                value = flag ? 1.0 : 0.0;
            } else if (encoded.kind == useq::json::Kind::String &&
                       engine->pool.live_slots[slot_index].variant ==
                           sig::NodePool::SlotVariant::Keyword) {
                char option[sig::MAX_LIVE_SLOT_OPTION_LEN] = {};
                if (useq::json::copy_string(encoded, option, sizeof(option))) {
                    const auto& slot = engine->pool.live_slots[slot_index];
                    for (uint8_t index = 0; index < slot.options_count; ++index) {
                        if (strncmp(slot.options[index], option,
                                    sig::MAX_LIVE_SLOT_OPTION_LEN) == 0) {
                            value = static_cast<double>(index);
                            value_valid = true;
                            break;
                        }
                    }
                }
            }
            if (value_valid) {
                engine->pool.set_live_slot_value_by_index(
                    static_cast<uint16_t>(slot_index), value);
                ++applied;
            }
        }
    }

    // If the request included a requestId, emit the ack response (§5.8).
    // If no requestId, this is fire-and-forget — no response emitted.
    if (m_request_id[0] != '\0')
    {
        JsonBuilder b;
        b.object_begin()
            .field("type", "response")
            .field("requestId", m_request_id)
            .field("success", true)
            .field("applied", applied)
            .object_end();
        write_json_str(b.build().c_str());
    }
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
    // Spec §3.3: JSON messages are bare `{...}\n` — no 0x1F/type-byte prefix.
    Serial.write(reinterpret_cast<const uint8_t*>(payload), len);
    Serial.write('\n');
#else
    // Desktop: write bare JSON to stdout for test observability.
    fwrite(payload, 1, len, stdout);
    putchar('\n');
    fflush(stdout);
#endif
}

void SerialProtocol::write_json_str(const char* payload)
{
    write_json(payload, strlen(payload));
}

// ── get-state handler (state-sync.md §2) ────────────────────────────────

static const char* output_name_for_index(uint16_t idx) {
    static char buf[4];
    char prefix;
    uint16_t num;
    if (idx < 8)       { prefix = 'a'; num = idx; }
    else if (idx < 16) { prefix = 'd'; num = idx - 8; }
    else if (idx < 24) { prefix = 's'; num = idx - 16; }
    else return nullptr;
    buf[0] = prefix;
    buf[1] = (char)('1' + num);
    buf[2] = '\0';
    return buf;
}

void SerialProtocol::handle_get_state(const char* /*payload*/, size_t /*len*/)
{
    if (!engine)
    {
        JsonBuilder b;
        b.object_begin()
            .field("requestId", m_request_id)
            .field("success", false)
            .field("text", "no engine attached")
            .object_end();
        write_json_str(b.build().c_str());
        return;
    }

    char numbuf[32];

    // Build the state sub-object first, then embed it in the response.
    JsonBuilder state;
    state.object_begin();

    // transport
    {
        JsonBuilder tr;
        tr.object_begin();
        tr.field("playing", engine->state.is_playing);
        snprintf(numbuf, sizeof(numbuf), "%.15g", engine->state.time_offset);
        tr.field_raw("timeOffset", String(numbuf));
        tr.object_end();
        state.field_raw("transport", tr.build());
    }

    // time (not stored in engine — caller provides via tick)
    state.field_raw("time", "0");

    // cells — only emit non-empty cells
    {
        JsonBuilder cells;
        cells.object_begin();
        for (size_t i = 0; i < sig::MAX_CELLS; i++) {
            const auto& cell = engine->cells.cells[i];
            if (cell.kind == sig::CellKind::Empty) continue;

            const String& name = getSymbolString((uint32_t)i);
            if (name.length() == 0) continue;

            JsonBuilder c;
            c.object_begin();
            switch (cell.kind) {
                case sig::CellKind::Number:
                    c.field("type", "number");
                    snprintf(numbuf, sizeof(numbuf), "%.15g", cell.value);
                    c.field_raw("value", String(numbuf));
                    break;
                case sig::CellKind::Data: {
                    c.field("type", "data");
                    uint16_t dlen = 0;
                    const double* dptr = engine->cells.get_data_table(cell.data_table_id, dlen);
                    String arr = "[";
                    for (uint16_t d = 0; d < dlen; d++) {
                        if (d > 0) arr += ",";
                        snprintf(numbuf, sizeof(numbuf), "%.15g", dptr[d]);
                        arr += numbuf;
                    }
                    arr += "]";
                    c.field_raw("values", arr);
                    break;
                }
                case sig::CellKind::Callable: {
                    c.field("type", "callable");
                    const auto& info = engine->cells.callables[i];
                    if (info.source_length > 0) {
                        const char* src = engine->arena.read(info.source_offset);
                        if (src) {
                            String src_str(src);
                            if (src_str.length() > info.source_length)
                                src_str = src_str.substring(0, info.source_length);
                            c.field("source", src_str.c_str());
                        }
                    }
                    break;
                }
                case sig::CellKind::Nil:
                    c.field("type", "nil");
                    break;
                default:
                    break;
            }
            c.object_end();
            cells.field_raw(name.c_str(), c.build());
        }
        cells.object_end();
        state.field_raw("cells", cells.build());
    }

    // outputs — only emit active outputs
    {
        JsonBuilder outs;
        outs.object_begin();
        for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
            const auto& slot = engine->pool.outputs[i];
            if (slot.root_node == sig::NODE_NONE && !engine->output_sources[i].has_source) continue;
            const char* oname = output_name_for_index(i);
            if (!oname) continue;

            JsonBuilder o;
            o.object_begin();
            // source text
            if (engine->output_sources[i].has_source) {
                const char* src = engine->arena.read(engine->output_sources[i].arena_offset);
                if (src) {
                    String src_str(src);
                    uint32_t slen = engine->output_sources[i].arena_length;
                    if (src_str.length() > slen)
                        src_str = src_str.substring(0, slen);
                    o.field("source", src_str.c_str());
                }
            }
            const char* health = sig::output_health_to_cstr(
                sig::output_health(engine->pool, i));
            o.field("health", health);
            snprintf(numbuf, sizeof(numbuf), "%.15g", slot.lkg_value);
            o.field_raw("lkgValue", String(numbuf));
            o.object_end();
            outs.field_raw(oname, o.build());
        }
        outs.object_end();
        state.field_raw("outputs", outs.build());
    }

    // stateSlots
    {
        String arr = "[";
        for (uint16_t i = 0; i < engine->pool.state_slot_count; i++) {
            if (i > 0) arr += ",";
            JsonBuilder s;
            s.object_begin();
            snprintf(numbuf, sizeof(numbuf), "state_%u", (unsigned)i);
            s.field("id", numbuf);
            snprintf(numbuf, sizeof(numbuf), "%.15g", engine->pool.state_values[i]);
            s.field_raw("value", String(numbuf));
            s.object_end();
            arr += s.build();
        }
        arr += "]";
        state.field_raw("stateSlots", arr);
    }

    // liveSlots
    {
        String arr = "[";
        for (uint16_t i = 0; i < engine->pool.live_slot_count; i++) {
            if (i > 0) arr += ",";
            const auto& slot = engine->pool.live_slots[i];
            JsonBuilder l;
            l.object_begin();
            l.field("id", slot.id);
            snprintf(numbuf, sizeof(numbuf), "%.15g", slot.value);
            l.field_raw("value", String(numbuf));
            snprintf(numbuf, sizeof(numbuf), "%.15g", slot.min_val);
            l.field_raw("min", String(numbuf));
            snprintf(numbuf, sizeof(numbuf), "%.15g", slot.max_val);
            l.field_raw("max", String(numbuf));
            l.object_end();
            arr += l.build();
        }
        arr += "]";
        state.field_raw("liveSlots", arr);
    }

    state.object_end();

    JsonBuilder b;
    b.object_begin()
        .field("requestId", m_request_id)
        .field("success", true)
        .field("type", "state-snapshot")
        .field_raw("state", state.build())
        .object_end();

    write_json_str(b.build().c_str());
}

} // namespace firmware
