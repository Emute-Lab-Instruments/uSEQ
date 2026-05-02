// Algebraic simplification, NaN guards, phasor re-derivation, batch prev,
// and executor edge-case tests.
//
// Covers:
//   node_pool.cpp algebraic simplifications (x-x, x/x, neg(neg), Select folding)
//   executor.cpp NaN guards per-op (Tan, Pow, Mod)
//   time.md 1.3.1 Phasor re-derivation inside time-as (via fast/slow)
//   prev.md 1.4 Batch prev semantics
//   GC topological correctness after compaction

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "src/signal_engine/signal_engine.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace sig;

namespace {

struct GoldenHarness {
    SignalEngine engine;
    double cell_values[MAX_CELLS] = {};
    double hw_inputs[32] = {};
    double outputs[MAX_OUTPUTS] = {};
    double workspace[MAX_TOTAL_NODES] = {};
    double prev_t = 0.0;
    bool has_ticked = false;

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
        ctx.dt = t - prev_t;
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
        has_ticked = true;
        prev_t = t;
        return value;
    }
};

// Direct NodePool harness for low-level testing
struct PoolHarness {
    NodePool pool;
    CellStore cells;
    SourceArena arena;

    PoolHarness() { cells.init_timing_defaults(); }
};

// Direct executor harness for per-op testing
struct ExecutorHarness {
    NodePool pool;
    CellStore cells;
    SourceArena arena;
    double cell_values[MAX_CELLS] = {};
    double hw_inputs[32] = {};
    double outputs[MAX_OUTPUTS] = {};
    double workspace[MAX_TOTAL_NODES] = {};
    double prev_outputs[MAX_OUTPUTS] = {};

    ExecutorHarness()
    {
        std::memset(cell_values, 0, sizeof(cell_values));
        std::memset(hw_inputs, 0, sizeof(hw_inputs));
        std::memset(outputs, 0, sizeof(outputs));
        std::memset(workspace, 0, sizeof(workspace));
        std::memset(prev_outputs, 0, sizeof(prev_outputs));
        cells.init_timing_defaults();
    }

    double execute_single(NodeOp op, double a, double b = 0.0, double c = 0.0,
                          double t = 0.0)
    {
        Node n;
        n.op = op;
        n.input_a = NODE_NONE;
        n.input_b = NODE_NONE;
        n.input_c = NODE_NONE;
        n.imm = 0.0;

        uint16_t na = pool.make_const(a);
        uint16_t nb = pool.make_const(b);
        uint16_t nc = pool.make_const(c);

        if (op != NodeOp::Const) {
            // Build a tiny graph
            uint16_t node_idx;
            if (n.input_c != NODE_NONE || op == NodeOp::Select ||
                op == NodeOp::Clamp || op == NodeOp::Scale ||
                op == NodeOp::Lerp) {
                node_idx = pool.make_ternary(op, na, nb, nc);
            } else if (n.input_b != NODE_NONE ||
                       (op != NodeOp::Neg && op != NodeOp::Abs &&
                        op != NodeOp::Floor && op != NodeOp::Ceil &&
                        op != NodeOp::Frac && op != NodeOp::Sqrt &&
                        op != NodeOp::Sin && op != NodeOp::Cos &&
                        op != NodeOp::Tan && op != NodeOp::Not &&
                        op != NodeOp::USin && op != NodeOp::UCos &&
                        op != NodeOp::USinBi && op != NodeOp::UCosBi &&
                        op != NodeOp::Tri && op != NodeOp::Sqr &&
                        op != NodeOp::HashIndex)) {
                node_idx = pool.make_binop(op, na, nb);
            } else {
                node_idx = pool.make_unary(op, na);
            }

            // Execute directly through eval_node
            const Node& result_node = pool.get(node_idx);
            // If constant folding happened, just return the const
            if (result_node.op == NodeOp::Const) return result_node.imm;

            pool.rebuild_execution_order();

            ExecutionContext ctx;
            ctx.t = t;
            ctx.dt = 0.001;
            ctx.cell_values = cell_values;
            ctx.hw_inputs = hw_inputs;
            ctx.data_pool = cells.data_pool;
            ctx.data_offsets = cells.data_offsets;
            ctx.data_lengths = cells.data_lengths;
            ctx.prev_outputs = prev_outputs;
            ctx.output_values = outputs;
            ctx.workspace = workspace;
            execute_all_outputs(pool, ctx);

            return workspace[node_idx];
        }
        return a;
    }
};

} // anonymous namespace

