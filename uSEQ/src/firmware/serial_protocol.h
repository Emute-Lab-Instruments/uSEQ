#ifndef FIRMWARE_SERIAL_PROTOCOL_H
#define FIRMWARE_SERIAL_PROTOCOL_H

// JSON-framed communication over USB serial
//
// SerialProtocol handles the wire format between the firmware and the editor.
// It does NOT evaluate code — it only shuttles bytes.
//
// Wire format (spec §3.3):
//   JSON messages: {payload}\n  — bare, no prefix bytes
//   Binary frames: [0x1F][type_byte][payload]  — for stream data
//
// Message types (inbound):
//   hello           — protocol negotiation, returns hardware config
//   ping            — keepalive heartbeat
//   eval            — code string (handled by caller after read_command)
//   stream-config   — configure output streaming rate/channels
//   set-live-inputs — write one or more live-edit slot values (§5.8)
//   get-state       — return full interpreter state snapshot (state-sync.md §2)
//
// Outbound:
//   ready         — sent once after init
//   response      — eval result with optional diagnostics and meta
//   stream        — binary output values for visualisation

#include "../signal_engine/cold_eval.h"
#include "../signal_engine/diagnostics.h"
#include "../utils/serial_message.h"
#include <cstdint>

namespace firmware {

// ── Serial Protocol ───────────────────────────────────────────────────────

struct SerialProtocol {

    // ── Lifecycle ──────────────────────────────────────────────────────────
    void init(unsigned long baud_rate = 115200);

    // ── Receive ────────────────────────────────────────────────────────────
    bool has_incoming();
    bool read_command(char* buf, size_t buf_size);

    // ── Test seam: inject raw bytes into the RX ring buffer ────────────────
    // Mirrors what drain_serial_into_rx_buf() does from the UART on hardware
    // (honours the RX_BUF_SIZE cap and drops overflow). Lets desktop tests
    // exercise the framing/resync path without a serial device. Returns the
    // number of bytes actually accepted into the ring.
    size_t rx_inject(const uint8_t* bytes, size_t count);

    // ── Send (must-deliver) ────────────────────────────────────────────────
    void send_eval_response(const sig::EvalResult& result);

    // ── Send (opportunistic — may be dropped if TX full) ───────────────────
    void send_diagnostics(const sig::Diagnostic* diags, uint8_t count);
    void send_stream_data(const double* output_values, size_t output_count,
                          const double* input_values = nullptr, size_t input_count = 0);

    // ── Control messages ───────────────────────────────────────────────────
    void handle_handshake();
    void send_ready();
    // Log envelope (§5.6) — replaces legacy TEXT/MSG_TO_EDITOR framed bytes.
    // level: one of "debug", "info", "notice", "warn", "error"
    void send_log(const char* level, const char* text);

    // ── Testable dispatch entry point ──────────────────────────────────────
    // Dispatch a fully-framed JSON message (without the trailing '\n').
    // Called by read_command for messages already in m_msg_buf, and exposed
    // as a testable entry point so unit tests can inject messages directly
    // without going through the ring buffer.
    // Returns true if the message is an eval command (caller should copy code
    // from the supplied buf/buf_size), false for all internally-handled types.
    bool dispatch_message(const char* payload, size_t len,
                          char* buf, size_t buf_size);

    // ── Stream config state (read by tick loop) ────────────────────────────
    // Stream channels are a separate namespace from output indices:
    //   wire channel 1 = time (always index 0 internally)
    //   wire channels 2+ = editor-subscribed signals (ain1, ain2, etc.)
    // The editor sends stream-config to define which signals map to which channels.
    static constexpr uint8_t MAX_STREAM_CHANNELS = 10;

    enum class StreamSource : uint8_t { Time = 0, Output = 1, Input = 2 };
    struct StreamChannel {
        bool enabled        = false;
        StreamSource source = StreamSource::Time;
        uint8_t source_idx  = 0;     // index into output_values[] or inputs[]
        bool on_change_only = false;  // only send when value differs from last sent
        double last_sent    = 0.0;    // for change detection
    };
    StreamChannel stream_channels[MAX_STREAM_CHANNELS] = {};
    uint8_t num_stream_channels = 1; // channel 0 (time) active by default
    unsigned long stream_rate_limit_us = SerialMsg::serial_message_rate_limit;
    double m_current_time = 0.0; // set by tick loop each frame

    // ── Hardware config (set by Firmware before init) ──────────────────────
    uint8_t num_serial_ins  = 0;
    uint8_t num_serial_outs = 0;

    // Engine access (set by Firmware after construction)
    sig::SignalEngine* engine = nullptr;

private:
    // ── Ring buffer for incoming bytes ──────────────────────────────────────
    static constexpr size_t RX_BUF_SIZE = 2048;
    char m_rx_buf[RX_BUF_SIZE] = {};
    size_t m_rx_head = 0;
    size_t m_rx_len  = 0;   // number of valid bytes in buffer

    // ── Parsed message state ───────────────────────────────────────────────
    bool m_message_ready   = false;
    char m_msg_buf[2048]   = {};
    size_t m_msg_len       = 0;

    // ── JSON mode (enabled after hello handshake) ──────────────────────────
    bool m_json_mode       = false;

    // ── Current request tracking ──────────────────────────────��────────────
    char m_request_id[64]  = {};

    // ── Stream rate limiting ──────────────────────────────────────────────
    unsigned long m_last_stream_us = 0;

    // ── Internal helpers ───────────────────────────────────────────────────
    void drain_serial_into_rx_buf();
    bool try_extract_message();

    // ── Inbound binary frame demux (§6.5 INPUT_SET) ────────────────────────
    enum class BinaryFrameResult { Consumed, Incomplete, Skip };
    // Read the RX ring buffer byte at logical offset from m_rx_head.
    uint8_t rx_byte_at(size_t offset) const;
    // Front byte is 0x1F: attempt to consume one complete binary frame.
    BinaryFrameResult try_consume_binary_frame();

    void write_json(const char* payload, size_t len);
    void write_json_str(const char* payload);
    bool can_write();

    void handle_hello(const char* payload, size_t len);
    void handle_ping(const char* payload, size_t len);
    void handle_stream_config(const char* payload, size_t len);
    void handle_set_live_inputs(const char* payload, size_t len);
    void handle_get_state(const char* payload, size_t len);
};

} // namespace firmware

#endif // FIRMWARE_SERIAL_PROTOCOL_H
