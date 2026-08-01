// Regression tests for the 2026-07 v1.2.0 audit fixes (A-series findings).
// One TEST_CASE (or section group) per landed finding; see commit messages
// for the finding numbers.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "src/signal_engine/signal_engine.h"
#include "src/modulisp/lisp/symbol_intern.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace sig;

namespace {

struct Harness {
    SignalEngine engine;

    Harness() { engine.init_defaults(120.0, 4); }

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

    // Execute one sample at time t and return the named output's value.
    double sample(int output_index, double t = 0.0, double dt = 0.001) {
        double cell_values[MAX_CELLS];
        engine.cells.snapshot_values(cell_values, MAX_CELLS);
        double hw_inputs[32] = {};
        double outputs[MAX_OUTPUTS] = {};
        double workspace[MAX_TOTAL_NODES] = {};

        ExecutionContext ctx;
        ctx.t = t;
        ctx.dt = dt;
        ctx.cell_values = cell_values;
        ctx.hw_inputs = hw_inputs;
        ctx.data_pool = engine.cells.data_pool;
        ctx.data_offsets = engine.cells.data_offsets;
        ctx.data_lengths = engine.cells.data_lengths;
        ctx.prev_outputs = engine.pool.prev_output_values;
        ctx.output_values = outputs;
        ctx.workspace = workspace;
        execute_all_outputs(engine.pool, ctx);
        return outputs[output_index];
    }

    double tick(int output_index, double t, double dt) {
        double cell_values[MAX_CELLS];
        engine.cells.snapshot_values(cell_values, MAX_CELLS);
        double hw_inputs[32] = {};
        double outputs[MAX_OUTPUTS] = {};
        double workspace[MAX_TOTAL_NODES] = {};
        ExecutionContext ctx{t, dt, cell_values, hw_inputs,
                             engine.cells.data_pool,
                             engine.cells.data_offsets,
                             engine.cells.data_lengths,
                             engine.pool.prev_output_values,
                             outputs, workspace};
        execute_all_outputs(engine.pool, ctx);
        commit_state(engine.pool, workspace);
        commit_outputs(engine.pool, outputs);
        return outputs[output_index];
    }
};

} // namespace

// ── A2: compile failure must not demote the active program (§2.6) ──────────
// failure-model.md §2.6: "Compile-time errors do not consume LKG. A program
// that fails to compile is not promoted, demoted, or substituted; the active
// program is unchanged." The old demotion of outputs[i].valid on compile
// failure also flapped against commit_outputs (which resurrects valid=true),
// corrupting the WASM batch-vis row packing.

TEST_CASE("A2: failed output compile leaves the active program valid",
          "[audit][a2]") {
    Harness h;

    h.eval_ok("(a1 0.75)");
    REQUIRE(h.engine.pool.outputs[0].valid);
    REQUIRE(h.sample(0) == Approx(0.75));

    // A compile failure on the same output...
    EvalResult r = h.eval("(a1 (no-such-fn 1 2))");
    REQUIRE(r.kind == EvalResult::Error);

    // ...leaves the previous program installed, valid, and running.
    REQUIRE(h.engine.pool.outputs[0].valid);
    REQUIRE(h.engine.pool.outputs[0].root_node != NODE_NONE);
    REQUIRE(h.sample(0) == Approx(0.75));
}

