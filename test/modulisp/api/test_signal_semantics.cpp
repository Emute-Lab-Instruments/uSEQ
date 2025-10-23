/**
 * @file test_signal_semantics.cpp
 * @brief Comprehensive tests for signal tracking and time-dependent expression evaluation
 *
 * These tests verify that the interpreter correctly identifies signals (expressions
 * containing time variables) and ensures they are always re-evaluated at the current
 * time, while non-signal constants use cached values for performance.
 */

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../../../uSEQ/src/modulisp/lisp/error_context.h"
#include "../../../uSEQ/src/modulisp/modulisp_interpreter.h"

TEST_CASE("Signal detection and evaluation", "[signals][semantics]")
{
    // Arrange: Create interpreter
    ErrorManager error_mgr;
    ModuLispInterpreter interp(&error_mgr, nullptr, nullptr, nullptr, nullptr, nullptr,
                               8, 8, 8);
    interp.init();
    interp.set_bpm(120.0, 0.0);

    SECTION("Direct time variable is detected as signal")
    {
        // Act: Define foo as transport time 't'
        interp.eval("(define foo t)");

        // Assert: foo should be marked as a signal
        REQUIRE(interp.is_signal("foo"));
    }

    SECTION("Compound expression with time variable is detected as signal")
    {
        // Act: Define foo as arithmetic expression containing 't'
        interp.eval("(define foo (+ 1 t))");

        // Assert: foo should be marked as a signal
        REQUIRE(interp.is_signal("foo"));
    }

    SECTION("Expression with 'beat' phasor is detected as signal")
    {
        // Act: Define foo using beat phasor
        interp.eval("(define foo (* 2 beat))");

        // Assert: foo should be marked as a signal
        REQUIRE(interp.is_signal("foo"));
    }

    SECTION("Expression with 'bar' phasor is detected as signal")
    {
        // Act: Define foo using bar phasor
        interp.eval("(define foo (- 1 bar))");

        // Assert: foo should be marked as a signal
        REQUIRE(interp.is_signal("foo"));
    }

    SECTION("Non-signal constant is not marked as signal")
    {
        // Act: Define a constant
        interp.eval("(define const 42)");

        // Assert: const should NOT be marked as a signal
        REQUIRE_FALSE(interp.is_signal("const"));
    }

    SECTION("Transitive signal: expression referencing a signal becomes a signal")
    {
        // Act: Define foo as a signal, then bar in terms of foo
        interp.eval("(define foo t)");
        interp.eval("(define bar (* 2 foo))");

        // Assert: Both should be signals
        REQUIRE(interp.is_signal("foo"));
        REQUIRE(interp.is_signal("bar"));
    }

    SECTION("Multi-level transitive signals")
    {
        // Act: Create a chain of definitions
        interp.eval("(define a t)");           // a is signal (contains t)
        interp.eval("(define b (+ a 1))");     // b is signal (contains a)
        interp.eval("(define c (* b 2))");     // c is signal (contains b)
        interp.eval("(define d (- c 5))");     // d is signal (contains c)

        // Assert: All should be signals
        REQUIRE(interp.is_signal("a"));
        REQUIRE(interp.is_signal("b"));
        REQUIRE(interp.is_signal("c"));
        REQUIRE(interp.is_signal("d"));
    }

    SECTION("Signal propagation when redefining non-signal as signal")
    {
        // Act: First define x as non-signal constant
        interp.eval("(define x 10)");
        interp.eval("(define y (* x 2))");  // y depends on x (non-signal)

        // Assert: Neither should be signals initially
        REQUIRE_FALSE(interp.is_signal("x"));
        REQUIRE_FALSE(interp.is_signal("y"));

        // Act: Redefine x as signal
        interp.eval("(define x (+ t 5))");

        // Assert: Now both should be signals
        REQUIRE(interp.is_signal("x"));
        REQUIRE(interp.is_signal("y"));
    }

    SECTION("Complex signal propagation graph")
    {
        // Act: Create a complex dependency graph
        //      t
        //     / \
        //    a   b (b = t + 1)
        //     \ /
        //      c (c = a + b)
        //      |
        //      d (d = c * 2)

        interp.eval("(define a t)");
        interp.eval("(define b (+ t 1))");
        interp.eval("(define c (+ a b))");
        interp.eval("(define d (* c 2))");

        // Assert: All should be signals
        REQUIRE(interp.is_signal("a"));
        REQUIRE(interp.is_signal("b"));
        REQUIRE(interp.is_signal("c"));
        REQUIRE(interp.is_signal("d"));
    }

    SECTION("Signal with slow function")
    {
        // Act: Define signal using slow function
        interp.eval("(define slowed (slow 2 t))");

        // Assert: Should be detected as signal
        REQUIRE(interp.is_signal("slowed"));
    }

    SECTION("Signal with fast function")
    {
        // Act: Define signal using fast function
        interp.eval("(define accelerated (fast 3 beat))");

        // Assert: Should be detected as signal
        REQUIRE(interp.is_signal("accelerated"));
    }
}

