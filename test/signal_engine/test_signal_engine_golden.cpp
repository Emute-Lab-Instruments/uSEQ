// Signal engine golden semantics.
//
// These tests are intentionally written at the language boundary: eval source
// text, sample named outputs, and assert user-visible values. Keep low-level
// node-shape tests in test_signal_engine.cpp.

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
    double prev_t = 0.0;       // track previous tick time for dt
    double last_dt = 0.0;      // dt used for current tick
    bool has_ticked = false;   // false until first tick completes
    bool state_committed_this_step = false; // prevents double-commit at same t

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

        last_dt = t - prev_t;

        // State update: at the start of each NEW time-step (after the very
        // first tick), execute update graphs and commit state before outputs.
        if (engine.pool.state_slot_count > 0 &&
            has_ticked && t != prev_t && !state_committed_this_step)
        {
            // Execute all nodes to evaluate state update subgraphs
            ExecutionContext state_ctx;
            state_ctx.t = t;
            state_ctx.dt = last_dt;
            state_ctx.cell_values = cell_values;
            state_ctx.hw_inputs = hw_inputs;
            state_ctx.data_pool = engine.cells.data_pool;
            state_ctx.data_offsets = engine.cells.data_offsets;
            state_ctx.data_lengths = engine.cells.data_lengths;
            state_ctx.prev_outputs = engine.pool.prev_output_values;
            state_ctx.output_values = outputs;
            state_ctx.workspace = workspace;
            execute_all_outputs(engine.pool, state_ctx);
            commit_state(engine.pool, workspace);
            state_committed_this_step = true;

            // Clear workspace and outputs for the real output evaluation
            std::memset(workspace, 0, sizeof(workspace));
            std::memset(outputs, 0, sizeof(outputs));
        }

        ExecutionContext ctx;
        ctx.t = t;
        ctx.dt = last_dt;
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
        if (t != prev_t) {
            state_committed_this_step = false;
        }
        double value = sample(output_name, t);
        commit_outputs(engine.pool, outputs);
        has_ticked = true;
        prev_t = t;
        return value;
    }

    // ── Diagnostic assertion helpers ───────────────────────────────────────

    // Assert that eval produces a diagnostic with the expected category.
    // Optionally check that message and suggestion contain given substrings.
    void expect_error(const std::string& code,
                      DiagnosticCategory expected_cat,
                      const char* message_contains = nullptr,
                      const char* suggestion_contains = nullptr)
    {
        EvalResult r = eval_result(code);
        INFO("code: " << code);
        REQUIRE(r.kind == EvalResult::Error);
        REQUIRE(r.diagnostic_count > 0);

        bool found = false;
        for (uint8_t i = 0; i < r.diagnostic_count; ++i) {
            if (r.diagnostics[i].category == expected_cat) {
                found = true;
                if (message_contains) {
                    REQUIRE(r.diagnostics[i].message != nullptr);
                    INFO("expected message to contain: " << message_contains);
                    INFO("actual message: " << r.diagnostics[i].message);
                    REQUIRE(std::string(r.diagnostics[i].message)
                            .find(message_contains) != std::string::npos);
                }
                if (suggestion_contains) {
                    REQUIRE(r.diagnostics[i].suggestion != nullptr);
                    INFO("expected suggestion to contain: " << suggestion_contains);
                    INFO("actual suggestion: " << r.diagnostics[i].suggestion);
                    REQUIRE(std::string(r.diagnostics[i].suggestion)
                            .find(suggestion_contains) != std::string::npos);
                }
                break;
            }
        }
        INFO("expected category: " << category_to_cstr(expected_cat));
        REQUIRE(found);
    }

    // Assert that eval produces a diagnostic at an exact span location.
    void expect_error_at(const std::string& code,
                         DiagnosticCategory expected_cat,
                         uint16_t span_start, uint16_t span_len,
                         const char* message_contains = nullptr)
    {
        EvalResult r = eval_result(code);
        INFO("code: " << code);
        REQUIRE(r.kind == EvalResult::Error);
        REQUIRE(r.diagnostic_count > 0);

        bool found = false;
        for (uint8_t i = 0; i < r.diagnostic_count; ++i) {
            if (r.diagnostics[i].category == expected_cat) {
                found = true;
                INFO("expected span_start: " << span_start
                     << " actual: " << r.diagnostics[i].span_start);
                REQUIRE(r.diagnostics[i].span_start == span_start);
                INFO("expected span_len: " << span_len
                     << " actual: " << r.diagnostics[i].span_len);
                REQUIRE(r.diagnostics[i].span_len == span_len);
                if (message_contains) {
                    REQUIRE(r.diagnostics[i].message != nullptr);
                    REQUIRE(std::string(r.diagnostics[i].message)
                            .find(message_contains) != std::string::npos);
                }
                break;
            }
        }
        REQUIRE(found);
    }

    // Assert that a suggestion from a diagnostic compiles successfully.
    // This validates that error suggestions are valid ModuLisp code.
    void expect_suggestion_compiles(const std::string& code,
                                     DiagnosticCategory expected_cat)
    {
        EvalResult r = eval_result(code);
        REQUIRE(r.kind == EvalResult::Error);
        REQUIRE(r.diagnostic_count > 0);

        for (uint8_t i = 0; i < r.diagnostic_count; ++i) {
            if (r.diagnostics[i].category == expected_cat &&
                r.diagnostics[i].suggestion != nullptr) {
                std::string suggestion = r.diagnostics[i].suggestion;
                // Strip "Try: " prefix if present
                if (suggestion.substr(0, 5) == "Try: ") {
                    suggestion = suggestion.substr(5);
                }
                // Only test suggestions that look like expressions
                // (start with '(' or are a bare symbol)
                if (!suggestion.empty() && suggestion[0] != 'C' &&
                    suggestion[0] != 'U' && suggestion[0] != 'E' &&
                    suggestion[0] != 'D') {
                    // Wrap in output assignment to test compilability
                    std::string test_code = "(a1 " + suggestion + ")";
                    EvalResult sr = eval_result(test_code);
                    INFO("suggestion code: " << test_code);
                    INFO("original error code: " << code);
                    // NOTE: Some suggestions are just symbol names, which might
                    // not be defined. We only check that parsing/compilation
                    // doesn't produce a Syntax error.
                    if (sr.kind == EvalResult::Error) {
                        for (uint8_t j = 0; j < sr.diagnostic_count; ++j) {
                            INFO("suggestion diagnostic: "
                                 << (sr.diagnostics[j].message ? sr.diagnostics[j].message : ""));
                            REQUIRE(sr.diagnostics[j].category != DiagnosticCategory::Syntax);
                        }
                    }
                }
                break;
            }
        }
    }

    // ── Sampling helpers ───────────────────────────────────────────────────

    // Sample across a time window and return values.
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

    // ── Batch comparison helper ────────────────────────────────────────────

    // Find the active-output index for batch buffer layout.
    // execute_batch packs only outputs with root_node != NODE_NONE, in
    // slot order. Returns the 0-based position of the requested output
    // within the active set.
    uint16_t batch_output_index(const char* output_name)
    {
        uint16_t slot = output_index(output_name);
        uint16_t active = 0;
        for (uint16_t o = 0; o < slot; o++) {
            if (engine.pool.outputs[o].root_node != NODE_NONE)
                active++;
        }
        return active;
    }

    // Verify batch execution matches single-sample across a time window.
    void verify_batch_matches_single(const char* output,
                                      double t_start, double t_end,
                                      size_t count,
                                      double tolerance = 1e-9)
    {
        engine.pool.allocate_batch_workspace();

        std::vector<double> times(count);
        for (size_t i = 0; i < count; ++i) {
            times[i] = t_start + (t_end - t_start)
                       * static_cast<double>(i) / static_cast<double>(count - 1);
        }

        // Count active outputs for buffer sizing
        uint16_t active_count = 0;
        for (uint16_t o = 0; o < MAX_OUTPUTS; o++) {
            if (engine.pool.outputs[o].root_node != NODE_NONE)
                active_count++;
        }

        std::vector<double> batch_buf(active_count * count, 0.0);
        engine.cells.snapshot_values(cell_values, MAX_CELLS);
        execute_batch(engine.pool,
                      times.data(),
                      count,
                      cell_values,
                      hw_inputs,
                      engine.cells.data_pool,
                      engine.cells.data_offsets,
                      engine.cells.data_lengths,
                      batch_buf.data(),
                      active_count);

        uint16_t idx = batch_output_index(output);
        for (size_t i = 0; i < count; ++i) {
            INFO("sample index: " << i << "  t: " << times[i]);
            double single_val = sample(output, times[i]);
            REQUIRE(batch_buf[idx * count + i] == Approx(single_val).margin(tolerance));
        }
    }

    // ── Multi-tick helper ──────────────────────────────────────────────────

    // Execute a sequence of ticks and return committed values at each.
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

