// Resource-reclamation regression tests (release audit F3/F4/F5/F8).
//
// Live coding means recompiling the same programs over and over. These tests
// lock in the invariant that recompiles RECLAIM (or reuse) every fixed-pool
// resource they consume — state slots, data tables, graph nodes, and the
// source arena — so that ordinary performance workflows can never exhaust a
// pool and silently kill or revert an output:
//
//   F3: anonymous stateful UGens must reuse state slots across recompiles
//       (structural identity via the StateResourceRegistry), and vector
//       literals must reuse data tables (content-interning in CellStore).
//   F4: on_cell_changed must gc unreachable nodes like every sibling
//       recompile path, or live cell edits exhaust the node pool.
//   F5: source-arena exhaustion must fail the eval with an explicit
//       diagnostic instead of installing a graph whose stored source text is
//       stale (which silently reverts the output on the next recompile).
//   F8: on_cell_changed must refresh the output's dependency list, or an
//       output stops reacting to cells introduced by a redefinition.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "src/signal_engine/signal_engine.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace sig;

namespace {

struct ReclaimHarness {
    SignalEngine engine;

    ReclaimHarness() { engine.init_defaults(120.0, 4); }

    EvalResult eval(const std::string& code) {
        return eval_cold(code.c_str(), (uint32_t)code.size(), engine);
    }

    void eval_ok(const std::string& code) {
        EvalResult r = eval(code);
        INFO("code: " << code);
        if (r.kind == EvalResult::Error && r.diagnostic_count > 0) {
            INFO("diagnostic: "
                 << (r.diagnostics[0].message ? r.diagnostics[0].message : ""));
        }
        REQUIRE(r.kind != EvalResult::Error);
    }

    // Execute one sample at t=0 and return the named output's value.
    double sample(const char* output_name) {
        double cell_values[MAX_CELLS];
        engine.cells.snapshot_values(cell_values, MAX_CELLS);
        double hw_inputs[32] = {};
        double outputs[MAX_OUTPUTS] = {};
        double workspace[MAX_TOTAL_NODES] = {};

        ExecutionContext ctx;
        ctx.t = 0.0;
        ctx.dt = 0.0;
        ctx.cell_values = cell_values;
        ctx.hw_inputs = hw_inputs;
        ctx.data_pool = engine.cells.data_pool;
        ctx.data_offsets = engine.cells.data_offsets;
        ctx.data_lengths = engine.cells.data_lengths;
        ctx.prev_outputs = engine.pool.prev_output_values;
        ctx.output_values = outputs;
        ctx.workspace = workspace;
        execute_all_outputs(engine.pool, ctx);

        SymbolID sym = internSymbol(output_name);
        uint16_t idx = GraphBuilder::resolve_output_index(sym);
        REQUIRE(idx != NODE_NONE);
        return outputs[idx];
    }
};

} // namespace

// ============================================================================
// F3: anonymous stateful UGens reuse state slots across recompiles
// ============================================================================

TEST_CASE("Reclaim: 100 re-evals of an anonymous lfo keep state slots bounded",
          "[reclaim][state_slots]") {
    ReclaimHarness h;

    h.eval_ok("(a1 (lfo 2))");
    uint16_t slots_after_first = h.engine.pool.state_slot_count;
    REQUIRE(slots_after_first >= 1);

    for (int i = 0; i < 100; i++) {
        EvalResult r = h.eval("(a1 (lfo 2))");
        INFO("iteration " << i);
        if (r.kind == EvalResult::Error && r.diagnostic_count > 0) {
            INFO("diagnostic: "
                 << (r.diagnostics[0].message ? r.diagnostics[0].message : ""));
        }
        REQUIRE(r.kind != EvalResult::Error);
    }

    // Recompiling the identical program must resolve to the SAME slots via
    // the registry's structural key — not allocate a fresh slot per eval.
    REQUIRE(h.engine.pool.state_slot_count == slots_after_first);
    REQUIRE(h.engine.pool.outputs[0].valid);
}

TEST_CASE("Reclaim: anonymous state value survives re-eval of identical source",
          "[reclaim][state_slots]") {
    ReclaimHarness h;

    h.eval_ok("(a1 (phasor 1))");
    REQUIRE(h.engine.pool.state_slot_count >= 1);

    // Simulate accumulated phase, then recompile the same program.
    h.engine.pool.state_values[0] = 0.625;
    h.eval_ok("(a1 (phasor 1))");

    // Structural identity: the recompiled graph reads the same slot and the
    // accumulated value is preserved (init values are hints, not resets).
    REQUIRE(h.engine.pool.state_values[0] == Approx(0.625));
}

TEST_CASE("Reclaim: distinct anonymous UGens across outputs get distinct slots",
          "[reclaim][state_slots]") {
    ReclaimHarness h;

    h.eval_ok("(a1 (phasor 1))");
    uint16_t after_a1 = h.engine.pool.state_slot_count;
    h.eval_ok("(a2 (phasor 1))");
    uint16_t after_a2 = h.engine.pool.state_slot_count;

    // Different output => different structural context => independent state.
    REQUIRE(after_a2 > after_a1);

    // But re-evaluating either output stays bounded.
    h.eval_ok("(a1 (phasor 1))");
    h.eval_ok("(a2 (phasor 1))");
    REQUIRE(h.engine.pool.state_slot_count == after_a2);
}

// ============================================================================
// F3 + F4: cell sweeps through on_cell_changed keep tables and nodes bounded
// ============================================================================

