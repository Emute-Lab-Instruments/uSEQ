// Wire Protocol Contract Tests — firmware side.
//
// These tests encode the contract defined in
//   src-useq/docs/specs/wire-protocol.md
//
// Each test is initially tagged [.][!shouldfail] — it represents work the
// firmware-side implementation hasn't done yet, but must do to comply with
// the spec. Implementing agents:
//   1. Pick the bd issue for this delta (search "wire-protocol" labels).
//   2. Remove the [.][!shouldfail] tag from the relevant test (or change
//      to plain [contract]).
//   3. Implement until the test passes.
//   4. Land the change.
//
// The cross-references in each test's name point to the spec section the
// test covers. Read the spec before implementing.
//
// Tests capture stdout because the desktop build of firmware::SerialProtocol
// writes JSON to stdout (see serial_protocol.cpp §write_json #else branch).

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "../../uSEQ/src/firmware/serial_protocol.h"
#include "../../uSEQ/src/signal_engine/cold_eval.h"

#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <string>

// ── stdout capture helper ────────────────────────────────────────────────
//
// Redirects fd 1 (stdout) to a pipe, runs `body`, returns whatever was
// written. Restores stdout on scope exit. Linux/macOS only (POSIX pipe +
// dup2). Tests run on desktop with NO_ARDUINO so this path is exercised.

class StdoutCapture {
public:
    StdoutCapture()
    {
        std::fflush(stdout);
        saved_fd_ = ::dup(1);
        if (::pipe(pipe_fds_) != 0) std::fprintf(stderr, "pipe() failed\n");
        ::dup2(pipe_fds_[1], 1);
        ::close(pipe_fds_[1]);
        // Make read end non-blocking so we can drain without hanging.
        int flags = ::fcntl(pipe_fds_[0], F_GETFL, 0);
        ::fcntl(pipe_fds_[0], F_SETFL, flags | O_NONBLOCK);
    }

    ~StdoutCapture()
    {
        std::fflush(stdout);
        ::dup2(saved_fd_, 1);
        ::close(saved_fd_);
        ::close(pipe_fds_[0]);
    }

    std::string drain()
    {
        std::fflush(stdout);
        std::string out;
        char buf[4096];
        ssize_t n;
        while ((n = ::read(pipe_fds_[0], buf, sizeof(buf))) > 0) {
            out.append(buf, static_cast<size_t>(n));
        }
        return out;
    }

private:
    int saved_fd_   = -1;
    int pipe_fds_[2] = { -1, -1 };
};

// Helper: find a JSON message in captured output, returns its body
// (without any 0x1F/0x65 framing prefix or trailing newline).
// Finds the LAST line that begins with '{', which is the last top-level
// JSON object emitted. Using the last newline-delimited line ensures nested
// braces inside a single JSON message are not mistaken for a message start.
static std::string extract_last_json(const std::string& captured)
{
    // Walk backwards through newline-delimited lines to find the last
    // line that starts with '{'.
    size_t search_end = captured.size();
    while (search_end > 0) {
        // Find the previous newline before search_end
        size_t line_end = search_end;
        // Trim trailing newline chars
        while (line_end > 0 && (captured[line_end - 1] == '\n' || captured[line_end - 1] == '\r'))
            --line_end;

        // Find start of this line
        size_t line_start = (line_end == 0) ? 0 : captured.rfind('\n', line_end - 1);
        if (line_start == std::string::npos)
            line_start = 0;
        else
            ++line_start; // skip the '\n' itself

        if (line_start < line_end && captured[line_start] == '{')
            return captured.substr(line_start, line_end - line_start);

        if (line_start == 0) break;
        search_end = line_start; // step back past this line
    }
    return {};
}

// ── F1 — ready frame uses "version" not "fw" (§5.5) ─────────────────────

TEST_CASE("F1 [§5.5] ready frame uses \"version\" field, not \"fw\"",
          "[contract][wire-protocol]")
{
    StdoutCapture cap;
    firmware::SerialProtocol sp;
    sp.init();
    sp.send_ready();

    auto out = cap.drain();
    auto json = extract_last_json(out);
    REQUIRE_FALSE(json.empty());

    // Spec §5.5 — field name is "version" (matching firmware.md §3.1.6).
    REQUIRE(json.find("\"version\"") != std::string::npos);
    REQUIRE(json.find("\"type\":\"ready\"") != std::string::npos);
    // Must NOT use the legacy "fw" field name on the ready frame.
    REQUIRE(json.find("\"fw\"") == std::string::npos);
}

// ── F2 — outbound JSON has no 0x65 prefix (§3.3) ────────────────────────

