// Failure-mode tests — docs/specs/failure-model.md §2.1/§3/§5.
//
// Mode A (FailureMode::LkgFallback, DEFAULT): a non-finite value reaching an
// output root substitutes the last-known-good value (or 0 with no LKG),
// sets the pool's runtime_fallback_mask bit, and never zeroes per node.
// Mode B (FailureMode::ZeroSquash, legacy): every non-finite node result is
// clamped to 0.0; no fallback, no diagnostic.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "src/signal_engine/signal_engine.h"
#include "src/modulisp/lisp/symbol_intern.h"

#include <cmath>
#include <cstring>
#include <string>

using namespace sig;

namespace {

struct Harness {
    SignalEngine engine;

    Harness() { engine.init_defaults(120.0, 4); }

    ~Harness() {
        // The failure mode is engine-global; restore the default so test
        // ordering can't leak ZeroSquash into other cases.
        set_failure_mode(FailureMode::LkgFallback);
    }

    void eval_ok(const std::string& code) {
        EvalResult r = eval_cold(code.c_str(), (uint32_t)code.size(), engine);
        INFO("code: " << code);
        if (r.kind == EvalResult::Error && r.diagnostic_count > 0) {
            INFO("diagnostic: "
                 << (r.diagnostics[0].message ? r.diagnostics[0].message : ""));
        }
        REQUIRE(r.kind != EvalResult::Error);
        engine.pool.rebuild_execution_order();
    }

    // Execute one sample at time t; optionally commit (tick semantics).
    double sample(int output_index, double t, bool commit = false) {
        double cell_values[MAX_CELLS];
        engine.cells.snapshot_values(cell_values, MAX_CELLS);
        double hw_inputs[32] = {};
        double outputs_arr[MAX_OUTPUTS] = {};
        double workspace[MAX_TOTAL_NODES] = {};

        ExecutionContext ctx;
        ctx.t = t;
        ctx.dt = 0.001;
        ctx.cell_values = cell_values;
        ctx.hw_inputs = hw_inputs;
        ctx.data_pool = engine.cells.data_pool;
        ctx.data_offsets = engine.cells.data_offsets;
        ctx.data_lengths = engine.cells.data_lengths;
        ctx.prev_outputs = engine.pool.prev_output_values;
        ctx.output_values = outputs_arr;
        ctx.workspace = workspace;
        execute_all_outputs(engine.pool, ctx);

        if (commit) {
            commit_state(engine.pool, workspace);
            commit_outputs(engine.pool, outputs_arr);
        }
        return outputs_arr[output_index];
    }

    bool in_fallback(int output_index) const {
        return (engine.pool.runtime_fallback_mask >> output_index) & 1;
    }
};

// (* (* t 1e308) 1e308) overflows to +inf for t > 0 and is exactly 0.0
// (finite) at t == 0 — a time-phase-dependent runtime error (§1.7) that
// cannot be constant-folded away.
const char* OVERFLOW_PROG = "(a1 (* (* t 1e308) 1e308))";

} // namespace

TEST_CASE("Mode A default: non-finite at root falls back to LKG",
          "[failure-mode][lkg]") {
    Harness h;
    REQUIRE(get_failure_mode() == FailureMode::LkgFallback);

    // Establish an LKG value of 0.75 on a1.
    h.eval_ok("(a1 0.75)");
    h.sample(0, 0.0, /*commit=*/true);
    REQUIRE(h.engine.pool.outputs[0].lkg_value == Approx(0.75));

    // Replace with a program that overflows to +inf at t > 0.
    h.eval_ok(OVERFLOW_PROG);

    double v = h.sample(0, 1.0);
    REQUIRE(std::isfinite(v));
    REQUIRE(v == Approx(0.75));       // LKG substituted, not 0.0
    REQUIRE(h.in_fallback(0));        // fallback recorded for diagnostics

    // A healthy sample (t == 0 → finite 0.0) clears the fallback bit.
    double v2 = h.sample(0, 0.0);
    REQUIRE(v2 == Approx(0.0));
    REQUIRE_FALSE(h.in_fallback(0));
}

TEST_CASE("Mode A bootstrap: non-finite with no LKG yields neutral default",
          "[failure-mode][lkg]") {
    Harness h;

    // Never-committed output: no LKG exists (valid == false).
    h.eval_ok(OVERFLOW_PROG);
    double v = h.sample(0, 1.0);
    REQUIRE(v == Approx(0.0));        // neutral default (§2.4)
    REQUIRE(h.in_fallback(0));
}

TEST_CASE("Optimizer preserves non-finite failure observability",
          "[failure-mode][optimizer][audit]") {
    Harness h;

    h.eval_ok("(a1 0.75)");
    h.sample(0, 0.0, /*commit=*/true);

    SECTION("dynamic x multiplied by zero") {
        h.eval_ok("(a1 (* (expt (- 0 t) 0.5) 0))");
        REQUIRE(h.sample(0, 1.0) == Approx(0.75));
        REQUIRE(h.in_fallback(0));
    }

    SECTION("dynamic x subtracted from itself") {
        h.eval_ok("(a1 (- (expt (- 0 t) 0.5) (expt (- 0 t) 0.5)))");
        REQUIRE(h.sample(0, 1.0) == Approx(0.75));
        REQUIRE(h.in_fallback(0));
    }

    SECTION("constant division by zero") {
        h.eval_ok("(a1 (/ 1 0))");
        REQUIRE(h.sample(0, 0.0) == Approx(0.75));
        REQUIRE(h.in_fallback(0));
    }

    SECTION("constant modulo by zero") {
        h.eval_ok("(a1 (% 1 0))");
        REQUIRE(h.sample(0, 0.0) == Approx(0.75));
        REQUIRE(h.in_fallback(0));
    }
}