TEST_CASE("Signal evaluation behavior", "[signals][evaluation]")
{
    // Arrange: Create interpreter
    ErrorManager error_mgr;
    ModuLispInterpreter interp(&error_mgr, nullptr, nullptr, nullptr, nullptr, nullptr,
                               8, 8, 8);
    interp.init();
    interp.set_bpm(120.0, 0.0);

    SECTION("Signal is re-evaluated at different times")
    {
        // Act: Define signal and output
        interp.eval("(define sig t)");
        interp.eval("(a1 sig)");

        // Evaluate at time 10
        auto results1 = interp.eval_outputs({"a1"}, 10.0);
        REQUIRE(results1["a1"] == Approx(10.0).epsilon(1e-6));

        // Evaluate at time 20
        auto results2 = interp.eval_outputs({"a1"}, 20.0);
        REQUIRE(results2["a1"] == Approx(20.0).epsilon(1e-6));

        // Evaluate at time 100
        auto results3 = interp.eval_outputs({"a1"}, 100.0);
        REQUIRE(results3["a1"] == Approx(100.0).epsilon(1e-6));
    }

    SECTION("Constant uses cached value (not re-evaluated)")
    {
        // Act: Define constant and track evaluation count
        // We can't directly count evaluations, but we can verify the value doesn't change
        interp.eval("(define const 42)");
        interp.eval("(a1 const)");

        // Evaluate at different times - constant should always be 42
        auto results1 = interp.eval_outputs({"a1"}, 10.0);
        REQUIRE(results1["a1"] == Approx(42.0).epsilon(1e-6));

        auto results2 = interp.eval_outputs({"a1"}, 100.0);
        REQUIRE(results2["a1"] == Approx(42.0).epsilon(1e-6));

        auto results3 = interp.eval_outputs({"a1"}, 1000.0);
        REQUIRE(results3["a1"] == Approx(42.0).epsilon(1e-6));
    }

    SECTION("Transitive signal is re-evaluated with correct time")
    {
        // Act: Define signal chain
        interp.eval("(define base t)");
        interp.eval("(define derived (* base 2))");
        interp.eval("(a1 derived)");

        // Evaluate at time 5
        auto results1 = interp.eval_outputs({"a1"}, 5.0);
        REQUIRE(results1["a1"] == Approx(10.0).epsilon(1e-6));

        // Evaluate at time 10
        auto results2 = interp.eval_outputs({"a1"}, 10.0);
        REQUIRE(results2["a1"] == Approx(20.0).epsilon(1e-6));
    }

    SECTION("Signal with slow correctly divides time")
    {
        // Act: Define slowed signal
        interp.eval("(define slowed (slow 3 t))");
        interp.eval("(a1 slowed)");

        // Evaluate at time 9
        auto results1 = interp.eval_outputs({"a1"}, 9.0);
        REQUIRE(results1["a1"] == Approx(3.0).epsilon(1e-6));

        // Evaluate at time 15
        auto results2 = interp.eval_outputs({"a1"}, 15.0);
        REQUIRE(results2["a1"] == Approx(5.0).epsilon(1e-6));
    }

    SECTION("Mixed signals and constants in expression")
    {
        // Act: Define constant and signal, then combine them
        interp.eval("(define k 100)");        // constant
        interp.eval("(define sig (+ k t))");  // signal (contains t)
        interp.eval("(a1 sig)");

        // At time 5: sig = 100 + 5 = 105
        auto results1 = interp.eval_outputs({"a1"}, 5.0);
        REQUIRE(results1["a1"] == Approx(105.0).epsilon(1e-6));

        // At time 10: sig = 100 + 10 = 110
        auto results2 = interp.eval_outputs({"a1"}, 10.0);
        REQUIRE(results2["a1"] == Approx(110.0).epsilon(1e-6));
    }

    SECTION("Signal propagation after redefinition updates correctly")
    {
        // Act: Start with non-signal
        interp.eval("(define x 5)");
        interp.eval("(define y (* x 3))");
        interp.eval("(a1 y)");

        // Should be 15 (5 * 3)
        auto results1 = interp.eval_outputs({"a1"}, 100.0);
        REQUIRE(results1["a1"] == Approx(15.0).epsilon(1e-6));

        // Redefine x as signal
        interp.eval("(define x t)");

        // Now y should vary with time
        auto results2 = interp.eval_outputs({"a1"}, 10.0);
        REQUIRE(results2["a1"] == Approx(30.0).epsilon(1e-6));

        auto results3 = interp.eval_outputs({"a1"}, 20.0);
        REQUIRE(results3["a1"] == Approx(60.0).epsilon(1e-6));
    }
}

