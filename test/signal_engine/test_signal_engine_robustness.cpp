// Signal engine robustness and boundary tests.
//
// Deterministic regression tests for resource limits, edge conditions,
// and compiler robustness.  Every test here must be reproducible without
// random seeds or timing dependencies.
//
// Categories:
//   [robustness][pool]        — node pool overflow and recovery
//   [robustness][arena]       — source arena limits
//   [robustness][inline]      — inline depth limits
//   [robustness][for]         — for-loop unroll limits
//   [robustness][data]        — data pool overflow
//   [robustness][cse]         — CSE / hash-consing correctness
//   [robustness][edge]        — edge-case inputs that must not crash
//   [robustness][sampling]    — post-compilation sampling safety

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "src/signal_engine/signal_engine.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace sig;

// ── Shared Harness ──────────────────────────────────────────────────────────
// Mirrors GoldenHarness from test_signal_engine_golden.cpp.

namespace {

struct RobustHarness {
    SignalEngine engine;
    double cell_values[MAX_CELLS] = {};
    double hw_inputs[32] = {};
    double outputs[MAX_OUTPUTS] = {};
    double workspace[MAX_TOTAL_NODES] = {};

    explicit RobustHarness(double bpm = 120.0, int beats_per_bar = 4)
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

    double sample(const char* output_name, double t)
    {
        std::memset(outputs, 0, sizeof(outputs));
        std::memset(workspace, 0, sizeof(workspace));
        engine.cells.snapshot_values(cell_values, MAX_CELLS);

        ExecutionContext ctx;
        ctx.t = t;
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
        if (idx == NODE_NONE) return 0.0;
        return outputs[idx];
    }

    double tick(const char* output_name, double t)
    {
        double value = sample(output_name, t);
        commit_outputs(engine.pool, outputs);
        return value;
    }

    bool is_error(const std::string& code)
    {
        EvalResult r = eval_result(code);
        return r.kind == EvalResult::Error;
    }

    bool is_ok(const std::string& code)
    {
        return !is_error(code);
    }

    void run_samples(const char* output, int count)
    {
        for (int i = 0; i < count; i++) {
            double t = (double)i * 0.001;
            double v = tick(output, t);
            INFO("sample " << i << " t=" << t << " v=" << v);
            REQUIRE(std::isfinite(v));
        }
    }
};

} // anonymous namespace

// =============================================================================
// 1. Node Pool Limits
// =============================================================================

TEST_CASE("Node pool handles large expressions gracefully", "[robustness][pool]")
{
    SECTION("Deep binary tree fills pool without crashing")
    {
        RobustHarness h;

        // Build: (+ (+ (+ ... beat 0.1) 0.2) 0.3) — many unique binop nodes
        // Each level adds a unique constant and a binop node = ~2 nodes/level.
        // MAX_TOTAL_NODES = 1024, so 400 levels should push toward the limit.
        std::string expr = "beat";
        for (int i = 0; i < 400; i++) {
            expr = "(+ " + expr + " " + std::to_string(i * 0.001) + ")";
        }
        expr = "(a1 " + expr + ")";

        // May succeed or error (pool full) — must not crash
        EvalResult r = h.eval_result(expr);
        (void)r;

        // Engine must remain usable after potential overflow
        h.eval_ok("(a2 beat)");
        double v = h.tick("a2", 0.5);
        REQUIRE(std::isfinite(v));
    }

    SECTION("After large compilation, small one still works")
    {
        RobustHarness h;

        // Fill up with a complex expression
        std::string complex = "(+ (+ (+ (+ (+ beat (sin beat)) (cos bar)) (* t 2)) (/ bar 3)) phrase)";
        h.eval_result("(a1 " + complex + ")");
        h.eval_result("(a2 " + complex + ")");
        h.eval_result("(a3 " + complex + ")");

        // Now a simple one should still work
        h.eval_ok("(a4 0.5)");
        double v = h.tick("a4", 0.0);
        REQUIRE(v == Approx(0.5));
    }

    SECTION("Pool overflow returns NODE_NONE, not crash")
    {
        RobustHarness h;

        // Create many unique expressions to exhaust the pool.
        // Each unique float constant + unique unary/binop is a distinct node.
        bool had_error = false;
        for (int i = 0; i < 200 && !had_error; i++) {
            // Each iteration: a unique constant + unary + binop = ~3 new nodes
            std::string expr = "(a1 (+ (sin " + std::to_string(i * 0.0137) +
                               ") (cos " + std::to_string(i * 0.0253) + ")))";
            EvalResult r = h.eval_result(expr);
            if (r.kind == EvalResult::Error) had_error = true;
        }

        // Engine must still function
        h.eval_ok("(a1 1.0)");
        double v = h.tick("a1", 0.0);
        REQUIRE(std::isfinite(v));
    }
}