void check_expr(const char* name,
                const char* expr,
                std::initializer_list<Sample> samples,
                double bpm = 120.0,
                int beats_per_bar = 4,
                const char* setup = nullptr,
                const char* output = "a1")
{
    DYNAMIC_SECTION(name) {
        GoldenHarness h(bpm, beats_per_bar);
        if (setup) h.eval_ok(setup);
        h.assign_ok(output, expr);

        for (const Sample& sample : samples) {
            INFO("expr: " << expr);
            INFO("t: " << sample.t);
            double actual = h.sample(output, sample.t);
            REQUIRE(actual == Approx(sample.expected).margin(sample.tolerance));
        }
    }
}

EvalResult eval_output_error(const char* expr)
{
    GoldenHarness h;
    std::string code = std::string("(a1 ") + expr + ")";
    return h.eval_result(code);
}

} // namespace

TEST_CASE("Golden semantics: numeric expressions are lifted into signals",
          "[golden][signal_engine][semantics]") {
    check_expr("constant arithmetic", "(+ 1 2)", {{0.0, 3.0}});
    check_expr("variadic addition", "(+ 1 2 3 4)", {{0.0, 10.0}});
    check_expr("variadic subtraction left folds", "(- 100 30 20 10)", {{0.0, 40.0}});
    check_expr("unary negation form", "(- 5)", {{0.0, -5.0}});
    check_expr("legacy pow order is exponent then base", "(pow 2 10)", {{0.0, 100.0}});
    check_expr("standard expt order is base then exponent", "(expt 2 10)", {{0.0, 1024.0}});
    check_expr("fractional arithmetic", "(+ 0.1 0.2)", {{0.0, 0.3, 1e-6}});
    check_expr("division fraction", "(/ 1 3)", {{0.0, 1.0 / 3.0, 1e-9}});
    check_expr("constant folding is time-invariant",
               "(+ (* 2 3) 4)",
               {{0.0, 10.0}, {0.5, 10.0}, {1.0, 10.0}});

    check_expr("numeric truthiness treats any non-zero as true", "(if -0.1 7 9)", {{0.0, 7.0}});
    check_expr("and returns numeric truth", "(and 2 -3)", {{0.0, 1.0}});
    check_expr("or returns numeric truth", "(or 0 -2)", {{0.0, 1.0}});
    check_expr("if without else defaults to zero", "(if 0 42)", {{0.0, 0.0}});

    check_expr("clamp below range", "(clamp -1 0 3)", {{0.0, 0.0}});
    check_expr("clamp above range", "(clamp 5 0 3)", {{0.0, 3.0}});
    check_expr("lerp midpoint", "(lerp 0 10 0.5)", {{0.0, 5.0}});
    check_expr("scale unipolar value", "(scale 0.5 100 200)", {{0.0, 150.0}});
}

TEST_CASE("Golden semantics: time, phasors, and time substitutions",
          "[golden][signal_engine][time]") {
    check_expr("raw t is seconds", "t", {{0.0, 0.0}, {1.25, 1.25}});
    check_expr("beat phasor at 120 bpm",
               "beat",
               {{0.0, 0.0}, {0.125, 0.25}, {0.25, 0.5}, {0.5, 0.0}});
    check_expr("beat phasor at 60 bpm",
               "beat",
               {{0.0, 0.0}, {0.5, 0.5}, {1.0, 0.0}},
               60.0);
    check_expr("bar phasor at 120 bpm 4/4",
               "bar",
               {{0.0, 0.0}, {0.5, 0.25}, {1.0, 0.5}, {2.0, 0.0}});
    check_expr("phrase phasor uses bars-per-phrase default",
               "phrase",
               {{0.0, 0.0}, {4.0, 0.5}, {8.0, 0.0}});
    check_expr("section phasor uses phrase defaults",
               "section",
               {{0.0, 0.0}, {16.0, 0.5}, {32.0, 0.0}});
    check_expr("beat-num is an unwrapped counter",
               "beat-num",
               {{0.0, 0.0}, {0.49, 0.0}, {0.5, 1.0}, {1.25, 2.0}});
    check_expr("bar-num is an unwrapped counter",
               "bar-num",
               {{0.0, 0.0}, {1.99, 0.0}, {2.0, 1.0}, {4.1, 2.0}});
    check_expr("beat-dur follows bpm", "beat-dur", {{0.0, 0.5}}, 120.0);
    check_expr("bar-dur follows metre", "bar-dur", {{0.0, 1.5}}, 120.0, 3);

    check_expr("fast doubles local time", "(fast 2 beat)", {{0.125, 0.5}, {0.25, 0.0}});
    check_expr("slow halves local time", "(slow 2 beat)", {{0.25, 0.25}, {0.5, 0.5}});
    check_expr("nested constant warps compose",
               "(fast 2 (slow 4 beat))",
               {{0.5, 0.5}, {1.0, 0.0}});
    check_expr("dynamic fast is pointwise substitution",
               "(fast (+ 1 beat) beat)",
               {{0.125, 0.3125}});
    check_expr("offset uses seconds",
               "(offset 0.25 beat)",
               {{0.0, 0.5}, {0.125, 0.75}});
    check_expr("shift aliases offset",
               "(shift 0.25 beat)",
               {{0.0, 0.5}, {0.125, 0.75}});
    check_expr("musical offset can use beat-dur",
               "(offset (* 0.5 beat-dur) beat)",
               {{0.0, 0.5}});
}

TEST_CASE("Golden semantics: waveform and sequence helpers",
          "[golden][signal_engine][sequences]") {
    check_expr("usin is unipolar sine over phase",
               "(usin beat)",
               {{0.0, 0.5, 1e-9}, {0.125, 1.0, 1e-9}, {0.375, 0.0, 1e-9}});
    check_expr("ucos is unipolar cosine over phase",
               "(ucos beat)",
               {{0.0, 1.0, 1e-9}, {0.25, 0.0, 1e-9}});
    check_expr("tri supports a pivot argument", "(tri 0.5 0.25)", {{0.0, 0.5, 1e-9}});
    check_expr("sqr thresholds phase at half", "(sqr beat)", {{0.125, 1.0}, {0.375, 0.0}});
    check_expr("pulse uses supplied width", "(pulse beat 0.25)", {{0.1, 1.0}, {0.2, 0.0}});
    check_expr("bipolar to unipolar", "(b>u -1)", {{0.0, 0.0}});
    check_expr("unipolar to bipolar", "(u>b 0.25)", {{0.0, -0.5}});

    check_expr("step over literal vector",
               "(step [10 20 30 40] beat)",
               {{0.0, 10.0}, {0.125, 20.0}, {0.25, 30.0}, {0.375, 40.0}});
    check_expr("from-list is stepped lookup",
               "(from-list [10 20 30] beat)",
               {{0.0, 10.0}, {0.25, 20.0}, {0.49, 30.0}});
    check_expr("seq aliases from-list", "(seq [100 200] beat)", {{0.0, 100.0}, {0.25, 200.0}});
    check_expr("interp linearly interpolates",
               "(interp [0 1 0] beat)",
               {{0.0, 0.0}, {0.125, 0.5}, {0.25, 1.0}, {0.375, 0.5}});
    check_expr("gates follows pattern", "(gates [1 0 1 0] beat)", {{0.0, 1.0}, {0.125, 0.0}});
    check_expr("trigs aliases gates", "(trigs [1 0 1 0] beat)", {{0.0, 1.0}, {0.125, 0.0}});
    check_expr("euclid distributes hits", "(euclid 3 8 beat)", {{0.0, 1.0}, {0.125, 0.0}});
}

TEST_CASE("Golden semantics: top-level cells, functions, and reactivity",
          "[golden][signal_engine][reactivity]") {
    check_expr("number cell reference",
               "freq",
               {{0.0, 440.0}},
               120.0,
               4,
               "(define freq 440)");
    check_expr("expression cell is time-varying",
               "sweep",
               {{0.0, 200.0}, {0.25, 250.0}},
               120.0,
               4,
               "(define sweep (+ 200 (* 200 t)))");
    check_expr("data cell feeds step",
               "(step pattern beat)",
               {{0.0, 10.0}, {0.25, 30.0}},
               120.0,
               4,
               "(define pattern [10 20 30 40])");
    check_expr("defn with one argument inlines at call site",
               "(double 5)",
               {{0.0, 10.0}},
               120.0,
               4,
               "(defn double [x] (* x 2))");
    check_expr("defn inherits caller time context",
               "(fast 2 (scaled-beat 3))",
               {{0.125, 1.5}},
               120.0,
               4,
               "(defn scaled-beat [s] (* s beat))");

    SECTION("redefining a depended-on cell recompiles output") {
        GoldenHarness h;
        h.eval_ok("(define freq 440)");
        h.assign_ok("a1", "freq");
        REQUIRE(h.sample("a1", 0.0) == Approx(440.0));

        h.eval_ok("(define freq 880)");
        REQUIRE(h.sample("a1", 0.0) == Approx(880.0));
    }

    SECTION("function redefinition recompiles callers") {
        GoldenHarness h;
        h.eval_ok("(defn f [x] (* x 2))");
        h.assign_ok("a1", "(f 5)");
        REQUIRE(h.sample("a1", 0.0) == Approx(10.0));

        h.eval_ok("(defn f [x] (* x 3))");
        REQUIRE(h.sample("a1", 0.0) == Approx(15.0));
    }
}