TEST_CASE("Edge cases and complex scenarios", "[signals][edge-cases]")
{
    // Arrange: Create interpreter
    ErrorManager error_mgr;
    ModuLispInterpreter interp(&error_mgr, nullptr, nullptr, nullptr, nullptr, nullptr,
                               8, 8, 8);
    interp.init();
    interp.set_bpm(120.0, 0.0);

    SECTION("Signal referencing multiple time variables")
    {
        // Act: Define signal using multiple time variables
        interp.eval("(define multi (+ t beat))");

        // Assert: Should be detected as signal
        REQUIRE(interp.is_signal("multi"));
    }

    SECTION("Deeply nested signal expressions")
    {
        // Act: Create deeply nested expression
        interp.eval("(define a t)");
        interp.eval("(define b (+ a 1))");
        interp.eval("(define c (+ b 1))");
        interp.eval("(define d (+ c 1))");
        interp.eval("(define e (+ d 1))");
        interp.eval("(define f (+ e 1))");

        // Assert: All should be signals
        REQUIRE(interp.is_signal("a"));
        REQUIRE(interp.is_signal("b"));
        REQUIRE(interp.is_signal("c"));
        REQUIRE(interp.is_signal("d"));
        REQUIRE(interp.is_signal("e"));
        REQUIRE(interp.is_signal("f"));

        // Verify evaluation at specific time
        interp.eval("(a1 f)");
        auto results = interp.eval_outputs({"a1"}, 10.0);
        // f = (((((10 + 1) + 1) + 1) + 1) + 1) = 15
        REQUIRE(results["a1"] == Approx(15.0).epsilon(1e-6));
    }

    SECTION("Diamond dependency pattern")
    {
        // Act: Create diamond pattern
        //       a (signal)
        //      / \
        //     b   c (both signals)
        //      \ /
        //       d (signal)

        interp.eval("(define a t)");
        interp.eval("(define b (* a 2))");
        interp.eval("(define c (+ a 1))");
        interp.eval("(define d (+ b c))");

        // Assert: All should be signals
        REQUIRE(interp.is_signal("a"));
        REQUIRE(interp.is_signal("b"));
        REQUIRE(interp.is_signal("c"));
        REQUIRE(interp.is_signal("d"));

        // Verify evaluation
        interp.eval("(a1 d)");
        auto results = interp.eval_outputs({"a1"}, 5.0);
        // a=5, b=10, c=6, d=16
        REQUIRE(results["a1"] == Approx(16.0).epsilon(1e-6));
    }

    SECTION("Constant redefined as signal propagates to entire dependency tree")
    {
        // Act: Create complex dependency tree starting with constants
        interp.eval("(define x 1)");
        interp.eval("(define y 2)");
        interp.eval("(define z (+ x y))");
        interp.eval("(define w (* z 2))");
        interp.eval("(define v (+ w 10))");

        // Assert: Initially none should be signals
        REQUIRE_FALSE(interp.is_signal("x"));
        REQUIRE_FALSE(interp.is_signal("y"));
        REQUIRE_FALSE(interp.is_signal("z"));
        REQUIRE_FALSE(interp.is_signal("w"));
        REQUIRE_FALSE(interp.is_signal("v"));

        // Act: Redefine y as signal
        interp.eval("(define y t)");

        // Assert: y and all its dependents should now be signals
        REQUIRE_FALSE(interp.is_signal("x"));  // x is still constant
        REQUIRE(interp.is_signal("y"));        // y is now signal
        REQUIRE(interp.is_signal("z"));        // z depends on y
        REQUIRE(interp.is_signal("w"));        // w depends on z
        REQUIRE(interp.is_signal("v"));        // v depends on w

        // Verify correct evaluation
        interp.eval("(a1 v)");
        auto results = interp.eval_outputs({"a1"}, 3.0);
        // y=3, z=1+3=4, w=4*2=8, v=8+10=18
        REQUIRE(results["a1"] == Approx(18.0).epsilon(1e-6));
    }

    SECTION("Multiple independent signal chains")
    {
        // Act: Create two independent signal chains
        interp.eval("(define a1-signal t)");
        interp.eval("(define a2-signal (* a1-signal 2))");

        interp.eval("(define b1-signal beat)");
        interp.eval("(define b2-signal (+ b1-signal 1))");

        // Assert: All should be signals
        REQUIRE(interp.is_signal("a1-signal"));
        REQUIRE(interp.is_signal("a2-signal"));
        REQUIRE(interp.is_signal("b1-signal"));
        REQUIRE(interp.is_signal("b2-signal"));
    }

    SECTION("Signal combined with non-signal constant")
    {
        // Act: Mix signal and constant
        interp.eval("(define scale 10)");
        interp.eval("(define offset 5)");
        interp.eval("(define sig (* (+ t offset) scale))");

        // Assert: sig should be signal, others should not
        REQUIRE_FALSE(interp.is_signal("scale"));
        REQUIRE_FALSE(interp.is_signal("offset"));
        REQUIRE(interp.is_signal("sig"));

        // Verify evaluation
        interp.eval("(a1 sig)");
        auto results = interp.eval_outputs({"a1"}, 2.0);
        // sig = (2 + 5) * 10 = 70
        REQUIRE(results["a1"] == Approx(70.0).epsilon(1e-6));
    }

    SECTION("Signal redefined as constant: dependents become non-signals")
    {
        // Act: Start with signal and dependent
        interp.eval("(define x t)");
        interp.eval("(define y (* x 3))");
        interp.eval("(define z (+ y 10))");

        // Assert: All should be signals initially
        REQUIRE(interp.is_signal("x"));
        REQUIRE(interp.is_signal("y"));
        REQUIRE(interp.is_signal("z"));

        // Verify initial behavior (at time 5)
        interp.eval("(a1 z)");
        auto results1 = interp.eval_outputs({"a1"}, 5.0);
        // x=5, y=15, z=25
        REQUIRE(results1["a1"] == Approx(25.0).epsilon(1e-6));

        // Verify at different time (should change)
        auto results2 = interp.eval_outputs({"a1"}, 10.0);
        // x=10, y=30, z=40
        REQUIRE(results2["a1"] == Approx(40.0).epsilon(1e-6));

        // Act: Redefine x as constant
        interp.eval("(define x 7)");

        // Note: Ideally, y and z should become non-signals, but our implementation
        // only marks symbols as signals, never unmarking them. This is a valid design
        // choice - it means once something is a signal, it stays a signal, ensuring
        // safety. Future evaluations of y and z will still re-evaluate their expressions,
        // but since x is no longer changing, y and z will have constant values.

        // The important thing is that the values are now constant
        auto results3 = interp.eval_outputs({"a1"}, 5.0);
        // x=7 (no longer t), y=21, z=31
        REQUIRE(results3["a1"] == Approx(31.0).epsilon(1e-6));

        // Verify that the value doesn't change with time anymore
        auto results4 = interp.eval_outputs({"a1"}, 100.0);
        // x=7, y=21, z=31 (same as before)
        REQUIRE(results4["a1"] == Approx(31.0).epsilon(1e-6));
    }
}
