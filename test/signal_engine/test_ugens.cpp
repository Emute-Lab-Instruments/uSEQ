// UGen tests: phasor, lfo, slew, one-pole, env-follow, sah, noise, toggle, count
//
// Tests exercise state-slot allocation, cross-tick accumulation, keyword
// parsing, and alias resolution.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "src/signal_engine/signal_engine.h"
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <initializer_list>

using namespace sig;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

struct GoldenHarness {
    SignalEngine engine;
    double cell_values[MAX_CELLS] = {};
    double hw_inputs[32] = {};
    double outputs[MAX_OUTPUTS] = {};
    double workspace[MAX_TOTAL_NODES] = {};
    double prev_t = 0.0;
    double last_dt = 0.0;
    bool has_ticked = false;
    bool state_committed_this_step = false;

    explicit GoldenHarness(double bpm = 120.0, int beats_per_bar = 4) {
        engine.init_defaults(bpm, beats_per_bar);
    }

    EvalResult eval_result(const std::string& code) {
        return eval_cold(code.c_str(), static_cast<uint32_t>(code.size()), engine);
    }

    void eval_ok(const std::string& code) {
        EvalResult r = eval_result(code);
        INFO("code: " << code);
        if (r.kind == EvalResult::Error && r.diagnostic_count > 0) {
            INFO("diagnostic: " << (r.diagnostics[0].message ? r.diagnostics[0].message : ""));
            INFO("suggestion: " << (r.diagnostics[0].suggestion ? r.diagnostics[0].suggestion : ""));
        }
        REQUIRE(r.kind != EvalResult::Error);
        engine.pool.rebuild_execution_order();
    }

    void assign_ok(const char* output, const char* expr) {
        eval_ok(std::string("(") + output + " " + expr + ")");
    }

    uint16_t output_index(const char* output_name) {
        SymbolID sym = internSymbol(output_name);
        uint16_t idx = GraphBuilder::resolve_output_index(sym);
        REQUIRE(idx != NODE_NONE);
        return idx;
    }

    double sample(const char* output_name, double t) {
        std::memset(outputs, 0, sizeof(outputs));
        std::memset(workspace, 0, sizeof(workspace));
        engine.cells.snapshot_values(cell_values, MAX_CELLS);

        last_dt = t - prev_t;

        if (engine.pool.state_slot_count > 0 &&
            has_ticked && t != prev_t && !state_committed_this_step)
        {
            ExecutionContext state_ctx;
            state_ctx.t = t;
            state_ctx.dt = last_dt;
            state_ctx.cell_values = cell_values;
            state_ctx.hw_inputs = hw_inputs;
            state_ctx.data_pool = engine.cells.data_pool;
            state_ctx.data_offsets = engine.cells.data_offsets;
            state_ctx.data_lengths = engine.cells.data_lengths;
            state_ctx.prev_outputs = engine.pool.prev_output_values;
            state_ctx.output_values = outputs;
            state_ctx.workspace = workspace;
            execute_all_outputs(engine.pool, state_ctx);
            commit_state(engine.pool, workspace);
            state_committed_this_step = true;
            std::memset(workspace, 0, sizeof(workspace));
            std::memset(outputs, 0, sizeof(outputs));
        }

        ExecutionContext ctx;
        ctx.t = t;
        ctx.dt = last_dt;
        ctx.cell_values = cell_values;
        ctx.hw_inputs = hw_inputs;
        ctx.data_pool = engine.cells.data_pool;
        ctx.data_offsets = engine.cells.data_offsets;
        ctx.data_lengths = engine.cells.data_lengths;
        ctx.prev_outputs = engine.pool.prev_output_values;
        ctx.output_values = outputs;
        ctx.workspace = workspace;
        execute_all_outputs(engine.pool, ctx);

        return outputs[output_index(output_name)];
    }

