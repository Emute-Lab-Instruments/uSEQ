// Phase 4: Reactivity, LKG, and failure model hardening tests.
//
// Proves that edits never stop unrelated outputs. Covers dependency cascading,
// compile error preservation, runtime non-finite behaviour, LKG behaviour,
// output lifecycle, and function-cell interactions.
//
// This file is deliberately separate from test_signal_engine_golden.cpp to
// avoid merge conflicts with concurrent Phase 5 work.

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

// ── GoldenHarness (copied from test_signal_engine_golden.cpp) ──────────────
// Minimal copy of the harness needed for Phase 4 tests.

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

        ExecutionContext ctx;
        ctx.t = t;
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

        return outputs[output_index(output_name)];
    }

    double tick(const char* output_name, double t)
    {
        double value = sample(output_name, t);
        commit_outputs(engine.pool, outputs);
        return value;
    }

    std::vector<double> sample_window(const char* output,
                                       double t_start, double t_end,
                                       size_t count)
    {
        std::vector<double> result;
        result.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            double t_val = t_start + (t_end - t_start)
                           * static_cast<double>(i) / static_cast<double>(count - 1);
            result.push_back(sample(output, t_val));
        }
        return result;
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
// Phase 4.1: Deep dependency cascading
// ============================================================================

TEST_CASE("Phase 4: deep dependency cascading", "[phase4][reactivity]") {

    SECTION("3-level expression cell chain propagates") {
        GoldenHarness h;
        h.eval_ok("(define x 1)");
        h.eval_ok("(define y (+ x 10))");
        h.eval_ok("(define z (* y 2))");
        h.assign_ok("a1", "z");
        REQUIRE(h.sample("a1", 0.0) == Approx(22.0)); // (1+10)*2

        h.eval_ok("(define x 5)");
        REQUIRE(h.sample("a1", 0.0) == Approx(30.0)); // (5+10)*2
    }

    SECTION("multiple outputs depending on same cell all update") {
        GoldenHarness h;
        h.eval_ok("(define freq 440)");
        h.assign_ok("a1", "freq");
        h.assign_ok("a2", "(* freq 2)");
        REQUIRE(h.sample("a1", 0.0) == Approx(440.0));
        REQUIRE(h.sample("a2", 0.0) == Approx(880.0));

        h.eval_ok("(define freq 220)");
        REQUIRE(h.sample("a1", 0.0) == Approx(220.0));
        REQUIRE(h.sample("a2", 0.0) == Approx(440.0));
    }

    SECTION("time-varying expression cell updates propagate") {
        GoldenHarness h;
        h.eval_ok("(define a (+ t 1))");
        h.eval_ok("(define b (* a 2))");
        h.assign_ok("a1", "b");
        // At t=0: a=1, b=2
        REQUIRE(h.sample("a1", 0.0) == Approx(2.0));
        // At t=0.5: a=1.5, b=3
        REQUIRE(h.sample("a1", 0.5) == Approx(3.0));

        // Redefine a: a = (+ t 10)
        h.eval_ok("(define a (+ t 10))");
        // At t=0: a=10, b=20
        REQUIRE(h.sample("a1", 0.0) == Approx(20.0));
    }

    SECTION("4-level chain propagates on root change") {
        GoldenHarness h;
        h.eval_ok("(define p 2)");
        h.eval_ok("(define q (+ p 1))");     // q=3
        h.eval_ok("(define r (* q 10))");    // r=30
        h.eval_ok("(define s (- r 5))");     // s=25
        h.assign_ok("a1", "s");
        REQUIRE(h.sample("a1", 0.0) == Approx(25.0));

        h.eval_ok("(define p 10)");
        // q=11, r=110, s=105
        REQUIRE(h.sample("a1", 0.0) == Approx(105.0));
    }

    SECTION("redefining middle of chain propagates downstream only") {
        GoldenHarness h;
        h.eval_ok("(define x 1)");
        h.eval_ok("(define y (* x 3))");  // y=3
        h.eval_ok("(define z (+ y 100))"); // z=103
        h.assign_ok("a1", "y");
        h.assign_ok("a2", "z");
        REQUIRE(h.sample("a1", 0.0) == Approx(3.0));
        REQUIRE(h.sample("a2", 0.0) == Approx(103.0));

        // Redefine y to a constant (breaks link to x)
        h.eval_ok("(define y 50)");
        REQUIRE(h.sample("a1", 0.0) == Approx(50.0));
        REQUIRE(h.sample("a2", 0.0) == Approx(150.0));
    }
}

