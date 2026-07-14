// P1 compiler correctness regression tests (v1.2.0 release audit).
//
// Three "silent wrong value" compiler bugs that must never ship — each is a
// case where the compiler quietly substituted a wrong value instead of either
// computing the right one or failing loudly:
//
//   P1a: Time-varying elements inside a data-vector (step/seq/gates/interp/…)
//        were silently replaced with 0. A vector lowers to a static double
//        table read by index, so a per-slot signal can't be represented — it
//        must be a compile error, not a silent zero (values-types.md §1.7).
//        `for` iterates over element *nodes* and still supports time-varying
//        elements, so it is unaffected.
//
//   P1b: `(define x N)` over a name that was previously a `defstate` cell was
//        silently ignored: the stale state marker (flags 0x02) survived, so
//        graph_builder kept emitting a state_load from the old slot and the new
//        value never took effect. define must establish a fresh binding.
//
//   P1c: `let` bindings past the 32-binding pool limit were silently dropped,
//        so a later reference resolved to the wrong value (or a global). It
//        must be a diagnostic instead.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "src/signal_engine/signal_engine.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace sig;

namespace {

struct P1Harness {
    SignalEngine engine;

    P1Harness() { engine.init_defaults(120.0, 4); }

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

    const Cell& cell(const char* name) {
        return engine.cells.cells[internSymbol(name)];
    }
};

} // namespace

// ============================================================================
// P1a: time-varying vector elements must be a compile error, not a silent 0
// ============================================================================

TEST_CASE("P1a: time-varying element in a step vector is a compile error",
          "[p1][vector]") {
    P1Harness h;

    EvalResult r = h.eval("(a1 (step [1 (sin beat) 3] beat))");
    REQUIRE(r.kind == EvalResult::Error);
    REQUIRE(r.diagnostic_count >= 1);
    REQUIRE(r.diagnostics[0].category == DiagnosticCategory::Type);
    // The output must NOT be installed with silently-zeroed data.
    REQUIRE_FALSE(h.engine.pool.outputs[0].valid);
}

TEST_CASE("P1a: a bare time-varying vector literal is a compile error",
          "[p1][vector]") {
    P1Harness h;
    EvalResult r = h.eval("(a1 [1 (sin beat) 3])");
    REQUIRE(r.kind == EvalResult::Error);
    REQUIRE(r.diagnostic_count >= 1);
    REQUIRE(r.diagnostics[0].category == DiagnosticCategory::Type);
}

TEST_CASE("P1a: constant vectors still compile and sample correctly",
          "[p1][vector]") {
    P1Harness h;
    h.eval_ok("(a1 (step [1 2 3] beat))");
    REQUIRE(h.engine.pool.outputs[0].valid);
    REQUIRE(h.sample("a1") == Approx(1.0)); // step value at phase 0
}

TEST_CASE("P1a: `for` still supports time-varying elements (per-slot signals)",
          "[p1][vector]") {
    P1Harness h;
    // for iterates over element nodes, so a time-varying element is valid here.
    h.eval_ok("(a1 (for i [1 (sin beat) 3] i))");
    REQUIRE(h.engine.pool.outputs[0].valid);
}

// ============================================================================
// P1b: define over a defstate cell must take effect (clear the state marker)
// ============================================================================

TEST_CASE("P1b: (define x N) over a defstate cell is honoured", "[p1][define]") {
    P1Harness h;

    h.eval_ok("(defstate x 5 (+ x 1))");
    REQUIRE(h.cell("x").flags == 0x02); // state marker set by defstate
    REQUIRE(h.cell("x").kind == CellKind::Number);

    h.eval_ok("(a1 x)");

    // Redefine x as a plain constant — this must sever the state association.
    h.eval_ok("(define x 42)");
    REQUIRE(h.cell("x").flags == 0);          // stale state marker cleared
    REQUIRE(h.cell("x").value == Approx(42.0));
    REQUIRE(h.cell("x").kind == CellKind::Number);

    // The dependent output must recompile to read the new constant, not the
    // old state slot.
    REQUIRE(h.sample("a1") == Approx(42.0));
}

TEST_CASE("P1b: define over defstate then re-defstate reuses a fresh binding",
          "[p1][define]") {
    P1Harness h;
    h.eval_ok("(defstate x 5 (+ x 1))");
    h.eval_ok("(define x 7)");
    REQUIRE(h.cell("x").flags == 0);
    // A subsequent defstate re-establishes state semantics cleanly.
    h.eval_ok("(defstate x 9 (+ x 1))");
    REQUIRE(h.cell("x").flags == 0x02);
}

// ============================================================================
// P1c: let bindings past the pool limit must diagnose, not silently drop
// ============================================================================

TEST_CASE("P1c: a let with exactly 32 bindings compiles", "[p1][let]") {
    P1Harness h;
    std::string code = "(a1 (let [";
    for (int i = 0; i < 32; i++) {
        code += "v" + std::to_string(i) + " " + std::to_string(i) + " ";
    }
    code += "] v0))";
    h.eval_ok(code);
    REQUIRE(h.engine.pool.outputs[0].valid);
    REQUIRE(h.sample("a1") == Approx(0.0)); // v0 == 0
}

TEST_CASE("P1c: a let with 33 bindings is a compile error", "[p1][let]") {
    P1Harness h;
    std::string code = "(a1 (let [";
    for (int i = 0; i < 33; i++) {
        code += "v" + std::to_string(i) + " " + std::to_string(i) + " ";
    }
    code += "] v0))";
    EvalResult r = h.eval(code);
    REQUIRE(r.kind == EvalResult::Error);
    REQUIRE(r.diagnostic_count >= 1);
    REQUIRE(r.diagnostics[0].category == DiagnosticCategory::Overflow);
    REQUIRE_FALSE(h.engine.pool.outputs[0].valid);
}