TEST_CASE("Golden semantics: outputs, prev, inputs, and batch execution",
          "[golden][signal_engine][execution]") {
    SECTION("unassigned outputs use neutral zero") {
        GoldenHarness h;
        REQUIRE(h.sample("a1", 0.0) == Approx(0.0));
        REQUIRE(h.sample("d1", 0.0) == Approx(0.0));
    }

    SECTION("bare output reference reads previous committed sample") {
        GoldenHarness h;
        h.eval_ok("(a1 0.75) (a2 a1)");

        REQUIRE(h.tick("a2", 0.0) == Approx(0.0));
        REQUIRE(h.tick("a2", 0.001) == Approx(0.75));
    }

    SECTION("explicit prev reads previous committed sample") {
        GoldenHarness h;
        h.eval_ok("(d1 1.0) (a1 (prev d1))");

        REQUIRE(h.tick("a1", 0.0) == Approx(0.0));
        REQUIRE(h.tick("a1", 0.001) == Approx(1.0));
    }

    SECTION("hardware input leaves read injected input values") {
        GoldenHarness h;
        h.hw_inputs[0] = 1.0;   // in1
        h.hw_inputs[2] = 0.25;  // ain1
        h.assign_ok("a1", "(+ in1 ain1)");
        REQUIRE(h.sample("a1", 0.0) == Approx(1.25));
    }

    SECTION("batch execution matches single-sample execution") {
        GoldenHarness h;
        h.assign_ok("a1", "(+ (usin beat) (* 0.25 bar))");
        h.engine.pool.allocate_batch_workspace();

        constexpr size_t sample_count = 5;
        double times[sample_count] = {0.0, 0.125, 0.25, 0.375, 0.5};
        double batch[MAX_OUTPUTS * sample_count] = {};

        h.engine.cells.snapshot_values(h.cell_values, MAX_CELLS);
        execute_batch(h.engine.pool,
                      times,
                      sample_count,
                      h.cell_values,
                      h.hw_inputs,
                      h.engine.cells.data_pool,
                      h.engine.cells.data_offsets,
                      h.engine.cells.data_lengths,
                      batch,
                      MAX_OUTPUTS);

        for (size_t i = 0; i < sample_count; ++i) {
            INFO("sample index: " << i);
            double single = h.sample("a1", times[i]);
            REQUIRE(batch[i] == Approx(single).margin(1e-9));
        }
    }
}

TEST_CASE("Golden semantics: diagnostics and failure isolation",
          "[golden][signal_engine][diagnostics]") {
    SECTION("unknown names produce a diagnostic with fuzzy suggestions") {
        GoldenHarness h;
        h.eval_ok("(define freq 440)");
        EvalResult r = h.eval_result("(a1 frq)");

        REQUIRE(r.kind == EvalResult::Error);
        REQUIRE(r.diagnostic_count > 0);
        REQUIRE(r.diagnostics[0].severity == DiagnosticSeverity::Error);
        REQUIRE(r.diagnostics[0].span_start == 4);
        REQUIRE(r.diagnostics[0].span_len == 3);
        REQUIRE(r.diagnostics[0].message != nullptr);
        REQUIRE(std::string(r.diagnostics[0].message).find("freq") != std::string::npos);
        REQUIRE(r.diagnostics[0].suggestion != nullptr);
    }

    SECTION("side effects are rejected inside output expressions") {
        EvalResult r = eval_output_error("(define x 1)");

        REQUIRE(r.kind == EvalResult::Error);
        REQUIRE(r.diagnostic_count > 0);
        REQUIRE(r.diagnostics[0].severity == DiagnosticSeverity::Error);
        REQUIRE(r.diagnostics[0].message != nullptr);
        REQUIRE(std::string(r.diagnostics[0].message).find("inside an output") != std::string::npos);
    }

    SECTION("recursion is rejected instead of hanging") {
        GoldenHarness h;
        h.eval_ok("(defn f [x] (f x))");
        EvalResult r = h.eval_result("(a1 (f 1))");

        REQUIRE(r.kind == EvalResult::Error);
        REQUIRE(r.diagnostic_count > 0);
        REQUIRE(r.diagnostics[0].message != nullptr);
        REQUIRE(std::string(r.diagnostics[0].message).find("recursive") != std::string::npos);
    }

    SECTION("compile error leaves previous output running") {
        GoldenHarness h;
        h.assign_ok("a1", "(+ beat 1)");
        REQUIRE(h.sample("a1", 0.25) == Approx(1.5));

        EvalResult bad = h.eval_result("(a1 (unknown-name beat))");
        REQUIRE(bad.kind == EvalResult::Error);
        REQUIRE(h.sample("a1", 0.25) == Approx(1.5));
    }

    SECTION("one output compile error does not affect another output") {
        GoldenHarness h;
        h.assign_ok("a1", "0.2");
        h.assign_ok("a2", "0.8");

        EvalResult bad = h.eval_result("(a1 (not-a-function 1))");
        REQUIRE(bad.kind == EvalResult::Error);
        REQUIRE(h.sample("a1", 0.0) == Approx(0.2));
        REQUIRE(h.sample("a2", 0.0) == Approx(0.8));
    }

    SECTION("expect_error helper: unknown name produces error with message") {
        GoldenHarness h;
        h.expect_error("(a1 nonexistent)", DiagnosticCategory::UndefinedName,
                        "Unknown");
    }

    SECTION("expect_error_at helper: span points at the bad symbol") {
        GoldenHarness h;
        h.eval_ok("(define freq 440)");
        // "(a1 frq)" — 'frq' starts at offset 4, length 3
        h.expect_error_at("(a1 frq)",
                          DiagnosticCategory::UndefinedName,
                          4, 3, "freq");
    }

    SECTION("expect_error helper: side effects rejected") {
        GoldenHarness h;
        h.expect_error("(a1 (define x 1))",
                        DiagnosticCategory::Boundary,
                        "inside an output");
    }
}

TEST_CASE("Golden semantics: sample_window and tick_sequence helpers",
          "[golden][signal_engine][helpers]") {
    SECTION("sample_window returns correct count and values") {
        GoldenHarness h;
        h.assign_ok("a1", "t");
        auto window = h.sample_window("a1", 0.0, 1.0, 5);
        REQUIRE(window.size() == 5);
        REQUIRE(window[0] == Approx(0.0));
        REQUIRE(window[1] == Approx(0.25));
        REQUIRE(window[2] == Approx(0.5));
        REQUIRE(window[3] == Approx(0.75));
        REQUIRE(window[4] == Approx(1.0));
    }

    SECTION("tick_sequence returns committed values at each step") {
        GoldenHarness h;
        h.eval_ok("(a1 0.75) (a2 a1)");

        auto seq = h.tick_sequence("a2", {0.0, 0.001, 0.002});
        REQUIRE(seq.size() == 3);
        REQUIRE(seq[0] == Approx(0.0));       // no prev yet
        REQUIRE(seq[1] == Approx(0.75));      // prev a1 is now 0.75
        REQUIRE(seq[2] == Approx(0.75));      // still 0.75
    }

    SECTION("verify_batch_matches_single for a compound expression") {
        GoldenHarness h;
        h.assign_ok("a1", "(+ (usin beat) (* 0.1 bar))");
        h.verify_batch_matches_single("a1", 0.0, 1.0, 10);
    }

    SECTION("verify_batch_matches_single for constant expression") {
        GoldenHarness h;
        h.assign_ok("a1", "42");
        h.verify_batch_matches_single("a1", 0.0, 2.0, 8);
    }
}

