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

TEST_CASE("Unassign clears runtime and reactive health with the program",
          "[health][unassign][transaction]") {
    Harness h;

    h.eval_ok("(define health-unassign-dep 1)");
    h.eval_ok("(a1 (* health-unassign-dep (* t 1e308)))");
    REQUIRE(h.tick(0, 2.0) == Approx(0.0));
    REQUIRE((h.engine.pool.runtime_fallback_mask & 1u) != 0);

    h.eval_ok("(defn health-unassign-dep [x] x)");
    REQUIRE(h.engine.output_compile_diagnostics[0].active);

    h.eval_ok("(unassign a1)");
    REQUIRE(output_health(h.engine.pool, 0) == OutputHealth::Idle);
    REQUIRE((h.engine.pool.runtime_fallback_mask & 1u) == 0);
    REQUIRE_FALSE(h.engine.output_compile_diagnostics[0].active);
}

TEST_CASE("State compaction remaps failure health and attributed source together",
          "[health][state][compaction]") {
    Harness h;
    auto& symbols = SymbolIntern::getInstance();
    SymbolID dropped = symbols.intern(String("health-remap-dropped"));
    SymbolID kept = symbols.intern(String("health-remap-kept"));

    h.eval_ok("(define health-remap-rate 0)");
    h.eval_ok("(defstate health-remap-dropped 0 (+ health-remap-dropped 1))");
    h.eval_ok("(defstate health-remap-kept 5 (/ 1 health-remap-rate))");
    h.eval_ok("(a1 health-remap-dropped)");
    h.eval_ok("(a2 health-remap-kept)");

    const uint16_t old_kept_slot = h.engine.cells.cells[kept].data_table_id;
    REQUIRE(old_kept_slot == 1);
    h.tick(0, 0.0);
    REQUIRE((h.engine.pool.state_update_failure_mask &
             ((uint64_t)1 << old_kept_slot)) != 0);

    // Retiring the lower slot compacts the still-failing state to slot zero.
    h.eval_ok("(define health-remap-dropped 7)");
    REQUIRE(h.engine.cells.cells[dropped].flags == 0);
    REQUIRE(h.engine.pool.state_slot_count == 1);
    REQUIRE(h.engine.cells.cells[kept].data_table_id == 0);
    REQUIRE(h.engine.state_sources[0].state_symbol == kept);
    REQUIRE((h.engine.pool.state_update_failure_mask & 1u) != 0);
    REQUIRE((h.engine.pool.state_update_failure_mask & ~uint64_t{1}) == 0);

    const StateUpdateSource& source = h.engine.state_sources[0];
    const char* text = h.engine.arena.read(source.arena_offset);
    REQUIRE(text != nullptr);
    REQUIRE(std::string(text, source.arena_length) ==
            "(/ 1 health-remap-rate)");

    h.eval_ok("(define health-remap-rate 2)");
    h.tick(1, 1.0);
    REQUIRE(h.engine.pool.state_values[0] == Approx(0.5));
    REQUIRE(h.engine.pool.state_update_failure_mask == 0);
}

TEST_CASE("Live-edit option reorder, rejected variant change, and reclamation are atomic",
          "[health][live-edit][transaction]") {
    Harness h;

    h.eval_ok("(a1 (live-edit :beta :id \"health-mode\" :options [:alpha :beta]))");
    REQUIRE(h.engine.pool.live_slot_count == 1);
    h.engine.pool.set_live_slot_value("health-mode", 1.0);
    REQUIRE(h.tick(0, 0.0) == Approx(1.0));

    h.eval_ok("(a1 (live-edit :alpha :id \"health-mode\" :options [:beta :alpha]))");
    const NodePool::LiveSlot before = h.engine.pool.live_slots[0];
    REQUIRE(before.variant == NodePool::SlotVariant::Keyword);
    REQUIRE(before.value == Approx(0.0));
    REQUIRE(std::string(before.options[0]) == ":beta");

    EvalResult rejected = h.eval(
        "(a1 (live-edit 0.5 :id \"health-mode\" :min 0 :max 1) 99)");
    REQUIRE(rejected.kind == EvalResult::Error);
    REQUIRE(std::memcmp(&h.engine.pool.live_slots[0], &before,
                        sizeof(before)) == 0);
    REQUIRE(h.tick(0, 1.0) == Approx(0.0));

    h.eval_ok("(unassign a1)");
    REQUIRE(h.engine.pool.live_slot_count == 0);
    h.eval_ok("(a1 (live-edit 0.25 :id \"health-mode\" :min 0 :max 1))");
    REQUIRE(h.engine.pool.live_slot_count == 1);
    REQUIRE(h.engine.pool.live_slots[0].variant ==
            NodePool::SlotVariant::Numeric);
    REQUIRE(h.engine.pool.live_slots[0].value == Approx(0.25));
}

TEST_CASE("One dependency mutation rejects every affected consumer independently",
          "[health][reactive][multi-consumer]") {
    Harness h;

    h.eval_ok("(define health-shared-dep 2)");
    h.eval_ok("(a1 (+ health-shared-dep 1))");
    h.eval_ok("(a2 (* health-shared-dep 3))");
    const uint16_t root_a1 = h.engine.pool.outputs[0].root_node;
    const uint16_t root_a2 = h.engine.pool.outputs[1].root_node;
    REQUIRE(h.tick(0, 0.0) == Approx(3.0));
    REQUIRE(h.outputs[1] == Approx(6.0));

    h.eval_ok("(defn health-shared-dep [x] x)");
    REQUIRE(h.engine.output_compile_diagnostics[0].active);
    REQUIRE(h.engine.output_compile_diagnostics[1].active);
    REQUIRE(h.engine.pool.outputs[0].root_node == root_a1);
    REQUIRE(h.engine.pool.outputs[1].root_node == root_a2);
    REQUIRE(h.tick(0, 1.0) == Approx(3.0));
    REQUIRE(h.outputs[1] == Approx(6.0));
}