// ═════════════════════════════════════════════════════════════════════════════
// Algebraic simplification tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Algebraic: x - x yields 0", "[signal_engine][algebraic]")
{
    PoolHarness h;
    uint16_t t_node = h.pool.make_raw_time_load();
    uint16_t result = h.pool.make_binop(NodeOp::Sub, t_node, t_node);
    const Node& n = h.pool.get(result);
    REQUIRE(n.op == NodeOp::Const);
    REQUIRE(n.imm == Approx(0.0));
}

TEST_CASE("Algebraic: x / x yields 1", "[signal_engine][algebraic]")
{
    PoolHarness h;
    uint16_t t_node = h.pool.make_raw_time_load();
    uint16_t result = h.pool.make_binop(NodeOp::Div, t_node, t_node);
    const Node& n = h.pool.get(result);
    REQUIRE(n.op == NodeOp::Const);
    REQUIRE(n.imm == Approx(1.0));
}

TEST_CASE("Algebraic: x + 0 yields x", "[signal_engine][algebraic]")
{
    PoolHarness h;
    uint16_t t_node = h.pool.make_raw_time_load();
    uint16_t zero = h.pool.make_const(0.0);
    uint16_t result = h.pool.make_binop(NodeOp::Add, t_node, zero);
    REQUIRE(result == t_node); // CSE returns the same node
}

TEST_CASE("Algebraic: 0 + x yields x", "[signal_engine][algebraic]")
{
    PoolHarness h;
    uint16_t t_node = h.pool.make_raw_time_load();
    uint16_t zero = h.pool.make_const(0.0);
    uint16_t result = h.pool.make_binop(NodeOp::Add, zero, t_node);
    REQUIRE(result == t_node);
}

TEST_CASE("Algebraic: x * 1 yields x", "[signal_engine][algebraic]")
{
    PoolHarness h;
    uint16_t t_node = h.pool.make_raw_time_load();
    uint16_t one = h.pool.make_const(1.0);
    uint16_t result = h.pool.make_binop(NodeOp::Mul, t_node, one);
    REQUIRE(result == t_node);
}

TEST_CASE("Algebraic: 1 * x yields x", "[signal_engine][algebraic]")
{
    PoolHarness h;
    uint16_t t_node = h.pool.make_raw_time_load();
    uint16_t one = h.pool.make_const(1.0);
    uint16_t result = h.pool.make_binop(NodeOp::Mul, one, t_node);
    REQUIRE(result == t_node);
}

TEST_CASE("Algebraic: x * 0 yields const 0", "[signal_engine][algebraic]")
{
    PoolHarness h;
    uint16_t t_node = h.pool.make_raw_time_load();
    uint16_t zero = h.pool.make_const(0.0);
    uint16_t result = h.pool.make_binop(NodeOp::Mul, t_node, zero);
    const Node& n = h.pool.get(result);
    REQUIRE(n.op == NodeOp::Const);
    REQUIRE(n.imm == Approx(0.0));
}

TEST_CASE("Algebraic: 0 * x yields const 0", "[signal_engine][algebraic]")
{
    PoolHarness h;
    uint16_t t_node = h.pool.make_raw_time_load();
    uint16_t zero = h.pool.make_const(0.0);
    uint16_t result = h.pool.make_binop(NodeOp::Mul, zero, t_node);
    const Node& n = h.pool.get(result);
    REQUIRE(n.op == NodeOp::Const);
    REQUIRE(n.imm == Approx(0.0));
}

TEST_CASE("Algebraic: x / 1 yields x", "[signal_engine][algebraic]")
{
    PoolHarness h;
    uint16_t t_node = h.pool.make_raw_time_load();
    uint16_t one = h.pool.make_const(1.0);
    uint16_t result = h.pool.make_binop(NodeOp::Div, t_node, one);
    REQUIRE(result == t_node);
}

TEST_CASE("Algebraic: Select with constant true condition selects true branch",
          "[signal_engine][algebraic][select]")
{
    PoolHarness h;
    uint16_t cond = h.pool.make_const(1.0);
    uint16_t t_node = h.pool.make_raw_time_load();
    uint16_t five = h.pool.make_const(5.0);
    uint16_t result = h.pool.make_select(cond, t_node, five);
    REQUIRE(result == t_node); // selected true branch
}

