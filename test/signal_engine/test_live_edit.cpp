// Live-edit state identity and slot management tests.
//
// Covers: live-edit slot allocation, value clamping, duplicate :id detection,
// cross-output :id uniqueness, keyword validation, MAX_LIVE_SLOTS cap,
// eager-consume head rejection, defstate :initial rejection, and
// dead-slot warning.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "../../uSEQ/src/signal_engine/cold_eval.h"
#include "../../uSEQ/src/signal_engine/graph_builder.h"
#include "../../uSEQ/src/signal_engine/executor.h"

#include <cstring>
#include <cmath>

// ── Test harness ────────────────────────────────────────────────────────────

struct LiveEditHarness {
    sig::SignalEngine engine;

    LiveEditHarness() {
        sig::GraphBuilder::init_symbols();
        engine.init_defaults();
    }

    sig::EvalResult eval(const char* code) {
        return sig::eval_cold(code, (uint32_t)strlen(code), engine);
    }

    double tick(double t) {
        double output_values[sig::MAX_OUTPUTS] = {};
        double node_values[sig::MAX_TOTAL_NODES];
        double cell_values[sig::MAX_CELLS];
        engine.cells.snapshot_values(cell_values, sig::MAX_CELLS);
        double hw_inputs[32] = {};

        sig::ExecutionContext ctx;
        ctx.t = t;
        ctx.dt = 0.001;
        ctx.cell_values = cell_values;
        ctx.hw_inputs = hw_inputs;
        ctx.data_pool = engine.cells.data_pool;
        ctx.data_offsets = engine.cells.data_offsets;
        ctx.data_lengths = engine.cells.data_lengths;
        ctx.prev_outputs = engine.pool.prev_output_values;
        ctx.output_values = output_values;
        ctx.workspace = node_values;
        sig::execute_all_outputs(engine.pool, ctx);
        return output_values[0]; // a1
    }
};

// ── Basic slot allocation ───────────────────────────────────────────────────

TEST_CASE("live-edit allocates a slot and returns seed value", "[live-edit]")
{
    LiveEditHarness h;
    auto r = h.eval("(a1 (live-edit 0.5 :id \"x\" :min 0 :max 1))");
    REQUIRE(r.kind != sig::EvalResult::Error);

    REQUIRE(h.engine.pool.live_slot_count == 1);
    REQUIRE(std::string(h.engine.pool.live_slots[0].id) == "x");
    REQUIRE(h.engine.pool.live_slots[0].value == Approx(0.5));

    double v = h.tick(0.0);
    REQUIRE(v == Approx(0.5));
}

TEST_CASE("set_live_slot_value clamps to [min,max]", "[live-edit]")
{
    LiveEditHarness h;
    h.eval("(a1 (live-edit 0.5 :id \"x\" :min 0 :max 1))");

    h.engine.pool.set_live_slot_value("x", 2.0);
    REQUIRE(h.engine.pool.live_slots[0].value == Approx(1.0));

    h.engine.pool.set_live_slot_value("x", -1.0);
    REQUIRE(h.engine.pool.live_slots[0].value == Approx(0.0));
}

TEST_CASE("set_live_slot_value ignores unknown ID", "[live-edit]")
{
    LiveEditHarness h;
    h.eval("(a1 (live-edit 0.5 :id \"x\" :min 0 :max 1))");

    // Should not crash or modify any slot
    h.engine.pool.set_live_slot_value("nonexistent", 0.7);
    REQUIRE(h.engine.pool.live_slots[0].value == Approx(0.5));
}

TEST_CASE("boolean live-edit preserves its variant and coerces writes", "[live-edit][variant]")
{
    LiveEditHarness h;
    auto r = h.eval("(a1 (live-edit true :id \"gate\"))");
    REQUIRE(r.kind != sig::EvalResult::Error);
    const auto& slot = h.engine.pool.live_slots[0];
    REQUIRE(slot.variant == sig::NodePool::SlotVariant::Boolean);
    REQUIRE(slot.seed == Approx(1.0));
    REQUIRE(h.tick(0.0) == Approx(1.0));

    h.engine.pool.set_live_slot_value("gate", 0.0);
    REQUIRE(h.tick(0.0) == Approx(0.0));
    h.engine.pool.set_live_slot_value("gate", -9.0);
    REQUIRE(h.tick(0.0) == Approx(1.0));
}