    double tick(const char* output_name, double t) {
        if (t != prev_t) state_committed_this_step = false;
        double value = sample(output_name, t);
        commit_outputs(engine.pool, outputs);
        has_ticked = true;
        prev_t = t;
        return value;
    }
};

} // anonymous namespace

// ── phasor ─────────────────────────────────────────────────────────────────

TEST_CASE("UGen: phasor produces phase ramp [0,1)", "[ugens][phasor]") {
    GoldenHarness h;
    h.assign_ok("a1", "(phasor 1.0)");

    REQUIRE(h.tick("a1", 0.0) == Approx(0.0));
    REQUIRE(h.tick("a1", 0.1) == Approx(0.1));
    REQUIRE(h.tick("a1", 0.2) == Approx(0.2));
}

TEST_CASE("UGen: phasor wraps at 1.0", "[ugens][phasor]") {
    GoldenHarness h;
    h.assign_ok("a1", "(phasor 1.0)");

    // Tick through one full cycle (10 × dt=0.1 = 1.0s)
    for (int i = 0; i < 10; i++) {
        h.tick("a1", i * 0.1);
    }
    // After 1.0s: phase = frac(1.0) = 0.0
    REQUIRE(h.tick("a1", 1.0) == Approx(0.0).margin(1e-10));
}

TEST_CASE("UGen: phasor with :phase init", "[ugens][phasor]") {
    GoldenHarness h;
    h.assign_ok("a1", "(phasor 1.0 :phase 0.5)");
    REQUIRE(h.tick("a1", 0.0) == Approx(0.5));
}

TEST_CASE("UGen: phasor at 2Hz", "[ugens][phasor]") {
    GoldenHarness h;
    h.assign_ok("a1", "(phasor 2.0)");
    REQUIRE(h.tick("a1", 0.0) == Approx(0.0));
    // dt=0.1, state = frac(0 + 2.0*0.1) = 0.2
    REQUIRE(h.tick("a1", 0.1) == Approx(0.2));
}

// ── lfo ─────────────────────────────────────────────────────────────────────

TEST_CASE("UGen: lfo default wave is sine", "[ugens][lfo]") {
    GoldenHarness h;
    h.assign_ok("a1", "(lfo 1.0)");

    // usin(0) = (sin(0)+1)/2 = 0.5
    REQUIRE(h.tick("a1", 0.0) == Approx(0.5));

    // After dt=0.25: phase = 0.25, usin(0.25) = (sin(2π*0.25)+1)/2 = 1.0
    REQUIRE(h.tick("a1", 0.25) == Approx(1.0));
}

TEST_CASE("UGen: lfo :wave :saw produces phasor output", "[ugens][lfo]") {
    GoldenHarness h;
    h.assign_ok("a1", "(lfo 1.0 :wave :saw)");

    REQUIRE(h.tick("a1", 0.0) == Approx(0.0));
    REQUIRE(h.tick("a1", 0.1) == Approx(0.1));
    REQUIRE(h.tick("a1", 0.2) == Approx(0.2));
}

TEST_CASE("UGen: lfo :wave :tri produces triangle", "[ugens][lfo]") {
    GoldenHarness h;
    h.assign_ok("a1", "(lfo 1.0 :wave :tri)");

    REQUIRE(h.tick("a1", 0.0) == Approx(0.0));  // tri(0) = 0
    // After dt=0.25: phase=0.25, tri(0.25) = 1 - |2*0.25 - 1| = 0.5
    REQUIRE(h.tick("a1", 0.25) == Approx(0.5));
}

TEST_CASE("UGen: lfo :wave :sqr produces square", "[ugens][lfo]") {
    GoldenHarness h;
    h.assign_ok("a1", "(lfo 1.0 :wave :sqr)");

    // phase=0 → frac(0)=0 < 0.5 → 1
    REQUIRE(h.tick("a1", 0.0) == Approx(1.0));
    // phase=0.1 → < 0.5 → 1
    REQUIRE(h.tick("a1", 0.1) == Approx(1.0));
}

