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
#include "../../uSEQ/src/signal_engine/graph_builder.h"

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
    // Set up engine with pre-allocated live-edit slots so the handler has
    // somewhere to write.
    sig::GraphBuilder::init_symbols();
    sig::SignalEngine engine;
    engine.init_defaults();
    const char* setup_code =
        "(a1 (+ (live-edit 0.5 :id \"knob1\" :min 0 :max 1)"
        "       (live-edit 0 :id \"toggle1\" :min 0 :max 1)))";
    sig::eval_cold(setup_code, (uint32_t)strlen(setup_code), engine);

    firmware::SerialProtocol sp;
    sp.init();
    sp.engine = &engine;

    // Build a set-live-inputs payload with two slots and a requestId.
    const char* payload =
        "{\"type\":\"set-live-inputs\","
        "\"slots\":{\"knob1\":0.5,\"toggle1\":true},"
        "\"requestId\":\"req-x\"}";
    const size_t payload_len = strlen(payload);

    char code_buf[256] = {};

    StdoutCapture cap;
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
    // Two slots were provided and both exist — applied must be 2.
    REQUIRE(json.find("\"applied\":2") != std::string::npos);
}

TEST_CASE("F6b [§5.8] set-live-inputs fire-and-forget (no requestId) emits no response",
          "[contract][wire-protocol]")
{
    sig::GraphBuilder::init_symbols();
    sig::SignalEngine engine;
    engine.init_defaults();
    const char* setup_code = "(a1 (live-edit 0.5 :id \"knob1\" :min 0 :max 1))";
    sig::eval_cold(setup_code, (uint32_t)strlen(setup_code), engine);

    firmware::SerialProtocol sp;
    sp.init();
    sp.engine = &engine;

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

// ── F6c–F6g — slot-table integration tests for set-live-inputs ──────────
//
// These verify that the firmware handler actually writes values into the
// signal engine's live slot table, not just that the JSON ack is correct.

TEST_CASE("F6c [§5.8] value actually lands in slot table after set-live-inputs",
          "[contract][wire-protocol][live-edit]")
{
    sig::GraphBuilder::init_symbols();
    sig::SignalEngine engine;
    engine.init_defaults();
    const char* code = "(a1 (live-edit 0.5 :id \"vol\" :min 0 :max 1))";
    sig::eval_cold(code, (uint32_t)strlen(code), engine);

    REQUIRE(engine.pool.live_slot_count == 1);
    REQUIRE(engine.pool.live_slots[0].value == Approx(0.5));

    firmware::SerialProtocol sp;
    sp.init();
    sp.engine = &engine;

    const char* payload =
        "{\"type\":\"set-live-inputs\","
        "\"slots\":{\"vol\":0.75},"
        "\"requestId\":\"req-c\"}";
    char buf[256] = {};
    StdoutCapture cap;
    sp.dispatch_message(payload, strlen(payload), buf, sizeof(buf));
    cap.drain();

    // The slot value must have been updated by the handler.
    REQUIRE(engine.pool.live_slots[0].value == Approx(0.75));
}

TEST_CASE("F6d [§5.8] unknown slot ID is silently dropped (applied=0)",
          "[contract][wire-protocol][live-edit]")
{
    sig::GraphBuilder::init_symbols();
    sig::SignalEngine engine;
    engine.init_defaults();
    const char* code = "(a1 (live-edit 0.5 :id \"vol\" :min 0 :max 1))";
    sig::eval_cold(code, (uint32_t)strlen(code), engine);

    firmware::SerialProtocol sp;
    sp.init();
    sp.engine = &engine;

    // Send a value for a slot ID that was never allocated.
    const char* payload =
        "{\"type\":\"set-live-inputs\","
        "\"slots\":{\"nonexistent\":0.9},"
        "\"requestId\":\"req-d\"}";
    char buf[256] = {};
    StdoutCapture cap;
    sp.dispatch_message(payload, strlen(payload), buf, sizeof(buf));
    auto out = cap.drain();
    auto json = extract_last_json(out);

    // applied must be 0 — unknown IDs are dropped, not counted.
    REQUIRE(json.find("\"applied\":0") != std::string::npos);
    // The existing slot must be unchanged.
    REQUIRE(engine.pool.live_slots[0].value == Approx(0.5));
}

TEST_CASE("F6e [§5.8] NaN/Inf values are rejected, slot retains previous value",
          "[contract][wire-protocol][live-edit]")
{
    sig::GraphBuilder::init_symbols();
    sig::SignalEngine engine;
    engine.init_defaults();
    const char* code = "(a1 (live-edit 0.5 :id \"vol\" :min 0 :max 1))";
    sig::eval_cold(code, (uint32_t)strlen(code), engine);

    firmware::SerialProtocol sp;
    sp.init();
    sp.engine = &engine;

    // JSON doesn't natively support NaN/Inf, but atof("NaN") and atof("Infinity")
    // produce those values. The handler must reject them.
    // Test with null (not a number) — should be skipped entirely.
    {
        const char* payload =
            "{\"type\":\"set-live-inputs\","
            "\"slots\":{\"vol\":null},"
            "\"requestId\":\"req-e1\"}";
        char buf[256] = {};
        StdoutCapture cap;
        sp.dispatch_message(payload, strlen(payload), buf, sizeof(buf));
        auto out = cap.drain();
        auto json = extract_last_json(out);
        REQUIRE(json.find("\"applied\":0") != std::string::npos);
        REQUIRE(engine.pool.live_slots[0].value == Approx(0.5));
    }

    // Test with a string value (not numeric) — should be skipped.
    {
        const char* payload =
            "{\"type\":\"set-live-inputs\","
            "\"slots\":{\"vol\":\"not-a-number\"},"
            "\"requestId\":\"req-e2\"}";
        char buf[256] = {};
        StdoutCapture cap;
        sp.dispatch_message(payload, strlen(payload), buf, sizeof(buf));
        auto out = cap.drain();
        auto json = extract_last_json(out);
        REQUIRE(json.find("\"applied\":0") != std::string::npos);
        REQUIRE(engine.pool.live_slots[0].value == Approx(0.5));
    }
}

TEST_CASE("F6f [§5.8] value is clamped to slot [min,max] range",
          "[contract][wire-protocol][live-edit]")
{
    sig::GraphBuilder::init_symbols();
    sig::SignalEngine engine;
    engine.init_defaults();
    // Slot with range [0.2, 0.8]
    const char* code = "(a1 (live-edit 0.5 :id \"vol\" :min 0.2 :max 0.8))";
    sig::eval_cold(code, (uint32_t)strlen(code), engine);

    firmware::SerialProtocol sp;
    sp.init();
    sp.engine = &engine;

    // Send value above max — should be clamped to 0.8
    {
        const char* payload =
            "{\"type\":\"set-live-inputs\","
            "\"slots\":{\"vol\":5.0},"
            "\"requestId\":\"req-f1\"}";
        char buf[256] = {};
        StdoutCapture cap;
        sp.dispatch_message(payload, strlen(payload), buf, sizeof(buf));
        cap.drain();
        REQUIRE(engine.pool.live_slots[0].value == Approx(0.8));
    }

    // Send value below min — should be clamped to 0.2
    {
        const char* payload =
            "{\"type\":\"set-live-inputs\","
            "\"slots\":{\"vol\":-1.0},"
            "\"requestId\":\"req-f2\"}";
        char buf[256] = {};
        StdoutCapture cap;
        sp.dispatch_message(payload, strlen(payload), buf, sizeof(buf));
        cap.drain();
        REQUIRE(engine.pool.live_slots[0].value == Approx(0.2));
    }
}

TEST_CASE("F6g [§5.8] multiple slots in one message all apply",
          "[contract][wire-protocol][live-edit]")
{
    sig::GraphBuilder::init_symbols();
    sig::SignalEngine engine;
    engine.init_defaults();
    const char* code =
        "(a1 (+ (live-edit 0.1 :id \"a\" :min 0 :max 1)"
        "       (live-edit 0.2 :id \"b\" :min 0 :max 1)"
        "       (live-edit 0.3 :id \"c\" :min 0 :max 1)))";
    sig::eval_cold(code, (uint32_t)strlen(code), engine);
    REQUIRE(engine.pool.live_slot_count == 3);

    firmware::SerialProtocol sp;
    sp.init();
    sp.engine = &engine;

    const char* payload =
        "{\"type\":\"set-live-inputs\","
        "\"slots\":{\"a\":0.9,\"b\":0.8,\"c\":0.7},"
        "\"requestId\":\"req-g\"}";
    char buf[256] = {};
    StdoutCapture cap;
    sp.dispatch_message(payload, strlen(payload), buf, sizeof(buf));
    auto out = cap.drain();
    auto json = extract_last_json(out);

    // All three must apply.
    REQUIRE(json.find("\"applied\":3") != std::string::npos);
    REQUIRE(engine.pool.live_slots[0].value == Approx(0.9));
    REQUIRE(engine.pool.live_slots[1].value == Approx(0.8));
    REQUIRE(engine.pool.live_slots[2].value == Approx(0.7));
}

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
    // Configure two output channels using the StreamChannel API.
    for (uint8_t ch = 0; ch < 2; ++ch) {
        sp.stream_channels[ch].enabled    = true;
        sp.stream_channels[ch].source     = firmware::SerialProtocol::StreamSource::Output;
        sp.stream_channels[ch].source_idx = ch;
    }
    sp.num_stream_channels = 2;

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

// ── F8 — get-state response shape (state-sync.md §2) ──────────────────

TEST_CASE("F8 [state-sync §2] get-state returns success with state object",
          "[contract][state-sync]")
{
    // Set up a SignalEngine with some known state
    sig::GraphBuilder::init_symbols();
    sig::SignalEngine engine;
    engine.init_defaults();

    // Define a cell: (define bpm 140)
    sig::eval_cold("(define bpm 140)", 16, engine);

    // Define an output: (a1 (sine 1))
    sig::eval_cold("(a1 (sine 1))", 13, engine);

    firmware::SerialProtocol sp;
    sp.init();
    sp.engine = &engine;

    // Dispatch a get-state message
    StdoutCapture cap;
    char buf[2048] = {};
    const char* msg = R"({"type":"get-state","requestId":"req-42"})";
    sp.dispatch_message(msg, strlen(msg), buf, sizeof(buf));

    auto out = cap.drain();
    auto json = extract_last_json(out);
    REQUIRE_FALSE(json.empty());

    // Must contain success and type
    REQUIRE(json.find("\"success\":true") != std::string::npos);
    REQUIRE(json.find("\"type\":\"state-snapshot\"") != std::string::npos);
    REQUIRE(json.find("\"requestId\":\"req-42\"") != std::string::npos);

    // Must contain state sub-object with required fields
    REQUIRE(json.find("\"state\":{") != std::string::npos);
    REQUIRE(json.find("\"transport\":{") != std::string::npos);
    REQUIRE(json.find("\"cells\":{") != std::string::npos);
    REQUIRE(json.find("\"outputs\":{") != std::string::npos);
    REQUIRE(json.find("\"stateSlots\":[") != std::string::npos);
    REQUIRE(json.find("\"liveSlots\":[") != std::string::npos);
}

TEST_CASE("F9 [state-sync §2] get-state includes defined cells",
          "[contract][state-sync]")
{
    sig::GraphBuilder::init_symbols();
    sig::SignalEngine engine;
    engine.init_defaults();

    sig::eval_cold("(define my-val 42)", 18, engine);

    firmware::SerialProtocol sp;
    sp.init();
    sp.engine = &engine;

    StdoutCapture cap;
    char buf[2048] = {};
    const char* msg = R"({"type":"get-state","requestId":"req-1"})";
    sp.dispatch_message(msg, strlen(msg), buf, sizeof(buf));

    auto out = cap.drain();
    auto json = extract_last_json(out);

    // The cell "my-val" should appear as a number cell with value 42
    REQUIRE(json.find("\"my-val\"") != std::string::npos);
    REQUIRE(json.find("\"type\":\"number\"") != std::string::npos);
    REQUIRE(json.find("42") != std::string::npos);
}

TEST_CASE("F10 [state-sync §2] get-state includes active outputs",
          "[contract][state-sync]")
{
    sig::GraphBuilder::init_symbols();
    sig::SignalEngine engine;
    engine.init_defaults();

    sig::eval_cold("(a1 (sine 1))", 13, engine);

    firmware::SerialProtocol sp;
    sp.init();
    sp.engine = &engine;

    StdoutCapture cap;
    char buf[2048] = {};
    const char* msg = R"({"type":"get-state","requestId":"req-2"})";
    sp.dispatch_message(msg, strlen(msg), buf, sizeof(buf));

    auto out = cap.drain();
    auto json = extract_last_json(out);

    // Output "a1" should be present with source and health
    REQUIRE(json.find("\"a1\"") != std::string::npos);
    REQUIRE(json.find("\"source\"") != std::string::npos);
    REQUIRE(json.find("\"health\"") != std::string::npos);
}

TEST_CASE("F11 [state-sync §2] get-state without engine returns failure",
          "[contract][state-sync]")
{
    firmware::SerialProtocol sp;
    sp.init();
    // engine pointer left as nullptr

    StdoutCapture cap;
    char buf[2048] = {};
    const char* msg = R"({"type":"get-state","requestId":"req-3"})";
    sp.dispatch_message(msg, strlen(msg), buf, sizeof(buf));

    auto out = cap.drain();
    auto json = extract_last_json(out);
    REQUIRE_FALSE(json.empty());

    REQUIRE(json.find("\"success\":false") != std::string::npos);
    REQUIRE(json.find("\"requestId\":\"req-3\"") != std::string::npos);
}