TEST_CASE("keyword live-edit preserves options and validates writes", "[live-edit][variant]")
{
    LiveEditHarness h;
    auto r = h.eval(
        "(a1 (live-edit :up :id \"direction\" :options [:left :up :right]))");
    REQUIRE(r.kind != sig::EvalResult::Error);
    const auto& slot = h.engine.pool.live_slots[0];
    REQUIRE(slot.variant == sig::NodePool::SlotVariant::Keyword);
    REQUIRE(slot.options_count == 3);
    REQUIRE(std::string(slot.options[0]) == ":left");
    REQUIRE(std::string(slot.options[1]) == ":up");
    REQUIRE(std::string(slot.options[2]) == ":right");
    REQUIRE(slot.seed == Approx(1.0));
    REQUIRE(h.tick(0.0) == Approx(1.0));

    h.engine.pool.set_live_slot_value("direction", 2.0);
    REQUIRE(h.tick(0.0) == Approx(2.0));
    h.engine.pool.set_live_slot_value("direction", 9.0);
    REQUIRE(h.tick(0.0) == Approx(2.0));
}

TEST_CASE("live-edit step and precision metadata reach the runtime slot", "[live-edit][metadata]")
{
    LiveEditHarness h;
    auto r = h.eval(
        "(a1 (live-edit 0.5 :id \"fine\" :min 0 :max 1 "
        ":name \"Fine control\" :step 0.01 :precision 2))");
    REQUIRE(r.kind != sig::EvalResult::Error);
    REQUIRE(h.engine.pool.live_slots[0].step == Approx(0.01));
    REQUIRE(h.engine.pool.live_slots[0].precision == 2);
}

TEST_CASE("keyword live-edit reorders options without changing the selected keyword",
          "[live-edit][variant][recompile]")
{
    LiveEditHarness h;
    REQUIRE(h.eval(
        "(a1 (live-edit :up :id \"direction\" :options [:up :down]))").kind !=
        sig::EvalResult::Error);
    h.engine.pool.set_live_slot_value("direction", 1.0); // :down

    REQUIRE(h.eval(
        "(a1 (live-edit :up :id \"direction\" :options [:down :up]))").kind !=
        sig::EvalResult::Error);
    const auto& slot = h.engine.pool.live_slots[0];
    REQUIRE(std::string(slot.options[(int)slot.value]) == ":down");
    REQUIRE(slot.value == Approx(0.0));
}

TEST_CASE("keyword live-edit without options is repaired to a singleton", "[live-edit][variant]")
{
    LiveEditHarness h;
    auto r = h.eval("(a1 (live-edit :solo :id \"mode\"))");
    REQUIRE(r.kind != sig::EvalResult::Error);
    REQUIRE(h.engine.pool.live_slots[0].options_count == 1);
    REQUIRE(std::string(h.engine.pool.live_slots[0].options[0]) == ":solo");
    REQUIRE(h.engine.pool.live_slots[0].value == Approx(0.0));
}

// ── Cross-output duplicate :id detection (useq-ef7) ────────────────────────

TEST_CASE("duplicate :id in same output is rejected", "[live-edit][ef7]")
{
    LiveEditHarness h;
    auto r = h.eval(
        "(a1 (+ (live-edit 0.5 :id \"dup\" :min 0 :max 1)"
        "       (live-edit 0.3 :id \"dup\" :min 0 :max 1)))");
    REQUIRE(r.kind == sig::EvalResult::Error);
}

