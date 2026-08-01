// Native contract tests for the WASM wrapper's observational evaluation and
// combined tick/projection boundary. The wrapper is compiled as ordinary C++
// here; empty output lists avoid native pointer-width differences in its WASM
// linear-memory buffer ABI.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <climits>
#include <limits>
#include <string>

#if defined(__linux__) && defined(__x86_64__)
#include <sys/mman.h>
#endif

extern "C" {
void useq_init();
char* useq_eval(const char* input);
double useq_eval_output(const char* name, double time_seconds);
char* useq_eval_outputs_time_window(
    const char* outputs_json, double start_time, double end_time, int num_samples);
const char* useq_active_diagnostics();
int useq_output_health(const char* name);
int useq_tick_and_project(
    const char* outputs_json,
    double tick_time,
    int projection_mode,
    double projection_end,
    int num_future_samples,
    int buffer_ptr,
    int buffer_length);
int useq_tick_synth_controls(double wall_time, int buffer_ptr, int buffer_length);
int useq_set_live_inputs(const char* json_str);
char* useq_last_error();
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

std::string last_error() {
    return take_owned(useq_last_error());
}

// The production ABI carries a WASM32 linear-memory address in an int. Native
// wrapper tests therefore need a mapping whose address is representable by
// that ABI rather than truncating an ordinary 64-bit process pointer.
class Wasm32Buffer {
public:
    explicit Wasm32Buffer(size_t count) : bytes_(count * sizeof(double)) {
#if defined(__linux__) && defined(__x86_64__) && defined(MAP_32BIT)
        mapping_ = mmap(nullptr, bytes_, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
        if (mapping_ == MAP_FAILED) mapping_ = nullptr;
#endif
    }

    ~Wasm32Buffer() {
#if defined(__linux__) && defined(__x86_64__) && defined(MAP_32BIT)
        if (mapping_) munmap(mapping_, bytes_);
#endif
    }

    bool available() const {
        return mapping_ && reinterpret_cast<uintptr_t>(mapping_) <= INT_MAX;
    }

    int wasm_ptr() const {
        return static_cast<int>(reinterpret_cast<uintptr_t>(mapping_));
    }

    double* data() const { return static_cast<double*>(mapping_); }

private:
    void* mapping_ = nullptr;
    size_t bytes_ = 0;
};

} // namespace

TEST_CASE("WASM sampling is diagnostic-pure and invalid projection is atomic",
          "[wasm-wrapper][projection][failure-mode]") {
    REQUIRE(useq_output_health("a1") == -1);
    useq_init();
    REQUIRE(useq_output_health("a2") == 0);

    // At t=0 this overflows, while at t=1 it is exactly zero. This lets a
    // healthy read-only sample try to clear a live fallback diagnostic.
    eval_ok("(a1 (* (* (- t 1) 1e308) 1e308))");
    REQUIRE(useq_tick_and_project("[]", 0.0, 0, 0.0, 0, 0, 0) == 0);
    std::string first_failure = active_diagnostics();
    REQUIRE(first_failure.find("\"output\":\"a1\"") != std::string::npos);
    REQUIRE(first_failure.find("\"state\":\"error\"") != std::string::npos);
    REQUIRE(useq_output_health("a1") == 3);
    REQUIRE(useq_output_health("not-an-output") == -1);

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
    REQUIRE(useq_tick_and_project("[]", 0.5, 1, 1.0, 1, 0, 0) == 0);
    std::string later_failure = active_diagnostics();
    REQUIRE(later_failure.find("\"output\":\"a1\"") != std::string::npos);
    // Observational sampling/projection cannot establish LKG, so this remains
    // a first-ever Error until an authoritative finite tick commits.
    REQUIRE(later_failure.find("\"state\":\"error\"") != std::string::npos);

    // Check the opposite transition too: a healthy live tick followed by a
    // failing future sample must not invent a live fallback diagnostic.
    REQUIRE(useq_tick_and_project("[]", 1.0, 1, 2.0, 1, 0, 0) == 0);
    REQUIRE(active_diagnostics() == "[]");
    REQUIRE(useq_output_health("a1") == 1);

    // Once one authoritative finite root exists, the next non-finite root is
    // observably Fallback rather than bootstrap Error.
    REQUIRE(useq_tick_synth_controls(1.5, 0, 0) == 0);
    REQUIRE(useq_output_health("a1") == 2);

    eval_ok("(a1 7)");
    REQUIRE(useq_output_health("a1") == 1);
    REQUIRE(active_diagnostics().find("\"output\":\"a1\"") ==
            std::string::npos);

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

    // The two public live-tick APIs share one authoritative VM frontier.
    // Exactly one owner may consume a wall-time instant; duplicates,
    // decreasing time, and non-finite time must fail without advancing prev.
    REQUIRE(useq_tick_synth_controls(10.0, 0, 0) == 0);
    CHECK(useq_tick_and_project("[]", 10.0, 0, 0.0, 0, 0, 0) == -1);
    CHECK(last_error().find("authoritative tick") != std::string::npos);
    CHECK(useq_tick_synth_controls(9.0, 0, 0) == -1);
    CHECK(useq_tick_synth_controls(
              std::numeric_limits<double>::infinity(), 0, 0) == -1);
    CHECK(useq_eval_output("a1", 10.0) == Approx(2.0));

    REQUIRE(useq_tick_and_project("[]", 11.0, 0, 0.0, 0, 0, 0) == 0);
    CHECK(useq_tick_synth_controls(11.0, 0, 0) == -1);
    CHECK(useq_eval_output("a1", 11.0) == Approx(3.0));

    // A reset-fill freezes live edits and every cross-tick/health vector in
    // the projection fork. Later live ticks may see edits, while extension
    // must continue from the frozen fork and restore authoritative state.
    eval_ok(
        "(a1 (live-edit 1 :id \"rate\" :min 0 :max 10))"
        "(a2 (+ (prev a2) 1))"
        "(a3 (integrate 1))"
        "(a4 (expt 1e154 (- t 20)))");
    REQUIRE(useq_set_live_inputs("{\"rate\":1}") == 1);

    Wasm32Buffer buffer(8);
    if (!buffer.available()) {
        WARN("native platform cannot allocate a WASM32-addressable test buffer");
        return;
    }

    REQUIRE(useq_tick_and_project(
                "[\"a1\",\"a2\",\"a3\",\"a4\"]",
                20.0, 1, 21.0, 1, buffer.wasm_ptr(), 8) == 4);
    CHECK(buffer.data()[0] == Approx(1.0));
    CHECK(buffer.data()[1] == Approx(1.0));
    CHECK(buffer.data()[2] == Approx(0.0));
    CHECK(buffer.data()[3] == Approx(1.0));
    CHECK(buffer.data()[4] == Approx(1.0));
    CHECK(buffer.data()[5] == Approx(2.0));
    CHECK(buffer.data()[6] == Approx(9.0));
    CHECK(buffer.data()[7] == Approx(1e154));

    REQUIRE(useq_set_live_inputs("{\"rate\":7}") == 1);
    REQUIRE(useq_tick_and_project(
                "[\"a1\",\"a2\",\"a3\",\"a4\"]",
                22.0, 2, 23.0, 1, buffer.wasm_ptr(), 8) == 4);

    CHECK(buffer.data()[0] == Approx(7.0));
    CHECK(buffer.data()[1] == Approx(2.0));
    CHECK(buffer.data()[2] == Approx(9.0));
    CHECK(buffer.data()[3] == Approx(1e308));
    CHECK(buffer.data()[4] == Approx(1.0));
    CHECK(buffer.data()[5] == Approx(3.0));
    CHECK(buffer.data()[6] == Approx(10.0));
    CHECK(buffer.data()[7] == Approx(1e154));

    REQUIRE(useq_eval_output("a1", 23.0) == Approx(7.0));
    REQUIRE(useq_eval_output("a2", 23.0) == Approx(3.0));
    REQUIRE(useq_eval_output("a3", 23.0) == Approx(11.0));
    REQUIRE(useq_eval_output("a4", 23.0) == Approx(1e308));

    // Background output recompilation keeps the previous graph and publishes
    // chain-of-blame until the changed dependency is repaired.
    eval_ok("(define wrapper-health-dep 1)");
    eval_ok("(a1 (+ wrapper-health-dep 0.25))");
    REQUIRE(useq_tick_synth_controls(24.0, 0, 0) == 0);
    eval_ok("(defn wrapper-health-dep [x] x)");
    std::string reactive = active_diagnostics();
    REQUIRE(reactive.find("\"output\":\"a1\"") != std::string::npos);
    REQUIRE(reactive.find("\"triggered_by\":\"wrapper-health-dep\"") !=
            std::string::npos);
    eval_ok("(unassign a1)");
    REQUIRE(useq_output_health("a1") == 0);
    REQUIRE(active_diagnostics().find("wrapper-health-dep") ==
            std::string::npos);
    eval_ok("(define wrapper-health-dep 2)");
    eval_ok("(a1 (+ wrapper-health-dep 0.25))");
    REQUIRE(active_diagnostics().find("wrapper-health-dep") ==
            std::string::npos);

    // A named-state update can fail while its consumer output remains finite.
    // The held-state diagnostic is attributed directly to the state and
    // clears on the first recovered update.
    eval_ok("(define wrapper-health-rate 1)");
    eval_ok("(defstate wrapper-health-state 0 (/ 1 wrapper-health-rate))");
    eval_ok("(a1 wrapper-health-state)");
    REQUIRE(useq_tick_synth_controls(25.0, 0, 0) == 0);
    eval_ok("(define wrapper-health-rate 0)");
    REQUIRE(useq_tick_synth_controls(26.0, 0, 0) == 0);
    std::string state_failure = active_diagnostics();
    REQUIRE(state_failure.find("declared state update") != std::string::npos);
    REQUIRE(state_failure.find("\"subject\":\"state\"") != std::string::npos);
    REQUIRE(state_failure.find("\"state\":\"wrapper-health-state\"") !=
            std::string::npos);
    eval_ok("(define wrapper-health-rate 2)");
    REQUIRE(useq_tick_synth_controls(27.0, 0, 0) == 0);
    REQUIRE(active_diagnostics().find("declared state update") ==
            std::string::npos);

    // State health remains directly observable even with no output consumer.
    eval_ok("(define wrapper-orphan-rate 1)");
    eval_ok("(defstate wrapper-orphan-state 0 (/ 1 wrapper-orphan-rate))");
    REQUIRE(useq_tick_synth_controls(28.0, 0, 0) == 0);
    eval_ok("(define wrapper-orphan-rate 0)");
    REQUIRE(useq_tick_synth_controls(29.0, 0, 0) == 0);
    std::string orphan_failure = active_diagnostics();
    REQUIRE(orphan_failure.find("\"subject\":\"state\"") !=
            std::string::npos);
    REQUIRE(orphan_failure.find("\"state\":\"wrapper-orphan-state\"") !=
            std::string::npos);
    eval_ok("(define wrapper-orphan-rate 2)");
    REQUIRE(useq_tick_synth_controls(30.0, 0, 0) == 0);
    REQUIRE(active_diagnostics().find("wrapper-orphan-state") ==
            std::string::npos);
}
