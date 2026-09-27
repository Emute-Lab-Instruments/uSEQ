// Regression proof for wire-r-stream-s8-dropped:
// the editor's default stream-config (state-sync.md §1.2) subscribes 2 inputs
// (ssin1, ssin2) + all 8 non-time serial outputs (s1..s8). With time that is
// 11 stream slots; MAX_STREAM_CHANNELS was 10, silently dropping s8.
// Fails (abort) with cap 10, passes with cap 12.
#include "firmware/serial_protocol.h"

#include <cstring>
#include <cstdio>
#include <string>

static std::string make_editor_stream_config() {
    std::string channels;
    channels += "{\"id\":1,\"name\":\"ssin1\",\"direction\":\"input\",\"enabled\":true,\"maxRateHz\":100},";
    channels += "{\"id\":2,\"name\":\"ssin2\",\"direction\":\"input\",\"enabled\":true,\"maxRateHz\":100}";
    for (int i = 1; i <= 8; ++i) {
        channels += ",{\"id\":" + std::to_string(i + 2) +
                    ",\"name\":\"s" + std::to_string(i) +
                    "\",\"direction\":\"output\",\"enabled\":true,\"maxRateHz\":100}";
    }
    return "{\"type\":\"stream-config\",\"requestId\":\"proof\",\"maxRateHz\":100,\"channels\":[" + channels + "]}";
}

int main() {
    firmware::SerialProtocol sp;

    // Wire the two hardware inputs the firmware advertises (firmware.cpp:
    // num_serial_ins = 2) so input source_idx resolution is in play.
    sp.num_serial_ins = 2;

    char eval_buf[256];
    const std::string config = make_editor_stream_config();
    sp.dispatch_message(config.c_str(), config.size(), eval_buf, sizeof(eval_buf));

    // Channel 0 (time) + 10 subscribed channels must all fit.
    if (sp.num_stream_channels != 11) {
        std::printf("FAIL: num_stream_channels=%u, expected 11 (time + 2 inputs + 8 outputs)\n",
                    sp.num_stream_channels);
        return 1;
    }

    // s8 must be present with source_idx 23 (16 + 8 - 1 in the 24-slot layout).
    bool s8_present = false;
    for (uint8_t ch = 1; ch < sp.num_stream_channels; ++ch) {
        const firmware::SerialProtocol::StreamChannel& sc = sp.stream_channels[ch];
        if (sc.source == firmware::SerialProtocol::StreamSource::Output &&
            sc.source_idx == 16 + 7 && sc.enabled) {
            s8_present = true;
        }
    }
    if (!s8_present) {
        std::printf("FAIL: s8 (source_idx 23) missing from stream channels — dropped by capacity cap\n");
        return 1;
    }

    std::printf("PASS: all 11 stream channels configured (time + 2 inputs + s1..s8 incl. s8)\n");
    return 0;
}