TEST_CASE("A2b: rejected graph build restores state, source, and capacity",
          "[audit][transaction]") {
    Harness h;

    h.eval_ok("(define tx-dep 1)");
    h.eval_ok("(a1 (+ tx-dep (phasor 1 :id \"tx-phase\")))");
    h.tick(0, 0.0, 0.0);
    h.tick(0, 0.25, 0.25);
    REQUIRE(h.sample(0, 0.25) == Approx(1.25));

    uint16_t slots_before = h.engine.pool.state_slot_count;
    uint16_t registry_before = h.engine.registry.entry_count;
    uint16_t update_before = h.engine.pool.state_update_roots[0];
    uint16_t live_before = h.engine.pool.live_slot_count;
    uint8_t tables_before = h.engine.cells.data_table_count;

    EvalResult rejected = h.eval(
        "(a1 (+ tx-dep (phasor 2 :id \"tx-phase\") "
        "(live-edit 0.5 :id \"rejected-slot\" :min 0 :max 1) "
        "[9 8 7] no-such-name))");
    REQUIRE(rejected.kind == EvalResult::Error);
    REQUIRE(h.engine.pool.state_slot_count == slots_before);
    REQUIRE(h.engine.registry.entry_count == registry_before);
    REQUIRE(h.engine.pool.state_update_roots[0] == update_before);
    REQUIRE(h.engine.pool.live_slot_count == live_before);
    REQUIRE(h.engine.cells.data_table_count == tables_before);

    // The rejected 2-Hz update did not steal the live 1-Hz state resource.
    h.tick(0, 0.5, 0.25);
    REQUIRE(h.sample(0, 0.5) == Approx(1.5));

    // The failed source was not published: dependency recompilation uses the
    // old valid expression and remains healthy.
    h.eval_ok("(define tx-dep 2)");
    REQUIRE(h.engine.pool.outputs[0].valid);
    REQUIRE(h.sample(0, 0.5) == Approx(2.5));
}

TEST_CASE("A2c: repeated rejected stateful builds do not exhaust state",
          "[audit][transaction][capacity]") {
    Harness h;
    for (int i = 0; i < (int)MAX_STATE_SLOTS + 4; ++i) {
        char code[160];
        snprintf(code, sizeof(code),
                 "(a1 (+ (integrate 1 :id \"rejected-%d\") missing-%d))",
                 i, i);
        EvalResult r = h.eval(code);
        REQUIRE(r.kind == EvalResult::Error);
        REQUIRE(h.engine.pool.state_slot_count == 0);
        REQUIRE(h.engine.registry.entry_count == 0);
    }
    h.eval_ok("(a1 (integrate 1 :id \"healthy-after-rejects\"))");
    REQUIRE(h.engine.pool.state_slot_count == 1);
}

TEST_CASE("A2d: side-effecting forms validate arity before publication",
          "[audit][transaction][arity]") {
    Harness h;
    auto& si = SymbolIntern::getInstance();

    EvalResult out = h.eval("(a1 0.25 9)");
    REQUIRE(out.kind == EvalResult::Error);
    REQUIRE(h.engine.pool.outputs[0].root_node == NODE_NONE);

    EvalResult def = h.eval("(define arity-y 7 8)");
    REQUIRE(def.kind == EvalResult::Error);
    SymbolID y = si.intern(String("arity-y"));
    REQUIRE(h.engine.cells.cells[y].kind == CellKind::Empty);

    EvalResult nested = h.eval("(do (a1 no-such-hidden) (define hidden-y 5))");
    REQUIRE(nested.kind == EvalResult::Error);
    SymbolID hidden = si.intern(String("hidden-y"));
    REQUIRE(h.engine.cells.cells[hidden].kind == CellKind::Empty);

    EvalResult defs = h.eval("(defs [defs-first 1 defs-second])");
    REQUIRE(defs.kind == EvalResult::Error);
    REQUIRE(h.engine.cells.cells[si.intern(String("defs-first"))].kind ==
            CellKind::Empty);
    REQUIRE(h.engine.cells.cells[si.intern(String("defs-second"))].kind ==
            CellKind::Empty);

    EvalResult defs_extra = h.eval("(defs [defs-extra 1] 9)");
    REQUIRE(defs_extra.kind == EvalResult::Error);
    REQUIRE(h.engine.cells.cells[si.intern(String("defs-extra"))].kind ==
            CellKind::Empty);

    std::string too_many_params = "(defn wide-fn [";
    for (uint16_t i = 0; i <= MAX_CALLABLE_PARAMS; i++) {
        too_many_params += " p" + std::to_string(i);
    }
    too_many_params += "] 1)";
    EvalResult wide = h.eval(too_many_params);
    REQUIRE(wide.kind == EvalResult::Error);
    REQUIRE(h.engine.cells.cells[si.intern(String("wide-fn"))].kind ==
            CellKind::Empty);
}