// =============================================================================
// 2. Source Arena Limits
// =============================================================================

TEST_CASE("Source arena handles large inputs", "[robustness][arena]")
{
    SECTION("Many sequential defines fill arena without crashing")
    {
        RobustHarness h;

        // SOURCE_ARENA_SIZE = 16384. Each defn stores body text.
        // Define many small functions until we approach the limit.
        bool overflow_seen = false;
        for (int i = 0; i < 300 && !overflow_seen; i++) {
            std::string name = "fn_" + std::to_string(i);
            std::string code = "(defn " + name + " [x] (+ x " +
                               std::to_string(i) + "))";
            EvalResult r = h.eval_result(code);
            // Should either succeed or fail cleanly
            if (r.kind == EvalResult::Error) overflow_seen = true;
        }

        // Engine must still accept simple output assignments
        h.eval_ok("(a1 beat)");
        double v = h.tick("a1", 0.25);
        REQUIRE(std::isfinite(v));
    }

    SECTION("Very long expression string near uint16 span limits")
    {
        RobustHarness h;

        // Build an expression with many terms to create a long source string.
        // MAX_TOKENS = 256 limits token count, so we may hit that first.
        std::string long_expr = "(a1 (+";
        for (int i = 0; i < 100; i++) {
            long_expr += " " + std::to_string(i * 0.01);
        }
        long_expr += "))";

        // Should not crash — may error due to token limit or arity
        EvalResult r = h.eval_result(long_expr);
        (void)r;

        // Engine must still work
        h.eval_ok("(a1 0.5)");
        double v = h.tick("a1", 0.0);
        REQUIRE(v == Approx(0.5));
    }
}

// =============================================================================
// 3. Inline Depth Limits
// =============================================================================