TEST_CASE("UGen: lfo :sqr with :pw", "[ugens][lfo]") {
    GoldenHarness h;
    h.assign_ok("a1", "(lfo 1.0 :wave :sqr :pw 0.25)");

    // phase=0 < 0.25 → high
    REQUIRE(h.tick("a1", 0.0) == Approx(1.0));
    // phase=0.1 < 0.25 → still high
    REQUIRE(h.tick("a1", 0.1) == Approx(1.0));
    // phase=0.3 > 0.25 → low
    h.tick("a1", 0.2);
    REQUIRE(h.tick("a1", 0.3) == Approx(0.0));
}

// ── lfo aliases ─────────────────────────────────────────────────────────────

TEST_CASE("UGen: osc is alias for sine lfo", "[ugens][lfo][alias]") {
    GoldenHarness h;
    h.assign_ok("a1", "(osc 1.0)");
    REQUIRE(h.tick("a1", 0.0) == Approx(0.5)); // usin(0) = 0.5
}

TEST_CASE("UGen: saw is alias for saw lfo", "[ugens][lfo][alias]") {
    GoldenHarness h;
    h.assign_ok("a1", "(saw 1.0)");
    REQUIRE(h.tick("a1", 0.0) == Approx(0.0));
    REQUIRE(h.tick("a1", 0.1) == Approx(0.1));
}

TEST_CASE("UGen: tri-osc is alias for tri lfo", "[ugens][lfo][alias]") {
    GoldenHarness h;
    h.assign_ok("a1", "(tri-osc 1.0)");
    REQUIRE(h.tick("a1", 0.0) == Approx(0.0)); // tri(0) = 0
}

TEST_CASE("UGen: sqr-osc is alias for sqr lfo", "[ugens][lfo][alias]") {
    GoldenHarness h;
    h.assign_ok("a1", "(sqr-osc 1.0)");
    REQUIRE(h.tick("a1", 0.0) == Approx(1.0)); // phase < 0.5 → 1
}

// ── slew ────────────────────────────────────────────────────────────────────

TEST_CASE("UGen: slew limits rate of change", "[ugens][slew]") {
    GoldenHarness h;
    // Use ain1 (bare symbol, not function call) as input
    h.assign_ok("a1", "(slew ain1 10.0)");

    h.hw_inputs[2] = 0.0; // ain1 = input index 2
    REQUIRE(h.tick("a1", 0.0) == Approx(0.0));

    // Jump target to 1.0, dt=0.1, max_step = 10*0.1 = 1.0
    h.hw_inputs[2] = 1.0;
    REQUIRE(h.tick("a1", 0.1) == Approx(1.0)); // delta=1.0 ≤ step=1.0
}

TEST_CASE("UGen: slew clamps when target changes too fast", "[ugens][slew]") {
    GoldenHarness h;
    h.assign_ok("a1", "(slew ain1 1.0)");

    h.hw_inputs[2] = 0.0;
    REQUIRE(h.tick("a1", 0.0) == Approx(0.0));

    // Target jumps to 10, max_step = 1.0*0.1 = 0.1
    h.hw_inputs[2] = 10.0;
    double val = h.tick("a1", 0.1);
    REQUIRE(val == Approx(0.1));

    val = h.tick("a1", 0.2);
    REQUIRE(val == Approx(0.2));
}

// ── one-pole ────────────────────────────────────────────────────────────────

TEST_CASE("UGen: one-pole low-pass filter", "[ugens][one-pole]") {
    GoldenHarness h;
    h.assign_ok("a1", "(one-pole ain1 10.0)");

    h.hw_inputs[2] = 1.0;
    REQUIRE(h.tick("a1", 0.0) == Approx(0.0)); // initial state = 0

    // dt=0.01, alpha = min(1, 2*pi*10*0.01) ≈ 0.628
    double val = h.tick("a1", 0.01);
    double alpha = std::min(1.0, 2.0 * M_PI * 10.0 * 0.01);
    REQUIRE(val == Approx(alpha).margin(0.01));
}

