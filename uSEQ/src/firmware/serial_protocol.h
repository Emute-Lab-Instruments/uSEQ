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
//   hello         — protocol negotiation, returns hardware config
//   ping          — keepalive heartbeat
//   eval          — code string (handled by caller after read_command)
//   stream-config — configure output streaming rate/channels
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

// ── Stream Channel Configuration ──────────────────────────────────────────
// Per-channel enable/rate from stream-config messages.

struct StreamChannelConfig {
    int id           = 0;
    bool enabled     = true;
    int max_rate_hz  = 0;
};

// ── Serial Protocol ───────────────────────────────────────────────────────

struct SerialProtocol {

    // ── Lifecycle ──────────────────────────────────────────────────────────
    void init(unsigned long baud_rate = 115200);

    // ── Receive ────────────────────────────────────────────────────────────
    bool has_incoming();
    bool read_command(char* buf, size_t buf_size);

    // ── Send (must-deliver) ────────────────────────────────────────────────
    void send_eval_response(const sig::EvalResult& result);

    // ── Send (opportunistic — may be dropped if TX full) ───────────────────
    void send_diagnostics(const sig::Diagnostic* diags, uint8_t count);
    void send_stream_data(const double* values, size_t count);

    // ── Control messages ───────────────────────────────────────────────────
    void handle_handshake();
    void send_ready();

    // ── Stream config state (read by tick loop) ────────────────────────────
    unsigned long stream_rate_limit_us = SerialMsg::serial_message_rate_limit;
    bool stream_channel_enabled[sig::MAX_OUTPUTS] = {};
    uint8_t num_stream_channels = 0;

    // ── Hardware config (set by Firmware before init) ──────────────────────
    uint8_t num_serial_ins  = 0;
    uint8_t num_serial_outs = 0;

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

    // ── Current request tracking ───────────────────────────────────────────
    char m_request_id[64]  = {};

    // ── Internal helpers ───────────────────────────────────────────────────
    void drain_serial_into_rx_buf();
    bool try_extract_message();

    void write_json(const char* payload, size_t len);
    void write_json_str(const char* payload);
    bool can_write();

    void handle_hello(const char* payload, size_t len);
    void handle_ping(const char* payload, size_t len);
    void handle_stream_config(const char* payload, size_t len);
};

} // namespace firmware

#endif // FIRMWARE_SERIAL_PROTOCOL_H
