// State-identity golden tests.
//
// These tests verify that the StateResourceRegistry and :id keyword system
// preserves state across recompilation, forks state for different identities,
// shares state across operator-compatible changes, and isolates state for
// incompatible resource kinds.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "src/signal_engine/signal_engine.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <initializer_list>

using namespace sig;

namespace {

struct Sample {
    double t;
    double expected;
    double tolerance = 1e-9;
};

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

    explicit GoldenHarness(double bpm = 120.0, int beats_per_bar = 4)
    {
        engine.init_defaults(bpm, beats_per_bar);
    }

    EvalResult eval_result(const std::string& code)
    {
        return eval_cold(code.c_str(), static_cast<uint32_t>(code.size()), engine);
    }

    void eval_ok(const std::string& code)
    {
        EvalResult r = eval_result(code);
        INFO("code: " << code);
        if (r.kind == EvalResult::Error && r.diagnostic_count > 0) {
            INFO("diagnostic: " << (r.diagnostics[0].message ? r.diagnostics[0].message : ""));
            INFO("suggestion: " << (r.diagnostics[0].suggestion ? r.diagnostics[0].suggestion : ""));
        }
        REQUIRE(r.kind != EvalResult::Error);
        engine.pool.rebuild_execution_order();
    }

    void assign_ok(const char* output, const char* expr)
    {
        eval_ok(std::string("(") + output + " " + expr + ")");
    }

    uint16_t output_index(const char* output_name)
    {
        SymbolID sym = internSymbol(output_name);
        uint16_t idx = GraphBuilder::resolve_output_index(sym);
        REQUIRE(idx != NODE_NONE);
        return idx;
    }

    double sample(const char* output_name, double t)
    {
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

    double tick(const char* output_name, double t)
    {
        if (t != prev_t) {
            state_committed_this_step = false;
        }
        double value = sample(output_name, t);
        commit_outputs(engine.pool, outputs);
        has_ticked = true;
        prev_t = t;
        return value;
    }

    std::vector<double> tick_sequence(const char* output,
                                      std::initializer_list<double> times)
    {
        std::vector<double> result;
        result.reserve(times.size());
        for (double t_val : times) {
            result.push_back(tick(output, t_val));
        }
        return result;
    }
};

} // namespace

// ============================================================================
// Test 1: Reorder with same :id preserves state
// ============================================================================

TEST_CASE("State identity: reorder with same :id preserves state",
          "[golden][state_identity]") {

    GoldenHarness h;
    h.eval_ok("(a1 (phasor 1 :id \"p\"))");

    // Tick several times to accumulate phase.
    // phasor(1) at dt=0.01 increments phase by 0.01 each tick.
    h.tick("a1", 0.0);
    h.tick("a1", 0.01);
    h.tick("a1", 0.02);
    h.tick("a1", 0.03);
    h.tick("a1", 0.04);

    // Read the accumulated phase from the state slot directly.
    // After 4 increments of dt=0.01 at freq=1, phase should be ~0.04.
    double phase_before = h.engine.pool.state_values[0];
    INFO("phase_before: " << phase_before);
    REQUIRE(phase_before > 0.01);  // definitely accumulated

    // Recompile the exact same expression — state must survive.
    h.eval_ok("(a1 (phasor 1 :id \"p\"))");

    double phase_after = h.engine.pool.state_values[0];
    INFO("phase_after: " << phase_after);
    REQUIRE(phase_after == Approx(phase_before).margin(1e-12));

    // Verify the phasor continues from where it was — the next tick should
    // produce a value close to phase_before (not reset to 0).
    double val = h.tick("a1", 0.05);
    REQUIRE(val > phase_before * 0.5);  // not reset to zero
    REQUIRE(val == Approx(phase_before + 0.01).margin(1e-6));  // advanced by one dt
}

// ============================================================================
// Test 2: Different :id forks state
// ============================================================================