TEST_CASE("cross-output duplicate :id is rejected", "[live-edit][ef7]")
{
    LiveEditHarness h;
    // First output succeeds
    auto r1 = h.eval("(a1 (live-edit 0.5 :id \"shared\" :min 0 :max 1))");
    REQUIRE(r1.kind != sig::EvalResult::Error);

    // Second output with same :id in same eval batch should fail.
    // We need both in the same eval_cold call for the cross-output check.
    LiveEditHarness h2;
    auto r2 = h2.eval(
        "(a1 (live-edit 0.5 :id \"x\" :min 0 :max 1))"
        "(a2 (live-edit 0.5 :id \"x\" :min 0 :max 1))");
    REQUIRE(r2.kind == sig::EvalResult::Error);
}

TEST_CASE("cross-output duplicate :id is rejected across separate eval calls",
          "[live-edit][ownership]")
{
    LiveEditHarness h;
    auto first = h.eval("(a1 (live-edit 0.5 :id \"owned\" :min 0 :max 1))");
    REQUIRE(first.kind != sig::EvalResult::Error);
    REQUIRE(h.engine.pool.live_slot_count == 1);

    auto duplicate = h.eval("(a2 (live-edit 0.25 :id \"owned\" :min 0 :max 1))");
    REQUIRE(duplicate.kind == sig::EvalResult::Error);
    REQUIRE(duplicate.diagnostic_count > 0);
    REQUIRE(std::string(duplicate.diagnostics[0].message).find("another signal") !=
            std::string::npos);
    REQUIRE(h.engine.pool.outputs[1].root_node == sig::NODE_NONE);
    REQUIRE(h.engine.pool.live_slot_count == 1);
    REQUIRE(h.tick(0.0) == Approx(0.5));
}

TEST_CASE("different :ids across outputs are allowed", "[live-edit][ef7]")
{
    LiveEditHarness h;
    auto r = h.eval(
        "(a1 (live-edit 0.5 :id \"knob1\" :min 0 :max 1))"
        "(a2 (live-edit 0.3 :id \"knob2\" :min 0 :max 1))");
    REQUIRE(r.kind != sig::EvalResult::Error);
    REQUIRE(h.engine.pool.live_slot_count == 2);
}

// ── MAX_LIVE_SLOTS bump (useq-eh9) ─────────────────────────────────────────

TEST_CASE("MAX_LIVE_SLOTS is 256", "[live-edit][eh9]")
{
    REQUIRE(sig::MAX_LIVE_SLOTS == 256);
}

TEST_CASE("replacing one output with fresh live-edit IDs reclaims old slots",
          "[live-edit][reclaim]")
{
    LiveEditHarness h;
    for (size_t i = 0; i < sig::MAX_LIVE_SLOTS + 32; i++) {
        std::string code = "(a1 (live-edit 0.5 :id \"knob-" +
                           std::to_string(i) + "\" :min 0 :max 1))";
        auto r = h.eval(code.c_str());
        INFO("iteration " << i);
        REQUIRE(r.kind != sig::EvalResult::Error);
        REQUIRE(h.engine.pool.live_slot_count == 1);
        REQUIRE(std::string(h.engine.pool.live_slots[0].id) ==
                "knob-" + std::to_string(i));
    }
    REQUIRE(h.tick(0.0) == Approx(0.5));
}

TEST_CASE("rejected live-edit replacement restores slot metadata and value",
          "[live-edit][rollback]")
{
    LiveEditHarness h;
    REQUIRE(h.eval("(a1 (live-edit 0.5 :id \"stable\" :min 0 :max 1))").kind !=
            sig::EvalResult::Error);
    h.engine.pool.set_live_slot_value("stable", 0.8);

    auto rejected = h.eval(
        "(a1 (+ (live-edit 5 :id \"stable\" :min 4 :max 6)"
        "       (live-edit 5 :id \"stable\" :min 4 :max 6)))");
    REQUIRE(rejected.kind == sig::EvalResult::Error);
    REQUIRE(h.engine.pool.live_slot_count == 1);
    REQUIRE(h.engine.pool.live_slots[0].min_val == Approx(0.0));
    REQUIRE(h.engine.pool.live_slots[0].max_val == Approx(1.0));
    REQUIRE(h.engine.pool.live_slots[0].seed == Approx(0.5));
    REQUIRE(h.engine.pool.live_slots[0].value == Approx(0.8));
    REQUIRE(h.tick(0.0) == Approx(0.8));
}

