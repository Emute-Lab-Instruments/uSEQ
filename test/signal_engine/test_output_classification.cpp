// Output Classification golden tests
// Validates that the compiler correctly classifies outputs as
// Pure, InputDep, or Stateful per visualisation.md §4.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "src/signal_engine/signal_engine.h"
#include <cstring>

using namespace sig;

// Helper: compile an expression into output a1, return its classification
static OutputClass classify(const char* expr) {
    SignalEngine engine;
    engine.init_defaults();

    char wrapped[4096];
    snprintf(wrapped, sizeof(wrapped), "(a1 %s)", expr);
    EvalResult r = eval_cold(wrapped, (uint32_t)strlen(wrapped), engine);
    REQUIRE(r.kind != EvalResult::Error);

    return engine.pool.output_class[0];
}

// Helper: classify and also return the input mask
static std::pair<OutputClass, uint32_t> classify_with_mask(const char* expr) {
    SignalEngine engine;
    engine.init_defaults();

    char wrapped[4096];
    snprintf(wrapped, sizeof(wrapped), "(a1 %s)", expr);
    EvalResult r = eval_cold(wrapped, (uint32_t)strlen(wrapped), engine);
    REQUIRE(r.kind != EvalResult::Error);

    return { engine.pool.output_class[0], engine.pool.output_input_mask[0] };
}

// ── Pure expressions ────────────────────────────────────────────────────────

TEST_CASE("Classification: constant is pure", "[classification]") {
    REQUIRE(classify("0.5") == OutputClass::Pure);
}

TEST_CASE("Classification: arithmetic on constants is pure", "[classification]") {
    REQUIRE(classify("(+ 1 2)") == OutputClass::Pure);
}

TEST_CASE("Classification: function of t is pure", "[classification]") {
    REQUIRE(classify("(sin (* t 2))") == OutputClass::Pure);
}

TEST_CASE("Classification: sin of beat (time-derived) is pure", "[classification]") {
    REQUIRE(classify("(usin beat)") == OutputClass::Pure);
}

TEST_CASE("Classification: nested time transforms are pure", "[classification]") {
    REQUIRE(classify("(fast 2 (usin beat))") == OutputClass::Pure);
}

TEST_CASE("Classification: seq of constants is pure", "[classification]") {
    REQUIRE(classify("(seq [0.2 0.5 0.8])") == OutputClass::Pure);
}

TEST_CASE("Classification: euclid pattern is pure", "[classification]") {
    REQUIRE(classify("(euclid 3 8)") == OutputClass::Pure);
}

TEST_CASE("Classification: step with data table is pure", "[classification]") {
    REQUIRE(classify("(step [0.2 0.5 0.8])") == OutputClass::Pure);
}

TEST_CASE("Classification: if with pure branches is pure", "[classification]") {
    REQUIRE(classify("(if (> beat 0.5) 1.0 0.0)") == OutputClass::Pure);
}

// ── Input-dependent expressions ─────────────────────────────────────────────

TEST_CASE("Classification: hardware input is input-dep", "[classification]") {
    auto [cls, mask] = classify_with_mask("ain1");
    REQUIRE(cls == OutputClass::InputDep);
    REQUIRE((mask & (1u << 2)) != 0);  // ain1 = input channel 2
}

TEST_CASE("Classification: expression using hardware input is input-dep", "[classification]") {
    auto [cls, mask] = classify_with_mask("(* ain1 (usin beat))");
    REQUIRE(cls == OutputClass::InputDep);
}

TEST_CASE("Classification: multiple inputs tracked in mask", "[classification]") {
    auto [cls, mask] = classify_with_mask("(+ ain1 ain2)");
    REQUIRE(cls == OutputClass::InputDep);
    REQUIRE((mask & 0xCu) == 0xCu);  // ain1=ch2 + ain2=ch3 → bits 2,3
}

TEST_CASE("Classification: if with input condition is input-dep", "[classification]") {
    auto [cls, mask] = classify_with_mask("(if (> ain1 0.5) 1.0 0.0)");
    REQUIRE(cls == OutputClass::InputDep);
}

// ── Stateful expressions ────────────────────────────────────────────────────

TEST_CASE("Classification: integrate is stateful", "[classification]") {
    REQUIRE(classify("(integrate 0.1)") == OutputClass::Stateful);
}

TEST_CASE("Classification: expression using prev is stateful", "[classification]") {
    REQUIRE(classify("(prev a1)") == OutputClass::Stateful);
}

// ── Multi-output classification ─────────────────────────────────────────────

TEST_CASE("Classification: multiple outputs classified independently", "[classification]") {
    SignalEngine engine;
    engine.init_defaults();

    // a1 = pure (sin of t)
    const char* src1 = "(a1 (usin beat))";
    eval_cold(src1, (uint32_t)strlen(src1), engine);

    // a2 = input-dep
    const char* src2 = "(a2 ain1)";
    eval_cold(src2, (uint32_t)strlen(src2), engine);

    // a3 = stateful
    const char* src3 = "(a3 (integrate 0.01))";
    eval_cold(src3, (uint32_t)strlen(src3), engine);

    REQUIRE(engine.pool.output_class[0] == OutputClass::Pure);
    REQUIRE(engine.pool.output_class[1] == OutputClass::InputDep);
    REQUIRE(engine.pool.output_class[2] == OutputClass::Stateful);
}

// ── Inactive outputs ────────────────────────────────────────────────────────

TEST_CASE("Classification: unassigned output is inactive", "[classification]") {
    SignalEngine engine;
    engine.init_defaults();

    // Only assign a1, leave a2 unassigned
    const char* src = "(a1 0.5)";
    eval_cold(src, (uint32_t)strlen(src), engine);

    REQUIRE(engine.pool.output_class[0] == OutputClass::Pure);
    REQUIRE(engine.pool.output_class[1] == OutputClass::Inactive);
}

// ── Reclassification on recompile ───────────────────────────────────────────

TEST_CASE("Classification: reclassifies when expression changes", "[classification]") {
    SignalEngine engine;
    engine.init_defaults();

    // Start pure
    const char* src1 = "(a1 (usin beat))";
    eval_cold(src1, (uint32_t)strlen(src1), engine);
    REQUIRE(engine.pool.output_class[0] == OutputClass::Pure);

    // Change to input-dep
    const char* src2 = "(a1 ain1)";
    eval_cold(src2, (uint32_t)strlen(src2), engine);
    REQUIRE(engine.pool.output_class[0] == OutputClass::InputDep);

    // Change to stateful
    const char* src3 = "(a1 (integrate 0.1))";
    eval_cold(src3, (uint32_t)strlen(src3), engine);
    REQUIRE(engine.pool.output_class[0] == OutputClass::Stateful);
}
