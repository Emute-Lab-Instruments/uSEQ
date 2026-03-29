// Fuzz tests for the firmware signal engine.
// Uses grammar-aware generation, mutation, boundary values, and temporal stress
// to verify the engine never crashes and outputs stay finite.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"
#include "firmware_test_harness.h"
#include "dsp_helpers.h"

#include <random>
#include <string>
#include <sstream>
#include <cmath>
#include <vector>

// ── Random Expression Generator ──────────────────────────────────────────────

namespace fuzzgen {

static const char* binops[] = {"+", "-", "*", "/", "%", "min", "max", "pow"};
static const int num_binops = 8;

static const char* unary_ops[] = {
    "abs", "floor", "ceil", "sin", "cos", "sqrt", "usin", "ucos", "neg", "not", "frac"
};
static const int num_unary = 11;

static const char* waveform_ops[] = {"tri", "sqr", "usin", "ucos"};
static const int num_waveforms = 4;

static const char* temporal_refs[] = {"beat", "bar", "phrase", "section", "t"};
static const int num_temporals = 5;

static const char* ternary_ops[] = {"clamp", "lerp", "scale"};
static const int num_ternary = 3;

static const char* output_names[] = {"a1", "a2", "a3", "a4", "d1", "d2"};
static const int num_outputs = 6;

std::string random_number(std::mt19937& rng)
{
    std::uniform_int_distribution<int> kind(0, 5);
    switch (kind(rng)) {
    case 0: return "0";
    case 1: return "1";
    case 2: return "-1";
    case 3: {
        // Small number
        std::uniform_real_distribution<double> dist(-10.0, 10.0);
        std::ostringstream os;
        os << dist(rng);
        return os.str();
    }
    case 4: {
        // Large number
        std::uniform_real_distribution<double> dist(-1000.0, 1000.0);
        std::ostringstream os;
        os << dist(rng);
        return os.str();
    }
    case 5: {
        // Very small
        std::uniform_real_distribution<double> dist(1e-10, 1e-5);
        std::ostringstream os;
        os << dist(rng);
        return os.str();
    }
    }
    return "0";
}

std::string random_atom(std::mt19937& rng)
{
    std::uniform_int_distribution<int> choice(0, 1);
    if (choice(rng) == 0) {
        return random_number(rng);
    } else {
        return temporal_refs[rng() % num_temporals];
    }
}

std::string random_expr(int depth, std::mt19937& rng)
{
    if (depth <= 0) return random_atom(rng);

    std::uniform_int_distribution<int> form(0, 8);
    switch (form(rng)) {
    case 0: // binary op
        return "(" + std::string(binops[rng() % num_binops]) + " " +
               random_expr(depth - 1, rng) + " " +
               random_expr(depth - 1, rng) + ")";
    case 1: // unary op
        return "(" + std::string(unary_ops[rng() % num_unary]) + " " +
               random_expr(depth - 1, rng) + ")";
    case 2: // waveform
        return "(" + std::string(waveform_ops[rng() % num_waveforms]) + " " +
               random_expr(depth - 1, rng) + ")";
    case 3: // temporal ref
        return temporal_refs[rng() % num_temporals];
    case 4: // number
        return random_number(rng);
    case 5: // if
        return "(if " + random_expr(depth - 1, rng) + " " +
               random_expr(depth - 1, rng) + " " +
               random_expr(depth - 1, rng) + ")";
    case 6: // ternary op
        return "(" + std::string(ternary_ops[rng() % num_ternary]) + " " +
               random_expr(depth - 1, rng) + " " +
               random_expr(depth - 1, rng) + " " +
               random_expr(depth - 1, rng) + ")";
    case 7: // comparison
    {
        const char* cmp_ops[] = {">", "<", ">=", "<=", "="};
        return "(" + std::string(cmp_ops[rng() % 5]) + " " +
               random_expr(depth - 1, rng) + " " +
               random_expr(depth - 1, rng) + ")";
    }
    case 8: // logic
    {
        const char* logic_ops[] = {"and", "or"};
        return "(" + std::string(logic_ops[rng() % 2]) + " " +
               random_expr(depth - 1, rng) + " " +
               random_expr(depth - 1, rng) + ")";
    }
    }
    return random_atom(rng);
}

std::string random_output(std::mt19937& rng)
{
    return output_names[rng() % num_outputs];
}

// ── Mutation helpers ─────────────────────────────────────────────────────────

std::string mutate_delete_char(const std::string& s, std::mt19937& rng)
{
    if (s.size() <= 1) return s;
    size_t pos = rng() % s.size();
    return s.substr(0, pos) + s.substr(pos + 1);
}

std::string mutate_insert_char(const std::string& s, std::mt19937& rng)
{
    const char chars[] = "()abcdefghijklmnopqrstuvwxyz0123456789 +-*/.";
    size_t pos = rng() % (s.size() + 1);
    char c = chars[rng() % (sizeof(chars) - 1)];
    return s.substr(0, pos) + c + s.substr(pos);
}

std::string mutate_replace_char(const std::string& s, std::mt19937& rng)
{
    if (s.empty()) return s;
    const char chars[] = "()abcdefghijklmnopqrstuvwxyz0123456789 +-*/.";
    std::string result = s;
    size_t pos = rng() % result.size();
    result[pos] = chars[rng() % (sizeof(chars) - 1)];
    return result;
}

std::string mutate_truncate(const std::string& s, std::mt19937& rng)
{
    if (s.size() <= 1) return s;
    size_t pos = 1 + rng() % (s.size() - 1);
    return s.substr(0, pos);
}

std::string mutate_swap_chars(const std::string& s, std::mt19937& rng)
{
    if (s.size() < 2) return s;
    std::string result = s;
    size_t a = rng() % result.size();
    size_t b = rng() % result.size();
    std::swap(result[a], result[b]);
    return result;
}

std::string mutate_duplicate_substr(const std::string& s, std::mt19937& rng)
{
    if (s.size() < 2) return s + s;
    size_t start = rng() % (s.size() - 1);
    size_t len = 1 + rng() % std::min<size_t>(s.size() - start, 5);
    std::string sub = s.substr(start, len);
    size_t insert_pos = rng() % (s.size() + 1);
    return s.substr(0, insert_pos) + sub + s.substr(insert_pos);
}

std::string mutate(const std::string& s, std::mt19937& rng)
{
    std::uniform_int_distribution<int> strategy(0, 5);
    switch (strategy(rng)) {
    case 0: return mutate_delete_char(s, rng);
    case 1: return mutate_insert_char(s, rng);
    case 2: return mutate_replace_char(s, rng);
    case 3: return mutate_truncate(s, rng);
    case 4: return mutate_swap_chars(s, rng);
    case 5: return mutate_duplicate_substr(s, rng);
    }
    return s;
}

} // namespace fuzzgen