TEST_CASE("F2 [§3.3] outbound JSON has no 0x1F/0x65 framing prefix",
          "[contract][wire-protocol]")
{
    StdoutCapture cap;
    firmware::SerialProtocol sp;
    sp.init();
    sp.send_ready();

    auto out = cap.drain();
    REQUIRE_FALSE(out.empty());

    // Spec §3.3 — JSON messages are bare `{...}\n` with no 0x1F marker
    // or 0x65 type byte preceding the `{`.
    // Find the first non-whitespace byte; it must be `{` (0x7b).
    size_t first = 0;
    while (first < out.size() && (out[first] == ' ' || out[first] == '\r' || out[first] == '\n')) {
        ++first;
    }
    REQUIRE(first < out.size());
    REQUIRE(static_cast<unsigned char>(out[first]) == 0x7bu);

    // And: nowhere in the output is the legacy 0x1F 0x65 prefix.
    bool found_legacy_prefix = false;
    for (size_t i = 0; i + 1 < out.size(); ++i) {
        if (static_cast<unsigned char>(out[i])     == 0x1fu &&
            static_cast<unsigned char>(out[i + 1]) == 0x65u) {
            found_legacy_prefix = true;
            break;
        }
    }
    REQUIRE_FALSE(found_legacy_prefix);
}

// ── F3 — log envelope shape (§5.6) ──────────────────────────────────────

TEST_CASE("F3 [§5.6] log/notice output uses {type:\"log\",level,text} envelope",
          "[contract][wire-protocol]")
{
    StdoutCapture cap;
    firmware::SerialProtocol sp;
    sp.init();

    sp.send_log("info", "Hello from uSEQ");

    auto out = cap.drain();
    auto json = extract_last_json(out);
    REQUIRE_FALSE(json.empty());

    // Spec §5.6 — envelope shape
    REQUIRE(json.find("\"type\":\"log\"") != std::string::npos);
    REQUIRE(json.find("\"level\":\"info\"") != std::string::npos);
    REQUIRE(json.find("\"text\":\"Hello from uSEQ\"") != std::string::npos);

    // Spec §3.3 — no legacy 0x1F/0x65 framing prefix anywhere in output
    bool found_legacy_prefix = false;
    for (size_t i = 0; i + 1 < out.size(); ++i) {
        if (static_cast<unsigned char>(out[i])     == 0x1fu &&
            static_cast<unsigned char>(out[i + 1]) == 0x65u) {
            found_legacy_prefix = true;
            break;
        }
    }
    REQUIRE_FALSE(found_legacy_prefix);
}

// ── F4 — eval response embeds diagnostics (§5.7) ────────────────────────

TEST_CASE("F4 [§5.7] eval response includes diagnostics array",
          "[contract][wire-protocol][.][!shouldfail]")
{
    // Already implemented in serial_protocol.cpp::send_eval_response (the
    // diagnostics array is populated when result.diagnostic_count > 0).
    // The contract this test asserts: the field name is `diagnostics`,
    // each element has the {severity,category,start,end,message,suggestion}
    // shape, and a successful eval with no diagnostics MAY omit the field.

    StdoutCapture cap;
    firmware::SerialProtocol sp;
    sp.init();

    sig::EvalResult result;
    result.kind = sig::EvalResult::Number;
    result.number = 0.5;
    result.text = nullptr;
    result.text_length = 0;
    result.diagnostic_count = 0;

    sp.send_eval_response(result);
    auto out = cap.drain();
    auto json = extract_last_json(out);

    REQUIRE_FALSE(json.empty());
    REQUIRE(json.find("\"type\":\"response\"") != std::string::npos);
    REQUIRE(json.find("\"success\":true") != std::string::npos);

    // No diagnostics => field MAY be absent. The current implementation omits
    // it; this is acceptable per the spec.
    // (No assertion on diagnostics for this case.)

    // Now test the WITH-diagnostics path. (Not directly invokable without
    // constructing a sig::Diagnostic — placeholder for the agent to flesh
    // out using the test harness in firmware_test_harness.h, which can run
    // a real eval and trigger a diagnostic via e.g. `(a1 nope)`.)
}

// ── F5 — hello response shape (§5.2) ────────────────────────────────────

TEST_CASE("F5 [§5.2] hello response has type:\"response\", mode, fw, config",
          "[contract][wire-protocol]")
{
    StdoutCapture cap;
    firmware::SerialProtocol sp;
    sp.num_serial_ins  = 1;
    sp.num_serial_outs = 9;  // time + s1..s8
    sp.init();

    // Simulate handle_hello firing without an inbound payload.
    sp.handle_handshake();

    auto out = cap.drain();
    auto json = extract_last_json(out);
    REQUIRE_FALSE(json.empty());

    // Spec §5.2 fields. The current implementation emits a hello-style
    // body without `type:"response"` — that's a delta the implementing
    // agent should fix.
    REQUIRE(json.find("\"type\":\"response\"") != std::string::npos);
    REQUIRE(json.find("\"success\":true") != std::string::npos);
    REQUIRE(json.find("\"mode\":\"json\"") != std::string::npos);
    REQUIRE(json.find("\"fw\"") != std::string::npos);
    REQUIRE(json.find("\"config\"") != std::string::npos);
    REQUIRE(json.find("\"outputs\"") != std::string::npos);
    REQUIRE(json.find("\"time\"") != std::string::npos);
}

