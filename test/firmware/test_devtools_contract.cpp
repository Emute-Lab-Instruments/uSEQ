// Devtools Debug Protocol Contract Tests
//
// These tests encode the contract for the devtools debug wire protocol
// defined in src-useq/docs/specs/devtools.md.
//
// Each test exercises a specific action of dt::handle_debug_message()
// (capabilities, configure, query, status) and verifies the JSON response
// shape. A simple capture function replaces the serial write path so we
// can inspect output without stdout redirection.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "../../uSEQ/src/devtools/devtools.h"
#include "../../uSEQ/src/signal_engine/signal_engine.h"

#include <cstring>
#include <string>

// ── Write-function capture ──────────────────────────────────────────────
//
// dt::handle_debug_message takes a WriteFn callback. We capture into a
// static std::string so tests can inspect the JSON response directly.

static std::string s_last_json;

static void capture_write(const char* data, size_t len) {
    s_last_json.assign(data, len);
}

// ── Helper: check substring presence ────────────────────────────────────

static bool json_contains(const std::string& json, const char* needle) {
    return json.find(needle) != std::string::npos;
}

// ══════════════════════════════════════════════════════════════════════════
// D1 — capabilities: response contains devtools:true and channels array
// ══════════════════════════════════════════════════════════════════════════

TEST_CASE("D1 capabilities response has devtools:true and channels array",
          "[contract][devtools]")
{
    dt::init(nullptr);
    s_last_json.clear();

    const char* msg =
        R"({"type":"debug","action":"capabilities","requestId":"cap-1"})";

    bool handled = dt::handle_debug_message(msg, strlen(msg), capture_write);

    REQUIRE(handled);
    REQUIRE_FALSE(s_last_json.empty());

    // Response envelope
    REQUIRE(json_contains(s_last_json, "\"type\":\"response\""));
    REQUIRE(json_contains(s_last_json, "\"requestId\":\"cap-1\""));
    REQUIRE(json_contains(s_last_json, "\"success\":true"));

    // Devtools flag
    REQUIRE(json_contains(s_last_json, "\"devtools\":true"));

    // Channels array with known channel names
    REQUIRE(json_contains(s_last_json, "\"channels\":["));
    REQUIRE(json_contains(s_last_json, "\"name\":\"tick\""));
    REQUIRE(json_contains(s_last_json, "\"name\":\"graph\""));
    REQUIRE(json_contains(s_last_json, "\"name\":\"state\""));
    REQUIRE(json_contains(s_last_json, "\"name\":\"eval\""));
    REQUIRE(json_contains(s_last_json, "\"name\":\"resources\""));
    REQUIRE(json_contains(s_last_json, "\"name\":\"io\""));
    REQUIRE(json_contains(s_last_json, "\"name\":\"protocol\""));
}

// ══════════════════════════════════════════════════════════════════════════
// D2 — configure: echoes channel configuration back
// ══════════════════════════════════════════════════════════════════════════

TEST_CASE("D2 configure response echoes channel modes",
          "[contract][devtools]")
{
    dt::init(nullptr);
    s_last_json.clear();

    const char* msg =
        R"({"type":"debug","action":"configure","tick":"stream","requestId":"cfg-1"})";

    bool handled = dt::handle_debug_message(msg, strlen(msg), capture_write);

    REQUIRE(handled);
    REQUIRE_FALSE(s_last_json.empty());

    // Response envelope
    REQUIRE(json_contains(s_last_json, "\"type\":\"response\""));
    REQUIRE(json_contains(s_last_json, "\"requestId\":\"cfg-1\""));
    REQUIRE(json_contains(s_last_json, "\"success\":true"));

    // Echoed channels object should contain the tick channel set to "stream"
    REQUIRE(json_contains(s_last_json, "\"channels\":{"));
    REQUIRE(json_contains(s_last_json, "\"tick\":\"stream\""));
}

// ══════════════════════════════════════════════════════════════════════════
// D3 — query tick: returns data with tick_count and phases
// ══════════════════════════════════════════════════════════════════════════