TEST_CASE("Golden semantics: additional coverage for spec gaps",
          "[golden][signal_engine][coverage]") {
    SECTION("variadic multiply folds correctly") {
        GoldenHarness h;
        h.assign_ok("a1", "(* 2 3 4)");
        REQUIRE(h.sample("a1", 0.0) == Approx(24.0));
    }

    SECTION("binary division") {
        GoldenHarness h;
        h.assign_ok("a1", "(/ 10 4)");
        REQUIRE(h.sample("a1", 0.0) == Approx(2.5));
    }

    SECTION("variadic division left folds") {
        GoldenHarness h;
        h.assign_ok("a1", "(/ 120 2 3)");
        REQUIRE(h.sample("a1", 0.0) == Approx(20.0));
    }

    SECTION("min selects minimum of two") {
        GoldenHarness h;
        h.assign_ok("a1", "(min 5 2)");
        REQUIRE(h.sample("a1", 0.0) == Approx(2.0));
    }

    SECTION("max selects maximum of two") {
        GoldenHarness h;
        h.assign_ok("a1", "(max 5 2)");
        REQUIRE(h.sample("a1", 0.0) == Approx(5.0));
    }

    SECTION("nil is falsy in if") {
        GoldenHarness h;
        h.assign_ok("a1", "(if 0 10 20)");
        REQUIRE(h.sample("a1", 0.0) == Approx(20.0));
    }

    SECTION("nested if picks correct branch") {
        GoldenHarness h;
        h.assign_ok("a1", "(if 1 (if 0 10 20) 30)");
        REQUIRE(h.sample("a1", 0.0) == Approx(20.0));
    }

    SECTION("let creates local scope") {
        GoldenHarness h;
        h.assign_ok("a1", "(let [x 10 y 20] (+ x y))");
        REQUIRE(h.sample("a1", 0.0) == Approx(30.0));
    }

    SECTION("data cell with time-varying lookup") {
        GoldenHarness h;
        h.eval_ok("(define notes [60 64 67 72])");
        h.assign_ok("a1", "(interp notes beat)");
        // At t=0.0, beat=0.0, scaled=0*3=0, interp = notes[0] = 60
        REQUIRE(h.sample("a1", 0.0) == Approx(60.0));
        // At t=0.25, beat=0.5, scaled=0.5*3=1.5, interp = 64 + 0.5*(67-64) = 65.5
        REQUIRE(h.sample("a1", 0.25) == Approx(65.5));
    }

    SECTION("diamond dependency: redefining root propagates through chain") {
        GoldenHarness h;
        h.eval_ok("(define root 1)");
        h.eval_ok("(define mid (+ root 10))");
        h.assign_ok("a1", "(+ mid 100)");
        REQUIRE(h.sample("a1", 0.0) == Approx(111.0));

        h.eval_ok("(define root 5)");
        REQUIRE(h.sample("a1", 0.0) == Approx(115.0));
    }

    SECTION("self-reference via prev for integration") {
        GoldenHarness h;
        h.eval_ok("(a1 (+ (prev a1) 0.1))");

        auto seq = h.tick_sequence("a1", {0.0, 0.001, 0.002, 0.003});
        REQUIRE(seq[0] == Approx(0.1));   // prev starts at 0
        REQUIRE(seq[1] == Approx(0.2));
        REQUIRE(seq[2] == Approx(0.3));
        REQUIRE(seq[3] == Approx(0.4));
    }

    SECTION("fast does not affect cells defined outside the warp") {
        GoldenHarness h;
        h.eval_ok("(define k 10)");
        h.assign_ok("a1", "(fast 2 (+ k beat))");
        // At t=0.125, fast-2 makes local t=0.25, beat=0.5, k=10 (unchanged)
        REQUIRE(h.sample("a1", 0.125) == Approx(10.5));
    }

    SECTION("phrase phasor wraps at expected period") {
        GoldenHarness h(120.0, 4);
        h.assign_ok("a1", "phrase");
        // At 120 bpm, 4/4, bars_per_phrase=4: one phrase = 4 bars = 8 seconds
        REQUIRE(h.sample("a1", 0.0) == Approx(0.0));
        REQUIRE(h.sample("a1", 4.0) == Approx(0.5));
        // At exactly 8.0 it wraps back to 0
        REQUIRE(h.sample("a1", 8.0) == Approx(0.0).margin(1e-9));
    }

    SECTION("section phasor wraps at expected period") {
        GoldenHarness h(120.0, 4);
        h.assign_ok("a1", "section");
        // 4 phrases per section, each phrase = 8s, so section = 32s
        REQUIRE(h.sample("a1", 0.0) == Approx(0.0));
        REQUIRE(h.sample("a1", 16.0) == Approx(0.5));
        REQUIRE(h.sample("a1", 32.0) == Approx(0.0).margin(1e-9));
    }

    SECTION("recursion rejection includes message about 'recursive'") {
        GoldenHarness h;
        h.eval_ok("(defn g [x] (g x))");
        h.expect_error("(a1 (g 1))", DiagnosticCategory::Runtime,
                        "recursive");
    }

    SECTION("defs batch define works") {
        GoldenHarness h;
        h.eval_ok("(defs [x 10 y 20])");
        h.assign_ok("a1", "(+ x y)");
        REQUIRE(h.sample("a1", 0.0) == Approx(30.0));
    }

    SECTION("fn anonymous lambda in signal context is rejected") {
        GoldenHarness h;
        h.expect_error("(a1 (fn [x] (* x 2)))", DiagnosticCategory::Boundary,
                        "Lambda");
    }

    SECTION("defn with multiple arguments") {
        GoldenHarness h;
        h.eval_ok("(defn add3 [a b c] (+ a b c))");
        h.assign_ok("a1", "(add3 1 2 3)");
        REQUIRE(h.sample("a1", 0.0) == Approx(6.0));
    }

    SECTION("multiple outputs are independent") {
        GoldenHarness h;
        h.assign_ok("a1", "0.25");
        h.assign_ok("a2", "0.75");
        h.assign_ok("d1", "1.0");

        REQUIRE(h.sample("a1", 0.0) == Approx(0.25));
        REQUIRE(h.sample("a2", 0.0) == Approx(0.75));
        REQUIRE(h.sample("d1", 0.0) == Approx(1.0));
    }

    SECTION("sample_window over beat shows monotonic ramp") {
        GoldenHarness h;
        h.assign_ok("a1", "beat");
        // Half a beat at 120 bpm = 0.25 seconds
        auto window = h.sample_window("a1", 0.0, 0.25, 5);
        REQUIRE(window.size() == 5);
        for (size_t i = 1; i < window.size(); ++i) {
            INFO("window[" << i-1 << "]=" << window[i-1]
                 << " window[" << i << "]=" << window[i]);
            REQUIRE(window[i] > window[i-1]);
        }
    }
}

// ============================================================================
// Phase 2: Close core semantic spec gaps
// ============================================================================

TEST_CASE("Golden semantics: variadic arithmetic edge cases (functions.md 1.7)",
          "[golden][signal_engine][variadic]") {

    SECTION("(+) nullary returns identity 0") {
        GoldenHarness h;
        h.assign_ok("a1", "(+)");
        REQUIRE(h.sample("a1", 0.0) == Approx(0.0));
    }

    SECTION("(*) nullary returns identity 1") {
        GoldenHarness h;
        h.assign_ok("a1", "(*)");
        REQUIRE(h.sample("a1", 0.0) == Approx(1.0));
    }

    SECTION("(/ x) unary division returns reciprocal (/ 1 x)") {
        GoldenHarness h;
        h.assign_ok("a1", "(/ 4)");
        REQUIRE(h.sample("a1", 0.0) == Approx(0.25));
    }

    SECTION("(- x) unary negation") {
        GoldenHarness h;
        h.assign_ok("a1", "(- 7)");
        REQUIRE(h.sample("a1", 0.0) == Approx(-7.0));
    }

    SECTION("variadic min with 3 args") {
        GoldenHarness h;
        h.assign_ok("a1", "(min 5 2 8)");
        REQUIRE(h.sample("a1", 0.0) == Approx(2.0));
    }

    SECTION("variadic min with 4 args") {
        GoldenHarness h;
        h.assign_ok("a1", "(min 10 3 7 1)");
        REQUIRE(h.sample("a1", 0.0) == Approx(1.0));
    }

    SECTION("variadic max with 3 args") {
        GoldenHarness h;
        h.assign_ok("a1", "(max 5 2 8)");
        REQUIRE(h.sample("a1", 0.0) == Approx(8.0));
    }

    SECTION("variadic max with 4 args") {
        GoldenHarness h;
        h.assign_ok("a1", "(max 1 9 3 6)");
        REQUIRE(h.sample("a1", 0.0) == Approx(9.0));
    }
}

TEST_CASE("Golden semantics: arity boundary errors",
          "[golden][signal_engine][arity]") {

    SECTION("(sin) with no args errors") {
        GoldenHarness h;
        h.expect_error("(a1 (sin))", DiagnosticCategory::Arity);
    }

    SECTION("(sin 1 2) with too many args errors") {
        GoldenHarness h;
        h.expect_error("(a1 (sin 1 2))", DiagnosticCategory::Arity);
    }

    SECTION("(clamp 1 2) missing third arg errors") {
        GoldenHarness h;
        h.expect_error("(a1 (clamp 1 2))", DiagnosticCategory::Arity);
    }

    SECTION("(step [1 2 3]) with no phasor defaults to beat (no error)") {
        GoldenHarness h;
        h.assign_ok("a1", "(step [1 2 3])");
        REQUIRE(h.sample("a1", 0.0) == Approx(1.0));
    }

    SECTION("(euclid 3) with only one arg errors") {
        GoldenHarness h;
        h.expect_error("(a1 (euclid 3))", DiagnosticCategory::Arity);
    }

    SECTION("error messages are plain language, not jargon") {
        GoldenHarness h;
        EvalResult r = h.eval_result("(a1 (sin))");
        REQUIRE(r.kind == EvalResult::Error);
        REQUIRE(r.diagnostic_count > 0);
        std::string msg = r.diagnostics[0].message ? r.diagnostics[0].message : "";
        INFO("message: " << msg);
        REQUIRE(msg.find("arity") == std::string::npos);
    }
}