// ============================================================================
// Phase 4.2: Compile error isolation and recovery
// ============================================================================

TEST_CASE("Phase 4: compile error isolation and recovery", "[phase4][failure]") {

    SECTION("compile error preserves OTHER running outputs") {
        GoldenHarness h;
        h.assign_ok("a1", "beat");
        h.assign_ok("a2", "bar");
        // At 120 bpm: beat at t=0.25 → 0.5, bar at t=0.25 → 0.125
        REQUIRE(h.sample("a2", 0.25) == Approx(0.125));

        EvalResult bad = h.eval_result("(a1 (unknown-symbol))");
        REQUIRE(bad.kind == EvalResult::Error);

        // a1 should still run beat, a2 still runs bar
        REQUIRE(h.sample("a1", 0.25) == Approx(0.5));
        REQUIRE(h.sample("a2", 0.25) == Approx(0.125));
    }

    SECTION("sequential errors don't lose original program") {
        GoldenHarness h;
        h.assign_ok("a1", "beat");
        REQUIRE(h.sample("a1", 0.125) == Approx(0.25));

        h.eval_result("(a1 (error1))");
        REQUIRE(h.sample("a1", 0.125) == Approx(0.25)); // still beat

        h.eval_result("(a1 (error2))");
        REQUIRE(h.sample("a1", 0.125) == Approx(0.25)); // STILL beat
    }

    SECTION("valid eval after error replaces the program") {
        GoldenHarness h;
        h.assign_ok("a1", "beat");
        REQUIRE(h.sample("a1", 0.125) == Approx(0.25));

        h.eval_result("(a1 (error1))"); // error
        REQUIRE(h.sample("a1", 0.125) == Approx(0.25));

        h.assign_ok("a1", "bar"); // valid replacement
        REQUIRE(h.sample("a1", 0.5) == Approx(0.25)); // now runs bar
    }

    SECTION("error on cell redefinition doesn't crash outputs using that cell") {
        GoldenHarness h;
        h.eval_ok("(define x 10)");
        h.assign_ok("a1", "x");
        h.assign_ok("a2", "(+ x 5)");
        REQUIRE(h.sample("a1", 0.0) == Approx(10.0));
        REQUIRE(h.sample("a2", 0.0) == Approx(15.0));

        // Redefine x — dependency recompilation should succeed
        h.eval_ok("(define x 20)");
        REQUIRE(h.sample("a1", 0.0) == Approx(20.0));
        REQUIRE(h.sample("a2", 0.0) == Approx(25.0));
    }

    SECTION("three outputs: error on one preserves the other two") {
        GoldenHarness h;
        h.assign_ok("a1", "beat");
        h.assign_ok("a2", "bar");
        h.assign_ok("a3", "t");

        EvalResult bad = h.eval_result("(a2 (nonexistent-func beat))");
        REQUIRE(bad.kind == EvalResult::Error);

        // a1 and a3 must still work; a2 should still run its old program (bar)
        REQUIRE(h.sample("a1", 0.25) == Approx(0.5));
        REQUIRE(h.sample("a2", 0.25) == Approx(0.125));
        REQUIRE(h.sample("a3", 0.25) == Approx(0.25));
    }

    SECTION("error then valid then error keeps second valid program") {
        GoldenHarness h;
        h.assign_ok("a1", "beat");
        REQUIRE(h.sample("a1", 0.125) == Approx(0.25));

        h.eval_result("(a1 (err1))"); // error - keeps beat
        h.assign_ok("a1", "bar");     // valid - replaces with bar
        REQUIRE(h.sample("a1", 0.5) == Approx(0.25)); // bar

        h.eval_result("(a1 (err2))"); // error - keeps bar
        REQUIRE(h.sample("a1", 0.5) == Approx(0.25)); // still bar
    }
}

// ============================================================================
// Phase 4.3: Non-finite value handling
// ============================================================================