// ── env-follow ──────────────────────────────────────────────────────────────

TEST_CASE("UGen: env-follow tracks absolute input", "[ugens][env-follow]") {
    GoldenHarness h;
    h.assign_ok("a1", "(env-follow ain1 100.0 10.0)");

    h.hw_inputs[2] = 1.0;
    REQUIRE(h.tick("a1", 0.0) == Approx(0.0));

    // dt=0.01, attack=100, a_coeff = min(1, 100*0.01) = 1.0
    // diff = 1.0 - 0 = 1.0, new = 0 + 1.0*1.0 = 1.0
    double val = h.tick("a1", 0.01);
    REQUIRE(val == Approx(1.0));
}

TEST_CASE("UGen: envelope-follower is alias", "[ugens][env-follow][alias]") {
    GoldenHarness h;
    h.assign_ok("a1", "(envelope-follower ain1 100.0 10.0)");
    h.hw_inputs[2] = 0.5;
    REQUIRE(h.tick("a1", 0.0) == Approx(0.0));
}

// ── sah (sample-and-hold) ───────────────────────────────────────────────────

TEST_CASE("UGen: sah samples on rising edge", "[ugens][sah]") {
    GoldenHarness h;
    h.assign_ok("a1", "(sah ain1 ain2)");

    h.hw_inputs[2] = 0.42; // input
    h.hw_inputs[3] = 0.0;  // trigger low
    REQUIRE(h.tick("a1", 0.0) == Approx(0.0)); // initial = 0, no trigger

    // Trigger rises: ain2 goes from 0 → 1 (> 0.5)
    h.hw_inputs[3] = 1.0;
    double val = h.tick("a1", 0.1);
    REQUIRE(val == Approx(0.42)); // sampled the input

    // Trigger stays high, input changes — should hold
    h.hw_inputs[2] = 0.99;
    val = h.tick("a1", 0.2);
    REQUIRE(val == Approx(0.42)); // still holding

    // Trigger goes low, then high again with new value
    h.hw_inputs[3] = 0.0;
    h.tick("a1", 0.3);
    h.hw_inputs[2] = 0.77;
    h.hw_inputs[3] = 1.0;
    val = h.tick("a1", 0.4);
    REQUIRE(val == Approx(0.77)); // sampled new value
}

TEST_CASE("UGen: latch is alias for sah", "[ugens][sah][alias]") {
    GoldenHarness h;
    h.assign_ok("a1", "(latch ain1 ain2)");

    h.hw_inputs[2] = 0.5;
    h.hw_inputs[3] = 0.0;
    h.tick("a1", 0.0);

    h.hw_inputs[3] = 1.0;
    REQUIRE(h.tick("a1", 0.1) == Approx(0.5));
}

// ── noise ───────────────────────────────────────────────────────────────────

TEST_CASE("UGen: noise produces values in [-1,1]", "[ugens][noise]") {
    GoldenHarness h;
    h.assign_ok("a1", "(noise)");

    bool found_positive = false;
    bool found_negative = false;

    for (int i = 0; i < 20; i++) {
        double val = h.tick("a1", i * 0.01);
        REQUIRE(val >= -1.0);
        REQUIRE(val <= 1.0);
        if (val > 0.1) found_positive = true;
        if (val < -0.1) found_negative = true;
    }

    REQUIRE(found_positive);
    REQUIRE(found_negative);
}

TEST_CASE("UGen: noise is deterministic", "[ugens][noise]") {
    GoldenHarness h1, h2;
    h1.assign_ok("a1", "(noise)");
    h2.assign_ok("a1", "(noise)");

    for (int i = 0; i < 10; i++) {
        double v1 = h1.tick("a1", i * 0.01);
        double v2 = h2.tick("a1", i * 0.01);
        REQUIRE(v1 == Approx(v2));
    }
}

// ── toggle ──────────────────────────────────────────────────────────────────

