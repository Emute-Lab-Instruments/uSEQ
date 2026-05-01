#include "serial_protocol.h"
#include "../utils/json_builder.h"
#include "../utils/serial_message.h"

#include <cstdio>
#include <cstring>

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <iostream>
#endif

// ── Minimal JSON field extraction ──────────────────────────────────────────
// Operates on raw char*/len to avoid String allocation on the hot path.
// Only extracts what the protocol layer needs; full parsing is not required.

namespace {

// Find the value region after "key": in a JSON buffer.
// Returns pointer to first non-whitespace char after the colon, or nullptr.
const char* find_field_value(const char* json, size_t json_len,
                             const char* field_name)
{
    const size_t field_len = strlen(field_name);

    // Build the search key: "field_name"
    char key[80];
    if (field_len + 3 > sizeof(key)) return nullptr;
    key[0] = '"';
    memcpy(key + 1, field_name, field_len);
    key[field_len + 1] = '"';
    key[field_len + 2] = '\0';
    const size_t key_len = field_len + 2;

    // Linear scan for the key
    for (size_t i = 0; i + key_len < json_len; ++i)
    {
        if (memcmp(json + i, key, key_len) == 0)
        {
            // Found key — skip to colon
            size_t j = i + key_len;
            while (j < json_len && json[j] != ':') ++j;
            if (j >= json_len) return nullptr;
            ++j; // skip colon
            while (j < json_len && (json[j] == ' ' || json[j] == '\t' ||
                                    json[j] == '\r' || json[j] == '\n'))
                ++j;
            if (j >= json_len) return nullptr;
            return json + j;
        }
    }
    return nullptr;
}

// Extract a string value (without quotes) into dst. Returns length or 0.
size_t extract_string(const char* json, size_t json_len,
                      const char* field_name,
                      char* dst, size_t dst_size)
{
    const char* v = find_field_value(json, json_len, field_name);
    if (!v || *v != '"') return 0;
    ++v; // skip opening quote

    size_t out = 0;
    bool escape = false;
    const char* end = json + json_len;
    while (v < end && out + 1 < dst_size)
    {
        char c = *v++;
        if (escape)
        {
            switch (c) {
                case '"':  dst[out++] = '"';  break;
                case '\\': dst[out++] = '\\'; break;
                case 'n':  dst[out++] = '\n'; break;
                case 'r':  dst[out++] = '\r'; break;
                case 't':  dst[out++] = '\t'; break;
                default:   dst[out++] = c;    break;
            }
            escape = false;
            continue;
        }
        if (c == '\\') { escape = true; continue; }
        if (c == '"')  break;
        dst[out++] = c;
    }
    dst[out] = '\0';
    return out;
}

// Extract an integer value. Returns the value or default_val if not found.
int extract_int(const char* json, size_t json_len,
                const char* field_name, int default_val = 0)
{
    const char* v = find_field_value(json, json_len, field_name);
    if (!v) return default_val;

    // Must start with digit or minus
    if (*v != '-' && (*v < '0' || *v > '9')) return default_val;
    return atoi(v);
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

    // Default: all stream channels enabled
    for (size_t i = 0; i < sig::MAX_OUTPUTS; ++i)
        stream_channel_enabled[i] = true;
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

bool SerialProtocol::try_extract_message()
{
    // Scan for a complete JSON message (starts with '{', ends with '\n').
    // The editor sends bare JSON objects, one per line.

    // Skip leading whitespace / garbage
    while (m_rx_len > 0)
    {
        char front = m_rx_buf[m_rx_head];
        if (front == '{') break;

        // Skip non-JSON byte
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

    if (newline_offset == SIZE_MAX) return false; // incomplete message

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

bool SerialProtocol::read_command(char* buf, size_t buf_size)
{
    if (!m_message_ready) return false;

    m_message_ready = false;

    // Parse the JSON message to determine type
    char type_buf[32] = {};
    extract_string(m_msg_buf, m_msg_len, "type", type_buf, sizeof(type_buf));

    // Extract request ID for response correlation
    extract_string(m_msg_buf, m_msg_len, "requestId",
                   m_request_id, sizeof(m_request_id));

    // Handle system messages internally
    if (strcmp(type_buf, "hello") == 0)
    {
        handle_hello(m_msg_buf, m_msg_len);
        return false; // not a user command
    }
    if (strcmp(type_buf, "ping") == 0)
    {
        handle_ping(m_msg_buf, m_msg_len);
        return false;
    }
    if (strcmp(type_buf, "stream-config") == 0)
    {
        handle_stream_config(m_msg_buf, m_msg_len);
        return false;
    }

    // Eval message — extract code field
    char code_buf[2048] = {};
    size_t code_len = extract_string(m_msg_buf, m_msg_len, "code",
                                     code_buf, sizeof(code_buf));

    if (code_len == 0)
    {
        // Malformed eval request — no code field
        JsonBuilder b;
        b.object_begin()
            .field("type", "response")
            .field("success", false)
            .field("console", "Malformed JSON request: missing code field")
            .field("text", "Malformed JSON request: missing code field")
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

void SerialProtocol::send_stream_data(const double* values, size_t count)
{
    // Stream data uses the binary wire format for efficiency:
    //   [0x1F] [STREAM=0] [channel_byte] [8 bytes double] ...
    // This matches the existing IOManager::serial_write format.

#ifdef ARDUINO
    if (!Serial.availableForWrite()) return;

    for (size_t i = 0; i < count; ++i)
    {
        if (i >= sig::MAX_OUTPUTS) break;
        if (!stream_channel_enabled[i]) continue;

        Serial.write(SerialMsg::message_begin_marker);
        Serial.write(static_cast<uint8_t>(SerialMsg::serial_message_types::STREAM));
        Serial.write(static_cast<uint8_t>(i + 1)); // 1-indexed channel
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&values[i]);
        for (size_t b = 0; b < 8; ++b)
            Serial.write(bytes[b]);
    }
#else
    (void)values;
    (void)count;
    // Desktop: stream data is not sent to stdout
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
        .field("version", "1.2.0")
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
        .field("fw", "1.2.0")
        .field_raw("config", config.build())
        .field("requestId", m_request_id)
        .object_end();

    write_json_str(response.build().c_str());
}

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
    // Parse maxRateHz
    int max_rate = extract_int(payload, len, "maxRateHz", 0);
    if (max_rate > 0)
    {
        stream_rate_limit_us =
            static_cast<unsigned long>(1000000UL / static_cast<unsigned long>(max_rate));
        if (stream_rate_limit_us == 0) stream_rate_limit_us = 1;
    }

    // Parse channels array — lightweight scan for id/enabled fields
    const char* channels_start = find_field_value(payload, len, "channels");
    if (channels_start && *channels_start == '[')
    {
        // Walk through the array looking for objects with "id" and "enabled"
        const char* end = payload + len;
        const char* p = channels_start + 1;
        int depth = 0;
        const char* obj_start = nullptr;

        while (p < end)
        {
            if (*p == '{')
            {
                if (depth == 0) obj_start = p;
                ++depth;
            }
            else if (*p == '}')
            {
                --depth;
                if (depth == 0 && obj_start)
                {
                    size_t obj_len = static_cast<size_t>(p - obj_start + 1);
                    int ch_id = extract_int(obj_start, obj_len, "id", -1);
                    if (ch_id >= 1 && ch_id <= static_cast<int>(sig::MAX_OUTPUTS))
                    {
                        // Check "enabled" field — default true
                        const char* ev = find_field_value(obj_start, obj_len, "enabled");
                        bool enabled = true;
                        if (ev)
                        {
                            if (strncmp(ev, "false", 5) == 0) enabled = false;
                        }

                        stream_channel_enabled[ch_id - 1] = enabled;

                        if (static_cast<uint8_t>(ch_id) > num_stream_channels)
                            num_stream_channels = static_cast<uint8_t>(ch_id);
                    }
                    obj_start = nullptr;
                }
            }
            else if (*p == ']' && depth == 0)
            {
                break; // end of channels array
            }
            ++p;
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

} // namespace firmware