TEST_CASE("State identity: different :id forks state",
          "[golden][state_identity]") {

    GoldenHarness h;

    // Two phasors with different :ids should get different registry entries.
    h.eval_ok("(a1 (phasor 1 :id \"alpha\"))");
    h.eval_ok("(a2 (phasor 2 :id \"beta\"))");

    // Registry should have at least 2 entries.
    REQUIRE(h.engine.registry.entry_count >= 2);

    // Tick to accumulate different phases.
    h.tick("a1", 0.0);
    h.tick("a1", 0.01);
    h.tick("a2", 0.0);
    h.tick("a2", 0.01);

    // The two phasors have different frequencies, so state values should differ.
    // Find the slots for each :id.
    uint16_t slot_alpha = NODE_NONE;
    uint16_t slot_beta = NODE_NONE;
    for (uint16_t i = 0; i < h.engine.registry.entry_count; i++) {
        auto& e = h.engine.registry.entries[i];
        if (e.key.state_id == internSymbol("alpha") &&
            e.key.kind == ResourceKind::OscillatorPhase) {
            slot_alpha = e.slot_index;
        }
        if (e.key.state_id == internSymbol("beta") &&
            e.key.kind == ResourceKind::OscillatorPhase) {
            slot_beta = e.slot_index;
        }
    }

    REQUIRE(slot_alpha != NODE_NONE);
    REQUIRE(slot_beta != NODE_NONE);
    REQUIRE(slot_alpha != slot_beta);

    // The two slots hold independent values — verify they are distinct.
    // Both phasors were ticked with the same times but at different
    // frequencies, so their accumulated phases must differ.
    REQUIRE(h.engine.pool.state_values[slot_alpha] !=
            h.engine.pool.state_values[slot_beta]);
}

// ============================================================================
// Test 3: Operator-compatible change preserves phase
// ============================================================================

TEST_CASE("State identity: operator-compatible change preserves phase",
          "[golden][state_identity]") {

    GoldenHarness h;

    // saw and tri-osc both use ResourceKind::OscillatorPhase, so sharing
    // an :id between them should resolve to the same state slot.
    h.eval_ok("(a1 (saw 1 :id \"x\"))");

    // Accumulate phase.
    h.tick("a1", 0.0);
    h.tick("a1", 0.01);
    h.tick("a1", 0.02);
    h.tick("a1", 0.03);

    // Record the accumulated phase.
    // Find the slot for :id "x" with OscillatorPhase kind.
    uint16_t slot = NODE_NONE;
    for (uint16_t i = 0; i < h.engine.registry.entry_count; i++) {
        auto& e = h.engine.registry.entries[i];
        if (e.key.state_id == internSymbol("x") &&
            e.key.kind == ResourceKind::OscillatorPhase) {
            slot = e.slot_index;
            break;
        }
    }
    REQUIRE(slot != NODE_NONE);

    double phase_before = h.engine.pool.state_values[slot];
    INFO("phase_before: " << phase_before);
    REQUIRE(phase_before > 0.0);

    // Recompile as tri-osc with the same :id.
    // The OscillatorPhase slot should be reused (same key).
    h.eval_ok("(a1 (tri-osc 1 :id \"x\"))");

    double phase_after = h.engine.pool.state_values[slot];
    INFO("phase_after: " << phase_after);
    REQUIRE(phase_after == Approx(phase_before).margin(1e-12));
}

// ============================================================================
// Test 4: Incompatible change gets separate resources
// ============================================================================

