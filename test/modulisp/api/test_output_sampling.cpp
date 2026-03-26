/**
 * @file test_output_sampling.cpp
 * @brief Unit tests for time-window output sampling
 *
 * These tests verify that the new eval_outputs(start, end, num_samples, outputs)
 * arity correctly samples outputs across a time window at specified resolution.
 */

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../../../uSEQ/src/modulisp/diagnostic.h"
#include "../../../uSEQ/src/modulisp/modulisp_interpreter.h"

TEST_CASE("Output time-window sampling", "[outputs][sampling]")
{
    // Arrange: Create interpreter with 8 analog, 8 digital, 8 serial outputs
    ModuLispInterpreter interp(nullptr, nullptr, nullptr,
                               8, 8, 8);

    // Initialize the interpreter
    interp.init();

    // Set BPM to avoid divide-by-zero in phasors
    interp.set_bpm(120.0, 0.0);

    SECTION("Single sample at start time")
    {
        // Act: Set up a1 to output 't' (transport time)
        interp.eval("(a1 t)");

        // Sample at start_time = 10.0 seconds with 1 sample
        auto results = interp.eval_outputs(10.0, 10.0, 1, {"a1"});

        // Assert: Should return map with "a1" key containing vector with 1 sample
        REQUIRE(results.find("a1") != results.end());
        REQUIRE(results["a1"].size() == 1);
        REQUIRE(results["a1"][0] == Approx(10.0).epsilon(1e-6));
    }

    SECTION("Multiple samples uniformly distributed across time window")
    {
        // Act: Set up a1 to output 't'
        interp.eval("(a1 t)");

        // Sample from 0 to 10 seconds with 11 samples (0, 1, 2, ..., 10)
        auto results = interp.eval_outputs(0.0, 10.0, 11, {"a1"});

        // Assert: Should return map with "a1" key containing vector with 11 samples
        REQUIRE(results.find("a1") != results.end());
        REQUIRE(results["a1"].size() == 11);

        // Check that each sample has the correct time value
        for (size_t i = 0; i < results["a1"].size(); ++i)
        {
            double expected_time = i * 1.0; // Time step is 10 / (11-1) = 1.0
            REQUIRE(results["a1"][i] == Approx(expected_time).epsilon(1e-6));
        }
    }

    SECTION("Multiple outputs sampled across time window")
    {
        // Act: Set up three outputs with time-dependent expressions
        interp.eval("(a1 t)");          // a1 = time
        interp.eval("(a2 (* t 2))");    // a2 = 2 * time
        interp.eval("(a3 (- t 5))");    // a3 = time - 5

        // Sample from 5 to 15 seconds with 3 samples (5, 10, 15)
        auto results = interp.eval_outputs(5.0, 15.0, 3, {"a1", "a2", "a3"});

        // Assert: Should return map with 3 keys, each containing vector with 3 samples
        REQUIRE(results.find("a1") != results.end());
        REQUIRE(results.find("a2") != results.end());
        REQUIRE(results.find("a3") != results.end());
        REQUIRE(results["a1"].size() == 3);
        REQUIRE(results["a2"].size() == 3);
        REQUIRE(results["a3"].size() == 3);

        // Sample 0: time = 5
        REQUIRE(results["a1"][0] == Approx(5.0).epsilon(1e-6));
        REQUIRE(results["a2"][0] == Approx(10.0).epsilon(1e-6));
        REQUIRE(results["a3"][0] == Approx(0.0).epsilon(1e-6));

        // Sample 1: time = 10
        REQUIRE(results["a1"][1] == Approx(10.0).epsilon(1e-6));
        REQUIRE(results["a2"][1] == Approx(20.0).epsilon(1e-6));
        REQUIRE(results["a3"][1] == Approx(5.0).epsilon(1e-6));

        // Sample 2: time = 15
        REQUIRE(results["a1"][2] == Approx(15.0).epsilon(1e-6));
        REQUIRE(results["a2"][2] == Approx(30.0).epsilon(1e-6));
        REQUIRE(results["a3"][2] == Approx(10.0).epsilon(1e-6));
    }

    SECTION("Mixed output families sample consistently across the same window")
    {
        interp.eval("(a1 (+ t 1))");
        interp.eval("(d1 (* t 2))");
        interp.eval("(s1 (- t 1))");

        auto results = interp.eval_outputs(1.0, 3.0, 3, {"a1", "d1", "s1"});

        REQUIRE(results.find("a1") != results.end());
        REQUIRE(results.find("d1") != results.end());
        REQUIRE(results.find("s1") != results.end());
        REQUIRE(results["a1"].size() == 3);
        REQUIRE(results["d1"].size() == 3);
        REQUIRE(results["s1"].size() == 3);

        REQUIRE(results["a1"][0] == Approx(2.0).epsilon(1e-6));
        REQUIRE(results["a1"][1] == Approx(3.0).epsilon(1e-6));
        REQUIRE(results["a1"][2] == Approx(4.0).epsilon(1e-6));

        REQUIRE(results["d1"][0] == Approx(2.0).epsilon(1e-6));
        REQUIRE(results["d1"][1] == Approx(4.0).epsilon(1e-6));
        REQUIRE(results["d1"][2] == Approx(6.0).epsilon(1e-6));

        REQUIRE(results["s1"][0] == Approx(0.0).epsilon(1e-6));
        REQUIRE(results["s1"][1] == Approx(1.0).epsilon(1e-6));
        REQUIRE(results["s1"][2] == Approx(2.0).epsilon(1e-6));
    }

    SECTION("Empty outputs vector samples all outputs")
    {
        // Act: Set up multiple outputs
        interp.eval("(a1 1)");
        interp.eval("(d1 0.5)");
        interp.eval("(s1 0.75)");

        // Sample with empty outputs vector (should use all)
        auto results = interp.eval_outputs(0.0, 1.0, 2, {});

        // Assert: Should contain all outputs, each with 2 samples
        REQUIRE(results.find("a1") != results.end());
        REQUIRE(results.find("d1") != results.end());
        REQUIRE(results.find("s1") != results.end());
        REQUIRE(results["a1"].size() == 2);
        REQUIRE(results["d1"].size() == 2);
        REQUIRE(results["s1"].size() == 2);
    }

    SECTION("Zero samples returns empty map")
    {
        // Act: Request 0 samples
        auto results = interp.eval_outputs(0.0, 10.0, 0, {"a1"});

        // Assert: Should return empty map
        REQUIRE(results.size() == 0);
    }

    SECTION("Sampling with constant expressions")
    {
        // Act: Set up a constant output
        interp.eval("(a1 0.5)");

        // Sample across time window with constant expression
        auto results = interp.eval_outputs(0.0, 100.0, 5, {"a1"});

        // Assert: Should return map with "a1" containing 5 samples, all with value 0.5
        REQUIRE(results.find("a1") != results.end());
        REQUIRE(results["a1"].size() == 5);
        for (const auto& value : results["a1"])
        {
            REQUIRE(value == Approx(0.5).epsilon(1e-6));
        }
    }

    SECTION("Sampling with phasor-based expression")
    {
        // Act: Set up output with beat phasor (beat cycles from 0 to 1)
        // With 120 BPM, beat length is 0.5 seconds
        interp.eval("(a1 beat)");

        // Sample from 0 to 2 seconds with 5 samples
        // Beat phasor should go: 0, 0.25, 0.5, 0.75, 0
        auto results = interp.eval_outputs(0.0, 2.0, 5, {"a1"});

        // Assert: Should return map with "a1" containing 5 samples
        REQUIRE(results.find("a1") != results.end());
        REQUIRE(results["a1"].size() == 5);

        // Samples at times: 0, 0.5, 1.0, 1.5, 2.0 seconds
        // Beat length = 0.5 seconds (120 BPM = 2 beats per second)
        // So beat phase at these times: 0, 0, 0, 0, 0 (or close to 0)
        for (const auto& value : results["a1"])
        {
            // Beat phase should be close to 0 (first beat of bar)
            REQUIRE(value < 0.1);
        }
    }

    SECTION("Time-window sampling reflects recompilation after dependency redefinition")
    {
        interp.eval("(define scale 2)");
        interp.eval("(a1 (* scale t))");

        auto initial_results = interp.eval_outputs(0.0, 2.0, 3, {"a1"});
        REQUIRE(initial_results.find("a1") != initial_results.end());
        REQUIRE(initial_results["a1"].size() == 3);
        REQUIRE(initial_results["a1"][0] == Approx(0.0).epsilon(1e-6));
        REQUIRE(initial_results["a1"][1] == Approx(2.0).epsilon(1e-6));
        REQUIRE(initial_results["a1"][2] == Approx(4.0).epsilon(1e-6));

        interp.eval("(define scale 5)");

        auto updated_results = interp.eval_outputs(0.0, 2.0, 3, {"a1"});
        REQUIRE(updated_results.find("a1") != updated_results.end());
        REQUIRE(updated_results["a1"].size() == 3);
        REQUIRE(updated_results["a1"][0] == Approx(0.0).epsilon(1e-6));
        REQUIRE(updated_results["a1"][1] == Approx(5.0).epsilon(1e-6));
        REQUIRE(updated_results["a1"][2] == Approx(10.0).epsilon(1e-6));
    }

    SECTION("Runtime failures fall back to the last known good graph for the batch")
    {
        interp.eval("(a1 (+ t 1))");
        auto healthy_results = interp.eval_outputs(0.0, 1.0, 5, {"a1"});
        REQUIRE(healthy_results.find("a1") != healthy_results.end());
        REQUIRE(healthy_results["a1"].size() == 5);

        interp.eval("(a1 (/ 1 (- beat beat)))");

        auto fallback_results = interp.eval_outputs(0.0, 1.0, 5, {"a1"});
        REQUIRE(fallback_results.find("a1") != fallback_results.end());
        REQUIRE(fallback_results["a1"].size() == healthy_results["a1"].size());

        for (size_t i = 0; i < healthy_results["a1"].size(); ++i)
        {
            REQUIRE(fallback_results["a1"][i] ==
                    Approx(healthy_results["a1"][i]).epsilon(1e-6));
        }
    }
}