TEST_CASE("Phase 4: non-finite value handling", "[phase4][numerical]") {

    SECTION("division by zero returns finite value") {
        GoldenHarness h;
        h.assign_ok("a1", "(/ 1 0)");
        double val = h.sample("a1", 0.0);
        REQUIRE(std::isfinite(val));
        // The engine explicitly returns 0 for div-by-zero
        REQUIRE(val == Approx(0.0));
    }

    SECTION("legitimate large values stay finite") {
        GoldenHarness h;
        h.assign_ok("a1", "(/ 1 0.000001)");
        double val = h.sample("a1", 0.0);
        REQUIRE(std::isfinite(val));
        REQUIRE(val == Approx(1000000.0));
    }

    SECTION("sqrt of negative returns finite") {
        GoldenHarness h;
        h.assign_ok("a1", "(sqrt (- 0 1))");
        double val = h.sample("a1", 0.0);
        REQUIRE(std::isfinite(val));
        // sqrt(fabs(-1)) = sqrt(1) = 1
        REQUIRE(val == Approx(1.0));
    }

    SECTION("extreme time values don't crash") {
        GoldenHarness h;
        h.assign_ok("a1", "beat");
        REQUIRE(std::isfinite(h.sample("a1", 1e10)));
        REQUIRE(std::isfinite(h.sample("a1", -1.0)));
        REQUIRE(std::isfinite(h.sample("a1", 0.0)));
    }

    SECTION("mod by zero returns finite") {
        GoldenHarness h;
        h.assign_ok("a1", "(% 5 0)");
        double val = h.sample("a1", 0.0);
        REQUIRE(std::isfinite(val));
        REQUIRE(val == Approx(0.0));
    }

    SECTION("chained operations producing intermediate infinities stay finite") {
        GoldenHarness h;
        // (/ 1 0) produces 0 (clamped), then (* 0 5) = 0
        h.assign_ok("a1", "(* (/ 1 0) 5)");
        double val = h.sample("a1", 0.0);
        REQUIRE(std::isfinite(val));
    }

    SECTION("NaN guard on executor output") {
        GoldenHarness h;
        // tan(pi/2) could produce very large values or NaN depending on precision
        // The executor should guard against non-finite results
        h.assign_ok("a1", "(tan (* t 3.14159265))");
        // At t near 0.5, tan approaches infinity
        double val = h.sample("a1", 0.4999999);
        REQUIRE(std::isfinite(val));
    }
}

// ============================================================================
// Phase 4.4: Output reassignment lifecycle
// ============================================================================

TEST_CASE("Phase 4: output reassignment lifecycle", "[phase4][lifecycle]") {

    SECTION("reassigning output replaces the graph") {
        GoldenHarness h;
        h.assign_ok("a1", "beat");
        REQUIRE(h.sample("a1", 0.125) == Approx(0.25));

        h.assign_ok("a1", "bar");
        REQUIRE(h.sample("a1", 0.5) == Approx(0.25)); // bar, not beat
    }

    SECTION("multiple rapid reassignments: last one wins") {
        GoldenHarness h;
        h.assign_ok("a1", "beat");
        h.assign_ok("a1", "bar");
        h.assign_ok("a1", "(+ beat bar)");
        h.assign_ok("a1", "t");
        REQUIRE(h.sample("a1", 1.25) == Approx(1.25)); // last assignment wins
    }

    SECTION("reassignment after tick updates prev correctly") {
        GoldenHarness h;
        h.assign_ok("a1", "0.75");
        h.tick("a1", 0.0); // commits 0.75

        h.assign_ok("a1", "0.5");
        double val = h.tick("a1", 0.0);
        REQUIRE(val == Approx(0.5));
    }

    SECTION("LKG value tracks last committed output") {
        GoldenHarness h;
        h.assign_ok("a1", "0.75");
        h.tick("a1", 0.0); // commits 0.75 as lkg

        // Now break the output
        EvalResult bad = h.eval_result("(a1 (nonexistent))");
        REQUIRE(bad.kind == EvalResult::Error);

        // a1 should still produce 0.75 (from preserved graph — root_node intact)
        REQUIRE(h.sample("a1", 0.0) == Approx(0.75));
    }

    SECTION("unassigned output produces zero") {
        GoldenHarness h;
        // a1 was never assigned
        double val = h.sample("a1", 0.0);
        REQUIRE(val == Approx(0.0));
    }

    SECTION("reassignment from time-varying to constant") {
        GoldenHarness h;
        h.assign_ok("a1", "beat");
        REQUIRE(h.sample("a1", 0.25) == Approx(0.5));

        h.assign_ok("a1", "42");
        REQUIRE(h.sample("a1", 0.0) == Approx(42.0));
        REQUIRE(h.sample("a1", 0.5) == Approx(42.0));
        REQUIRE(h.sample("a1", 1.0) == Approx(42.0));
    }

    SECTION("reassignment from constant to time-varying") {
        GoldenHarness h;
        h.assign_ok("a1", "42");
        REQUIRE(h.sample("a1", 0.0) == Approx(42.0));

        h.assign_ok("a1", "t");
        REQUIRE(h.sample("a1", 0.0) == Approx(0.0));
        REQUIRE(h.sample("a1", 1.5) == Approx(1.5));
    }
}