TEST_CASE("D3 query tick channel returns tick data",
          "[contract][devtools]")
{
    dt::init(nullptr);

    // Run a few tick cycles to populate tick profiling data
    for (int i = 0; i < 5; ++i) {
        dt::tick_begin();
        dt::mark("test_phase");
        dt::tick_end();
    }

    s_last_json.clear();

    const char* msg =
        R"({"type":"debug","action":"query","channel":"tick","requestId":"q-tick-1"})";

    bool handled = dt::handle_debug_message(msg, strlen(msg), capture_write);

    REQUIRE(handled);
    REQUIRE_FALSE(s_last_json.empty());

    // Response envelope
    REQUIRE(json_contains(s_last_json, "\"type\":\"response\""));
    REQUIRE(json_contains(s_last_json, "\"requestId\":\"q-tick-1\""));
    REQUIRE(json_contains(s_last_json, "\"success\":true"));
    REQUIRE(json_contains(s_last_json, "\"channel\":\"tick\""));

    // Data payload must contain tick profiling fields
    REQUIRE(json_contains(s_last_json, "\"data\":{"));
    REQUIRE(json_contains(s_last_json, "\"tick_count\":"));
    REQUIRE(json_contains(s_last_json, "\"phases\":{"));
}

// ══════════════════════════════════════════════════════════════════════════
// D4 — query unknown channel: returns success:false
// ══════════════════════════════════════════════════════════════════════════

TEST_CASE("D4 query unknown channel returns failure",
          "[contract][devtools]")
{
    dt::init(nullptr);
    s_last_json.clear();

    const char* msg =
        R"({"type":"debug","action":"query","channel":"nonexistent","requestId":"q-bad-1"})";

    bool handled = dt::handle_debug_message(msg, strlen(msg), capture_write);

    REQUIRE(handled);
    REQUIRE_FALSE(s_last_json.empty());

    // Response envelope
    REQUIRE(json_contains(s_last_json, "\"type\":\"response\""));
    REQUIRE(json_contains(s_last_json, "\"requestId\":\"q-bad-1\""));
    REQUIRE(json_contains(s_last_json, "\"success\":false"));
    REQUIRE(json_contains(s_last_json, "\"error\":\"unknown channel\""));
}

// ══════════════════════════════════════════════════════════════════════════
// D5 — status: returns current channel modes
// ══════════════════════════════════════════════════════════════════════════

TEST_CASE("D5 status response returns current channel modes",
          "[contract][devtools]")
{
    dt::init(nullptr);

    // First configure a channel so we can verify the status reflects it
    {
        s_last_json.clear();
        const char* cfg =
            R"({"type":"debug","action":"configure","tick":"poll","requestId":"cfg-pre"})";
        dt::handle_debug_message(cfg, strlen(cfg), capture_write);
    }

    s_last_json.clear();

    const char* msg =
        R"({"type":"debug","action":"status","requestId":"st-1"})";

    bool handled = dt::handle_debug_message(msg, strlen(msg), capture_write);

    REQUIRE(handled);
    REQUIRE_FALSE(s_last_json.empty());

    // Response envelope
    REQUIRE(json_contains(s_last_json, "\"type\":\"response\""));
    REQUIRE(json_contains(s_last_json, "\"requestId\":\"st-1\""));
    REQUIRE(json_contains(s_last_json, "\"success\":true"));

    // Channels object with current modes
    REQUIRE(json_contains(s_last_json, "\"channels\":{"));
    // tick was set to poll in the configure step above
    REQUIRE(json_contains(s_last_json, "\"tick\":\"poll\""));

    // streamRateHz field
    REQUIRE(json_contains(s_last_json, "\"streamRateHz\":"));
}

// ══════════════════════════════════════════════════════════════════════════
// D6 — unknown action: returns false (unhandled)
// ══════════════════════════════════════════════════════════════════════════