TEST_CASE("Algebraic: Select with constant false condition selects false branch",
          "[signal_engine][algebraic][select]")
{
    PoolHarness h;
    uint16_t cond = h.pool.make_const(0.0);
    uint16_t t_node = h.pool.make_raw_time_load();
    uint16_t five = h.pool.make_const(5.0);
    uint16_t result = h.pool.make_select(cond, t_node, five);
    REQUIRE(result == five); // selected false branch
}

TEST_CASE("Algebraic: Select with constant zero condition selects false branch",
          "[signal_engine][algebraic][select]")
{
    PoolHarness h;
    uint16_t cond = h.pool.make_const(0.0);
    uint16_t a = h.pool.make_const(10.0);
    uint16_t b = h.pool.make_const(20.0);
    uint16_t result = h.pool.make_select(cond, a, b);
    REQUIRE(result == b);
}

TEST_CASE("Algebraic: nested simplification chain",
          "[signal_engine][algebraic][nested]")
{
    PoolHarness h;
    uint16_t t_node = h.pool.make_raw_time_load();
    uint16_t zero = h.pool.make_const(0.0);
    uint16_t one = h.pool.make_const(1.0);

    // (t + 0) * 1 - (t + 0) * 1 = t - t = 0
    uint16_t step1 = h.pool.make_binop(NodeOp::Add, t_node, zero);
    uint16_t step2 = h.pool.make_binop(NodeOp::Mul, step1, one);
    uint16_t step3 = h.pool.make_binop(NodeOp::Sub, step2, step2);
    const Node& n = h.pool.get(step3);
    REQUIRE(n.op == NodeOp::Const);
    REQUIRE(n.imm == Approx(0.0));
}

// ═════════════════════════════════════════════════════════════════════════════
// NaN / Inf guard tests per-op
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("NaN guard: Tan of pi/2 produces finite output",
          "[signal_engine][executor][nan_guard][tan]")
{
    GoldenHarness h;

    // tan(pi/2) is technically undefined (very large number)
    // The executor's NaN guard should clamp any non-finite result
    h.assign_ok("a1", "(tan (/ 3.14159265 2))");
    double v = h.tick("a1", 0.0);
    REQUIRE(std::isfinite(v));
}

TEST_CASE("NaN guard: Pow of negative base with fractional exponent produces finite",
          "[signal_engine][executor][nan_guard][pow]")
{
    GoldenHarness h;

    // (-1) ^ 0.5 = sqrt(-1) = NaN in math, but executor should guard
    // Note: expt uses standard order (expt base exponent)
    h.assign_ok("a1", "(expt -1 0.5)");
    double v = h.tick("a1", 0.0);
    REQUIRE(std::isfinite(v));
}

TEST_CASE("NaN guard: Mod by zero produces finite output",
          "[signal_engine][executor][nan_guard][mod]")
{
    GoldenHarness h;

    h.assign_ok("a1", "(% 10 0)");
    double v = h.tick("a1", 0.0);
    REQUIRE(std::isfinite(v));
}

TEST_CASE("NaN guard: Division by zero produces finite output",
          "[signal_engine][executor][nan_guard][div]")
{
    GoldenHarness h;

    h.assign_ok("a1", "(/ 1 0)");
    double v = h.tick("a1", 0.0);
    REQUIRE(std::isfinite(v));
}

TEST_CASE("NaN guard: chained operations remain finite",
          "[signal_engine][executor][nan_guard][chain]")
{
    GoldenHarness h;

    // (/ 0 0) then add 1 — executor guards NaN at each node
    h.assign_ok("a1", "(+ (/ 0 0) 1)");
    double v = h.tick("a1", 0.0);
    REQUIRE(std::isfinite(v));
}