TEST_CASE("A2e: bare and compound named-state observations agree",
          "[audit][state][observation]") {
    Harness h;
    h.eval_ok("(defstate observed-c 0 (+ observed-c 1))");
    h.eval_ok("(a1 observed-c)");
    h.tick(0, 0.0, 1.0);
    h.tick(0, 1.0, 1.0);
    h.tick(0, 2.0, 1.0);

    EvalResult bare = h.eval("observed-c");
    EvalResult compound = h.eval("(+ observed-c 0)");
    REQUIRE(bare.kind == EvalResult::Number);
    REQUIRE(compound.kind == EvalResult::Number);
    REQUIRE(bare.number == Approx(compound.number));
    REQUIRE(bare.number == Approx(3.0));
}

TEST_CASE("A2f: tokenizer rejects typed delimiter mismatch and long symbols",
          "[audit][parser]") {
    Harness h;

    EvalResult mismatch = h.eval("(define typed-x [1 2))");
    REQUIRE(mismatch.kind == EvalResult::Error);

    std::string long_name(256, 'x');
    EvalResult overlong = h.eval("(define " + long_name + " 1)");
    REQUIRE(overlong.kind == EvalResult::Error);
    REQUIRE(overlong.diagnostic_count >= 1);
    REQUIRE(overlong.diagnostics[0].category == DiagnosticCategory::Syntax);
}

TEST_CASE("A2g: rejected reactive recompile preserves the active graph",
          "[audit][transaction][reactive]") {
    Harness h;

    h.eval_ok("(define reactive-dep 1)");
    h.eval_ok("(a1 (+ (phasor 1 :id \"reactive-phase\") reactive-dep))");
    h.tick(0, 0.0, 0.0);
    h.tick(0, 0.25, 0.25);
    REQUIRE(h.sample(0, 0.25) == Approx(1.25));

    uint16_t old_root = h.engine.pool.outputs[0].root_node;
    uint16_t old_update = h.engine.pool.state_update_roots[0];

    // Publishing the function succeeds, but it makes the stored output
    // source ill-typed: a function requiring an argument cannot be read as a
    // signal.  The rejected candidate first encounters the same state id at
    // 2 Hz, so this also proves its state-resource mutation is rolled back.
    h.eval_ok("(defn reactive-dep [x] x)");
    REQUIRE(h.engine.pool.outputs[0].valid);
    REQUIRE(h.engine.pool.outputs[0].root_node == old_root);
    REQUIRE(h.engine.pool.state_update_roots[0] == old_update);

    h.tick(0, 0.5, 0.25);
    REQUIRE(h.sample(0, 0.5) == Approx(1.5));

    // Dependencies were retained with the old graph, so restoring a numeric
    // cell causes a healthy recompile and publishes the new value.
    h.eval_ok("(define reactive-dep 2)");
    REQUIRE(h.engine.pool.outputs[0].valid);
    REQUIRE(h.sample(0, 0.5) == Approx(2.5));
}

// ── A4: useq-clear must fully clear defstate resources (§4.5) ──────────────

TEST_CASE("A4: re-defstate after useq-clear gets a fresh, working slot",
          "[audit][a4]") {
    Harness h;

    auto& si = SymbolIntern::getInstance();
    SymbolID c = si.intern(String("a4-counter"));

    h.eval_ok("(defstate a4-counter 5 (+ a4-counter 1))");
    REQUIRE(h.engine.pool.state_slot_count == 1);

    h.eval_ok("(useq-clear)");
    REQUIRE(h.engine.pool.state_slot_count == 0);
    // Cell marker must be gone — the cell no longer refers to a state slot.
    REQUIRE(h.engine.cells.cells[c].flags == 0);
    // Sources and update roots cleared.
    REQUIRE_FALSE(h.engine.state_sources[0].has_source);
    REQUIRE(h.engine.pool.state_update_roots[0] == NODE_NONE);

    // Re-defstate: fresh slot, fresh init value (not frozen pre-clear state).
    h.eval_ok("(defstate a4-counter 10 (+ a4-counter 2))");
    REQUIRE(h.engine.pool.state_slot_count == 1);
    uint16_t slot = h.engine.cells.cells[c].data_table_id;
    REQUIRE(h.engine.pool.state_values[slot] == Approx(10.0));
    REQUIRE(h.engine.cells.cells[c].value == Approx(10.0));
}