TEST_CASE("UGen: toggle flips on rising edge", "[ugens][toggle]") {
    GoldenHarness h;
    h.assign_ok("a1", "(toggle ain1)");

    h.hw_inputs[2] = 0.0;
    REQUIRE(h.tick("a1", 0.0) == Approx(0.0)); // initial = 0

    // Trigger rises → toggle: 0 → 1
    h.hw_inputs[2] = 1.0;
    REQUIRE(h.tick("a1", 0.1) == Approx(1.0));

    // Trigger stays high → no change
    REQUIRE(h.tick("a1", 0.2) == Approx(1.0));

    // Trigger goes low → no change
    h.hw_inputs[2] = 0.0;
    REQUIRE(h.tick("a1", 0.3) == Approx(1.0));

    // Trigger rises again → toggle: 1 → 0
    h.hw_inputs[2] = 1.0;
    REQUIRE(h.tick("a1", 0.4) == Approx(0.0));

    // And again → 0 → 1
    h.hw_inputs[2] = 0.0;
    h.tick("a1", 0.5);
    h.hw_inputs[2] = 1.0;
    REQUIRE(h.tick("a1", 0.6) == Approx(1.0));
}

// ── count ───────────────────────────────────────────────────────────────────

TEST_CASE("UGen: count increments on trigger", "[ugens][count]") {
    GoldenHarness h;
    h.assign_ok("a1", "(count ain1)");

    h.hw_inputs[2] = 0.0;
    REQUIRE(h.tick("a1", 0.0) == Approx(0.0));

    // Rising edge → count = 1
    h.hw_inputs[2] = 1.0;
    REQUIRE(h.tick("a1", 0.1) == Approx(1.0));

    // Still high → no increment
    REQUIRE(h.tick("a1", 0.2) == Approx(1.0));

    // Low → no increment
    h.hw_inputs[2] = 0.0;
    REQUIRE(h.tick("a1", 0.3) == Approx(1.0));

    // Rising again → count = 2
    h.hw_inputs[2] = 1.0;
    REQUIRE(h.tick("a1", 0.4) == Approx(2.0));
}

TEST_CASE("UGen: count with :reset", "[ugens][count]") {
    GoldenHarness h;
    h.assign_ok("a1", "(count ain1 :reset ain2)");

    h.hw_inputs[2] = 0.0; // trigger
    h.hw_inputs[3] = 0.0; // reset
    h.tick("a1", 0.0);

    // Count up: 2 triggers
    h.hw_inputs[2] = 1.0;
    h.tick("a1", 0.1);
    h.hw_inputs[2] = 0.0;
    h.tick("a1", 0.2);
    h.hw_inputs[2] = 1.0;
    REQUIRE(h.tick("a1", 0.3) == Approx(2.0));

    // Reset with rising edge on ain2
    h.hw_inputs[3] = 1.0;
    REQUIRE(h.tick("a1", 0.4) == Approx(0.0)); // count reset to 0

    // Count again after reset: trigger goes low then high
    h.hw_inputs[2] = 0.0;
    h.hw_inputs[3] = 0.0;
    h.tick("a1", 0.5); // trigger low, reset low
    h.hw_inputs[2] = 1.0;
    REQUIRE(h.tick("a1", 0.6) == Approx(1.0));
}

// ── Classification ──────────────────────────────────────────────────────────

TEST_CASE("Classification: UGens are stateful", "[ugens][classification]") {
    GoldenHarness h;
    h.assign_ok("a1", "(phasor 1.0)");
    REQUIRE(h.engine.pool.output_class[h.output_index("a1")] == OutputClass::Stateful);

    GoldenHarness h2;
    h2.assign_ok("a1", "(lfo 1.0)");
    REQUIRE(h2.engine.pool.output_class[h2.output_index("a1")] == OutputClass::Stateful);

    GoldenHarness h3;
    h3.assign_ok("a1", "(noise)");
    REQUIRE(h3.engine.pool.output_class[h3.output_index("a1")] == OutputClass::Stateful);
}