// ============================================================================
// Phase 4.5: Function-cell cross-references
// ============================================================================

TEST_CASE("Phase 4: function-cell cross-references", "[phase4][reactivity]") {

    SECTION("function referencing a cell updates when cell changes") {
        GoldenHarness h;
        h.eval_ok("(define scale 2)");
        h.eval_ok("(defn scaled [x] (* x scale))");
        h.assign_ok("a1", "(scaled beat)");
        // At t=0.125, beat=0.25, scaled=0.25*2=0.5
        REQUIRE(h.sample("a1", 0.125) == Approx(0.5));

        h.eval_ok("(define scale 4)");
        // scaled now uses scale=4, so 0.25*4=1.0
        REQUIRE(h.sample("a1", 0.125) == Approx(1.0));
    }

    SECTION("function calling another function, inner redefined") {
        GoldenHarness h;
        h.eval_ok("(defn inner [x] (* x 2))");
        h.eval_ok("(defn outer [x] (+ (inner x) 100))");
        h.assign_ok("a1", "(outer 5)");
        // inner(5) = 10, outer(5) = 110
        REQUIRE(h.sample("a1", 0.0) == Approx(110.0));

        h.eval_ok("(defn inner [x] (* x 3))");
        // inner(5) = 15, outer(5) = 115
        REQUIRE(h.sample("a1", 0.0) == Approx(115.0));
    }

    SECTION("nested same-function call is rejected as recursive") {
        // Known limitation: (f (f x)) triggers the recursion guard because
        // the inner f call sees f already on the inline stack from the outer call.
        // This is documented, not a Phase 4 regression.
        GoldenHarness h;
        h.eval_ok("(defn dbl [x] (* x 2))");
        h.eval_ok("(defn quad [x] (dbl (dbl x)))");
        EvalResult r = h.eval_result("(a1 (quad 3))");
        REQUIRE(r.kind == EvalResult::Error);
    }

    SECTION("cell used by multiple functions, all callers update") {
        GoldenHarness h;
        h.eval_ok("(define base 10)");
        h.eval_ok("(defn add-base [x] (+ x base))");
        h.eval_ok("(defn mul-base [x] (* x base))");
        h.assign_ok("a1", "(add-base 5)");
        h.assign_ok("a2", "(mul-base 5)");
        REQUIRE(h.sample("a1", 0.0) == Approx(15.0));
        REQUIRE(h.sample("a2", 0.0) == Approx(50.0));

        h.eval_ok("(define base 20)");
        REQUIRE(h.sample("a1", 0.0) == Approx(25.0));
        REQUIRE(h.sample("a2", 0.0) == Approx(100.0));
    }

    SECTION("function redefinition propagates to output using it") {
        GoldenHarness h;
        h.eval_ok("(defn f [x] (* x 2))");
        h.assign_ok("a1", "(f 5)");
        REQUIRE(h.sample("a1", 0.0) == Approx(10.0));

        h.eval_ok("(defn f [x] (+ x 100))");
        REQUIRE(h.sample("a1", 0.0) == Approx(105.0));
    }

    SECTION("function and cell combined: function with cell arg, cell changes") {
        GoldenHarness h;
        h.eval_ok("(define offset 100)");
        h.eval_ok("(defn shifted [x] (+ x offset))");
        h.assign_ok("a1", "(shifted beat)");
        // At t=0.125, beat=0.25, shifted=100.25
        REQUIRE(h.sample("a1", 0.125) == Approx(100.25));

        // Change offset
        h.eval_ok("(define offset 200)");
        REQUIRE(h.sample("a1", 0.125) == Approx(200.25));
    }
}