TEST_CASE("Clear is fresh-session equivalent for compiler-owned storage",
          "[audit][clear][session-reset]") {
    Harness h;
    auto& si = SymbolIntern::getInstance();
    SymbolID scalar = si.intern(String("clear-scalar"));
    SymbolID callable = si.intern(String("clear-callable"));

    h.eval_ok("(define clear-scalar [1 2 3])");
    h.eval_ok("(defn clear-callable [x] (+ x 1))");
    h.eval_ok("(defstate clear-state 5 (+ clear-state 1))");
    h.eval_ok("(a1 (+ (clear-callable 2)"
              " (live-edit 0.25 :id \"clear-knob\" :min 0 :max 1)))");

    REQUIRE(h.engine.cells.data_table_count > 0);
    REQUIRE(h.engine.arena.write_head > 0);
    REQUIRE(h.engine.pool.live_slot_count > 0);
    REQUIRE(h.engine.pool.node_count > 0);
    REQUIRE(h.engine.pool.state_slot_count > 0);
    REQUIRE(h.engine.output_sources[0].has_source);

    uint32_t generation = h.engine.session_generation;
    h.eval_ok("(useq-clear)");

    REQUIRE(h.engine.session_generation == generation + 1);
    REQUIRE(h.engine.cells.cells[scalar].kind == CellKind::Empty);
    REQUIRE(h.engine.cells.cells[callable].kind == CellKind::Empty);
    REQUIRE(h.engine.cells.callables[callable].source_length == 0);
    REQUIRE(h.engine.cells.data_table_count == 0);
    REQUIRE(h.engine.arena.write_head == 0);
    REQUIRE(h.engine.pool.node_count == 0);
    REQUIRE(h.engine.pool.exec_count == 0);
    REQUIRE(h.engine.pool.state_slot_count == 0);
    REQUIRE(h.engine.pool.live_slot_count == 0);
    REQUIRE(h.engine.pool.external_root_count == 0);
    REQUIRE(h.engine.registry.entry_count == 0);
    REQUIRE_FALSE(h.engine.output_sources[0].has_source);
    REQUIRE_FALSE(h.engine.state_sources[0].has_source);
    REQUIRE(h.engine.synth_graph.declaration_count() == 0);
    REQUIRE(h.engine.pool.output_class[0] == OutputClass::Inactive);
    REQUIRE(h.engine.pool.output_input_mask[0] == 0);

    // The same names and resource IDs can be used immediately as fresh
    // definitions; no stale callable source or table reference survives.
    h.eval_ok("(define clear-scalar [9 8])");
    h.eval_ok("(defn clear-callable [x] (* x 2))");
    h.eval_ok("(a1 (clear-callable 4))");
    REQUIRE(h.engine.cells.cells[scalar].data_table_id == 0);
    REQUIRE(h.sample(0) == Approx(8.0));
}

TEST_CASE("Inactive outputs overwrite reused caller buffers with neutral zero",
          "[audit][clear][neutral]") {
    Harness h;
    h.eval_ok("(a1 0.75)");
    REQUIRE(h.sample(0) == Approx(0.75));
    h.eval_ok("(useq-clear)");

    double cell_values[MAX_CELLS] = {};
    double hw_inputs[32] = {};
    double outputs[MAX_OUTPUTS];
    double workspace[MAX_TOTAL_NODES] = {};
    for (double& output : outputs) output = 0.75;
    ExecutionContext ctx;
    ctx.cell_values = cell_values;
    ctx.hw_inputs = hw_inputs;
    ctx.data_pool = h.engine.cells.data_pool;
    ctx.data_offsets = h.engine.cells.data_offsets;
    ctx.data_lengths = h.engine.cells.data_lengths;
    ctx.prev_outputs = h.engine.pool.prev_output_values;
    ctx.output_values = outputs;
    ctx.workspace = workspace;
    execute_all_outputs(h.engine.pool, ctx);

    for (double output : outputs) REQUIRE(output == 0.0);
}

// ── A5: scratch eval must not clobber fresh scratch state with live state ──
// state-identity.md §6.6: scratch evals are isolated. The old code compiled
// the scratch expression (which writes init values into freshly-allocated
// scratch slots) and THEN memcpy'd the live pool's state_values over the
// scratch pool, so a stateful expression evaluated via set/eval read a live
// state value that happened to share the same slot index instead of its own
// init.