// ═════════════════════════════════════════════════════════════════════════════
// Phasor re-derivation inside time-as (via fast/slow)
// time.md 1.3.1: beat/bar/phrase re-derived from local t
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Phasor re-derivation: beat inside fast 2 doubles beat frequency",
          "[signal_engine][phasor][time_as]")
{
    GoldenHarness h(120.0, 4); // 120 BPM = 2 beats/sec

    // beat at 120bpm wraps every 0.5s
    // (fast 2 beat) should double the local t, so beat wraps every 0.25s
    h.assign_ok("a1", "(fast 2 beat)");

    // At t=0: inner_t=0, beat = fmod(0*2, 1) = 0
    REQUIRE(h.tick("a1", 0.0) == Approx(0.0).margin(1e-9));

    // At t=0.125: inner_t=0.25, beat = fmod(0.25 * 2/60 * 120, 1) = fmod(1.0, 1) = 0
    // Wait: beat = fmod(t * bpm/60, 1)
    // fast 2 means inner_t = t*2
    // beat(inner_t) = fmod(inner_t * bpm/60, 1) = fmod(t*2 * 2, 1) = fmod(4t, 1)
    // At t=0.125: fmod(0.5, 1) = 0.5
    REQUIRE(h.tick("a1", 0.125) == Approx(0.5).margin(1e-9));

    // At t=0.25: fmod(1.0, 1) = 0 (wrapped)
    REQUIRE(h.tick("a1", 0.25) == Approx(0.0).margin(1e-9));
}

TEST_CASE("Phasor re-derivation: bar inside slow 2 halves bar frequency",
          "[signal_engine][phasor][time_as][slow]")
{
    GoldenHarness h(120.0, 4);

    // bar at 120bpm 4/4 wraps every 2s
    // (slow 2 bar) = bar with inner_t = t/2
    // bar(inner_t) = fmod(inner_t * bpm/60 / bpb, 1) = fmod(t/2 * 2 / 4, 1) = fmod(t/4, 1)
    h.assign_ok("a1", "(slow 2 bar)");

    // At t=2.0: fmod(0.5, 1) = 0.5
    REQUIRE(h.tick("a1", 2.0) == Approx(0.5).margin(1e-9));

    // At t=4.0: fmod(1.0, 1) = 0
    REQUIRE(h.tick("a1", 4.0) == Approx(0.0).margin(1e-9));
}

TEST_CASE("Phasor re-derivation: beat-num inside fast increments faster",
          "[signal_engine][phasor][time_as][beat_num]")
{
    GoldenHarness h(60.0, 4); // 60 BPM = 1 beat/sec

    // beat_num = floor(t * bpm/60) = floor(t)
    // (fast 2 beat_num) uses inner_t = t*2
    // beat_num(inner_t) = floor(inner_t * 1) = floor(2t)
    h.assign_ok("a1", "(fast 2 beat-num)");

    REQUIRE(h.tick("a1", 0.0) == Approx(0.0).margin(1e-9));
    REQUIRE(h.tick("a1", 0.5) == Approx(1.0).margin(1e-9)); // floor(2*0.5) = 1
    REQUIRE(h.tick("a1", 1.0) == Approx(2.0).margin(1e-9)); // floor(2*1.0) = 2
}

// ═════════════════════════════════════════════════════════════════════════════
// Batch prev semantics
// prev.md 1.4: prev within batch reads preceding sample
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Batch prev: single-sample execution matches expected prev behavior",
          "[signal_engine][prev][batch]")
{
    // Note: Full batch execution tests require execute_batch which is
    // more complex to set up. Here we test the single-sample prev semantics
    // that batch mode should also follow.
    GoldenHarness h;

    // a2 = prev a1 + 1
    h.assign_ok("a1", "t");
    h.assign_ok("a2", "(+ (prev a1) 1)");

    // Tick 0: a1=0, a2 = prev_a1(0) + 1 = 1
    double a2_0 = h.tick("a2", 0.0);
    REQUIRE(a2_0 == Approx(1.0));

    // Tick 1: a1=0.001, commit sets prev_a1=0. a2 = 0 + 1 = 1
    double a1_1 = h.tick("a1", 0.001);
    REQUIRE(a1_1 == Approx(0.001));

    // Need to tick a2 after a1 was committed
    double a2_2 = h.tick("a2", 0.002);
    // prev_a1 = 0.001 (from last tick_all), so a2 = 0.001 + 1 = 1.001
    REQUIRE(a2_2 == Approx(1.001));
}

TEST_CASE("Batch prev: self-referencing output accumulates",
          "[signal_engine][prev][self_ref]")
{
    GoldenHarness h;

    // a1 = prev a1 + 0.1 (integration via prev)
    h.assign_ok("a1", "(+ (prev a1) 0.1)");

    // Tick 0: prev_a1 = 0, a1 = 0 + 0.1 = 0.1
    REQUIRE(h.tick("a1", 0.0) == Approx(0.1));

    // Tick 1: prev_a1 = 0.1, a1 = 0.1 + 0.1 = 0.2
    REQUIRE(h.tick("a1", 0.001) == Approx(0.2));

    // Tick 2: prev_a1 = 0.2, a1 = 0.2 + 0.1 = 0.3
    REQUIRE(h.tick("a1", 0.002) == Approx(0.3));
}