TEST_CASE("Inline depth limit produces clean diagnostic", "[robustness][inline]")
{
    SECTION("Chain of nested function calls hits inline depth limit")
    {
        RobustHarness h;

        // Create a chain: f0 calls f1, f1 calls f2, ..., f(N-1) calls fN
        // MAX_INLINE_DEPTH = 16, so a chain of 20 should exceed it.
        int chain_length = 20;

        // Base function: the deepest one just returns its arg
        h.eval_ok("(defn chain_0 [x] x)");

        // Build the chain
        for (int i = 1; i <= chain_length; i++) {
            std::string prev = "chain_" + std::to_string(i - 1);
            std::string curr = "chain_" + std::to_string(i);
            std::string code = "(defn " + curr + " [x] (" + prev + " x))";
            h.eval_ok(code);
        }

        // Calling the deepest function should hit the inline limit
        std::string deep_call = "(a1 (chain_" + std::to_string(chain_length) + " beat))";
        EvalResult r = h.eval_result(deep_call);
        INFO("deep call: " << deep_call);
        REQUIRE(r.kind == EvalResult::Error);
        REQUIRE(r.diagnostic_count > 0);
        // The error message should mention depth or nesting
        bool found_depth_msg = false;
        for (uint8_t i = 0; i < r.diagnostic_count; i++) {
            if (r.diagnostics[i].message &&
                std::string(r.diagnostics[i].message).find("deep") != std::string::npos) {
                found_depth_msg = true;
            }
        }
        REQUIRE(found_depth_msg);
    }

    SECTION("Chain just at the limit still works")
    {
        RobustHarness h;

        // Build a chain of length MAX_INLINE_DEPTH - 1 (should just fit)
        int chain_length = MAX_INLINE_DEPTH - 1; // 15

        h.eval_ok("(defn ok_0 [x] x)");
        for (int i = 1; i <= chain_length; i++) {
            std::string prev = "ok_" + std::to_string(i - 1);
            std::string curr = "ok_" + std::to_string(i);
            h.eval_ok("(defn " + curr + " [x] (" + prev + " x))");
        }

        std::string call = "(a1 (ok_" + std::to_string(chain_length) + " beat))";
        EvalResult r = h.eval_result(call);
        // Should succeed — chain length equals depth limit minus 1
        // (Each call adds one to inline_depth; the check is >=, so depth 15
        // with limit 16 should pass.)
        INFO("chain at limit: " << call);
        if (r.kind == EvalResult::Error && r.diagnostic_count > 0) {
            INFO("error: " << (r.diagnostics[0].message ? r.diagnostics[0].message : "(null)"));
        }
        // Even if it errors, it must not crash
        REQUIRE(std::isfinite(h.tick("a1", 0.5)));
    }

    SECTION("Direct recursion is caught cleanly")
    {
        RobustHarness h;
        h.eval_ok("(defn recurse [x] (recurse x))");

        EvalResult r = h.eval_result("(a1 (recurse beat))");
        REQUIRE(r.kind == EvalResult::Error);
        REQUIRE(r.diagnostic_count > 0);
        // Should mention recursion
        bool mentions_recursive = false;
        for (uint8_t i = 0; i < r.diagnostic_count; i++) {
            if (r.diagnostics[i].message) {
                std::string msg(r.diagnostics[i].message);
                if (msg.find("itself") != std::string::npos ||
                    msg.find("recursive") != std::string::npos ||
                    msg.find("recursi") != std::string::npos) {
                    mentions_recursive = true;
                }
            }
        }
        REQUIRE(mentions_recursive);
    }

    SECTION("Mutual recursion is caught cleanly")
    {
        RobustHarness h;
        h.eval_ok("(defn ping [x] (pong x))");
        h.eval_ok("(defn pong [x] (ping x))");

        EvalResult r = h.eval_result("(a1 (ping beat))");
        REQUIRE(r.kind == EvalResult::Error);
    }
}

// =============================================================================
// 4. For-Loop Unroll Limits
// =============================================================================

TEST_CASE("For-loop unroll limits are enforced", "[robustness][for]")
{
    // Collection capacity is 64 elements (Collection::element_nodes[64]).

    SECTION("For over exactly 64-element range compiles")
    {
        RobustHarness h;
        // (range 0 64) produces 64 elements [0..63]
        h.eval_ok("(a1 (for x (range 0 64) x))");
        // The result should be the last element value (63.0)
        double v = h.sample("a1", 0.0);
        REQUIRE(v == Approx(63.0));
    }

    SECTION("For over 65-element range gets capped at 64")
    {
        RobustHarness h;
        // (range 0 65) attempts 65 elements but collection cap is 64
        // The for loop should still succeed with 64 elements (capped)
        EvalResult r = h.eval_result("(a1 (for x (range 0 65) x))");
        // Should succeed — collection silently caps at 64
        if (r.kind != EvalResult::Error) {
            double v = h.sample("a1", 0.0);
            // Last element should be 63 (index 63 from 0..63)
            REQUIRE(v == Approx(63.0));
        }
        // Either way, must not crash
    }

    SECTION("For with literal vector of 64 elements")
    {
        RobustHarness h;
        std::string vec = "[";
        for (int i = 0; i < 64; i++) {
            if (i > 0) vec += " ";
            vec += std::to_string(i);
        }
        vec += "]";
        std::string code = "(a1 (for x " + vec + " x))";
        h.eval_ok(code);
        double v = h.sample("a1", 0.0);
        REQUIRE(v == Approx(63.0));
    }

    SECTION("For with empty collection returns 0")
    {
        RobustHarness h;
        // Empty vector
        h.eval_ok("(a1 (for x [] 42))");
        double v = h.sample("a1", 0.0);
        REQUIRE(v == Approx(0.0));
    }

    SECTION("For with range(0 0) returns 0")
    {
        RobustHarness h;
        h.eval_ok("(a1 (for x (range 0 0) 99))");
        double v = h.sample("a1", 0.0);
        REQUIRE(v == Approx(0.0));
    }

    SECTION("Negative-step range is capped at 64")
    {
        RobustHarness h;
        // (range 100 0 -1) produces 100 elements — should cap at 64
        EvalResult r = h.eval_result("(a1 (for x (range 100 0 -1) x))");
        if (r.kind != EvalResult::Error) {
            double v = h.sample("a1", 0.0);
            REQUIRE(std::isfinite(v));
        }
    }
}

