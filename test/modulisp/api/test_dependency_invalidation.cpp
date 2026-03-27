/**
 * @file test_dependency_invalidation.cpp
 * @brief Unit tests for proactive dependency invalidation of compiled output programs.
 *
 * When a user redefines a symbol (via define, defn, set), any compiled output
 * program whose dependency set includes that symbol should be marked dirty and
 * recompiled at the next eval.
 */

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../../../uSEQ/src/modulisp/modulisp_interpreter.h"

static std::unique_ptr<ModuLispInterpreter> make_interp()
{
    auto interp = std::make_unique<ModuLispInterpreter>(nullptr, nullptr, nullptr, 8, 8, 8);
    interp->init();
    interp->set_bpm(120.0, 0.0);
    return interp;
}

TEST_CASE("Redefining a symbol updates dependent output", "[outputs][invalidation]")
{
    auto ip = make_interp();
    auto& interp = *ip;

    // Define a constant and assign it to output a1
    interp.eval("(define myval 0.25)");
    interp.eval("(a1 myval)");

    // First eval: a1 should return 0.25
    const double eval_time = 1.0;
    auto results = interp.eval_outputs({"a1"}, eval_time);
    REQUIRE(results.find("a1") != results.end());
    REQUIRE(results["a1"] == Approx(0.25).epsilon(1e-9));

    // Redefine myval to a different constant
    interp.eval("(define myval 0.75)");

    // Second eval: a1 should now return 0.75 (recompiled after invalidation)
    results = interp.eval_outputs({"a1"}, eval_time);
    REQUIRE(results.find("a1") != results.end());
    REQUIRE(results["a1"] == Approx(0.75).epsilon(1e-9));
}

TEST_CASE("Redefining a symbol does not affect unrelated outputs", "[outputs][invalidation]")
{
    auto ip = make_interp();
    auto& interp = *ip;

    // Define two independent symbols and assign to different outputs
    interp.eval("(define foo 0.3)");
    interp.eval("(define bar2 0.7)");
    interp.eval("(a1 foo)");
    interp.eval("(a2 bar2)");

    // Verify each independently first
    const double eval_time = 1.0;
    auto r1 = interp.eval_outputs({"a1"}, eval_time);
    REQUIRE(r1["a1"] == Approx(0.3).epsilon(1e-9));

    auto r2 = interp.eval_outputs({"a2"}, eval_time);
    REQUIRE(r2["a2"] == Approx(0.7).epsilon(1e-9));

    // Redefine only foo -- a2 should be unaffected
    interp.eval("(define foo 0.9)");

    auto results = interp.eval_outputs({"a1", "a2"}, eval_time);
    REQUIRE(results["a1"] == Approx(0.9).epsilon(1e-9));
    REQUIRE(results["a2"] == Approx(0.7).epsilon(1e-9));
}

TEST_CASE("set invalidates dependent output", "[outputs][invalidation]")
{
    auto ip = make_interp();
    auto& interp = *ip;

    // Use set to create a binding, then assign to output
    interp.eval("(set x 0.4)");
    interp.eval("(a1 x)");

    const double eval_time = 1.0;
    auto results = interp.eval_outputs({"a1"}, eval_time);
    REQUIRE(results["a1"] == Approx(0.4).epsilon(1e-9));

    // Mutate with set
    interp.eval("(set x 0.8)");

    results = interp.eval_outputs({"a1"}, eval_time);
    REQUIRE(results["a1"] == Approx(0.8).epsilon(1e-9));
}

TEST_CASE("LKG fallback preserves last healthy program", "[outputs][lkg]")
{
    auto ip = make_interp();
    auto& interp = *ip;

    // Define a valid expression and assign to output
    interp.eval("(define myexpr (+ t 1.0))");
    interp.eval("(a1 myexpr)");

    // Evaluate to prove it healthy
    const double eval_time = 2.0;
    auto results = interp.eval_outputs({"a1"}, eval_time);
    REQUIRE(results.find("a1") != results.end());
    REQUIRE(results["a1"] == Approx(3.0).epsilon(1e-6));

    // The active program is now observed good.
    // Redefine myexpr to something that still compiles and works
    interp.eval("(define myexpr (+ t 10.0))");

    // After recompile, the old program should have been promoted to LKG
    results = interp.eval_outputs({"a1"}, eval_time);
    REQUIRE(results["a1"] == Approx(12.0).epsilon(1e-6));
}

TEST_CASE("Expression with arithmetic dependency recompiles on symbol change",
          "[outputs][invalidation]")
{
    auto ip = make_interp();
    auto& interp = *ip;

    // Define amplitude and assign expression using it
    interp.eval("(define amp 0.5)");
    interp.eval("(a1 (* amp t))");

    const double eval_time = 4.0;
    auto results = interp.eval_outputs({"a1"}, eval_time);
    REQUIRE(results["a1"] == Approx(2.0).epsilon(1e-6));

    // Change amplitude
    interp.eval("(define amp 0.25)");

    results = interp.eval_outputs({"a1"}, eval_time);
    REQUIRE(results["a1"] == Approx(1.0).epsilon(1e-6));
}