TEST_CASE("Reclaim: 300-step cell sweep keeps data tables and nodes bounded",
          "[reclaim][tables][nodes]") {
    ReclaimHarness h;

    h.eval_ok("(define off 0)");
    h.eval_ok("(a1 (+ off (step [1 2 3] beat)))");

    uint8_t tables_after_first = h.engine.cells.data_table_count;
    uint16_t nodes_after_first = h.engine.pool.node_count;
    REQUIRE(tables_after_first >= 1);

    for (int i = 1; i <= 300; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "(define off %d)", i);
        h.eval_ok(buf);

        // Every dependent recompile must reuse the [1 2 3] table
        // (content-interning) instead of appending a duplicate.
        REQUIRE(h.engine.cells.data_table_count == tables_after_first);

        // And on_cell_changed must gc the orphaned old graph. The bound is
        // loose (the changed constant makes node counts wobble by a node or
        // two) but must not grow linearly with edits — pre-fix this reached
        // 434 nodes after 300 edits and 360 (the firmware cap) after ~237.
        REQUIRE(h.engine.pool.node_count <= nodes_after_first + 8);
    }

    // The output still compiles and tracks the swept cell.
    REQUIRE(h.engine.pool.outputs[0].valid);
    REQUIRE(h.sample("a1") == Approx(301.0)); // off=300 + step value 1 at t=0
}

TEST_CASE("Reclaim: anonymous UGen output survives repeated dependency edits",
          "[reclaim][state_slots][nodes]") {
    ReclaimHarness h;

    // A stateful output that depends on a cell: every (define rate N)
    // triggers an on_cell_changed recompile of a graph with an anonymous lfo.
    h.eval_ok("(define rate 1)");
    h.eval_ok("(a1 (lfo rate))");

    uint16_t slots_after_first = h.engine.pool.state_slot_count;
    uint16_t nodes_after_first = h.engine.pool.node_count;

    for (int i = 2; i <= 100; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "(define rate %d)", i);
        h.eval_ok(buf);
    }

    REQUIRE(h.engine.pool.state_slot_count == slots_after_first);
    REQUIRE(h.engine.pool.node_count <= nodes_after_first + 8);
    REQUIRE(h.engine.pool.outputs[0].valid);
}

// ============================================================================
// F5: source-arena exhaustion fails loudly instead of reverting outputs
// ============================================================================

TEST_CASE("Reclaim: arena exhaustion fails the eval and never reverts the output",
          "[reclaim][arena]") {
    ReclaimHarness h;

    h.eval_ok("(define off 1)");
    h.eval_ok("(a1 (+ off 111))");
    REQUIRE(h.sample("a1") == Approx(112.0));

    // Exhaust the arena, then try to install a new program.
    h.engine.arena.write_head = SOURCE_ARENA_SIZE - 4;
    EvalResult r = h.eval("(a1 (+ off 999))");

    // Must be an explicit error, not a silent success...
    REQUIRE(r.kind == EvalResult::Error);
    REQUIRE(r.diagnostic_count >= 1);
    REQUIRE(r.diagnostics[0].message != nullptr);
    REQUIRE(std::string(r.diagnostics[0].message).find("storage") !=
            std::string::npos);

    // ...and the OLD program must still be the active one.
    REQUIRE(h.sample("a1") == Approx(112.0));

    // The killer pre-fix symptom: a later dependency change recompiled the
    // output from stale source text, silently REVERTING it to the rejected
    // program's predecessor with mismatched semantics. Now the old program
    // is still the honestly-active one and follows its dependencies.
    h.eval_ok("(define off 2)");
    REQUIRE(h.sample("a1") == Approx(113.0)); // old program, new off
    REQUIRE(h.engine.pool.outputs[0].valid);
}

TEST_CASE("Reclaim: arena exhaustion fails define/defn/defstate loudly",
          "[reclaim][arena]") {
    ReclaimHarness h;

    h.eval_ok("(define x (+ 1 2))");
    h.engine.arena.write_head = SOURCE_ARENA_SIZE - 2;

    EvalResult r1 = h.eval("(define y (+ 3 4))");
    REQUIRE(r1.kind == EvalResult::Error);

    EvalResult r2 = h.eval("(defn f [a] (+ a 1))");
    REQUIRE(r2.kind == EvalResult::Error);

    EvalResult r3 = h.eval("(defstate c 0 (+ c 1))");
    REQUIRE(r3.kind == EvalResult::Error);

    // x's stored source is untouched and still usable.
    EvalResult get = h.eval("(get-expr x)");
    REQUIRE(get.kind == EvalResult::Text);
}

// ============================================================================
// F8: on_cell_changed refreshes the dependency list
// ============================================================================

TEST_CASE("Reclaim: output tracks cells introduced by a redefinition",
          "[reclaim][deps]") {
    ReclaimHarness h;

    h.eval_ok("(define x 1)");
    h.eval_ok("(define y 10)");
    h.eval_ok("(a1 (* x 2))");
    REQUIRE(h.sample("a1") == Approx(2.0));

    // Redefine x from a number to an expression referencing y. The
    // on_cell_changed recompile of a1 must pick up the NEW dep set {y,...},
    // not keep the stale {x}-era list.
    h.eval_ok("(define x (+ y 1))");
    REQUIRE(h.sample("a1") == Approx(22.0));

    // Pre-fix, this change was invisible to a1 forever.
    h.eval_ok("(define y 20)");
    REQUIRE(h.sample("a1") == Approx(42.0));

    // And it keeps tracking on further edits.
    h.eval_ok("(define y 30)");
    REQUIRE(h.sample("a1") == Approx(62.0));
}