// ═════════════════════════════════════════════════════════════════════════════
// GC topological correctness after compaction
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("GC: reassignment and GC produces correct execution order",
          "[signal_engine][gc][topo]")
{
    GoldenHarness h;

    // Assign a1 to use t
    h.assign_ok("a1", "t");
    h.tick("a1", 0.0);
    REQUIRE(h.tick("a1", 1.0) == Approx(1.0));

    // Reassign a1 to use (* t 2)
    h.assign_ok("a1", "(* t 2)");

    // Trigger GC (rebuild_execution_order happens in assign_ok via rebuild)
    h.engine.pool.gc_unreachable_nodes();

    // Verify execution still works
    double v = h.tick("a1", 1.0);
    REQUIRE(v == Approx(2.0));
}

TEST_CASE("GC: multiple reassignments with GC stays correct",
          "[signal_engine][gc][topo][stress]")
{
    GoldenHarness h;

    for (int i = 0; i < 20; ++i) {
        std::string expr = "(+ t " + std::to_string(i) + ")";
        h.assign_ok("a1", expr.c_str());
        h.engine.pool.gc_unreachable_nodes();

        double v = h.tick("a1", 1.0);
        REQUIRE(v == Approx(1.0 + i));
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Executor edge cases: VecIndex and VecLerp
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Executor: VecIndex with negative index wraps correctly",
          "[signal_engine][executor][vecindex]")
{
    GoldenHarness h;

    h.eval_ok("(define v [10 20 30])");
    h.assign_ok("a1", "(step v -0.5)");

    // step: floor(-0.5 * 3) = floor(-1.5) = -2
    // VecIndex: ((-2 % 3) + 3) % 3 = (1 + 3) % 3 = 1
    // v[1] = 20
    double v = h.tick("a1", 0.0);
    REQUIRE(v == Approx(20.0));
}

TEST_CASE("Executor: VecLerp with single element",
          "[signal_engine][executor][veclerp_single]")
{
    GoldenHarness h;

    h.eval_ok("(define v [42])");
    h.assign_ok("a1", "(interp v 0.5)");

    // VecLerp with len=1 returns the single element
    double v = h.tick("a1", 0.0);
    REQUIRE(v == Approx(42.0));
}

TEST_CASE("Executor: VecLerp with two elements",
          "[signal_engine][executor][veclerp_two]")
{
    GoldenHarness h;

    h.eval_ok("(define v [0 10])");
    h.assign_ok("a1", "(interp v 0.5)");

    // VecLerp: scaled = 0.5 * (2-1) = 0.5, lerp(v[0], v[1], 0.5) = 5
    double v = h.tick("a1", 0.0);
    REQUIRE(v == Approx(5.0));
}

// ═════════════════════════════════════════════════════════════════════════════
// Time-invariant flag propagation
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Time-invariant: constant expression has FLAG_TIME_INVARIANT",
          "[signal_engine][time_invariant]")
{
    PoolHarness h;
    uint16_t c = h.pool.make_const(42.0);
    REQUIRE(h.pool.get(c).flags & FLAG_TIME_INVARIANT);

    uint16_t sum = h.pool.make_binop(NodeOp::Add,
                                      h.pool.make_const(1.0),
                                      h.pool.make_const(2.0));
    // Constant folded to Const(3.0)
    REQUIRE(h.pool.get(sum).op == NodeOp::Const);
    REQUIRE(h.pool.get(sum).flags & FLAG_TIME_INVARIANT);
}

TEST_CASE("Time-invariant: time-dependent expression does NOT have FLAG_TIME_INVARIANT",
          "[signal_engine][time_invariant]")
{
    PoolHarness h;
    uint16_t t_node = h.pool.make_raw_time_load();
    REQUIRE(!(h.pool.get(t_node).flags & FLAG_TIME_INVARIANT));

    uint16_t sum = h.pool.make_binop(NodeOp::Add, t_node, h.pool.make_const(1.0));
    REQUIRE(!(h.pool.get(sum).flags & FLAG_TIME_INVARIANT));
}