TEST_CASE("State identity: incompatible change gets separate resources",
          "[golden][state_identity]") {

    GoldenHarness h;

    // phasor uses ResourceKind::OscillatorPhase; count uses Counter +
    // TriggerMemory + ResetLatch. Even with the same :id "x", they
    // should resolve to different registry entries / slots.
    h.eval_ok("(a1 (phasor 1 :id \"x\"))");

    uint16_t entries_after_phasor = h.engine.registry.entry_count;
    INFO("entries after phasor: " << entries_after_phasor);
    REQUIRE(entries_after_phasor >= 1);

    // count with same :id "x" — needs Counter, TriggerMemory, ResetLatch.
    // Use a1 on a different output to avoid overwriting.
    // We compile as a separate output so both co-exist.
    h.eval_ok("(a2 (count (sqr beat) :id \"x\"))");

    uint16_t entries_after_count = h.engine.registry.entry_count;
    INFO("entries after count: " << entries_after_count);

    // count allocates 3 slots (Counter, TriggerMemory, ResetLatch).
    // phasor allocated 1 (OscillatorPhase). Total should be at least 4.
    REQUIRE(entries_after_count >= entries_after_phasor + 3);

    // Verify the phasor slot is distinct from every count slot.
    uint16_t phasor_slot = NODE_NONE;
    std::vector<uint16_t> count_slots;

    for (uint16_t i = 0; i < h.engine.registry.entry_count; i++) {
        auto& e = h.engine.registry.entries[i];
        if (e.key.state_id == internSymbol("x")) {
            if (e.key.kind == ResourceKind::OscillatorPhase) {
                phasor_slot = e.slot_index;
            } else {
                count_slots.push_back(e.slot_index);
            }
        }
    }

    REQUIRE(phasor_slot != NODE_NONE);
    REQUIRE(count_slots.size() == 3);

    for (auto s : count_slots) {
        REQUIRE(s != phasor_slot);
    }
}

// ============================================================================
// Test 5: Init not replayed on recompile
// ============================================================================

TEST_CASE("State identity: init not replayed on recompile",
          "[golden][state_identity]") {

    GoldenHarness h;

    // Compile a phasor with :phase 0.0 and :id "p", then accumulate.
    h.eval_ok("(a1 (phasor 2 :id \"p\" :phase 0.0))");

    h.tick("a1", 0.0);
    h.tick("a1", 0.01);
    h.tick("a1", 0.02);
    h.tick("a1", 0.03);
    h.tick("a1", 0.04);

    // Phase should have accumulated significantly (freq=2, 4 ticks of dt=0.01).
    double phase_before = h.engine.pool.state_values[0];
    INFO("phase_before: " << phase_before);
    REQUIRE(phase_before > 0.01);

    // Recompile the same expression — :phase 0.0 is an init hint, NOT a
    // reset command. The registry should find the existing slot and skip
    // re-initialization.
    h.eval_ok("(a1 (phasor 2 :id \"p\" :phase 0.0))");

    double phase_after = h.engine.pool.state_values[0];
    INFO("phase_after: " << phase_after);
    REQUIRE(phase_after == Approx(phase_before).margin(1e-12));
}

// ============================================================================
// Test 6: Anonymous slots still work
// ============================================================================

TEST_CASE("State identity: anonymous slots still work",
          "[golden][state_identity]") {

    GoldenHarness h;

    // No :id — should allocate state slots via the old anonymous path.
    h.eval_ok("(a1 (phasor 1))");

    REQUIRE(h.engine.pool.state_slot_count >= 1);

    // Tick several times and verify the phasor accumulates phase.
    h.tick("a1", 0.0);
    h.tick("a1", 0.01);
    h.tick("a1", 0.02);
    h.tick("a1", 0.03);

    // After multiple ticks at freq=1, state should have accumulated.
    double phase = h.engine.pool.state_values[0];
    INFO("accumulated anonymous phase: " << phase);
    REQUIRE(phase > 0.01);

    // Verify tick produces monotonically increasing values.
    double v3 = h.tick("a1", 0.04);
    double v4 = h.tick("a1", 0.05);
    REQUIRE(v4 > v3);
}

// ============================================================================
// Test 7: useq-clear resets registry
// ============================================================================