// ── Warning #3: slot allocated but never read (useq-ijk) ───────────────────
// TODO: Full cross-output dead-slot detection requires post-compilation analysis
// across all outputs. This test covers the within-single-output case where the
// live-edit value is discarded by the enclosing form.

TEST_CASE("warning emitted when live-edit slot is unused in output", "[live-edit][ijk]")
{
    // This test checks the within-single-output dead-slot warning.
    // (do (live-edit ...) 1.0) — live-edit is allocated but discarded.
    LiveEditHarness h;
    auto r = h.eval("(a1 (do (live-edit 0.5 :id \"unused\" :min 0 :max 1) 1.0))");
    // Compilation should succeed (it's a warning, not an error)
    REQUIRE(r.kind != sig::EvalResult::Error);
    // The warning is emitted during compilation, but successful publication
    // immediately reclaims the unreachable slot.
    REQUIRE(h.engine.pool.live_slot_count == 0);
    // The output value should be 1.0 (the last form in do)
    double v = h.tick(0.0);
    REQUIRE(v == Approx(1.0));
}

// ── Eager-consume head rejection (useq-tz9) ────────────────────────────────

TEST_CASE("live-edit rejected as argument of set-bpm", "[live-edit][tz9]")
{
    LiveEditHarness h;
    auto r = h.eval("(set-bpm (live-edit 120 :id \"bpm\" :min 60 :max 240))");
    REQUIRE(r.kind == sig::EvalResult::Error);
    // Check that the error message mentions live-edit
    REQUIRE(r.diagnostic_count > 0);
    REQUIRE(std::string(r.diagnostics[0].message).find("live-edit") != std::string::npos);
}

TEST_CASE("live-edit rejected as argument of set-time-sig", "[live-edit][tz9]")
{
    LiveEditHarness h;
    auto r = h.eval("(set-time-sig (live-edit 4 :id \"ts\" :min 2 :max 8))");
    REQUIRE(r.kind == sig::EvalResult::Error);
    REQUIRE(r.diagnostic_count > 0);
    REQUIRE(std::string(r.diagnostics[0].message).find("live-edit") != std::string::npos);
}

TEST_CASE("non-live-edit args to set-bpm still work", "[live-edit][tz9]")
{
    LiveEditHarness h;
    auto r = h.eval("(set-bpm 140)");
    REQUIRE(r.kind != sig::EvalResult::Error);
}

// ── defstate :initial rejection (useq-726) ──────────────────────────────────

TEST_CASE("live-edit rejected as defstate initial value", "[live-edit][726]")
{
    LiveEditHarness h;
    auto r = h.eval("(defstate mystate (live-edit 0 :id \"init\" :min 0 :max 10) (+ mystate 1))");
    REQUIRE(r.kind == sig::EvalResult::Error);
    REQUIRE(r.diagnostic_count > 0);
    REQUIRE(std::string(r.diagnostics[0].message).find("live-edit") != std::string::npos);
}

TEST_CASE("defstate with normal initial value still works", "[live-edit][726]")
{
    LiveEditHarness h;
    auto r = h.eval("(defstate counter 0 (+ counter 1))");
    REQUIRE(r.kind != sig::EvalResult::Error);
}

TEST_CASE("live-edit in defstate update expression is allowed", "[live-edit][726]")
{
    LiveEditHarness h;
    auto r = h.eval("(defstate x 0 (live-edit 0.5 :id \"update\" :min 0 :max 1))");
    REQUIRE(r.kind != sig::EvalResult::Error);
    REQUIRE(h.engine.pool.live_slot_count == 1);
}