TEST_CASE("D6 unknown action returns false (unhandled)",
          "[contract][devtools]")
{
    dt::init(nullptr);
    s_last_json.clear();

    const char* msg =
        R"({"type":"debug","action":"bogus","requestId":"x-1"})";

    bool handled = dt::handle_debug_message(msg, strlen(msg), capture_write);

    REQUIRE_FALSE(handled);
    // No response should have been written for an unknown action
    REQUIRE(s_last_json.empty());
}

// ══════════════════════════════════════════════════════════════════════════
// D7 — resources: returns memory watermarks and engine capacity ratios
// ══════════════════════════════════════════════════════════════════════════

TEST_CASE("D7 resources response exposes memory and capacity telemetry",
          "[contract][devtools]")
{
    sig::SignalEngine engine;
    engine.init_defaults();
    dt::init(&engine);
    s_last_json.clear();

    const char* msg =
        R"({"type":"debug","action":"query","channel":"resources","requestId":"q-res-1"})";

    bool handled = dt::handle_debug_message(msg, strlen(msg), capture_write);

    REQUIRE(handled);
    REQUIRE(json_contains(s_last_json, "\"success\":true"));
    REQUIRE(json_contains(s_last_json, "\"channel\":\"resources\""));
    REQUIRE(json_contains(s_last_json, "\"heap_free\":"));
    REQUIRE(json_contains(s_last_json, "\"heap_min_free\":"));
    REQUIRE(json_contains(s_last_json,
                          "\"core0_stack_margin_intact\":true"));
    REQUIRE(json_contains(s_last_json, "\"core0_stack\":{"));
    REQUIRE(json_contains(s_last_json, "\"initialized\":false"));
    REQUIRE(json_contains(s_last_json, "\"nodes\":{"));
    REQUIRE(json_contains(s_last_json, "\"arena\":{"));
    REQUIRE(json_contains(s_last_json, "\"cells\":{"));
    REQUIRE(json_contains(s_last_json, "\"data_entries\":{"));
    REQUIRE(json_contains(s_last_json, "\"state_slots\":{"));
    REQUIRE(json_contains(s_last_json, "\"live_slots\":{"));
    REQUIRE(json_contains(s_last_json, "\"synth_declarations\":{"));
    REQUIRE(json_contains(s_last_json, "\"synth_controls\":{"));
    REQUIRE(json_contains(s_last_json, "\"used\":"));
    REQUIRE(json_contains(s_last_json, "\"capacity\":"));
}

TEST_CASE("D8 eval query exposes target compile duration and outcome counts",
          "[contract][devtools]")
{
    dt::init(nullptr);
    dt::eval_begin();
    dt::eval_end(true);
    dt::eval_begin();
    dt::eval_end(false);
    s_last_json.clear();

    const char* msg =
        R"({"type":"debug","action":"query","channel":"eval","requestId":"q-eval-1"})";
    REQUIRE(dt::handle_debug_message(msg, strlen(msg), capture_write));

    REQUIRE(json_contains(s_last_json, "\"success\":true"));
    REQUIRE(json_contains(s_last_json, "\"channel\":\"eval\""));
    REQUIRE(json_contains(s_last_json, "\"last_us\":"));
    REQUIRE(json_contains(s_last_json, "\"max_us\":"));
    REQUIRE(json_contains(s_last_json, "\"count\":2"));
    REQUIRE(json_contains(s_last_json, "\"error_count\":1"));
    REQUIRE(json_contains(s_last_json, "\"events\":["));
}

TEST_CASE("D9 eval timing query omits the event ring for bounded polling",
          "[contract][devtools]")
{
    dt::init(nullptr);
    dt::eval_begin();
    dt::eval_end(true);
    s_last_json.clear();

    const char* msg =
        R"({"type":"debug","action":"query","channel":"eval","output":"timing","requestId":"q-eval-timing"})";
    REQUIRE(dt::handle_debug_message(msg, strlen(msg), capture_write));
    REQUIRE(json_contains(s_last_json, "\"last_us\":"));
    REQUIRE(json_contains(s_last_json, "\"count\":1"));
    REQUIRE_FALSE(json_contains(s_last_json, "\"events\":"));
}