// ── Helper: check all outputs finite ─────────────────────────────────────────

static void assert_all_outputs_finite(FirmwareTestHarness& h)
{
    for (int ch = 0; ch < 24; ch++) {
        double v = h.get_output(ch);
        INFO("channel " << ch << " value " << v);
        REQUIRE(std::isfinite(v));
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Test 1: Grammar-Aware Expression Fuzzing
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Grammar-aware random expression fuzzing", "[fuzz][grammar]")
{
    std::mt19937 rng(42);

    for (int i = 0; i < 500; i++) {
        FirmwareTestHarness h;
        h.init();

        int depth = 1 + (rng() % 4); // depth 1-4
        std::string body = fuzzgen::random_expr(depth, rng);
        std::string output = fuzzgen::random_output(rng);
        std::string expr = "(" + output + " " + body + ")";

        INFO("iteration " << i << ": " << expr);

        // Eval must not crash
        auto result = h.eval(expr);

        // If it succeeded (assigned an output), tick and verify
        if (result.kind != sig::EvalResult::Error) {
            h.run_ticks(5);
            assert_all_outputs_finite(h);
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Test 2: Mutation Fuzzing
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Mutation fuzzing of valid expressions", "[fuzz][mutation]")
{
    const std::vector<std::string> seeds = {
        "(a1 (usin beat))",
        "(define x 42)",
        "(a1 (* beat 0.5))",
        "(a1 (if (> beat 0.5) 1.0 0.0))",
        "(a1 (euclid 3 8 beat))",
        "(a2 (+ (sin t) 0.5))",
        "(d1 (sqr beat))",
        "(a1 (clamp beat 0.0 1.0))",
        "(set-bpm 120)",
        "(a3 (lerp (usin beat) 0.2 0.8))",
    };

    std::mt19937 rng(123);

    for (int i = 0; i < 500; i++) {
        FirmwareTestHarness h;
        h.init();

        // Pick a seed and apply 1-3 mutations
        std::string expr = seeds[rng() % seeds.size()];
        int num_mutations = 1 + (rng() % 3);
        for (int m = 0; m < num_mutations; m++) {
            expr = fuzzgen::mutate(expr, rng);
        }

        INFO("iteration " << i << ": " << expr);

        // Eval — may succeed or fail, both are fine
        auto result = h.eval(expr);

        // If it didn't error, tick and check
        if (result.kind != sig::EvalResult::Error) {
            h.run_ticks(5);
        }
        // Regardless, tick once to make sure engine is stable
        h.tick();
        assert_all_outputs_finite(h);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Test 3: Boundary Value Fuzzing
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Boundary value expressions", "[fuzz][boundary]")
{
    SECTION("Extreme numeric values")
    {
        const std::vector<std::string> exprs = {
            "(a1 1e308)",
            "(a1 (* 1e200 1e200))",
            "(a1 1e-308)",
            "(a1 (/ 1e-300 1e300))",
            "(a1 0)",
            "(a1 (/ 0 0))",
            "(a1 (/ 1 0))",
            "(a1 -1)",
            "(a1 (* -1e100 1e100))",
            "(a1 (- 0 1e308))",
            "(a1 (+ 1e308 1e308))",
            "(a1 (sqrt -1))",
            "(a1 (/ 1 1e-308))",
            "(a1 (pow 2 1000))",
            "(a1 (pow -1 0.5))",
        };

        for (size_t i = 0; i < exprs.size(); i++) {
            FirmwareTestHarness h;
            h.init();

            INFO("expr: " << exprs[i]);
            h.eval(exprs[i]);
            h.run_ticks(10);
            assert_all_outputs_finite(h);
        }
    }

    SECTION("Deep nesting")
    {
        // Build a 30-level deep expression: (+ (+ (+ ... 1 2) 3) 4) ...
        FirmwareTestHarness h;
        h.init();

        std::string expr = "1";
        for (int depth = 0; depth < 30; depth++) {
            expr = "(+ " + expr + " " + std::to_string(depth) + ")";
        }
        expr = "(a1 " + expr + ")";

        INFO("deep nesting: " << expr.substr(0, 80) << "...");
        h.eval(expr);
        // May fail due to depth limit — that's OK
        h.run_ticks(5);
        assert_all_outputs_finite(h);
    }

    SECTION("Empty and whitespace inputs")
    {
        const std::vector<std::string> inputs = {
            "",
            " ",
            "  ",
            "\n",
            "\t",
            "\n\n\n",
        };

        for (const auto& input : inputs) {
            FirmwareTestHarness h;
            h.init();

            INFO("input: [" << input << "]");
            h.eval(input);
            h.tick();
            assert_all_outputs_finite(h);
        }
    }

    SECTION("Malformed parentheses")
    {
        const std::vector<std::string> inputs = {
            "((()))",
            ")))((((",
            "((((((",
            "))))))",
            "(",
            ")",
            "(a1",
            "a1)",
            "(a1 ())",
            "(a1 (+ ))",
            "(a1 (+ 1))",
            "(a1 (+ 1 2 3 4 5 6 7 8 9 10))",
        };

        for (const auto& input : inputs) {
            FirmwareTestHarness h;
            h.init();

            INFO("input: " << input);
            h.eval(input);
            h.tick();
            assert_all_outputs_finite(h);
        }
    }

    SECTION("BPM extremes")
    {
        const std::vector<std::string> bpm_cmds = {
            "(set-bpm 1)",
            "(set-bpm 10000)",
            "(set-bpm 0)",
            "(set-bpm -1)",
            "(set-bpm 0.001)",
            "(set-bpm 999999)",
        };

        for (const auto& cmd : bpm_cmds) {
            FirmwareTestHarness h;
            h.init();

            INFO("bpm cmd: " << cmd);
            h.eval(cmd);
            h.eval("(a1 (usin beat))");
            h.run_ticks(50);
            assert_all_outputs_finite(h);
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Test 4: Temporal Fuzzing (long-running tick with random injections)
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Temporal fuzzing with random command injection", "[fuzz][temporal]")
{
    std::mt19937 rng(12345);
    FirmwareTestHarness h;
    h.init();

    // Start with a valid output
    h.eval("(a1 (usin beat))");

    for (int tick = 0; tick < 5000; tick++) {
        // 10% chance: inject random valid expression as output
        if (rng() % 10 == 0) {
            std::string output = fuzzgen::random_output(rng);
            std::string body = fuzzgen::random_expr(2, rng);
            std::string expr = "(" + output + " " + body + ")";
            INFO("tick " << tick << " inject: " << expr);
            h.eval(expr);
        }

        // 2% chance: change BPM
        if (rng() % 50 == 0) {
            int bpm = 30 + (rng() % 300);
            std::string cmd = "(set-bpm " + std::to_string(bpm) + ")";
            h.eval(cmd);
        }

        // 5% chance: define a variable
        if (rng() % 20 == 0) {
            double val = static_cast<double>(rng() % 1000) / 100.0;
            std::string cmd = "(define fuzz_var " + std::to_string(val) + ")";
            h.eval(cmd);
        }

        h.tick();

        // Invariant: all outputs must be finite
        assert_all_outputs_finite(h);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Test 5: Rapid Redefine Stress
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Rapid redefine stress", "[fuzz][stress]")
{
    FirmwareTestHarness h;
    h.init();

    std::mt19937 rng(42);

    for (int i = 0; i < 500; i++) {
        // Assign a1 to a random expression each iteration
        std::string body = fuzzgen::random_expr(2, rng);
        std::string expr = "(a1 " + body + ")";

        INFO("iteration " << i << ": " << expr);
        h.eval(expr);
        h.tick();

        // Output must always be finite
        double v = h.get_output(0);
        INFO("a1 = " << v);
        REQUIRE(std::isfinite(v));
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Test 6: Multi-Output Fuzz
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Multi-output random assignment", "[fuzz][multi_output]")
{
    FirmwareTestHarness h;
    h.init();

    std::mt19937 rng(77);

    // Initialise all outputs with known-good expressions
    h.eval("(a1 (usin beat))");
    h.eval("(a2 0.5)");
    h.eval("(a3 beat)");
    h.eval("(a4 bar)");
    h.eval("(d1 (sqr beat))");
    h.eval("(d2 (tri beat))");
    h.run_ticks(10);

    for (int i = 0; i < 200; i++) {
        // Randomly reassign one of the 6 outputs
        std::string output = fuzzgen::random_output(rng);
        std::string body = fuzzgen::random_expr(2, rng);
        std::string expr = "(" + output + " " + body + ")";

        INFO("iteration " << i << ": " << expr);
        h.eval(expr);

        h.tick();

        // ALL outputs must be finite
        assert_all_outputs_finite(h);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Test 7: Sequential multi-mutation stress (single harness, many mutations)
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Sequential mutation storm on single harness", "[fuzz][mutation][stress]")
{
    FirmwareTestHarness h;
    h.init();

    std::mt19937 rng(999);

    // Start with valid output
    h.eval("(a1 (usin beat))");
    h.run_ticks(10);

    const std::vector<std::string> seeds = {
        "(a1 (usin beat))",
        "(a1 (* (sin t) 0.5))",
        "(a2 (tri bar))",
        "(d1 (> beat 0.5))",
    };

    for (int i = 0; i < 300; i++) {
        std::string expr = seeds[rng() % seeds.size()];
        // Apply 1-5 mutations
        int num_mut = 1 + (rng() % 5);
        for (int m = 0; m < num_mut; m++) {
            expr = fuzzgen::mutate(expr, rng);
        }

        INFO("iteration " << i << ": " << expr);
        h.eval(expr);
        h.run_ticks(3);
        assert_all_outputs_finite(h);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Test 8: Variable definition storm
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Variable definition and usage storm", "[fuzz][stress]")
{
    FirmwareTestHarness h;
    h.init();

    std::mt19937 rng(314);

    // Define a bunch of variables
    const char* var_names[] = {"x", "y", "z", "aa", "bb", "cc"};
    const int num_vars = 6;

    for (int i = 0; i < 200; i++) {
        // 50% define a variable, 50% use one in an output
        if (rng() % 2 == 0) {
            const char* var = var_names[rng() % num_vars];
            std::string body = fuzzgen::random_expr(2, rng);
            std::string cmd = "(define " + std::string(var) + " " + body + ")";
            INFO("define iteration " << i << ": " << cmd);
            h.eval(cmd);
        } else {
            std::string output = fuzzgen::random_output(rng);
            // Use a variable reference or a fresh expression
            std::string body;
            if (rng() % 3 == 0) {
                body = var_names[rng() % num_vars]; // may or may not exist
            } else {
                body = fuzzgen::random_expr(2, rng);
            }
            std::string cmd = "(" + output + " " + body + ")";
            INFO("output iteration " << i << ": " << cmd);
            h.eval(cmd);
        }

        h.tick();
        assert_all_outputs_finite(h);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Test 9: Output capture integrity under fuzz
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Captured output stays finite under random expressions", "[fuzz][capture]")
{
    std::mt19937 rng(2024);
    FirmwareTestHarness h;
    h.init();

    for (int round = 0; round < 50; round++) {
        // Assign random expression to a1
        std::string body = fuzzgen::random_expr(3, rng);
        std::string expr = "(a1 " + body + ")";
        INFO("round " << round << ": " << expr);
        h.eval(expr);

        h.start_capture();
        h.run_ticks(100);
        h.stop_capture();

        auto buf = h.get_captured(0);
        REQUIRE(buf.size() == 100);
        REQUIRE(dsp::all_finite(buf));
    }
}