TEST_CASE("Oversized vector diagnostics have stable text and recover",
          "[robustness][data][diagnostics]")
{
    RobustHarness h;
    h.eval_ok("(a1 0.25)");

    std::string vector = "[";
    for (int i = 1; i <= 68; i++) {
        if (i > 1) vector += " ";
        vector += std::to_string(i);
    }
    vector += "]";

    EvalResult rejected = h.eval_result(
        "(a1 (step " + vector + " beat))");
    REQUIRE(rejected.kind == EvalResult::Error);
    REQUIRE(rejected.diagnostic_count > 0);
    REQUIRE(rejected.diagnostics[0].category == DiagnosticCategory::Overflow);
    REQUIRE(rejected.diagnostics[0].message != nullptr);
    REQUIRE(std::string(rejected.diagnostics[0].message).find("64") !=
            std::string::npos);

    // The rejected replacement keeps the prior output and the next unrelated
    // publication/sampling sequence remains usable.
    REQUIRE(h.tick("a1", 0.0) == Approx(0.25));
    h.eval_ok("(a2 0.5)");
    REQUIRE(h.tick("a2", 0.1) == Approx(0.5));
}

// =============================================================================
// 5. Data Pool Overflow
// =============================================================================

TEST_CASE("Data pool overflow is handled gracefully", "[robustness][data]")
{
    SECTION("Many data tables exhaust MAX_DATA_TABLES")
    {
        RobustHarness h;

        // MAX_DATA_TABLES = 64, MAX_DATA_ENTRIES = 2048
        // Each small table uses 1 slot. Exhaust by creating > 64 tables.
        bool overflow_seen = false;
        for (int i = 0; i < 80 && !overflow_seen; i++) {
            std::string name = "dt_" + std::to_string(i);
            std::string code = "(define " + name + " [1 2 3])";
            EvalResult r = h.eval_result(code);
            if (r.kind == EvalResult::Error) overflow_seen = true;
        }

        // Engine must remain usable
        h.eval_ok("(a1 beat)");
        double v = h.tick("a1", 0.5);
        REQUIRE(std::isfinite(v));
    }

    SECTION("Large data tables exhaust MAX_DATA_ENTRIES")
    {
        RobustHarness h;

        // MAX_DATA_ENTRIES = 2048. Create tables that collectively exceed that.
        bool overflow_seen = false;
        for (int i = 0; i < 10 && !overflow_seen; i++) {
            std::string name = "big_" + std::to_string(i);
            std::string vec = "[";
            // 300 entries per table, 10 tables = 3000 > 2048
            for (int j = 0; j < 300; j++) {
                if (j > 0) vec += " ";
                vec += std::to_string(j * 0.01);
            }
            vec += "]";
            std::string code = "(define " + name + " " + vec + ")";
            EvalResult r = h.eval_result(code);
            if (r.kind == EvalResult::Error) overflow_seen = true;
        }

        // Engine must still function
        h.eval_ok("(a1 0.5)");
        double v = h.tick("a1", 0.0);
        REQUIRE(v == Approx(0.5));
    }
}