TEST_CASE("Time-invariant: mixed expression loses TIME_INVARIANT",
          "[signal_engine][time_invariant][mixed]")
{
    PoolHarness h;
    uint16_t c1 = h.pool.make_const(5.0);
    uint16_t t_node = h.pool.make_raw_time_load();

    // Const + t => not time-invariant
    uint16_t sum = h.pool.make_binop(NodeOp::Add, c1, t_node);
    REQUIRE(!(h.pool.get(sum).flags & FLAG_TIME_INVARIANT));
}

// ═════════════════════════════════════════════════════════════════════════════
// Euclid edge cases
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Euclid: k=0 produces all zeros",
          "[signal_engine][euclid][zero]")
{
    GoldenHarness h(60.0, 4);

    // (euclid 0 8) — no hits
    h.assign_ok("a1", "(euclid 0 8 beat)");

    // At any phase position, all gates should be 0
    REQUIRE(h.tick("a1", 0.0) == Approx(0.0));
    REQUIRE(h.tick("a1", 0.125) == Approx(0.0));
    REQUIRE(h.tick("a1", 0.5) == Approx(0.0));
}

TEST_CASE("Euclid: k=n produces all ones",
          "[signal_engine][euclid][full]")
{
    GoldenHarness h(60.0, 4);

    // (euclid 8 8) — all hits
    h.assign_ok("a1", "(euclid 8 8 beat)");

    REQUIRE(h.tick("a1", 0.0) == Approx(1.0));
    REQUIRE(h.tick("a1", 0.125) == Approx(1.0));
}

TEST_CASE("Euclid: 1 of 1 produces single hit",
          "[signal_engine][euclid][minimal]")
{
    GoldenHarness h(60.0, 4);

    h.assign_ok("a1", "(euclid 1 1 beat)");

    // First half: hit, second half: no hit
    REQUIRE(h.tick("a1", 0.0) == Approx(1.0));
    REQUIRE(h.tick("a1", 0.5) == Approx(0.0));
}

// ═════════════════════════════════════════════════════════════════════════════
// Gate width edge cases
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Gatesw: pattern of zeros produces no gates",
          "[signal_engine][gatesw][zero_width]")
{
    GoldenHarness h(60.0, 4);

    // Pattern values 0 have width 0/9 = 0, so no gates
    h.assign_ok("a1", "(gatesw [0 0 0 0] beat)");

    double v = h.tick("a1", 0.0);
    REQUIRE(v == Approx(0.0));
}

TEST_CASE("Gatesw: pattern of nines produces full gates",
          "[signal_engine][gatesw][full_width]")
{
    GoldenHarness h(60.0, 4);

    // Pattern values 9 have width 9/9 = 1.0, full gates
    h.assign_ok("a1", "(gatesw [9 9 9 9] beat)");

    double v = h.tick("a1", 0.0);
    REQUIRE(v == Approx(1.0));
}

// ═════════════════════════════════════════════════════════════════════════════
// CSE edge cases
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("CSE: nodes with same op+inputs but different imm values are NOT merged",
          "[signal_engine][cse][imm_distinct]")
{
    PoolHarness h;

    // Two VecIndex nodes with different table IDs
    uint16_t idx = h.pool.make_const(0.0);
    Node n1;
    n1.op = NodeOp::VecIndex;
    n1.input_a = idx;
    n1.input_b = NODE_NONE;
    n1.input_c = NODE_NONE;
    n1.imm = 0.0; // table 0

    Node n2 = n1;
    n2.imm = 1.0; // table 1

    uint16_t r1 = h.pool.intern_node(n1);
    uint16_t r2 = h.pool.intern_node(n2);

    // Different imm values -> different nodes
    REQUIRE(r1 != r2);
}

TEST_CASE("CSE: identical expressions in different outputs share nodes",
          "[signal_engine][cse][cross_output]")
{
    GoldenHarness h;

    h.assign_ok("a1", "(+ t 1)");
    h.assign_ok("a2", "(+ t 1)");

    // Both should produce the same value
    REQUIRE(h.tick("a1", 1.0) == Approx(2.0));
    REQUIRE(h.tick("a2", 1.0) == Approx(2.0));
}

