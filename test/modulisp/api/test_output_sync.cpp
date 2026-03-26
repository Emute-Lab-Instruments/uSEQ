/**
 * @file test_output_sync.cpp
 * @brief Unit tests for output evaluation synchronization
 *
 * These tests verify that multiple outputs evaluate the same expression
 * at the same time point and produce consistent results.
 */

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../../../uSEQ/src/modulisp/diagnostic.h"
#include "../../../uSEQ/src/modulisp/modulisp_interpreter.h"

TEST_CASE("Outputs evaluate transport time synchronously", "[outputs][sync]")
{
    // Arrange: Create interpreter with 8 analog, 8 digital, 8 serial outputs
    ModuLispInterpreter interp(nullptr, nullptr, nullptr,
                               8, 8, 8);

    // Initialize the interpreter
    interp.init();

    // Set BPM to avoid divide-by-zero in phasors
    interp.set_bpm(120.0, 0.0);

    SECTION("Three outputs with (a1 t), (a2 t), (a3 t) all return same value at t=123.45")
    {
        // Act: Evaluate the three expressions to store them in output slots
        String expr1 = "(a1 t)";
        String expr2 = "(a2 t)";
        String expr3 = "(a3 t)";

        interp.eval(expr1);
        interp.eval(expr2);
        interp.eval(expr3);

        // Evaluate all three outputs at t=123.45
        const double test_time = 123.45;
        std::vector<String> outputs = {"a1", "a2", "a3"};
        std::map<String, double> results = interp.eval_outputs(outputs, test_time);

        // Assert: All three outputs evaluated successfully
        REQUIRE(results.find("a1") != results.end());
        REQUIRE(results.find("a2") != results.end());
        REQUIRE(results.find("a3") != results.end());

        // Assert: All values are equal (they all evaluate 't' at the same time)
        REQUIRE(results["a1"] == Approx(results["a2"]).epsilon(1e-9));
        REQUIRE(results["a2"] == Approx(results["a3"]).epsilon(1e-9));
        REQUIRE(results["a1"] == Approx(results["a3"]).epsilon(1e-9));

        // Assert: The values are approximately equal to the test time
        // Note: 't' is transport time which should equal the time we set
        REQUIRE(results["a1"] == Approx(test_time).epsilon(1e-6));
    }

    SECTION("Variable bound to transport time evaluates correctly at specific time")
    {
        // Act: Define foo as transport time, then assign it to a1
        interp.eval("(define foo t)");
        interp.eval("(a1 foo)");

        // Evaluate output a1 at t=1234.5
        const double test_time = 1234.5;
        std::vector<String> outputs = {"a1"};
        std::map<String, double> results = interp.eval_outputs(outputs, test_time);

        // Assert: Output was evaluated successfully
        REQUIRE(results.find("a1") != results.end());

        // Assert: The value equals the transport time
        REQUIRE(results["a1"] == Approx(test_time).epsilon(1e-6));
    }

    SECTION("Variable bound to slow function evaluates with time division")
    {
        // Act: Define foo as (slow 2 t), then assign it to a1
        interp.eval("(define foo (slow 2 t))");
        interp.eval("(a1 foo)");

        // Evaluate output a1 at t=5
        const double test_time = 5.0;
        std::vector<String> outputs = {"a1"};
        std::map<String, double> results = interp.eval_outputs(outputs, test_time);

        // Assert: Output was evaluated successfully
        REQUIRE(results.find("a1") != results.end());

        // Assert: The value equals time divided by slow factor (5 / 2 = 2.5)
        const double expected_value = test_time / 2.0;
        REQUIRE(results["a1"] == Approx(expected_value).epsilon(1e-6));
    }
}