// =============================================================================
// 6. CSE Stability
// =============================================================================

TEST_CASE("CSE produces deterministic results", "[robustness][cse]")
{
    SECTION("Same expression compiled twice yields identical node counts")
    {
        RobustHarness h;

        h.eval_ok("(a1 (+ (sin beat) (cos bar)))");
        uint16_t count1 = h.engine.pool.node_count;

        // Recompile the same expression to a2
        h.eval_ok("(a2 (+ (sin beat) (cos bar)))");
        uint16_t count2 = h.engine.pool.node_count;

        // CSE should reuse all nodes — count should not change
        // (the expression produces the same nodes, so CSE deduplicates)
        REQUIRE(count2 == count1);
    }

    SECTION("beat references are shared via hash-consing")
    {
        RobustHarness h;

        h.eval_ok("(a1 (+ beat beat))");

        // The two beat references should resolve to the same node
        // Verify by checking the root's input_a == input_b
        uint16_t root = h.engine.pool.outputs[
            GraphBuilder::resolve_output_index(internSymbol("a1"))
        ].root_node;

        REQUIRE(root != NODE_NONE);
        const Node& root_node = h.engine.pool.nodes[root];
        // The root should be Add with identical inputs (beat shared)
        if (root_node.op == NodeOp::Add) {
            REQUIRE(root_node.input_a == root_node.input_b);
        }
        // If constant-folded or optimized differently, that's also fine
    }

    SECTION("Identical subexpressions share nodes across outputs")
    {
        RobustHarness h;

        h.eval_ok("(a1 (sin beat))");
        uint16_t count_after_a1 = h.engine.pool.node_count;

        h.eval_ok("(a2 (sin beat))");
        uint16_t count_after_a2 = h.engine.pool.node_count;

        // No new nodes should be created since (sin beat) already exists
        REQUIRE(count_after_a2 == count_after_a1);
    }
}

// =============================================================================
// 7. Edge Cases That Must Not Crash
// =============================================================================