TEST_CASE("Golden semantics: let/do/if edge cases",
          "[golden][signal_engine][control_flow]") {

    SECTION("(do expr1 expr2) returns last value") {
        GoldenHarness h;
        h.assign_ok("a1", "(do 1 2 3)");
        REQUIRE(h.sample("a1", 0.0) == Approx(3.0));
    }

    SECTION("(let [] 42) empty bindings returns body") {
        GoldenHarness h;
        h.assign_ok("a1", "(let [] 42)");
        REQUIRE(h.sample("a1", 0.0) == Approx(42.0));
    }

    SECTION("let bindings can reference earlier bindings (let* semantics)") {
        GoldenHarness h;
        h.assign_ok("a1", "(let [x 1 y (+ x 1)] y)");
        REQUIRE(h.sample("a1", 0.0) == Approx(2.0));
    }

    SECTION("if without else returns 0 for false condition") {
        GoldenHarness h;
        h.assign_ok("a1", "(if 0 42)");
        REQUIRE(h.sample("a1", 0.0) == Approx(0.0));
    }

    SECTION("if without else returns value for true condition") {
        GoldenHarness h;
        h.assign_ok("a1", "(if 1 42)");
        REQUIRE(h.sample("a1", 0.0) == Approx(42.0));
    }

    SECTION("nested let in signal context works") {
        GoldenHarness h;
        h.assign_ok("a1", "(let [x (+ beat 1)] (let [y (* x 2)] y))");
        REQUIRE(h.sample("a1", 0.0) == Approx(2.0));
        REQUIRE(h.sample("a1", 0.125) == Approx(2.5));
    }

    SECTION("let with time-varying binding") {
        GoldenHarness h;
        h.assign_ok("a1", "(let [x beat] (* x 10))");
        REQUIRE(h.sample("a1", 0.125) == Approx(2.5));
    }

    SECTION("do with multiple expressions returns last") {
        GoldenHarness h;
        h.assign_ok("a1", "(do (+ 1 2) (* 3 4) (- 10 1))");
        REQUIRE(h.sample("a1", 0.0) == Approx(9.0));
    }
}

TEST_CASE("Golden semantics: sequence/index boundary cases",
          "[golden][signal_engine][sequences_boundary]") {

    SECTION("step with single-element vector returns constant") {
        GoldenHarness h;
        h.assign_ok("a1", "(step [42] beat)");
        REQUIRE(h.sample("a1", 0.0) == Approx(42.0));
        REQUIRE(h.sample("a1", 0.25) == Approx(42.0));
        REQUIRE(h.sample("a1", 0.499) == Approx(42.0));
    }

    SECTION("interp with single element returns that value") {
        GoldenHarness h;
        h.assign_ok("a1", "(interp [5] beat)");
        REQUIRE(h.sample("a1", 0.0) == Approx(5.0));
        REQUIRE(h.sample("a1", 0.25) == Approx(5.0));
    }

    SECTION("from-list at exact boundaries") {
        GoldenHarness h;
        h.assign_ok("a1", "(from-list [10 20 30] beat)");
        REQUIRE(h.sample("a1", 0.0) == Approx(10.0));
        REQUIRE(h.sample("a1", 0.25) == Approx(20.0));
    }

    SECTION("large vector step (32 elements)") {
        GoldenHarness h;
        h.assign_ok("a1", "(step [0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31] beat)");
        REQUIRE(h.sample("a1", 0.0) == Approx(0.0));
        REQUIRE(h.sample("a1", 0.25) == Approx(16.0));
    }

    SECTION("step with empty vector returns zero") {
        GoldenHarness h;
        h.assign_ok("a1", "(step [] beat)");
        REQUIRE(h.sample("a1", 0.0) == Approx(0.0));
    }

    SECTION("gates with empty vector returns zero") {
        GoldenHarness h;
        h.assign_ok("a1", "(gates [] beat)");
        REQUIRE(h.sample("a1", 0.0) == Approx(0.0));
    }
}

TEST_CASE("Golden semantics: for unrolling (compilation.md 1.8)",
          "[golden][signal_engine][for_loop]") {

    SECTION("basic for returns last iteration value") {
        GoldenHarness h;
        h.assign_ok("a1", "(for x [1 2 3] x)");
        REQUIRE(h.sample("a1", 0.0) == Approx(3.0));
    }

    SECTION("for producing signal expression") {
        GoldenHarness h;
        h.assign_ok("a1", "(for x [1 2 3] (* x beat))");
        REQUIRE(h.sample("a1", 0.125) == Approx(0.75));
    }

    SECTION("for with accumulation via let") {
        GoldenHarness h;
        h.assign_ok("a1", "(for x [10 20 30] (+ x 5))");
        REQUIRE(h.sample("a1", 0.0) == Approx(35.0));
    }

    SECTION("for with empty collection returns 0") {
        GoldenHarness h;
        h.assign_ok("a1", "(for x [] x)");
        REQUIRE(h.sample("a1", 0.0) == Approx(0.0));
    }
}

TEST_CASE("Golden semantics: quote and symbol rejection",
          "[golden][signal_engine][quote]") {

    SECTION("(quote x) in signal context is rejected") {
        GoldenHarness h;
        h.expect_error("(a1 (quote x))", DiagnosticCategory::Boundary,
                        "quote");
    }
}

TEST_CASE("Golden semantics: alias semantics verification",
          "[golden][signal_engine][aliases]") {

    SECTION("from-list and seq produce identical results") {
        GoldenHarness h;
        h.assign_ok("a1", "(from-list [10 20 30] beat)");
        h.assign_ok("a2", "(seq [10 20 30] beat)");
        auto w1 = h.sample_window("a1", 0.0, 0.49, 10);
        auto w2 = h.sample_window("a2", 0.0, 0.49, 10);
        for (size_t i = 0; i < w1.size(); ++i) {
            INFO("i: " << i);
            REQUIRE(w1[i] == Approx(w2[i]));
        }
    }

    SECTION("trigs and gates produce identical results") {
        GoldenHarness h;
        h.assign_ok("a1", "(gates [1 0 1 0] beat)");
        h.assign_ok("a2", "(trigs [1 0 1 0] beat)");
        auto w1 = h.sample_window("a1", 0.0, 0.49, 10);
        auto w2 = h.sample_window("a2", 0.0, 0.49, 10);
        for (size_t i = 0; i < w1.size(); ++i) {
            INFO("i: " << i);
            REQUIRE(w1[i] == Approx(w2[i]));
        }
    }

    SECTION("shift and offset produce identical results") {
        GoldenHarness h;
        h.assign_ok("a1", "(offset 0.1 beat)");
        h.assign_ok("a2", "(shift 0.1 beat)");
        auto w1 = h.sample_window("a1", 0.0, 0.49, 10);
        auto w2 = h.sample_window("a2", 0.0, 0.49, 10);
        for (size_t i = 0; i < w1.size(); ++i) {
            INFO("i: " << i);
            REQUIRE(w1[i] == Approx(w2[i]));
        }
    }

    SECTION("expt and pow have consistent but different arg order") {
        GoldenHarness h;
        h.assign_ok("a1", "(expt 2 10)");
        REQUIRE(h.sample("a1", 0.0) == Approx(1024.0));
        h.assign_ok("a2", "(pow 2 10)");
        REQUIRE(h.sample("a2", 0.0) == Approx(100.0));
    }
}

TEST_CASE("Golden semantics: non-numeric roots and string diagnostics",
          "[golden][signal_engine][type_errors]") {

    SECTION("string literal in output context produces error") {
        GoldenHarness h;
        EvalResult r = h.eval_result("(a1 \"hello\")");
        REQUIRE(r.kind == EvalResult::Error);
        REQUIRE(r.diagnostic_count > 0);
    }
}

// ============================================================================
// Phase 3: Diagnostic Categories as First-Class Compiler Contract
// ============================================================================

TEST_CASE("Diagnostics: UndefinedName category",
          "[golden][signal_engine][diagnostics][category]") {

    SECTION("unknown bare symbol produces UndefinedName") {
        GoldenHarness h;
        h.expect_error("(a1 nonexistent)", DiagnosticCategory::UndefinedName,
                        "Unknown");
    }

    SECTION("typo with fuzzy match produces UndefinedName with suggestion") {
        GoldenHarness h;
        h.eval_ok("(define freq 440)");
        h.expect_error("(a1 frq)", DiagnosticCategory::UndefinedName,
                        "freq", "freq");
    }

    SECTION("typo sni suggests sin") {
        GoldenHarness h;
        h.expect_error("(a1 (sni beat))", DiagnosticCategory::UndefinedName,
                        "sin", "sin");
    }

    SECTION("totally unknown symbol still gets UndefinedName category") {
        GoldenHarness h;
        h.expect_error("(a1 zzzzzzz)", DiagnosticCategory::UndefinedName,
                        "Unknown");
    }

    SECTION("unknown function in call position produces UndefinedName") {
        GoldenHarness h;
        h.expect_error("(a1 (not-a-function 1))", DiagnosticCategory::UndefinedName);
    }

    SECTION("user-defined symbol typo gets fuzzy match") {
        GoldenHarness h;
        h.eval_ok("(define tempo 120)");
        h.expect_error("(a1 tmpo)", DiagnosticCategory::UndefinedName,
                        "tempo");
    }
}

