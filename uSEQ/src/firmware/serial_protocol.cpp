#include "serial_protocol.h"
#include "../devtools/devtools.h"
#include "../utils/serial_message.h"

#include <cstring>

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <iostream>
#endif

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

} // namespace firmware