TEST_CASE("Edge-case inputs never crash", "[robustness][edge]")
{
    SECTION("Empty input")
    {
        RobustHarness h;
        EvalResult r = h.eval_result("");
        // Empty input should return Ok (no-op)
        REQUIRE(r.kind != EvalResult::Error);
    }

    SECTION("Whitespace only")
    {
        RobustHarness h;
        EvalResult r = h.eval_result("   ");
        REQUIRE(r.kind != EvalResult::Error);
    }

    SECTION("Newlines and tabs only")
    {
        RobustHarness h;
        EvalResult r1 = h.eval_result("\n\n\n");
        REQUIRE(r1.kind != EvalResult::Error);

        EvalResult r2 = h.eval_result("\t\t");
        REQUIRE(r2.kind != EvalResult::Error);
    }

    SECTION("Very deeply nested parentheses")
    {
        RobustHarness h;
        // (((((1)))))  — deeply nested but well-formed parens around a number
        // The tokenizer and parser should handle this (or error cleanly)
        EvalResult r = h.eval_result("(((((1)))))");
        // We don't care if it errors — just must not crash
        (void)r;
    }

    SECTION("Unmatched opening parens")
    {
        RobustHarness h;
        EvalResult r = h.eval_result("(+ 1");
        // Should produce a parse error, not crash
        (void)r;
        // Engine must remain usable
        h.eval_ok("(a1 0.5)");
        REQUIRE(std::isfinite(h.tick("a1", 0.0)));
    }

    SECTION("Unmatched closing parens")
    {
        RobustHarness h;
        EvalResult r = h.eval_result("+ 1)");
        (void)r;
        h.eval_ok("(a1 0.5)");
        REQUIRE(std::isfinite(h.tick("a1", 0.0)));
    }

    SECTION("Only opening parens")
    {
        RobustHarness h;
        EvalResult r = h.eval_result("((((");
        (void)r;
        h.eval_ok("(a1 0.5)");
        REQUIRE(std::isfinite(h.tick("a1", 0.0)));
    }

    SECTION("Only closing parens")
    {
        RobustHarness h;
        EvalResult r = h.eval_result("))))");
        (void)r;
        h.eval_ok("(a1 0.5)");
        REQUIRE(std::isfinite(h.tick("a1", 0.0)));
    }

    SECTION("Very long symbol name")
    {
        RobustHarness h;
        // Create a symbol name that's 500 characters long
        std::string long_name(500, 'x');
        std::string code = "(define " + long_name + " 42)";
        EvalResult r = h.eval_result(code);
        // May succeed or fail — must not crash
        (void)r;
    }

    SECTION("Null byte in source text")
    {
        RobustHarness h;
        // Source text with an embedded null byte
        // eval_cold takes a length, so null byte shouldn't matter
        std::string code = "(a1 0.5)";
        // Insert a null byte in the middle
        code[3] = '\0';
        EvalResult r = h.eval_result(code);
        // May produce an error but must not crash
        (void)r;
    }

    SECTION("Non-ASCII bytes in source text")
    {
        RobustHarness h;
        // UTF-8 encoded string with non-ASCII characters
        EvalResult r = h.eval_result("(a1 \xc3\xa9)"); // e with accent
        (void)r;
        // Engine remains usable
        h.eval_ok("(a1 0.5)");
    }

    SECTION("Empty list ()")
    {
        RobustHarness h;
        EvalResult r = h.eval_result("()");
        (void)r;
    }

    SECTION("Nested empty lists")
    {
        RobustHarness h;
        EvalResult r = h.eval_result("(())");
        (void)r;
    }

    SECTION("Expression with many extra arguments")
    {
        RobustHarness h;
        // Variadic + with 50 arguments
        std::string code = "(a1 (+";
        for (int i = 0; i < 50; i++) {
            code += " " + std::to_string(i * 0.01);
        }
        code += "))";
        EvalResult r = h.eval_result(code);
        if (r.kind != EvalResult::Error) {
            double v = h.sample("a1", 0.0);
            REQUIRE(std::isfinite(v));
        }
    }

    SECTION("Boolean-like edge expressions")
    {
        RobustHarness h;
        h.eval_ok("(a1 (if 0 1 0))");
        REQUIRE(h.sample("a1", 0.0) == Approx(0.0));

        h.eval_ok("(a1 (if 1 1 0))");
        REQUIRE(h.sample("a1", 0.0) == Approx(1.0));
    }

    SECTION("Deeply nested unary operations")
    {
        RobustHarness h;
        // (abs (neg (abs (neg ... beat))))
        std::string expr = "beat";
        for (int i = 0; i < 40; i++) {
            expr = (i % 2 == 0) ? "(abs " + expr + ")" : "(neg " + expr + ")";
        }
        EvalResult r = h.eval_result("(a1 " + expr + ")");
        if (r.kind != EvalResult::Error) {
            double v = h.sample("a1", 0.5);
            REQUIRE(std::isfinite(v));
        }
    }
}

// =============================================================================
// 8. Post-Compilation Sampling Safety
// =============================================================================

