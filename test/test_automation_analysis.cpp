#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"

#include "../uSEQ/src/modulisp/lisp/value.h"
#include "../uSEQ/src/modulisp/lisp/environment.h"
#include <cmath>

////////////////////////////////////////////////////////////////////////////////
/// AUTOMATION ANALYSIS TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Zero crossing analysis", "[automation][analysis][zero_crossings]") {
    // Time parameter should cross zero at t=0
    auto crossings = Value::t.find_zero_crossings(-5.0, 5.0);
    REQUIRE(crossings.size() == 1);
    REQUIRE(crossings[0] == Approx(0.0).epsilon(0.001));
    
    // Next zero crossing from negative time should be at 0
    double next_crossing = Value::t.get_next_zero_crossing(-2.0);
    REQUIRE(next_crossing == Approx(0.0).epsilon(0.001));
    
    // Check if zero crossings exist in range
    REQUIRE(Value::t.has_zero_crossings_in_range(-1.0, 1.0));
    REQUIRE_FALSE(Value::t.has_zero_crossings_in_range(1.0, 5.0));
    
    // Constants don't have zero crossings (unless they are zero)
    Value constant(3.14);
    auto const_crossings = constant.find_zero_crossings(-10.0, 10.0);
    REQUIRE(const_crossings.size() == 0);
    
    Value zero_constant(0.0);
    REQUIRE_FALSE(zero_constant.has_zero_crossings_in_range(-1.0, 1.0));
}

TEST_CASE("Threshold crossing analysis", "[automation][analysis][threshold_crossings]") {
    // Time parameter should cross threshold=2.5 at t=2.5
    auto crossings = Value::t.find_threshold_crossings(2.5, 0.0, 5.0);
    REQUIRE(crossings.size() == 1);
    REQUIRE(crossings[0] == Approx(2.5).epsilon(0.001));
    
    // Next threshold crossing from t=1 should be at t=2.5
    double next_crossing = Value::t.get_next_threshold_crossing(2.5, 1.0, true);
    REQUIRE(next_crossing == Approx(2.5).epsilon(0.001));
    
    // Constants don't cross thresholds (unless they equal the threshold)
    Value constant(3.14);
    auto const_crossings = constant.find_threshold_crossings(2.0, -10.0, 10.0);
    REQUIRE(const_crossings.size() == 0);
}

TEST_CASE("Value range analysis", "[automation][analysis][value_ranges]") {
    // Time parameter is in range [2.5, 3.2] from t=2.5 to t=3.2
    auto ranges = Value::t.find_value_ranges(2.5, 3.2, 0.0, 5.0);
    REQUIRE(ranges.size() == 1);
    REQUIRE(ranges[0].first == Approx(2.5).epsilon(0.001));
    REQUIRE(ranges[0].second == Approx(3.2).epsilon(0.001));
    
    // Check if value is in range at specific time
    REQUIRE(Value::t.is_in_range(2.0, 4.0, 3.0));
    REQUIRE_FALSE(Value::t.is_in_range(2.0, 4.0, 1.0));
    
    // Calculate total time in range
    double time_in_range = Value::t.get_time_in_range(2.0, 4.0, 1.0, 5.0);
    REQUIRE(time_in_range == Approx(2.0).epsilon(0.001)); // From t=2 to t=4
    
    // Constants are always in range if their value is within bounds
    Value constant(3.14);
    REQUIRE(constant.is_in_range(2.0, 4.0, 100.0)); // Time doesn't matter for constants
}

TEST_CASE("Extrema analysis", "[automation][analysis][extrema]") {
    // Time parameter is monotonic increasing, so:
    // - Local maximum at the END of interval (global max for this interval)
    // - Local minimum at the START of interval (global min for this interval)
    auto maxima = Value::t.find_local_maxima(0.0, 10.0);
    REQUIRE(maxima.size() == 1);
    REQUIRE(maxima[0] == Approx(10.0).epsilon(0.001)); // Maximum at end of interval
    
    auto minima = Value::t.find_local_minima(0.0, 10.0);
    REQUIRE(minima.size() == 1);
    REQUIRE(minima[0] == Approx(0.0).epsilon(0.001)); // Minimum at start of interval
    
    // Global extrema are at the endpoints for monotonic functions
    double max_time = Value::t.get_global_maximum_time(2.0, 8.0); 
    REQUIRE(max_time == Approx(8.0).epsilon(0.001)); // Maximum at end of interval
    
    double min_time = Value::t.get_global_minimum_time(2.0, 8.0);
    REQUIRE(min_time == Approx(2.0).epsilon(0.001)); // Minimum at start of interval
    
    // Constants have no extrema (every point is the same value)
    Value constant(5.0);
    auto const_maxima = constant.find_local_maxima(-5.0, 5.0);
    REQUIRE(const_maxima.size() == 0);
}

