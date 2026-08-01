#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "src/signal_engine/signal_engine.h"
#include "src/modulisp/lisp/symbol_intern.h"

#include <cmath>
#include <string>

using namespace sig;

namespace {

struct Harness {
    SignalEngine engine;
    double outputs[MAX_OUTPUTS] = {};
    double workspace[MAX_TOTAL_NODES] = {};

    Harness() {
        engine.init_defaults(120.0, 4);
        set_failure_mode(FailureMode::LkgFallback);
    }

    ~Harness() { set_failure_mode(FailureMode::LkgFallback); }

    EvalResult eval(const std::string& code) {
        return eval_cold(code.c_str(), (uint32_t)code.size(), engine);
    }

    void eval_ok(const std::string& code) {
        EvalResult r = eval(code);
        INFO("code: " << code);
        if (r.kind == EvalResult::Error && r.diagnostic_count > 0)
            INFO("diagnostic: " << (r.diagnostics[0].message
                ? r.diagnostics[0].message : ""));
        REQUIRE(r.kind != EvalResult::Error);
    }

    double tick(uint16_t output_index, double t, double dt = 0.001) {
        double cells[MAX_CELLS];
        double inputs[32] = {};
        engine.cells.snapshot_values(cells, MAX_CELLS);
        for (double& value : outputs) value = 0.0;

        ExecutionContext ctx{t, dt, cells, inputs,
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

constexpr const char* OVERFLOW = "(a1 (* (* t 1e308) 1e308))";

} // namespace

TEST_CASE("First failure has Error health until a finite root establishes LKG",
          "[health][output][lkg]") {
    Harness h;
    REQUIRE(output_health(h.engine.pool, 0) == OutputHealth::Idle);
    h.eval_ok(OVERFLOW);

    REQUIRE(h.engine.pool.outputs[0].valid);
    REQUIRE_FALSE(h.engine.pool.outputs[0].has_lkg);
    REQUIRE(output_health(h.engine.pool, 0) == OutputHealth::Running);

    REQUIRE(h.tick(0, 1.0) == Approx(0.0));
    REQUIRE_FALSE(h.engine.pool.outputs[0].has_lkg);
    REQUIRE(output_health(h.engine.pool, 0) == OutputHealth::Error);

    REQUIRE(h.tick(0, 0.0) == Approx(0.0));
    REQUIRE(h.engine.pool.outputs[0].has_lkg);
    REQUIRE(output_health(h.engine.pool, 0) == OutputHealth::Running);

    REQUIRE(h.tick(0, 1.0) == Approx(0.0));
    REQUIRE(output_health(h.engine.pool, 0) == OutputHealth::Fallback);

    // Successful replacement clears the superseded program's runtime health
    // immediately while retaining its finite LKG as a safety net.
    h.eval_ok("(a1 7)");
    REQUIRE(output_health(h.engine.pool, 0) == OutputHealth::Running);
    REQUIRE((h.engine.pool.runtime_fallback_mask & 1u) == 0);
    REQUIRE(h.engine.pool.outputs[0].has_lkg);
}

TEST_CASE("Non-finite named-state update holds value and clears its diagnostic on recovery",
          "[health][state][runtime]") {
    Harness h;
    auto& symbols = SymbolIntern::getInstance();
    SymbolID state_symbol = symbols.intern(String("health-state"));

    h.eval_ok("(define health-rate 1)");
    h.eval_ok("(defstate health-state 0 (/ 1 health-rate))");
    uint16_t slot = h.engine.cells.cells[state_symbol].data_table_id;
    REQUIRE(h.engine.state_sources[slot].state_symbol == state_symbol);

    h.tick(0, 0.0);
    REQUIRE(h.engine.pool.state_values[slot] == Approx(1.0));
    REQUIRE((h.engine.pool.state_update_failure_mask &
             ((uint64_t)1 << slot)) == 0);

    h.eval_ok("(define health-rate 0)");
    double held = h.engine.pool.state_values[slot];
    h.tick(0, 1.0);
    REQUIRE(h.engine.pool.state_values[slot] == Approx(held));
    REQUIRE((h.engine.pool.state_update_failure_mask &
             ((uint64_t)1 << slot)) != 0);

    h.eval_ok("(define health-rate 2)");
    h.tick(0, 2.0);
    REQUIRE(h.engine.pool.state_values[slot] == Approx(0.5));
    REQUIRE((h.engine.pool.state_update_failure_mask &
             ((uint64_t)1 << slot)) == 0);
}

TEST_CASE("Rejected reactive output candidate is attributed and clears on repair",
          "[health][reactive][output]") {
    Harness h;
    auto& symbols = SymbolIntern::getInstance();
    SymbolID dep = symbols.intern(String("health-reactive-dep"));

    h.eval_ok("(define health-reactive-dep 1)");
    h.eval_ok("(a1 (+ health-reactive-dep 0.25))");
    uint16_t old_root = h.engine.pool.outputs[0].root_node;
    REQUIRE(h.tick(0, 0.0) == Approx(1.25));

    h.eval_ok("(defn health-reactive-dep [x] x)");
    const ActiveCompileDiagnostic& active =
        h.engine.output_compile_diagnostics[0];
    REQUIRE(active.active);
    REQUIRE(active.triggered_by == dep);
    REQUIRE(active.diagnostic.message != nullptr);
    REQUIRE(h.engine.pool.outputs[0].root_node == old_root);
    REQUIRE(h.tick(0, 1.0) == Approx(1.25));

    h.eval_ok("(define health-reactive-dep 2)");
    REQUIRE_FALSE(h.engine.output_compile_diagnostics[0].active);
    REQUIRE(h.tick(0, 2.0) == Approx(2.25));
}

TEST_CASE("Rejected reactive state update is attributed and keeps its prior writer",
          "[health][reactive][state]") {
    Harness h;
    auto& symbols = SymbolIntern::getInstance();
    SymbolID dep = symbols.intern(String("health-state-dep"));
    SymbolID state_symbol = symbols.intern(String("health-reactive-state"));

    h.eval_ok("(define health-state-dep 1)");
    h.eval_ok("(defstate health-reactive-state 0 (+ health-reactive-state health-state-dep))");
    uint16_t slot = h.engine.cells.cells[state_symbol].data_table_id;
    uint16_t old_root = h.engine.pool.state_update_roots[slot];
    h.tick(0, 0.0);
    REQUIRE(h.engine.pool.state_values[slot] == Approx(1.0));

    h.eval_ok("(defn health-state-dep [x] x)");
    const ActiveCompileDiagnostic& active =
        h.engine.state_compile_diagnostics[slot];
    REQUIRE(active.active);
    REQUIRE(active.triggered_by == dep);
    REQUIRE(active.diagnostic.message != nullptr);
    REQUIRE(h.engine.pool.state_update_roots[slot] == old_root);
    h.tick(0, 1.0);
    REQUIRE(h.engine.pool.state_values[slot] == Approx(2.0));

    h.eval_ok("(define health-state-dep 2)");
    REQUIRE_FALSE(h.engine.state_compile_diagnostics[slot].active);
    h.tick(0, 2.0);
    REQUIRE(h.engine.pool.state_values[slot] == Approx(4.0));
}
