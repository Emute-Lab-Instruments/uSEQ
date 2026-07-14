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
};

} // namespace

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