TEST_CASE("State identity: useq-clear resets registry",
          "[golden][state_identity]") {

    GoldenHarness h;

    // Build up some registry state.
    h.eval_ok("(a1 (phasor 1 :id \"p1\"))");
    h.eval_ok("(a2 (phasor 2 :id \"p2\"))");

    // Tick to accumulate.
    h.tick("a1", 0.0);
    h.tick("a1", 0.01);
    h.tick("a2", 0.0);
    h.tick("a2", 0.01);

    REQUIRE(h.engine.registry.entry_count >= 2);
    REQUIRE(h.engine.pool.state_slot_count >= 2);

    // useq-clear should reset the registry.
    h.eval_ok("(useq-clear)");

    REQUIRE(h.engine.registry.entry_count == 0);
}

// ============================================================================
// Additional: Named :id across multiple recompiles preserves continuity
// ============================================================================

TEST_CASE("State identity: multiple recompiles with same :id are stable",
          "[golden][state_identity]") {

    GoldenHarness h;
    h.eval_ok("(a1 (phasor 1 :id \"stable\"))");

    // Tick 10 times.
    for (int i = 0; i < 10; i++) {
        h.tick("a1", i * 0.01);
    }

    double phase_a = h.engine.pool.state_values[0];

    // Recompile 5 times. Phase must not reset.
    for (int i = 0; i < 5; i++) {
        h.eval_ok("(a1 (phasor 1 :id \"stable\"))");
        double phase_now = h.engine.pool.state_values[0];
        REQUIRE(phase_now == Approx(phase_a).margin(1e-12));
    }
}

// ============================================================================
// Additional: Registry entry_count does not grow on re-resolve
// ============================================================================

TEST_CASE("State identity: re-resolve does not grow entry count",
          "[golden][state_identity]") {

    GoldenHarness h;
    h.eval_ok("(a1 (phasor 1 :id \"re\"))");

    uint16_t count_after_first = h.engine.registry.entry_count;

    // Recompile same expression multiple times.
    h.eval_ok("(a1 (phasor 1 :id \"re\"))");
    h.eval_ok("(a1 (phasor 1 :id \"re\"))");
    h.eval_ok("(a1 (phasor 1 :id \"re\"))");

    REQUIRE(h.engine.registry.entry_count == count_after_first);
}

// ============================================================================
// Projection fork: simulate save/restore cycle and verify invariants
// ============================================================================