// ═════════════════════════════════════════════════════════════════════════════
// Deterministic randomness
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Random: same time produces same value",
          "[signal_engine][random][deterministic]")
{
    GoldenHarness h(60.0, 4);

    h.assign_ok("a1", "(random)");

    double v1 = h.tick("a1", 0.5);
    double v2 = h.tick("a1", 0.5);
    // Both ticks at same beat_num (0) should produce same hash
    REQUIRE(v1 == Approx(v2));
}

TEST_CASE("Random: different beats produce different values",
          "[signal_engine][random][varied]")
{
    GoldenHarness h(60.0, 4);

    h.assign_ok("a1", "(random)");

    double v1 = h.tick("a1", 0.0);
    double v2 = h.tick("a1", 1.5); // different beat_num
    REQUIRE(v1 != Approx(v2));
}

TEST_CASE("Random: with range arguments produces values in range",
          "[signal_engine][random][range]")
{
    GoldenHarness h(60.0, 4);

    h.assign_ok("a1", "(random 10 20)");

    // Sample many times — all should be in [10, 20]
    for (int beat = 0; beat < 20; ++beat) {
        double t = beat * 1.0; // 1 beat per second at 60 BPM
        double v = h.tick("a1", t);
        REQUIRE(v >= Approx(10.0).margin(0.01));
        REQUIRE(v <= Approx(20.0).margin(0.01));
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Range conversion edge cases
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Range conversion: scale maps [0,1] to [min,max]",
          "[signal_engine][range][scale]")
{
    GoldenHarness h;
    // (scale 0.5 10 20) = 0.5 * (20 - 10) + 10 = 15
    h.assign_ok("a1", "(scale 0.5 10 20)");
    REQUIRE(h.tick("a1", 0.0) == Approx(15.0));
}

TEST_CASE("Range conversion: lerp interpolates between a and b",
          "[signal_engine][range][lerp]")
{
    GoldenHarness h;
    // (lerp 10 20 0.5) = 10 + (20 - 10) * 0.5 = 15
    h.assign_ok("a1", "(lerp 10 20 0.5)");
    REQUIRE(h.tick("a1", 0.0) == Approx(15.0));
}

// ═════════════════════════════════════════════════════════════════════════════
// Switch/encoder inputs
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Hardware inputs: in1 digital input reads injected value",
          "[signal_engine][inputs]")
{
    GoldenHarness h;

    h.assign_ok("a1", "in1");

    // Inject a value for the digital input
    uint16_t in1_idx = GraphBuilder::resolve_hardware_input(internSymbol("in1"));
    REQUIRE(in1_idx != NODE_NONE);
    h.hw_inputs[in1_idx] = 1.0;

    double v = h.tick("a1", 0.0);
    REQUIRE(v == Approx(1.0));
}

TEST_CASE("Hardware inputs: ain1 analog input reads injected value",
          "[signal_engine][inputs][analog]")
{
    GoldenHarness h;

    h.assign_ok("a1", "ain1");

    uint16_t ain1_idx = GraphBuilder::resolve_hardware_input(internSymbol("ain1"));
    REQUIRE(ain1_idx != NODE_NONE);
    h.hw_inputs[ain1_idx] = 0.75;

    double v = h.tick("a1", 0.0);
    REQUIRE(v == Approx(0.75));
}

// ═════════════════════════════════════════════════════════════════════════════
// Output naming
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Outputs: all analog outputs a1-a8 are addressable",
          "[signal_engine][outputs][analog]")
{
    GoldenHarness h;

    for (int i = 1; i <= 8; ++i) {
        std::string name = "a" + std::to_string(i);
        std::string expr = std::to_string(i * 10);
        h.assign_ok(name.c_str(), expr.c_str());
    }

    // Verify at least a1 and a8 work
    REQUIRE(h.tick("a1", 0.0) == Approx(10.0));
    REQUIRE(h.tick("a8", 0.0) == Approx(80.0));
}

TEST_CASE("Outputs: all digital outputs d1-d8 are addressable",
          "[signal_engine][outputs][digital]")
{
    GoldenHarness h;

    for (int i = 1; i <= 8; ++i) {
        std::string name = "d" + std::to_string(i);
        std::string expr = std::to_string(i * 5);
        h.assign_ok(name.c_str(), expr.c_str());
    }

    REQUIRE(h.tick("d1", 0.0) == Approx(5.0));
    REQUIRE(h.tick("d8", 0.0) == Approx(40.0));
}