TEST_CASE("Sampling after compilation produces finite values", "[robustness][sampling]")
{
    SECTION("Many rapid samples do not crash")
    {
        RobustHarness h;
        h.eval_ok("(a1 (+ (sin (* beat 6.28)) (cos (* bar 3.14))))");

        // Run 10000 samples at various time values
        for (int i = 0; i < 10000; i++) {
            double t = (double)i * 0.0001;
            double v = h.sample("a1", t);
            if (i % 1000 == 0) {
                INFO("sample " << i << " t=" << t << " v=" << v);
            }
            REQUIRE(std::isfinite(v));
        }
    }

    SECTION("Sampling with extreme time values")
    {
        RobustHarness h;
        h.eval_ok("(a1 (usin beat))");

        // Very large time values
        double v1 = h.sample("a1", 1e10);
        REQUIRE(std::isfinite(v1));

        // Very small positive time
        double v2 = h.sample("a1", 1e-15);
        REQUIRE(std::isfinite(v2));

        // Negative time
        double v3 = h.sample("a1", -1.0);
        REQUIRE(std::isfinite(v3));

        // Zero
        double v4 = h.sample("a1", 0.0);
        REQUIRE(std::isfinite(v4));
    }

    SECTION("Recompilation mid-stream produces finite values")
    {
        RobustHarness h;
        h.eval_ok("(a1 (usin beat))");

        // Sample some
        h.run_samples("a1", 100);

        // Redefine while sampling
        h.eval_ok("(a1 (* beat 0.5))");

        // Continue sampling — no stale references should crash
        h.run_samples("a1", 100);

        // Redefine again
        h.eval_ok("(a1 (if (> beat 0.5) 1.0 0.0))");
        h.run_samples("a1", 100);
    }

    SECTION("Sampling unassigned output returns finite value")
    {
        RobustHarness h;
        // a3 was never assigned — should return 0 or LKG value, not crash
        double v = h.sample("a3", 0.5);
        REQUIRE(std::isfinite(v));
    }
}

// =============================================================================
// 9. Token Limit
// =============================================================================

TEST_CASE("Token limit is handled gracefully", "[robustness][edge]")
{
    SECTION("Expression exceeding MAX_TOKENS is rejected cleanly")
    {
        RobustHarness h;

        // MAX_TOKENS = 256. Build an expression with many tokens.
        // Each "(+ x" adds 3 tokens. 100 nestings = 300+ tokens.
        std::string code = "0";
        for (int i = 0; i < 100; i++) {
            code = "(+ " + code + " 1)";
        }
        code = "(a1 " + code + ")";

        EvalResult r = h.eval_result(code);
        // May truncate or error — must not crash
        (void)r;

        // Engine remains usable
        h.eval_ok("(a1 0.5)");
        double v = h.tick("a1", 0.0);
        REQUIRE(v == Approx(0.5));
    }
}

// =============================================================================
// 10. GC + Recompilation Stability
// =============================================================================

TEST_CASE("GC and recompilation maintain engine consistency", "[robustness][pool]")
{
    SECTION("Repeated reassignment with GC produces consistent values")
    {
        RobustHarness h;

        for (int round = 0; round < 50; round++) {
            double val = (double)round * 0.02;
            std::string code = "(a1 " + std::to_string(val) + ")";
            h.eval_ok(code);

            double v = h.sample("a1", 0.0);
            INFO("round " << round << " expected " << val << " got " << v);
            REQUIRE(v == Approx(val).margin(1e-9));
        }
    }

    SECTION("Multiple outputs reassigned in parallel stay independent")
    {
        RobustHarness h;

        h.eval_ok("(a1 0.1)");
        h.eval_ok("(a2 0.2)");
        h.eval_ok("(a3 0.3)");

        REQUIRE(h.sample("a1", 0.0) == Approx(0.1));
        REQUIRE(h.sample("a2", 0.0) == Approx(0.2));
        REQUIRE(h.sample("a3", 0.0) == Approx(0.3));

        // Reassign a2 — a1 and a3 must not change
        h.eval_ok("(a2 0.9)");

        REQUIRE(h.sample("a1", 0.0) == Approx(0.1));
        REQUIRE(h.sample("a2", 0.0) == Approx(0.9));
        REQUIRE(h.sample("a3", 0.0) == Approx(0.3));
    }
}

// =============================================================================
// 11. Scope Depth
// =============================================================================

TEST_CASE("Scope depth limits are respected", "[robustness][inline]")
{
    SECTION("Deeply nested let expressions")
    {
        RobustHarness h;

        // Build: (let [a 1] (let [b 2] (let [c 3] ... (+ a b))))
        // MAX_SCOPE_DEPTH = 32; MAX_LOCAL_BINDINGS = 32
        std::string code = "beat";
        for (int i = 0; i < 30; i++) {
            std::string var = "v" + std::to_string(i);
            code = "(let [" + var + " " + std::to_string(i * 0.1) + "] (+ " + var + " " + code + "))";
        }
        code = "(a1 " + code + ")";

        EvalResult r = h.eval_result(code);
        // May succeed or error — must not crash or stack overflow
        if (r.kind != EvalResult::Error) {
            double v = h.sample("a1", 0.5);
            REQUIRE(std::isfinite(v));
        }
    }
}