TEST_CASE("Mode A: fallback does not poison LKG or other outputs",
          "[failure-mode][lkg]") {
    Harness h;

    h.eval_ok("(a1 0.5)");
    h.eval_ok("(a2 0.25)");
    h.sample(0, 0.0, /*commit=*/true);

    h.eval_ok(OVERFLOW_PROG);
    double v = h.sample(0, 2.0, /*commit=*/true);
    REQUIRE(v == Approx(0.5));
    // Committing the substituted value must not overwrite LKG with garbage.
    REQUIRE(h.engine.pool.outputs[0].lkg_value == Approx(0.5));
    REQUIRE(std::isfinite(h.engine.pool.prev_output_values[0]));

    // Other outputs are unaffected (§10.1 per-output isolation).
    REQUIRE_FALSE(h.in_fallback(1));
    REQUIRE(h.sample(1, 2.0) == Approx(0.25));
}

TEST_CASE("Mode A: healthy reassignment returns output to running",
          "[failure-mode][lkg]") {
    Harness h;

    h.eval_ok("(a1 0.75)");
    h.sample(0, 0.0, /*commit=*/true);
    h.eval_ok(OVERFLOW_PROG);
    h.sample(0, 1.0);
    REQUIRE(h.in_fallback(0));

    h.eval_ok("(a1 0.125)");
    REQUIRE(h.sample(0, 1.0) == Approx(0.125));
    REQUIRE_FALSE(h.in_fallback(0));   // fallback → running (§5.2)
}

TEST_CASE("Mode A: non-finite state update keeps previous state value",
          "[failure-mode][lkg][state]") {
    Harness h;

    // A state slot whose update expression overflows at t > 0.
    h.eval_ok("(a1 (slew (* (* t 1e308) 1e308) 0.5))");
    h.sample(0, 0.0, /*commit=*/true);
    double before = h.engine.pool.state_values[0];
    REQUIRE(std::isfinite(before));

    h.sample(0, 1.0, /*commit=*/true);
    // The poisoned update must not have been committed.
    REQUIRE(std::isfinite(h.engine.pool.state_values[0]));
}

TEST_CASE("Mode A: fallback freezes state owned by the failed output",
          "[failure-mode][lkg][state][ownership]") {
    Harness h;

    h.eval_ok(
        "(a1 (+ (* (* t 1e308) 1e308) "
        "       (phasor 1 :id \"frozen-phase\")))");
    REQUIRE(h.engine.pool.state_slot_count == 1);
    double initial = h.engine.pool.state_values[0];

    h.sample(0, 1.0, /*commit=*/true);
    REQUIRE(h.in_fallback(0));
    REQUIRE(h.engine.pool.state_values[0] == Approx(initial));
    h.sample(0, 2.0, /*commit=*/true);
    REQUIRE(h.engine.pool.state_values[0] == Approx(initial));

    // Once the same owner publishes healthy samples, its state resumes from
    // the last value the listener actually heard rather than jumping ahead.
    h.eval_ok("(a1 (phasor 1 :id \"frozen-phase\"))");
    h.sample(0, 3.0, /*commit=*/true);
    REQUIRE_FALSE(h.in_fallback(0));
    REQUIRE(h.engine.pool.state_values[0] > initial);
}

TEST_CASE("Mode B legacy: non-finite squashes to zero per node",
          "[failure-mode][zero-squash]") {
    Harness h;
    set_failure_mode(FailureMode::ZeroSquash);

    h.eval_ok("(a1 0.75)");
    h.sample(0, 0.0, /*commit=*/true);
    h.eval_ok(OVERFLOW_PROG);

    double v = h.sample(0, 1.0);
    REQUIRE(v == Approx(0.0));        // squashed, NOT the 0.75 LKG
    REQUIRE_FALSE(h.in_fallback(0));  // no fallback recorded in Mode B
}

TEST_CASE("Batch execution honours the failure mode",
          "[failure-mode][batch]") {
    Harness h;
    h.engine.pool.allocate_batch_workspace();

    h.eval_ok("(a1 0.75)");
    h.sample(0, 0.0, /*commit=*/true);
    h.eval_ok(OVERFLOW_PROG);

    double cell_values[MAX_CELLS];
    h.engine.cells.snapshot_values(cell_values, MAX_CELLS);
    double hw_inputs[32] = {};
    const double t_array[3] = {0.0, 1.0, 2.0};
    double out_buf[3] = {};

    SECTION("Mode A: per-sample LKG substitution + fallback mask") {
        execute_batch(h.engine.pool, t_array, 3, cell_values, hw_inputs,
                      h.engine.cells.data_pool, h.engine.cells.data_offsets,
                      h.engine.cells.data_lengths, out_buf, 1);
        REQUIRE(out_buf[0] == Approx(0.0));    // finite sample untouched
        REQUIRE(out_buf[1] == Approx(0.75));   // inf → LKG
        REQUIRE(out_buf[2] == Approx(0.75));
        REQUIRE(h.in_fallback(0));
    }

    SECTION("Mode B: per-sample zero squash") {
        set_failure_mode(FailureMode::ZeroSquash);
        execute_batch(h.engine.pool, t_array, 3, cell_values, hw_inputs,
                      h.engine.cells.data_pool, h.engine.cells.data_offsets,
                      h.engine.cells.data_lengths, out_buf, 1);
        REQUIRE(out_buf[1] == Approx(0.0));
        REQUIRE(out_buf[2] == Approx(0.0));
        REQUIRE_FALSE(h.in_fallback(0));
    }
}