TEST_CASE("A5: scratch-evaluated stateful expression keeps its init value",
          "[audit][a5]") {
    Harness h;

    // Occupy live state slot 0 with a conspicuous value.
    h.eval_ok("(defstate a5-live 99 (+ a5-live 0))");
    REQUIRE(h.engine.pool.state_values[0] == Approx(99.0));

    // Scratch-eval a fresh integrator (init 0). It allocates scratch slot 0;
    // pre-fix this returned 99 (live slot 0 leaked over the fresh init).
    EvalResult r = h.eval("(integrate 0)");
    REQUIRE(r.kind == EvalResult::Number);
    REQUIRE(r.number == Approx(0.0));

    // Reading live named state through a scratch eval still works.
    EvalResult r2 = h.eval("(+ a5-live 1)");
    REQUIRE(r2.kind == EvalResult::Number);
    REQUIRE(r2.number == Approx(100.0));

    // set with a stateful RHS: same isolation rule.
    h.eval_ok("(set a5-x (integrate 0))");
    auto& si = SymbolIntern::getInstance();
    SymbolID x = si.intern(String("a5-x"));
    REQUIRE(h.engine.cells.cells[x].value == Approx(0.0));

    // Live state untouched by the scratch evals.
    REQUIRE(h.engine.pool.state_values[0] == Approx(99.0));
}

// ── A6: failed defstate must not corrupt the previous binding ───────────────

TEST_CASE("A6: defstate compile failure rolls back cell and state slot",
          "[audit][a6]") {
    Harness h;
    auto& si = SymbolIntern::getInstance();

    // Existing plain-number binding survives a failed defstate of same name.
    h.eval_ok("(define a6-x 5)");
    SymbolID x = si.intern(String("a6-x"));
    uint16_t slots_before = h.engine.pool.state_slot_count;

    EvalResult r = h.eval("(defstate a6-x 0 (no-such-fn 1))");
    REQUIRE(r.kind == EvalResult::Error);

    REQUIRE(h.engine.cells.cells[x].kind == CellKind::Number);
    REQUIRE(h.engine.cells.cells[x].flags == 0);           // not a state cell
    REQUIRE(h.engine.cells.cells[x].value == Approx(5.0)); // old value intact
    REQUIRE(h.engine.pool.state_slot_count == slots_before); // slot rolled back

    // The binding still evaluates as before.
    EvalResult r2 = h.eval("(+ a6-x 1)");
    REQUIRE(r2.kind == EvalResult::Number);
    REQUIRE(r2.number == Approx(6.0));

    // A successful re-defstate of an EXISTING state cell that then fails
    // keeps the old update program and value.
    h.eval_ok("(defstate a6-c 3 (+ a6-c 1))");
    SymbolID c = si.intern(String("a6-c"));
    uint16_t slot = h.engine.cells.cells[c].data_table_id;
    uint16_t old_root = h.engine.pool.state_update_roots[slot];
    EvalResult r3 = h.eval("(defstate a6-c 7 (no-such-fn 1))");
    REQUIRE(r3.kind == EvalResult::Error);
    REQUIRE(h.engine.cells.cells[c].flags == 0x02);
    REQUIRE(h.engine.cells.cells[c].data_table_id == slot);
    REQUIRE(h.engine.pool.state_update_roots[slot] == old_root);
    REQUIRE(h.engine.pool.state_values[slot] == Approx(3.0));
}

// ── A7: set on a defstate cell writes the state slot ────────────────────────

TEST_CASE("A7: set on a defstate cell updates the live state value",
          "[audit][a7]") {
    Harness h;
    auto& si = SymbolIntern::getInstance();

    h.eval_ok("(defstate a7-c 3 (+ a7-c 1))");
    SymbolID c = si.intern(String("a7-c"));
    uint16_t slot = h.engine.cells.cells[c].data_table_id;
    REQUIRE(h.engine.pool.state_values[slot] == Approx(3.0));

    // Numeric set writes the state slot, keeps the state marker.
    h.eval_ok("(set a7-c 42)");
    REQUIRE(h.engine.pool.state_values[slot] == Approx(42.0));
    REQUIRE(h.engine.cells.cells[c].flags == 0x02);
    REQUIRE(h.engine.cells.cells[c].data_table_id == slot);

    // Expression set too.
    h.eval_ok("(set a7-c (+ 10 5))");
    REQUIRE(h.engine.pool.state_values[slot] == Approx(15.0));

    // Reads see the new value.
    EvalResult r = h.eval("(+ a7-c 0)");
    REQUIRE(r.kind == EvalResult::Number);
    REQUIRE(r.number == Approx(15.0));
}