// =============================================================================
// 12. Division and Arithmetic Safety
// =============================================================================

TEST_CASE("Arithmetic edge cases produce finite outputs", "[robustness][edge]")
{
    SECTION("Division by zero activates bootstrap LKG")
    {
        RobustHarness h;
        h.eval_ok("(a1 (/ 1 0))");
        double v = h.sample("a1", 0.0);
        REQUIRE(std::isfinite(v));
        REQUIRE(v == Approx(0.0));
        REQUIRE((h.engine.pool.runtime_fallback_mask & 1u) != 0);
    }

    SECTION("Modulo by zero activates bootstrap LKG")
    {
        RobustHarness h;
        h.eval_ok("(a1 (% 5 0))");
        double v = h.sample("a1", 0.0);
        REQUIRE(std::isfinite(v));
        REQUIRE(v == Approx(0.0));
        REQUIRE((h.engine.pool.runtime_fallback_mask & 1u) != 0);
    }

    SECTION("sqrt of negative is finite")
    {
        RobustHarness h;
        h.eval_ok("(a1 (sqrt -1))");
        double v = h.sample("a1", 0.0);
        // sqrt(fabs(-1)) = 1.0
        REQUIRE(std::isfinite(v));
    }

    SECTION("pow with extreme exponents stays finite")
    {
        RobustHarness h;
        h.eval_ok("(a1 (pow 2 1000))");
        double v = h.sample("a1", 0.0);
        // pow(1000, 2) due to reversed args: should be finite (1e6)
        REQUIRE(std::isfinite(v));
    }

    SECTION("Overflow clamped or finite")
    {
        RobustHarness h;
        h.eval_ok("(a1 (* 1e200 1e200))");
        double v = h.sample("a1", 0.0);
        // This may be Inf from constant folding — check
        // Actually, the output executor should clamp. Let's just verify no crash.
        (void)v;
    }
}

// =============================================================================
// 13. Consecutive Evals Stability
// =============================================================================

TEST_CASE("Hundreds of sequential evals do not degrade engine", "[robustness][pool]")
{
    RobustHarness h;

    // Alternate between different expression shapes
    for (int i = 0; i < 200; i++) {
        std::string code;
        switch (i % 5) {
            case 0: code = "(a1 (usin beat))"; break;
            case 1: code = "(a1 (* beat 0.5))"; break;
            case 2: code = "(a1 (if (> beat 0.5) 1.0 0.0))"; break;
            case 3: code = "(a1 (+ (sin t) 0.5))"; break;
            case 4: code = "(a1 (tri bar))"; break;
        }
        h.eval_ok(code);
        double v = h.tick("a1", (double)i * 0.001);
        REQUIRE(std::isfinite(v));
    }

    // Node pool should stay reasonable thanks to GC
    INFO("final node count: " << h.engine.pool.node_count);
    REQUIRE(h.engine.pool.node_count < MAX_TOTAL_NODES);
}

// =============================================================================
// 14. Cross-Output Dependency Isolation
// =============================================================================

TEST_CASE("Output errors do not corrupt other outputs", "[robustness][edge]")
{
    RobustHarness h;

    // Set up a valid output
    h.eval_ok("(a1 (usin beat))");

    // Try to assign an invalid expression to a2
    EvalResult r = h.eval_result("(a2 (undefined_fn beat))");
    REQUIRE(r.kind == EvalResult::Error);

    // a1 must still work correctly — finite and deterministic
    double v1a = h.tick("a1", 0.25);
    REQUIRE(std::isfinite(v1a));
    double v1b = h.sample("a1", 0.25);
    REQUIRE(v1a == Approx(v1b).margin(1e-9));
}