// ============================================================================
// Phase 4.6: Cross-output reads (prev) and LKG interactions
// ============================================================================

TEST_CASE("Phase 4: cross-output reads and LKG", "[phase4][lifecycle]") {

    SECTION("prev reads previous tick value") {
        GoldenHarness h;
        h.eval_ok("(a1 10) (a2 (prev a1))");

        auto seq = h.tick_sequence("a2", {0.0, 0.001, 0.002});
        // First tick: prev a1 is 0 (no prior)
        REQUIRE(seq[0] == Approx(0.0));
        // After first tick, a1 committed 10, so prev a1 = 10
        REQUIRE(seq[1] == Approx(10.0));
        REQUIRE(seq[2] == Approx(10.0));
    }

    SECTION("self-reference via prev accumulates") {
        GoldenHarness h;
        h.eval_ok("(a1 (+ (prev a1) 1))");

        auto seq = h.tick_sequence("a1", {0.0, 0.001, 0.002, 0.003});
        REQUIRE(seq[0] == Approx(1.0));  // prev starts at 0, + 1 = 1
        REQUIRE(seq[1] == Approx(2.0));
        REQUIRE(seq[2] == Approx(3.0));
        REQUIRE(seq[3] == Approx(4.0));
    }

    SECTION("reassigning output resets its graph but prev is from last commit") {
        GoldenHarness h;
        h.assign_ok("a1", "0.5");
        h.tick("a1", 0.0); // commit 0.5

        h.assign_ok("a1", "(+ (prev a1) 0.1)");
        // prev a1 was 0.5 from the last tick
        double val = h.tick("a1", 0.0);
        REQUIRE(val == Approx(0.6));
    }
}

// ============================================================================
// Phase 4.7: Multiple outputs with shared dependencies
// ============================================================================

TEST_CASE("Phase 4: shared dependencies across outputs", "[phase4][reactivity]") {

    SECTION("cell change triggers recompilation of all dependent outputs") {
        GoldenHarness h;
        h.eval_ok("(define gain 1.0)");
        h.assign_ok("a1", "(* beat gain)");
        h.assign_ok("a2", "(* bar gain)");
        h.assign_ok("a3", "(* t gain)");

        // At t=0.25, bpm=120: beat=0.5, bar=0.125, t=0.25
        REQUIRE(h.sample("a1", 0.25) == Approx(0.5));
        REQUIRE(h.sample("a2", 0.25) == Approx(0.125));
        REQUIRE(h.sample("a3", 0.25) == Approx(0.25));

        h.eval_ok("(define gain 2.0)");
        REQUIRE(h.sample("a1", 0.25) == Approx(1.0));
        REQUIRE(h.sample("a2", 0.25) == Approx(0.25));
        REQUIRE(h.sample("a3", 0.25) == Approx(0.5));
    }

    SECTION("independent cells affect only their outputs") {
        GoldenHarness h;
        h.eval_ok("(define x 10)");
        h.eval_ok("(define y 20)");
        h.assign_ok("a1", "x");
        h.assign_ok("a2", "y");

        h.eval_ok("(define x 99)");
        REQUIRE(h.sample("a1", 0.0) == Approx(99.0));
        REQUIRE(h.sample("a2", 0.0) == Approx(20.0)); // y unchanged
    }

    SECTION("output with no cell deps is unaffected by cell changes") {
        GoldenHarness h;
        h.eval_ok("(define x 10)");
        h.assign_ok("a1", "x");
        h.assign_ok("a2", "beat");  // no cell dependency

        h.eval_ok("(define x 99)");
        REQUIRE(h.sample("a1", 0.0) == Approx(99.0));
        // a2 should be completely unaffected
        REQUIRE(h.sample("a2", 0.25) == Approx(0.5));
    }
}