TEST_CASE("Diagnostics: Boundary category",
          "[golden][signal_engine][diagnostics][category]") {

    SECTION("define inside output is Boundary") {
        GoldenHarness h;
        h.expect_error("(a1 (define x 1))", DiagnosticCategory::Boundary,
                        "inside an output");
    }

    SECTION("defn inside output is Boundary") {
        GoldenHarness h;
        h.expect_error("(a1 (defn f [x] x))", DiagnosticCategory::Boundary,
                        "inside an output");
    }

    SECTION("lambda inside output is Boundary") {
        GoldenHarness h;
        h.expect_error("(a1 (fn [x] (* x 2)))", DiagnosticCategory::Boundary,
                        "Lambda");
    }

    SECTION("set inside output is Boundary") {
        GoldenHarness h;
        h.expect_error("(a1 (set x 1))", DiagnosticCategory::Boundary,
                        "inside an output");
    }

    SECTION("quote inside output is Boundary") {
        GoldenHarness h;
        h.expect_error("(a1 (quote (1 2 3)))", DiagnosticCategory::Boundary,
                        "quote");
    }
}

TEST_CASE("Diagnostics: Arity category",
          "[golden][signal_engine][diagnostics][category]") {

    SECTION("user function with too many args is Arity") {
        GoldenHarness h;
        h.eval_ok("(defn double [x] (* x 2))");
        h.expect_error("(a1 (double 1 2))", DiagnosticCategory::Arity,
                        "Too many");
    }

    SECTION("user function with zero args when it needs one is Arity") {
        GoldenHarness h;
        h.eval_ok("(defn double [x] (* x 2))");
        h.expect_error("(a1 (double))", DiagnosticCategory::Arity,
                        "Wrong number");
    }

    SECTION("bare function name (needs args) is Arity") {
        GoldenHarness h;
        h.eval_ok("(defn f [x] (* x 2))");
        h.expect_error("(a1 f)", DiagnosticCategory::Arity,
                        "needs arguments");
    }
}

TEST_CASE("Diagnostics: Syntax category",
          "[golden][signal_engine][diagnostics][category]") {

    SECTION("non-symbol after open paren is Syntax") {
        GoldenHarness h;
        h.expect_error("(a1 (123 beat))", DiagnosticCategory::Syntax,
                        "Expected a function name");
    }

    SECTION("for with non-symbol variable is Syntax") {
        GoldenHarness h;
        h.expect_error("(a1 (for 123 [1 2 3] 1))", DiagnosticCategory::Syntax,
                        "'for' needs a variable name");
    }
}

TEST_CASE("Diagnostics: Type category",
          "[golden][signal_engine][diagnostics][category]") {

    SECTION("prev with non-output name is Type") {
        GoldenHarness h;
        h.expect_error("(a1 (prev beat))", DiagnosticCategory::Type,
                        "output name");
    }
}

TEST_CASE("Diagnostics: Runtime category (correct usage)",
          "[golden][signal_engine][diagnostics][category]") {

    SECTION("direct recursion is Runtime") {
        GoldenHarness h;
        h.eval_ok("(defn g [x] (g x))");
        h.expect_error("(a1 (g 1))", DiagnosticCategory::Runtime,
                        "recursive");
    }

    SECTION("mutual recursion is Runtime") {
        GoldenHarness h;
        h.eval_ok("(define a (+ 1 b))");
        h.eval_ok("(define b (+ 1 a))");
        h.expect_error("(a1 a)", DiagnosticCategory::Runtime,
                        "recursive");
    }
}

// ============================================================================
// Span Accuracy Tests
// ============================================================================

TEST_CASE("Diagnostics: span points at the correct subexpression",
          "[golden][signal_engine][diagnostics][span]") {

    SECTION("bare unknown symbol span") {
        GoldenHarness h;
        h.eval_ok("(define freq 440)");
        // "(a1 frq)" — 'frq' starts at offset 4, length 3
        h.expect_error_at("(a1 frq)",
                          DiagnosticCategory::UndefinedName,
                          4, 3, "freq");
    }

    SECTION("nested unknown symbol span") {
        GoldenHarness h;
        // "(a1 (+ 1 (sni beat)))"
        //  0123456789...
        // 'sni' starts at offset 10, length 3
        h.expect_error_at("(a1 (+ 1 (sni beat)))",
                          DiagnosticCategory::UndefinedName,
                          10, 3, "sin");
    }

    SECTION("define inside output span points at define") {
        GoldenHarness h;
        // "(a1 (define x 1))" — 'define' starts at offset 5, length 6
        h.expect_error_at("(a1 (define x 1))",
                          DiagnosticCategory::Boundary,
                          5, 6, "inside an output");
    }

    SECTION("unknown function in call position span") {
        GoldenHarness h;
        // "(a1 (blorg 1))" — 'blorg' starts at offset 5, length 5
        h.expect_error_at("(a1 (blorg 1))",
                          DiagnosticCategory::UndefinedName,
                          5, 5);
    }
}

// ============================================================================
// Fuzzy Matching Quality Tests
// ============================================================================

TEST_CASE("Diagnostics: fuzzy match quality",
          "[golden][signal_engine][diagnostics][fuzzy]") {

    SECTION("distance 1 transposition: sni -> sin") {
        GoldenHarness h;
        h.expect_error("(a1 (sni beat))", DiagnosticCategory::UndefinedName,
                        "sin", "sin");
    }

    SECTION("distance 1 substitution: beet -> beat") {
        GoldenHarness h;
        h.expect_error("(a1 beet)", DiagnosticCategory::UndefinedName,
                        "beat", "beat");
    }

    SECTION("user-defined symbol fuzzy match: frq -> freq") {
        GoldenHarness h;
        h.eval_ok("(define freq 440)");
        h.expect_error("(a1 frq)", DiagnosticCategory::UndefinedName,
                        "freq", "freq");
    }

    SECTION("no match for very different name — still UndefinedName") {
        GoldenHarness h;
        h.expect_error("(a1 zzzzzzz)", DiagnosticCategory::UndefinedName);
    }

    SECTION("function name fuzzy match in call position") {
        GoldenHarness h;
        // Should suggest 'sin' when 'sni' is used as a function
        EvalResult r = h.eval_result("(a1 (sni beat))");
        REQUIRE(r.kind == EvalResult::Error);
        REQUIRE(r.diagnostic_count > 0);
        bool found_suggestion = false;
        for (uint8_t i = 0; i < r.diagnostic_count; ++i) {
            if (r.diagnostics[i].category == DiagnosticCategory::UndefinedName &&
                r.diagnostics[i].suggestion != nullptr) {
                found_suggestion = true;
                REQUIRE(std::string(r.diagnostics[i].suggestion).find("sin")
                        != std::string::npos);
            }
        }
        REQUIRE(found_suggestion);
    }
}

// ============================================================================
// Message Plainness Tests — no jargon
// ============================================================================

TEST_CASE("Diagnostics: messages use plain language",
          "[golden][signal_engine][diagnostics][language]") {

    SECTION("arity error says 'Wrong number' not 'arity mismatch'") {
        GoldenHarness h;
        h.eval_ok("(defn f [x] (* x 2))");
        EvalResult r = h.eval_result("(a1 (f 1 2))");
        REQUIRE(r.kind == EvalResult::Error);
        REQUIRE(r.diagnostic_count > 0);
        std::string msg = r.diagnostics[0].message ? r.diagnostics[0].message : "";
        // Must not contain jargon
        REQUIRE(msg.find("arity") == std::string::npos);
        REQUIRE(msg.find("predicate") == std::string::npos);
        REQUIRE(msg.find("lvalue") == std::string::npos);
    }

    SECTION("boundary error uses plain language") {
        GoldenHarness h;
        EvalResult r = h.eval_result("(a1 (define x 1))");
        REQUIRE(r.kind == EvalResult::Error);
        REQUIRE(r.diagnostic_count > 0);
        std::string msg = r.diagnostics[0].message ? r.diagnostics[0].message : "";
        REQUIRE(msg.find("side-effect") == std::string::npos);
        REQUIRE(msg.find("boundary violation") == std::string::npos);
    }

    SECTION("unknown name error uses plain language") {
        GoldenHarness h;
        EvalResult r = h.eval_result("(a1 nonexistent)");
        REQUIRE(r.kind == EvalResult::Error);
        REQUIRE(r.diagnostic_count > 0);
        std::string msg = r.diagnostics[0].message ? r.diagnostics[0].message : "";
        REQUIRE(msg.find("undefined variable") == std::string::npos);
        REQUIRE(msg.find("unbound") == std::string::npos);
    }
}

// ============================================================================
// Suggestion Validity Meta-Tests
// ============================================================================

