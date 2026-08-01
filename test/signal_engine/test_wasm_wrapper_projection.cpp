// Native contract tests for the WASM wrapper's observational evaluation and
// combined tick/projection boundary. The wrapper is compiled as ordinary C++
// here; empty output lists avoid native pointer-width differences in its WASM
// linear-memory buffer ABI.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>

extern "C" {
void useq_init();
char* useq_eval(const char* input);
double useq_eval_output(const char* name, double time_seconds);
char* useq_eval_outputs_time_window(
    const char* outputs_json, double start_time, double end_time, int num_samples);
const char* useq_active_diagnostics();
int useq_tick_and_project(
    const char* outputs_json,
    double tick_time,
    int projection_mode,
    double projection_end,
    int num_future_samples,
    int buffer_ptr,
    int buffer_length);
}

namespace {

std::string take_owned(const char* value) {
    REQUIRE(value != nullptr);
    std::string result(value);
    std::free(const_cast<char*>(value));
    return result;
}

void eval_ok(const char* source) {
    std::string result = take_owned(useq_eval(source));
    INFO("source: " << source);
    INFO("result: " << result);
    REQUIRE(result.rfind("Error:", 0) != 0);
}

std::string active_diagnostics() {
    return take_owned(useq_active_diagnostics());
}

} // namespace

TEST_CASE("WASM sampling is diagnostic-pure and invalid projection is atomic",
          "[wasm-wrapper][projection][failure-mode]") {
    useq_init();

    // At t=0 this overflows, while at t=1 it is exactly zero. This lets a
    // healthy read-only sample try to clear a live fallback diagnostic.
    eval_ok("(a1 (* (* (- t 1) 1e308) 1e308))");
    REQUIRE(useq_tick_and_project("[]", 0.0, 0, 0.0, 0, 0, 0) == 0);
    REQUIRE(active_diagnostics().find("\"output\":\"a1\"") !=
            std::string::npos);

    REQUIRE(useq_eval_output("a1", 1.0) == Approx(0.0));
    REQUIRE(active_diagnostics().find("\"output\":\"a1\"") !=
            std::string::npos);

    std::string samples = take_owned(
        useq_eval_outputs_time_window("[\"a1\"]", 1.0, 1.0, 1));
    REQUIRE(samples.find("\"a1\":[0") != std::string::npos);
    REQUIRE(active_diagnostics().find("\"output\":\"a1\"") !=
            std::string::npos);

    // The live tick falls back at t=0; the projection's final sample is
    // healthy at t=1. Projection must restore the post-tick live mask.
    REQUIRE(useq_tick_and_project("[]", 0.0, 1, 1.0, 1, 0, 0) == 0);
    REQUIRE(active_diagnostics().find("\"output\":\"a1\"") !=
            std::string::npos);

    // Check the opposite transition too: a healthy live tick followed by a
    // failing future sample must not invent a live fallback diagnostic.
    REQUIRE(useq_tick_and_project("[]", 1.0, 1, 2.0, 1, 0, 0) == 0);
    REQUIRE(active_diagnostics() == "[]");

    // User evaluation invalidates the fork. Neither a missing extend fork nor
    // a reset-fill endpoint at/before its origin may advance prev(a1).
    eval_ok("(a1 (+ (prev a1) 1))");
    REQUIRE(useq_eval_output("a1", 10.0) == Approx(1.0));

    REQUIRE(useq_tick_and_project("[]", 10.0, 2, 11.0, 1, 0, 0) == -1);
    REQUIRE(useq_tick_and_project("[]", 10.0, 1, 10.0, 1, 0, 0) == -1);
    REQUIRE(useq_tick_and_project(nullptr, 10.0, 0, 0.0, 0, 0, 0) == -1);
    REQUIRE(useq_tick_and_project(
                "[]", std::numeric_limits<double>::quiet_NaN(),
                0, 0.0, 0, 0, 0) == -1);
    REQUIRE(useq_tick_and_project("[\"a1\"]", 10.0, 0, 0.0, 0, 0, 1) == -1);

    REQUIRE(useq_eval_output("a1", 10.0) == Approx(1.0));
}