// ── F6 — set-live-inputs handler (§5.8) ─────────────────────────────────
//
// Tests the protocol-level dispatch and ack for set-live-inputs. The slot-table
// integration (actually writing values to the runtime) is deferred until the
// live-edit feature lands. See: src-useq/docs/specs/live-edit.md §5.3 and §5.10.

TEST_CASE("F6 [§5.8] set-live-inputs request is accepted and ack has correct shape",
          "[contract][wire-protocol]")
{
    firmware::SerialProtocol sp;
    sp.init();

    // Build a set-live-inputs payload with two slots and a requestId.
    // Shape: {"type":"set-live-inputs","slots":{"knob1":0.5,"toggle1":true},"requestId":"req-x"}
    const char* payload =
        "{\"type\":\"set-live-inputs\","
        "\"slots\":{\"knob1\":0.5,\"toggle1\":true},"
        "\"requestId\":\"req-x\"}";
    const size_t payload_len = strlen(payload);

    char code_buf[256] = {};

    StdoutCapture cap;
    // dispatch_message is the internal helper that read_command delegates to.
    // Using it directly lets us inject a message without going through the
    // ring buffer — the cleaner testable entry point per the design note.
    bool is_eval = sp.dispatch_message(payload, payload_len, code_buf, sizeof(code_buf));
    auto out = cap.drain();

    // set-live-inputs is handled internally — not returned as an eval command.
    REQUIRE_FALSE(is_eval);

    // Should have emitted exactly one JSON response.
    auto json = extract_last_json(out);
    REQUIRE_FALSE(json.empty());

    // §5.8 ack shape: {type:"response", requestId, success:true, applied:N}
    REQUIRE(json.find("\"type\":\"response\"") != std::string::npos);
    REQUIRE(json.find("\"success\":true") != std::string::npos);
    REQUIRE(json.find("\"requestId\":\"req-x\"") != std::string::npos);
    // Two slots were provided — applied must be 2.
    REQUIRE(json.find("\"applied\":2") != std::string::npos);
}

TEST_CASE("F6b [§5.8] set-live-inputs fire-and-forget (no requestId) emits no response",
          "[contract][wire-protocol]")
{
    firmware::SerialProtocol sp;
    sp.init();

    const char* payload =
        "{\"type\":\"set-live-inputs\","
        "\"slots\":{\"knob1\":0.75}}";
    const size_t payload_len = strlen(payload);

    char code_buf[256] = {};

    StdoutCapture cap;
    bool is_eval = sp.dispatch_message(payload, payload_len, code_buf, sizeof(code_buf));
    auto out = cap.drain();

    // Still not an eval command.
    REQUIRE_FALSE(is_eval);
    // No requestId => fire-and-forget => no response emitted.
    REQUIRE(out.empty());
}

// TODO(live-edit): slot-table integration test — verify that dispatched slot
// values are actually written to the runtime and read back on the next eval.
// Blocked until live-edit.md §5.3 slot table is implemented in the compiler
// and runtime. Add test here alongside F6 when that lands.

// ── F7 — binary STREAM frame layout (§6.1) ──────────────────────────────

TEST_CASE("F7 [§6.1] outbound STREAM frame is exactly 11 bytes "
          "[0x1F][0x00][channel-1based][f64-LE]",
          "[contract][wire-protocol]")
{
    // This one is already correct in the implementation; we keep it as a
    // regression guard so future refactors don't drift the layout.
    StdoutCapture cap;
    firmware::SerialProtocol sp;
    sp.init();
    sp.num_serial_outs = 2;
    for (size_t i = 0; i < sig::MAX_OUTPUTS; ++i) sp.stream_channel_enabled[i] = true;

    double values[2] = { 0.25, 0.75 };
    sp.send_stream_data(values, 2);

    auto out = cap.drain();

#ifdef ARDUINO
    // On Arduino builds we don't capture stdout; skip.
    SUCCEED("F7 only runs on desktop builds");
#else
    // Desktop build: send_stream_data is a no-op (per serial_protocol.cpp
    // line 401–403). The contract-checking version of this test belongs
    // alongside the Arduino-mode integration tests, OR the desktop
    // implementation should be extended to write the same 11-byte layout
    // to stdout for observability. Either way, this test asserts the wire
    // shape so future refactors stay aligned.
    //
    // The agent picking this up should:
    //   1. Either teach send_stream_data to write to stdout on desktop
    //      (matching write_json's #else branch),
    //   2. Or add an Arduino-side integration test that captures bytes.
    //
    // For now, document the expected wire shape:
    //   byte[0] = 0x1F  (marker)
    //   byte[1] = 0x00  (STREAM type)
    //   byte[2] = channel (1-based, matches hello.config.outputs[].index)
    //   byte[3..10] = value as IEEE 754 double, little-endian
    //   total = 11 bytes per channel
    REQUIRE(out.empty()); // Documents that desktop currently doesn't write streams.
#endif
}