TEST_CASE("Diagnostics: suggestions compile successfully",
          "[golden][signal_engine][diagnostics][suggestion]") {

    SECTION("boundary error suggestion compiles") {
        GoldenHarness h;
        // The suggestion for define-inside-output is "Use it at the top level instead"
        // which is advice, not code — verify it's present
        EvalResult r = h.eval_result("(a1 (define x 1))");
        REQUIRE(r.kind == EvalResult::Error);
        REQUIRE(r.diagnostic_count > 0);
        REQUIRE(r.diagnostics[0].suggestion != nullptr);
    }

    SECTION("fuzzy match suggestion compiles") {
        GoldenHarness h;
        h.eval_ok("(define freq 440)");
        h.expect_suggestion_compiles("(a1 frq)", DiagnosticCategory::UndefinedName);
    }

    SECTION("lambda boundary suggestion is actionable") {
        GoldenHarness h;
        EvalResult r = h.eval_result("(a1 (fn [x] x))");
        REQUIRE(r.kind == EvalResult::Error);
        REQUIRE(r.diagnostic_count > 0);
        REQUIRE(r.diagnostics[0].suggestion != nullptr);
        std::string sug = r.diagnostics[0].suggestion;
        // Should mention 'defn'
        REQUIRE(sug.find("defn") != std::string::npos);
    }

    SECTION("syntax error suggestion is present") {
        GoldenHarness h;
        EvalResult r = h.eval_result("(a1 (123 beat))");
        REQUIRE(r.kind == EvalResult::Error);
        REQUIRE(r.diagnostic_count > 0);
        REQUIRE(r.diagnostics[0].suggestion != nullptr);
    }
}

// ============================================================================
// Multiple Diagnostics (recoverable errors)
// ============================================================================

TEST_CASE("Diagnostics: error leaves previous output intact",
          "[golden][signal_engine][diagnostics][isolation]") {

    SECTION("compile error preserves last known good output") {
        GoldenHarness h;
        h.assign_ok("a1", "(+ beat 1)");
        REQUIRE(h.sample("a1", 0.25) == Approx(1.5));

        EvalResult bad = h.eval_result("(a1 (unknown-name beat))");
        REQUIRE(bad.kind == EvalResult::Error);
        REQUIRE(bad.diagnostic_count > 0);
        REQUIRE(bad.diagnostics[0].category == DiagnosticCategory::UndefinedName);

        // Previous output still runs
        REQUIRE(h.sample("a1", 0.25) == Approx(1.5));
    }

    SECTION("one output error does not affect another output") {
        GoldenHarness h;
        h.assign_ok("a1", "0.2");
        h.assign_ok("a2", "0.8");

        EvalResult bad = h.eval_result("(a1 (not-a-function 1))");
        REQUIRE(bad.kind == EvalResult::Error);
        REQUIRE(bad.diagnostics[0].category == DiagnosticCategory::UndefinedName);

        REQUIRE(h.sample("a1", 0.0) == Approx(0.2));
        REQUIRE(h.sample("a2", 0.0) == Approx(0.8));
    }
}

// ============================================================================
// Comprehensive Category Coverage Audit
// ============================================================================

TEST_CASE("Diagnostics: every category has at least one strict test",
          "[golden][signal_engine][diagnostics][coverage]") {

    SECTION("Syntax category coverage") {
        GoldenHarness h;
        // Non-symbol after open paren
        h.expect_error("(a1 (123 beat))", DiagnosticCategory::Syntax);
    }

    SECTION("UndefinedName category coverage") {
        GoldenHarness h;
        h.expect_error("(a1 nonexistent)", DiagnosticCategory::UndefinedName);
    }

    SECTION("Arity category coverage") {
        GoldenHarness h;
        h.eval_ok("(defn f [x] x)");
        h.expect_error("(a1 (f 1 2))", DiagnosticCategory::Arity);
    }

    SECTION("Type category coverage") {
        GoldenHarness h;
        // prev with non-output argument
        h.expect_error("(a1 (prev beat))", DiagnosticCategory::Type);
    }

    SECTION("Boundary category coverage") {
        GoldenHarness h;
        h.expect_error("(a1 (define x 1))", DiagnosticCategory::Boundary);
    }

    SECTION("Runtime category coverage") {
        GoldenHarness h;
        h.eval_ok("(defn g [x] (g x))");
        h.expect_error("(a1 (g 1))", DiagnosticCategory::Runtime);
    }
}

// ============================================================================
// Phase 5: Stateful Compiler Surface — defstate and integrate
// ============================================================================

TEST_CASE("State: defstate creates a cell with initial value and update body",
          "[golden][signal_engine][state]") {

    SECTION("defstate counter increments each tick") {
        GoldenHarness h;
        h.eval_ok("(defstate counter 0 (+ counter 1))");
        h.assign_ok("a1", "counter");
        // First tick: counter should be 0 (initial value read before update)
        REQUIRE(h.tick("a1", 0.0) == Approx(0.0));
        // After first tick, counter updates to 0+1=1
        REQUIRE(h.tick("a1", 0.001) == Approx(1.0));
        REQUIRE(h.tick("a1", 0.002) == Approx(2.0));
        REQUIRE(h.tick("a1", 0.003) == Approx(3.0));
    }

    SECTION("defstate with dt-based accumulation") {
        GoldenHarness h;
        h.eval_ok("(defstate accum 0 (+ accum dt))");
        h.assign_ok("a1", "accum");
        // First tick at t=0: accum=0
        REQUIRE(h.tick("a1", 0.0) == Approx(0.0));
        // dt = 0.001 - 0.0 = 0.001, accum = 0 + 0.001
        REQUIRE(h.tick("a1", 0.001) == Approx(0.001));
        // dt = 0.002 - 0.001 = 0.001, accum = 0.001 + 0.001
        REQUIRE(h.tick("a1", 0.002) == Approx(0.002));
    }

    SECTION("defstate update body can reference cells") {
        GoldenHarness h;
        h.eval_ok("(define rate 10)");
        h.eval_ok("(defstate accum 0 (+ accum (* rate dt)))");
        h.assign_ok("a1", "accum");
        // accum grows at rate=10 per second
        REQUIRE(h.tick("a1", 0.0) == Approx(0.0));
        // dt=0.001, accum = 0 + 10*0.001 = 0.01
        REQUIRE(h.tick("a1", 0.001) == Approx(0.01));
        // dt=0.001, accum = 0.01 + 10*0.001 = 0.02
        REQUIRE(h.tick("a1", 0.002) == Approx(0.02));
    }

    SECTION("defstate value persists across cell redefinition") {
        GoldenHarness h;
        h.eval_ok("(define rate 1)");
        h.eval_ok("(defstate phase 0 (+ phase (* rate dt)))");
        h.assign_ok("a1", "phase");

        // Tick a few times at rate=1
        REQUIRE(h.tick("a1", 0.0) == Approx(0.0));
        REQUIRE(h.tick("a1", 0.001) == Approx(0.001));
        REQUIRE(h.tick("a1", 0.002) == Approx(0.002));
        double before = 0.002;

        // Redefine rate — phase's state should NOT reset
        h.eval_ok("(define rate 2)");
        // dt=0.001, phase = 0.002 + 2*0.001 = 0.004
        REQUIRE(h.tick("a1", 0.003) == Approx(before + 2.0 * 0.001));
    }

    SECTION("defstate with non-zero initial value") {
        GoldenHarness h;
        h.eval_ok("(defstate x 100 (+ x 1))");
        h.assign_ok("a1", "x");
        REQUIRE(h.tick("a1", 0.0) == Approx(100.0));
        REQUIRE(h.tick("a1", 0.001) == Approx(101.0));
        REQUIRE(h.tick("a1", 0.002) == Approx(102.0));
    }

    SECTION("multiple defstate cells are independent") {
        GoldenHarness h;
        h.eval_ok("(defstate a 0 (+ a 1))");
        h.eval_ok("(defstate b 10 (+ b 2))");
        h.assign_ok("a1", "a");
        h.assign_ok("a2", "b");
        REQUIRE(h.tick("a1", 0.0) == Approx(0.0));
        REQUIRE(h.tick("a2", 0.0) == Approx(10.0));  // same tick via harness
        REQUIRE(h.tick("a1", 0.001) == Approx(1.0));
        REQUIRE(h.tick("a2", 0.001) == Approx(12.0));
    }

    SECTION("defstate inside output expression is rejected") {
        GoldenHarness h;
        h.expect_error("(a1 (defstate x 0 (+ x 1)))",
                        DiagnosticCategory::Boundary);
    }
}

TEST_CASE("State: integrate accumulates rate over time",
          "[golden][signal_engine][state]") {

    SECTION("integrate constant rate approximates linear ramp") {
        GoldenHarness h;
        h.assign_ok("a1", "(integrate 1.0)");
        // integrate(1.0) = state += 1.0 * dt per tick
        REQUIRE(h.tick("a1", 0.0) == Approx(0.0));
        double dt = 0.001;
        REQUIRE(h.tick("a1", dt) == Approx(dt));
        REQUIRE(h.tick("a1", 2*dt) == Approx(2*dt));
        REQUIRE(h.tick("a1", 3*dt) == Approx(3*dt));
    }

    SECTION("integrate with rate from cell") {
        GoldenHarness h;
        h.eval_ok("(define rate 5.0)");
        h.assign_ok("a1", "(integrate rate)");
        // state += 5.0 * dt per tick
        REQUIRE(h.tick("a1", 0.0) == Approx(0.0));
        double dt = 0.001;
        REQUIRE(h.tick("a1", dt) == Approx(5.0 * dt));
        REQUIRE(h.tick("a1", 2*dt) == Approx(2 * 5.0 * dt));
    }

    SECTION("integrate with time-varying rate") {
        GoldenHarness h;
        h.assign_ok("a1", "(integrate t)");
        // state += t * dt per tick (t changes)
        REQUIRE(h.tick("a1", 0.0) == Approx(0.0));
        // dt=0.001, state = 0 + 0.001 * 0.001 = 0.000001
        REQUIRE(h.tick("a1", 0.001) == Approx(0.001 * 0.001));
    }

    SECTION("multiple independent integrates in same output") {
        GoldenHarness h;
        h.assign_ok("a1", "(+ (integrate 1.0) (integrate 2.0))");
        // Two separate state slots, summed
        REQUIRE(h.tick("a1", 0.0) == Approx(0.0));
        double dt = 0.001;
        REQUIRE(h.tick("a1", dt) == Approx(3.0 * dt));
        REQUIRE(h.tick("a1", 2*dt) == Approx(2 * 3.0 * dt));
    }
}