TEST_CASE("State identity: projection fork preserves live state",
          "[golden][state_identity][projection]") {
    GoldenHarness h;

    h.eval_ok("(a1 (phasor 1 :id \"proj-p\"))");
    h.tick("a1", 0.0);
    h.tick("a1", 0.01);
    h.tick("a1", 0.02);

    // Snapshot live state before projection
    double live_state[MAX_STATE_SLOTS];
    memcpy(live_state, h.engine.pool.state_values, sizeof(live_state));
    uint16_t live_slot_count = h.engine.pool.state_slot_count;
    uint16_t live_registry_count = h.engine.registry.entry_count;
    double live_prev_outputs[MAX_OUTPUTS];
    memcpy(live_prev_outputs, h.engine.pool.prev_output_values, sizeof(live_prev_outputs));

    // Simulate projection fork: save → install fork → advance → restore
    // Save
    double saved_state[MAX_STATE_SLOTS];
    uint16_t saved_slot_count = h.engine.pool.state_slot_count;
    StateResourceRegistry saved_registry = h.engine.registry;
    double saved_prev_outputs[MAX_OUTPUTS];
    memcpy(saved_state, h.engine.pool.state_values, sizeof(saved_state));
    memcpy(saved_prev_outputs, h.engine.pool.prev_output_values, sizeof(saved_prev_outputs));

    // Execute several projection samples (advances state in the engine)
    for (int s = 0; s < 10; s++) {
        double t = 0.03 + s * 0.01;
        std::memset(h.outputs, 0, sizeof(h.outputs));
        std::memset(h.workspace, 0, sizeof(h.workspace));
        h.engine.cells.snapshot_values(h.cell_values, MAX_CELLS);

        ExecutionContext ctx;
        ctx.t = t;
        ctx.dt = 0.01;
        ctx.cell_values = h.cell_values;
        ctx.hw_inputs = h.hw_inputs;
        ctx.data_pool = h.engine.cells.data_pool;
        ctx.data_offsets = h.engine.cells.data_offsets;
        ctx.data_lengths = h.engine.cells.data_lengths;
        ctx.prev_outputs = h.engine.pool.prev_output_values;
        ctx.output_values = h.outputs;
        ctx.workspace = h.workspace;
        execute_all_outputs(h.engine.pool, ctx);
        commit_state(h.engine.pool, h.workspace);
        commit_outputs(h.engine.pool, h.outputs);
    }

    // State has been mutated by projection execution
    REQUIRE(h.engine.pool.state_values[0] != live_state[0]);

    // Restore live state
    memcpy(h.engine.pool.state_values, saved_state, sizeof(saved_state));
    h.engine.pool.state_slot_count = saved_slot_count;
    h.engine.registry = saved_registry;
    memcpy(h.engine.pool.prev_output_values, saved_prev_outputs, sizeof(saved_prev_outputs));

    // Verify live state is unchanged
    REQUIRE(h.engine.pool.state_slot_count == live_slot_count);
    REQUIRE(h.engine.registry.entry_count == live_registry_count);
    for (uint16_t i = 0; i < live_slot_count; i++) {
        REQUIRE(h.engine.pool.state_values[i] == Approx(live_state[i]));
    }
    for (uint16_t i = 0; i < MAX_OUTPUTS; i++) {
        REQUIRE(h.engine.pool.prev_output_values[i] == Approx(live_prev_outputs[i]));
    }
}

TEST_CASE("State identity: repeated projection doesn't grow registry",
          "[golden][state_identity][projection]") {
    GoldenHarness h;

    h.eval_ok("(a1 (phasor 1 :id \"rp\"))");
    h.tick("a1", 0.0);

    uint16_t initial_entries = h.engine.registry.entry_count;
    uint16_t initial_slots = h.engine.pool.state_slot_count;

    // Simulate 5 projection fork cycles
    for (int cycle = 0; cycle < 5; cycle++) {
        double saved_state[MAX_STATE_SLOTS];
        uint16_t saved_slot_count = h.engine.pool.state_slot_count;
        memcpy(saved_state, h.engine.pool.state_values, sizeof(saved_state));

        // Advance 3 samples in "fork"
        for (int s = 0; s < 3; s++) {
            double t = 0.01 * (cycle * 3 + s + 1);
            std::memset(h.outputs, 0, sizeof(h.outputs));
            std::memset(h.workspace, 0, sizeof(h.workspace));
            h.engine.cells.snapshot_values(h.cell_values, MAX_CELLS);

            ExecutionContext ctx;
            ctx.t = t;
            ctx.dt = 0.01;
            ctx.cell_values = h.cell_values;
            ctx.hw_inputs = h.hw_inputs;
            ctx.data_pool = h.engine.cells.data_pool;
            ctx.data_offsets = h.engine.cells.data_offsets;
            ctx.data_lengths = h.engine.cells.data_lengths;
            ctx.prev_outputs = h.engine.pool.prev_output_values;
            ctx.output_values = h.outputs;
            ctx.workspace = h.workspace;
            execute_all_outputs(h.engine.pool, ctx);
            commit_state(h.engine.pool, h.workspace);
        }

        // Restore
        memcpy(h.engine.pool.state_values, saved_state, sizeof(saved_state));
        h.engine.pool.state_slot_count = saved_slot_count;
    }

    REQUIRE(h.engine.registry.entry_count == initial_entries);
    REQUIRE(h.engine.pool.state_slot_count == initial_slots);
}