// ── A8: duplicate active :id in one program is a compile error (§5.3/§8.1) ──

TEST_CASE("A8: duplicate active :id rejected; cross-kind :id sharing allowed",
          "[audit][a8]") {
    Harness h;

    // Two oscillators updating the same phase resource in one program —
    // ambiguous, must be rejected (state-identity.md §5.3).
    EvalResult r = h.eval("(a1 (+ (saw 1 :id \"pA\") (saw 2 :id \"pA\")))");
    REQUIRE(r.kind == EvalResult::Error);
    REQUIRE(r.diagnostic_count >= 1);
    REQUIRE(r.diagnostics[0].category == DiagnosticCategory::Boundary);

    // Same :id across INCOMPATIBLE primitives resolves to disjoint resources
    // (§3.5) — toggle and count must not collide on TriggerMemory either.
    h.eval_ok("(a2 (+ (toggle (sqr beat) :id \"x\") (count (sqr beat) :id \"x\")))");

    // Recompiling the same program with the same :id stays fine (per-build
    // detection only).
    h.eval_ok("(a3 (saw 1 :id \"pB\"))");
    h.eval_ok("(a3 (saw 2 :id \"pB\"))");
}

// ── A9: unknown keywords on stateful primitives error out ──────────────────

TEST_CASE("A9: unknown keywords, :fresh, and non-constant :phase are errors",
          "[audit][a9]") {
    Harness h;

    const char* bad_forms[] = {
        "(a1 (phasor 1 :bogus 3))",
        "(a1 (lfo 1 :bogus 3))",
        "(a1 (integrate 1 :bogus 3))",
        "(a1 (toggle (sqr beat) :bogus 3))",
        "(a1 (count (sqr beat) :bogus 3))",
        "(a1 (slew (saw 1) 1 :bogus 3))",
        "(a1 (live-edit 0.5 :id \"k\" :bogus 3))",
        "(a1 (phasor 1 :fresh))",
        // Non-constant :phase must error, not be silently ignored.
        "(a1 (phasor 1 :phase (saw 1)))",
        "(a1 (lfo 1 :phase (saw 1)))",
    };
    for (const char* f : bad_forms) {
        INFO("form: " << f);
        EvalResult r = h.eval(f);
        REQUIRE(r.kind == EvalResult::Error);
        REQUIRE(r.diagnostic_count >= 1);
        REQUIRE(r.diagnostics[0].category == DiagnosticCategory::Type);
    }

    // Known keywords still work.
    h.eval_ok("(a1 (phasor 1 :phase 0.25 :id \"p\"))");
    h.eval_ok("(a2 (lfo 2 :wave :saw :pw 0.3))");
}

// ── A10: no unsound Div a a -> 1 fold ───────────────────────────────────────
// Runtime division defines a/0 = 0 (eval_ops.h), so x/x is 0 at x = 0. The
// old fold rewrote (/ x x) to the constant 1 regardless.

TEST_CASE("A10: (/ x x) evaluates per runtime semantics, not folded to 1",
          "[audit][a10]") {
    Harness h;

    // saw(1) is 0 at t=0 → 0/0 must be 0, not 1.
    h.eval_ok("(a1 (/ (saw 1) (saw 1)))");
    REQUIRE(h.sample(0, 0.0) == Approx(0.0));

    // Non-zero point still gives 1.
    h.eval_ok("(a2 (/ t t))");
    REQUIRE(h.sample(1, 0.5) == Approx(1.0));
}

// ── A12: cell-store revision counter for snapshot skipping ──────────────────

TEST_CASE("A12: store_revision bumps on mutating evals", "[audit][a12]") {
    Harness h;

    uint32_t r0 = h.engine.cells.store_revision;
    REQUIRE(r0 >= 1); // init_timing_defaults counts as a mutation

    h.eval_ok("(define a12-x 1)");
    uint32_t r1 = h.engine.cells.store_revision;
    REQUIRE(r1 > r0);

    h.eval_ok("(set a12-x 2)");
    REQUIRE(h.engine.cells.store_revision > r1);
}