// ── Top-level expression eval (scratch isolation) ──────────────────────────

TEST_CASE("Golden: top-level bar returns value at injected time",
          "[golden][scratch_eval]") {
    GoldenHarness h;
    // At 120 BPM, beat period = 0.5s. bar = fmod(t * bpm/60, 1) at t=0.125
    // beat = fmod(0.125 * 2, 1) = 0.25
    h.engine.state.current_time = 0.125;
    h.engine.state.current_dt = 0.0;
    EvalResult r = h.eval_result("bar");
    REQUIRE(r.kind == EvalResult::Number);
    // bar = fmod(t * bpm / 60 / beats_per_bar, 1.0) = fmod(0.125*120/60/4, 1) = fmod(0.0625, 1) = 0.0625
    REQUIRE(r.number == Approx(0.0625).margin(1e-9));
}

TEST_CASE("Golden: top-level (* bar 0.5) returns number",
          "[golden][scratch_eval]") {
    GoldenHarness h;
    h.engine.state.current_time = 0.0;
    EvalResult r = h.eval_result("(* bar 0.5)");
    REQUIRE(r.kind == EvalResult::Number);
    // At t=0, bar = 0, so (* 0 0.5) = 0
    REQUIRE(r.number == Approx(0.0));
}

TEST_CASE("Golden: top-level eval-at-time returns number",
          "[golden][scratch_eval]") {
    GoldenHarness h;
    h.engine.state.current_time = 0.0;
    EvalResult r = h.eval_result("(eval-at-time 0.5 bar)");
    REQUIRE(r.kind == EvalResult::Number);
    // eval-at-time 0.5 bar: bar at t=0.5 at 120bpm 4/4 = fmod(0.5*120/60/4, 1) = fmod(0.25, 1) = 0.25
    REQUIRE(r.number == Approx(0.25).margin(1e-9));
}

TEST_CASE("Golden: top-level vector returns text with samples",
          "[golden][scratch_eval]") {
    GoldenHarness h;
    h.engine.state.current_time = 0.0;
    EvalResult r = h.eval_result("[(eval-at-time 0 bar) (eval-at-time 0.5 bar)]");
    REQUIRE(r.kind == EvalResult::Text);
    // Should contain [0 0.25] or similar
    REQUIRE(r.text != nullptr);
    REQUIRE(r.text_length > 0);
    std::string text(r.text, r.text_length);
    REQUIRE(text[0] == '[');
    REQUIRE(text[text.size()-1] == ']');
}

TEST_CASE("Golden: repeated top-level eval doesn't grow live node pool",
          "[golden][scratch_eval]") {
    GoldenHarness h;
    h.engine.state.current_time = 0.0;

    uint16_t initial_count = h.engine.pool.node_count;

    for (int i = 0; i < 10; i++) {
        EvalResult r = h.eval_result("(* bar 0.5)");
        REQUIRE(r.kind == EvalResult::Number);
    }

    REQUIRE(h.engine.pool.node_count == initial_count);
}

TEST_CASE("Golden: repeated top-level vector eval doesn't grow live data tables",
          "[golden][scratch_eval]") {
    GoldenHarness h;
    h.engine.state.current_time = 0.0;

    uint8_t initial_tables = h.engine.cells.data_table_count;

    for (int i = 0; i < 5; i++) {
        EvalResult r = h.eval_result("[(eval-at-time 0 bar) (eval-at-time 0.5 bar)]");
        REQUIRE(r.kind == EvalResult::Text);
    }

    REQUIRE(h.engine.cells.data_table_count == initial_tables);
}

TEST_CASE("Golden: invalid top-level signal expression returns error",
          "[golden][scratch_eval]") {
    GoldenHarness h;
    EvalResult r = h.eval_result("(nonexistent-function bar)");
    REQUIRE(r.kind == EvalResult::Error);
    REQUIRE(r.diagnostic_count > 0);
}

TEST_CASE("Golden: defined cell visible in top-level expression",
          "[golden][scratch_eval]") {
    GoldenHarness h;
    h.eval_ok("(define freq 440)");
    h.engine.state.current_time = 0.0;
    EvalResult r = h.eval_result("(* freq 2)");
    REQUIRE(r.kind == EvalResult::Number);
    REQUIRE(r.number == Approx(880.0));
}

// ── State Resource Registry ─────────────────────────────────────────────────

TEST_CASE("Registry: resolve allocates new slot on first call",
          "[golden][registry]") {
    StateResourceRegistry reg;
    double state_values[MAX_STATE_SLOTS] = {};
    uint16_t slot_count = 0;

    StateResourceKey key = { internSymbol("phase-A"), ResourceKind::OscillatorPhase, 0 };
    uint16_t slot = reg.resolve(key, 0.0, state_values, slot_count);

    REQUIRE(slot == 0);
    REQUIRE(slot_count == 1);
    REQUIRE(reg.entry_count == 1);
    REQUIRE(state_values[0] == 0.0);
}

TEST_CASE("Registry: resolve returns existing slot for matching key",
          "[golden][registry]") {
    StateResourceRegistry reg;
    double state_values[MAX_STATE_SLOTS] = {};
    uint16_t slot_count = 0;

    StateResourceKey key = { internSymbol("phase-A"), ResourceKind::OscillatorPhase, 0 };
    uint16_t slot1 = reg.resolve(key, 0.0, state_values, slot_count);
    state_values[slot1] = 0.75;

    uint16_t slot2 = reg.resolve(key, 0.0, state_values, slot_count);
    REQUIRE(slot2 == slot1);
    REQUIRE(slot_count == 1);
    REQUIRE(state_values[slot2] == 0.75);
}

TEST_CASE("Registry: different keys get different slots",
          "[golden][registry]") {
    StateResourceRegistry reg;
    double state_values[MAX_STATE_SLOTS] = {};
    uint16_t slot_count = 0;

    StateResourceKey k1 = { internSymbol("A"), ResourceKind::OscillatorPhase, 0 };
    StateResourceKey k2 = { internSymbol("B"), ResourceKind::OscillatorPhase, 0 };
    StateResourceKey k3 = { internSymbol("A"), ResourceKind::TriggerMemory, 0 };

    uint16_t s1 = reg.resolve(k1, 0.0, state_values, slot_count);
    uint16_t s2 = reg.resolve(k2, 0.0, state_values, slot_count);
    uint16_t s3 = reg.resolve(k3, 1.0, state_values, slot_count);

    REQUIRE(s1 != s2);
    REQUIRE(s1 != s3);
    REQUIRE(s2 != s3);
    REQUIRE(slot_count == 3);
    REQUIRE(state_values[s3] == 1.0);
}

TEST_CASE("Registry: mark_all_inactive and re-resolve reactivates",
          "[golden][registry]") {
    StateResourceRegistry reg;
    double state_values[MAX_STATE_SLOTS] = {};
    uint16_t slot_count = 0;

    StateResourceKey key = { internSymbol("X"), ResourceKind::Integrator, 0 };
    reg.resolve(key, 0.0, state_values, slot_count);
    REQUIRE(reg.entries[0].active == true);

    reg.mark_all_inactive();
    REQUIRE(reg.entries[0].active == false);

    reg.resolve(key, 0.0, state_values, slot_count);
    REQUIRE(reg.entries[0].active == true);
    REQUIRE(slot_count == 1);
}

TEST_CASE("Registry: clear resets completely",
          "[golden][registry]") {
    StateResourceRegistry reg;
    double state_values[MAX_STATE_SLOTS] = {};
    uint16_t slot_count = 0;

    StateResourceKey key = { internSymbol("Y"), ResourceKind::SlewAccumulator, 0 };
    reg.resolve(key, 0.5, state_values, slot_count);
    REQUIRE(reg.entry_count == 1);

    reg.clear();
    REQUIRE(reg.entry_count == 0);
}

TEST_CASE("Golden: repeated set with expression doesn't grow live pool",
          "[golden][scratch_eval]") {
    GoldenHarness h;
    h.eval_ok("(define x 1)");

    uint16_t initial_nodes = h.engine.pool.node_count;

    for (int i = 0; i < 10; i++) {
        h.eval_ok("(set x (+ 1 2))");
    }

    REQUIRE(h.engine.pool.node_count == initial_nodes);
    EvalResult r = h.eval_result("x");
    REQUIRE(r.kind == EvalResult::Number);
    REQUIRE(r.number == Approx(3.0));
}