TEST_CASE("Monotonicity analysis", "[automation][analysis][monotonicity]") {
    // Time parameter is monotonically increasing
    REQUIRE(Value::t.is_monotonic_in_range(0.0, 10.0));
    REQUIRE(Value::t.is_increasing_in_range(0.0, 10.0));
    REQUIRE_FALSE(Value::t.is_decreasing_in_range(0.0, 10.0));
    
    // Constants are trivially monotonic (constant slope = 0)
    Value constant(2.71);
    REQUIRE(constant.is_monotonic_in_range(-5.0, 5.0));
    // Constants are neither strictly increasing nor decreasing
    REQUIRE_FALSE(constant.is_increasing_in_range(-5.0, 5.0));
    REQUIRE_FALSE(constant.is_decreasing_in_range(-5.0, 5.0));
}

TEST_CASE("Basic signal properties", "[automation][analysis][signal_properties]") {
    // Test smoothness and continuity
    REQUIRE(Value::t.is_smooth());
    REQUIRE(Value::t.is_continuous());
    
    // Test monotonicity properties
    REQUIRE(Value::t.is_monotonic());
    REQUIRE_FALSE(Value::t.is_constant());
    
    // Test bounds
    REQUIRE(std::isinf(Value::t.get_min()));
    REQUIRE(std::isinf(Value::t.get_max()));
    
    // Test periodicity
    REQUIRE_FALSE(Value::t.is_periodic());
    
    // Constants should be smooth, continuous, and constant
    Value constant(42.0);
    REQUIRE(constant.is_smooth());
    REQUIRE(constant.is_continuous());
    REQUIRE(constant.is_constant());
    REQUIRE(constant.is_monotonic()); // Constants are trivially monotonic
    REQUIRE(constant.get_min() == Approx(42.0).epsilon(0.001));
    REQUIRE(constant.get_max() == Approx(42.0).epsilon(0.001));
}

TEST_CASE("Transcendental functions", "[automation][analysis][transcendental]") {
    // Test with constants
    Value pi(3.14159);
    
    Value sine = pi.sin();
    REQUIRE(sine.as_float() == Approx(0.0).margin(0.01)); // sin(π) ≈ 0
    
    Value cosine = pi.cos();
    REQUIRE(cosine.as_float() == Approx(-1.0).epsilon(0.01)); // cos(π) ≈ -1
    
    Value zero(0.0);
    Value exponential = zero.exp();
    REQUIRE(exponential.as_float() == Approx(1.0).epsilon(0.001)); // e^0 = 1
    
    Value one(1.0);
    Value logarithm = one.log();
    REQUIRE(logarithm.as_float() == Approx(0.0).epsilon(0.001)); // ln(1) = 0
    
    Value four(4.0);
    Value square_root = four.sqrt();
    REQUIRE(square_root.as_float() == Approx(2.0).epsilon(0.001)); // √4 = 2
    
    Value negative(-5.0);
    Value absolute = negative.abs();
    REQUIRE(absolute.as_float() == Approx(5.0).epsilon(0.001)); // |-5| = 5
}

TEST_CASE("Arithmetic operations with signals", "[automation][analysis][arithmetic]") {
    // Test signal arithmetic - should produce signal results
    Value sum = Value::t + Value(2.0);
    REQUIRE(sum.is_signal()); // Signal + constant = signal
    REQUIRE(sum.is_smooth());
    REQUIRE(sum.is_continuous());
    
    Value product = Value::t * Value(3.0);  
    REQUIRE(product.is_signal()); // Signal * constant = signal
    REQUIRE(product.is_smooth());
    REQUIRE(product.is_continuous());
    
    // Test constant arithmetic - should produce numeric results
    Value const1(5.0);
    Value const2(3.0);
    Value const_sum = const1 + const2;
    REQUIRE(const_sum.is_number());
    REQUIRE(const_sum.as_float() == Approx(8.0).epsilon(0.001));
    
    Value const_product = const1 * const2;
    REQUIRE(const_product.is_number());
    REQUIRE(const_product.as_float() == Approx(15.0).epsilon(0.001));
}