// ── A14: numeric literals are plain decimal with clean boundaries ──────────

TEST_CASE("A14: tokenizer rejects 0x/inf/nan and trailing-symbol numerics",
          "[audit][a14]") {
    Harness h;

    Token tokens[64];
    Diagnostic errs[8];
    uint8_t err_count = 0;

    auto tokenize_one = [&](const char* src) -> Token {
        err_count = 0;
        uint16_t n = TokenStream::tokenize(src, (uint32_t)strlen(src),
                                           tokens, 64, errs, &err_count);
        REQUIRE(n >= 1);
        return tokens[0];
    };

    // strtod special forms must not become numbers.
    for (const char* s : {"inf", "nan", "0x10", "-inf", "INF"}) {
        INFO("source: " << s);
        Token t = tokenize_one(s);
        REQUIRE(t.kind == TokenKind::Symbol);
    }

    // "2x" is one symbol, not number 2 + symbol x.
    {
        err_count = 0;
        const char* s = "2x";
        uint16_t n = TokenStream::tokenize(s, 2, tokens, 64, errs, &err_count);
        REQUIRE(n == 2); // symbol + EOF
        REQUIRE(tokens[0].kind == TokenKind::Symbol);
        REQUIRE(tokens[0].span_len == 2);
    }

    // Ordinary literals still tokenize as numbers.
    struct { const char* src; double v; } good[] = {
        {"2", 2.0}, {"-1.5", -1.5}, {".5", 0.5}, {"1e3", 1000.0},
        {"2.5e-2", 0.025},
    };
    for (auto& g : good) {
        INFO("source: " << g.src);
        Token t = tokenize_one(g.src);
        REQUIRE(t.kind == TokenKind::Number);
        REQUIRE(t.number == Approx(g.v));
    }

    // Boundary chars like ')' still terminate numbers.
    {
        const char* s = "(+ 1 2)";
        err_count = 0;
        uint16_t n = TokenStream::tokenize(s, (uint32_t)strlen(s),
                                           tokens, 64, errs, &err_count);
        REQUIRE(n == 6); // ( + 1 2 ) EOF
        REQUIRE(tokens[2].kind == TokenKind::Number);
        REQUIRE(tokens[3].kind == TokenKind::Number);
    }

    // End-to-end: (a1 2x) errors instead of silently reading 2.
    EvalResult r = h.eval("(a1 (+ 1 2x))");
    REQUIRE(r.kind == EvalResult::Error);
}

// NOTE: the A1 case floods the shared symbol interner past MAX_CELLS, which
// makes any fresh name interned after it out-of-range. Keep it LAST.

// ── A1: cell writes must bounds-check symbol IDs >= MAX_CELLS ───────────────

TEST_CASE("A1: defining more names than MAX_CELLS errors instead of OOB write",
          "[audit][a1]") {
    Harness h;

    // Force the interner past MAX_CELLS so subsequent fresh names are
    // guaranteed to have out-of-range IDs.
    auto& si = SymbolIntern::getInstance();
    for (int i = 0; i < (int)MAX_CELLS + 8; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "a1-flood-%d", i);
        si.intern(String(buf));
    }

    SymbolID big = si.intern(String("a1-oob-name"));
    REQUIRE(big >= MAX_CELLS);

    // Every cell-write form must reject the out-of-range name with an
    // Overflow diagnostic — not write out of bounds.
    const char* forms[] = {
        "(define a1-oob-name 1)",
        "(def a1-oob-name 2)",
        "(defn a1-oob-name [x] (+ x 1))",
        "(set a1-oob-name 3)",
        "(defs [a1-oob-name 4])",
        "(defstate a1-oob-name 0 (+ a1-oob-name 1))",
    };
    for (const char* f : forms) {
        INFO("form: " << f);
        EvalResult r = h.eval(f);
        REQUIRE(r.kind == EvalResult::Error);
        REQUIRE(r.diagnostic_count >= 1);
        REQUIRE(r.diagnostics[0].category == DiagnosticCategory::Overflow);
    }

    // Engine still works after the rejections.
    h.eval_ok("(a1 0.25)");
    REQUIRE(h.sample(0) == Approx(0.25));
}
