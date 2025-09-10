#include "../uSEQ/src/modulisp/lisp/environment.h"
#include "../uSEQ/src/modulisp/lisp/value.h"
#include "catch.hpp"
#include <cmath>
#include <iostream>

TEST_CASE("Static t variable initialization", "[signals][initialization]")
{
    // Test that t is a signal
    REQUIRE(Value::t.is_signal());
    REQUIRE(Value::t.type == Value::SIGNAL);

    // Test global t reference
    REQUIRE(t.is_signal());
    REQUIRE(t.type == Value::SIGNAL);

    // Test that t and Value::t are the same
    REQUIRE(&t == &Value::t);
}

// Commented out test function for signal metadata (converted to Catch2 format but
// kept commented) TEST_CASE("Signal metadata properties",
// "[signals][metadata][basic]") {
//     // IS
//     REQUIRE(Value::t.is_monotonic());
//     REQUIRE(Value::t.is_increasing());
//     REQUIRE(Value::t.get_period() == INFINITY);
//     // ISN'T
//     REQUIRE_FALSE(Value::t.is_constant());
//     REQUIRE_FALSE(Value::t.is_periodic());
//     REQUIRE_FALSE(Value::t.is_bounded());

//     // Test bounds (should be unbounded)
//     REQUIRE(std::isinf(Value::t.get_min()) && Value::t.get_min() < 0);
//     REQUIRE(std::isinf(Value::t.get_max()) && Value::t.get_max() > 0);
// }

// Commented out test function for t zeros (converted to Catch2 format but kept
// commented) TEST_CASE("Time variable zeros", "[signals][zeros]") {
//     REQUIRE(t.find_zeros() == 0.0); // t is only zero at time = 0
//     REQUIRE(t.find_zeros() == 0.0); // t is only zero at time = 0
// }

TEST_CASE("Signal arithmetic operations", "[signals][arithmetic]")
{
    // Test addition
    Value result1 = Value::t + Value(5);
    REQUIRE(result1.is_signal());
    REQUIRE_FALSE(result1.is_constant());
    REQUIRE(result1.as_float() ==
            Approx(5.0).epsilon(1e-9)); // t starts at 0, so t + 5 = 5

    // Test subtraction
    Value result2 = Value::t - Value(3);
    REQUIRE(result2.is_signal());
    REQUIRE_FALSE(result2.is_constant());
    REQUIRE(result2.as_float() ==
            Approx(-3.0).epsilon(1e-9)); // t starts at 0, so t - 3 = -3

    // Test multiplication
    Value result3 = Value::t * Value(2);
    REQUIRE(result3.is_signal());
    REQUIRE_FALSE(result3.is_constant());
    REQUIRE(result3.as_float() ==
            Approx(0.0).epsilon(1e-9)); // t starts at 0, so t * 2 = 0

    // Test division
    Value result4 = (Value::t + Value(4)) / Value(2);
    REQUIRE(result4.is_signal());
    REQUIRE_FALSE(result4.is_constant());
    REQUIRE(result4.as_float() ==
            Approx(2.0).epsilon(1e-9)); // (t + 4) / 2 = (0 + 4) / 2 = 2
}

TEST_CASE("Signal modulo operation (periodic signals)",
          "[signals][modulo][periodic]")
{
    // Test the example from the user: (t % 1.5) * 2
    Value mod_result = Value::t % Value(1.5);
    REQUIRE(mod_result.is_signal());
    REQUIRE_FALSE(mod_result.is_constant());
    REQUIRE(mod_result.is_periodic());
    REQUIRE(mod_result.get_period() == Approx(1.5).epsilon(1e-9));
    REQUIRE(mod_result.get_min() == Approx(0.0).epsilon(1e-9));
    REQUIRE(mod_result.get_max() == Approx(1.5).epsilon(1e-9));

    Value final_result = mod_result * Value(2);
    REQUIRE(final_result.is_signal());
    REQUIRE_FALSE(final_result.is_constant());
    // After multiplication, bounds should be [0, 3)
    REQUIRE(final_result.get_min() == Approx(0.0).epsilon(1e-9));
    REQUIRE(final_result.get_max() == Approx(3.0).epsilon(1e-9));

    // Test actual computation: t starts at 0, so (0 % 1.5) * 2 = 0 * 2 = 0
    REQUIRE(final_result.as_float() == Approx(0.0).epsilon(1e-9));
}

TEST_CASE("Signal pow operation", "[signals][power]")
{
    // Test t^2
    Value pow_result = Value::t.pow(Value(2));
    REQUIRE(pow_result.is_signal());
    REQUIRE_FALSE(pow_result.is_constant());
    REQUIRE_FALSE(pow_result.is_monotonic()); // Power operations break monotonicity
    REQUIRE_FALSE(pow_result.is_periodic());  // Power operations break periodicity

    // Test actual computation: t starts at 0, so 0^2 = 0
    REQUIRE(pow_result.as_float() == Approx(0.0).epsilon(1e-9));

    // Test (t + 2)^2
    Value base_plus_2 = Value::t + Value(2);
    Value pow_result2 = base_plus_2.pow(Value(2));
    REQUIRE(pow_result2.is_signal());
    REQUIRE_FALSE(pow_result2.is_constant());

    // Test actual computation: (0 + 2)^2 = 4
    REQUIRE(pow_result2.as_float() == Approx(4.0).epsilon(1e-9));
}

TEST_CASE("Metadata propagation", "[signals][metadata]")
{
    // Test that operations between constants don't create signals
    Value const1       = Value(3);
    Value const2       = Value(7);
    Value const_result = const1 + const2;
    REQUIRE_FALSE(const_result.is_signal());
    REQUIRE(const_result.is_constant());

    // Test that operations with at least one signal create signals
    Value mixed_result = Value::t + const1;
    REQUIRE(mixed_result.is_signal());
    REQUIRE_FALSE(mixed_result.is_constant());

    // Test that signal + signal creates signal
    Value t2            = Value::t * Value(2); // Create another signal
    Value signal_result = Value::t + t2;
    REQUIRE(signal_result.is_signal());
    REQUIRE_FALSE(signal_result.is_constant());
}

TEST_CASE("Complex signal expressions", "[signals][complex]")
{
    // Test complex expression: ((t * 2) + 3) % 5
    Value t_times_2        = Value::t * Value(2);
    Value t_times_2_plus_3 = t_times_2 + Value(3);
    Value complex_result   = t_times_2_plus_3 % Value(5);

    REQUIRE(complex_result.is_signal());
    REQUIRE_FALSE(complex_result.is_constant());
    REQUIRE(complex_result.is_periodic());
    REQUIRE(complex_result.get_period() == Approx(5.0).epsilon(1e-9));

    // Test actual computation: ((0 * 2) + 3) % 5 = 3 % 5 = 3
    REQUIRE(complex_result.as_float() == Approx(3.0).epsilon(1e-9));
}

TEST_CASE("is_signal() method", "[signals][method]")
{
    // Constants should not be signals
    REQUIRE_FALSE(Value(42).is_signal());
    REQUIRE_FALSE(Value(3.14).is_signal());
    REQUIRE_FALSE(Value::string("hello").is_signal());

    // t should be a signal
    REQUIRE(Value::t.is_signal());

    // Results of operations with t should be signals
    REQUIRE((Value::t + Value(1)).is_signal());
    REQUIRE((Value::t * Value(2)).is_signal());
    REQUIRE((Value::t % Value(3)).is_signal());
    REQUIRE(Value::t.pow(Value(2)).is_signal());
}

TEST_CASE("Signal edge cases", "[signals][edge_cases]")
{
    // Test modulo with zero (should handle gracefully)
    Value zero_mod = Value::t % Value(0);
    REQUIRE(zero_mod.is_signal());
    // Note: The actual modulo result with 0 is undefined, but metadata should
    // be conservative

    // Test modulo with negative values
    Value neg_mod = Value::t % Value(-2.5);
    REQUIRE(neg_mod.is_signal());
    REQUIRE_FALSE(neg_mod.is_constant());

    // Test operations with very small numbers
    Value tiny_result = Value::t + Value(1e-10);
    REQUIRE(tiny_result.is_signal());
    REQUIRE_FALSE(tiny_result.is_constant());
}

TEST_CASE("Metadata query methods", "[signals][metadata][query]")
{
    // Test on constants (should return value itself for min/max)
    Value const_val = Value(42);
    REQUIRE(const_val.is_constant());
    REQUIRE_FALSE(const_val.is_periodic());
    REQUIRE(const_val.is_monotonic()); // Constants are trivially monotonic
    REQUIRE(const_val.get_min() == Approx(42.0).epsilon(1e-9));
    REQUIRE(const_val.get_max() == Approx(42.0).epsilon(1e-9));
    REQUIRE(const_val.get_period() == Approx(0.0).epsilon(1e-9));

    // Test on signals
    Value bounded_signal = Value::t % Value(10); // Periodic signal with period 10
    REQUIRE_FALSE(bounded_signal.is_constant());
    REQUIRE(bounded_signal.is_periodic());
    REQUIRE_FALSE(bounded_signal.is_monotonic());
    REQUIRE(bounded_signal.get_min() == Approx(0.0).epsilon(1e-9));
    REQUIRE(bounded_signal.get_max() == Approx(10.0).epsilon(1e-9));
    REQUIRE(bounded_signal.get_period() == Approx(10.0).epsilon(1e-9));
}

////////////////////////////////////////////////////////////////////////////////
/// EXTENDED METADATA TESTS (TDD-STYLE - TESTS FIRST)
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Smoothness and continuity metadata", "[signals][metadata][smoothness]")
{
    // Basic time parameter should be analytic (smooth)
    REQUIRE(Value::t.is_smooth());
    REQUIRE(Value::t.is_continuous());
    REQUIRE(Value::t.is_analytic());

    // Polynomial operations should preserve smoothness
    Value t_squared = Value::t * Value::t;
    REQUIRE(t_squared.is_smooth());
    REQUIRE(t_squared.is_analytic());

    // Absolute value creates non-smooth points
    Value abs_t = Value::t.abs();
    REQUIRE(abs_t.is_continuous());
    REQUIRE_FALSE(abs_t.is_smooth()); // Not differentiable at t=0
    REQUIRE_FALSE(abs_t.is_analytic());

    // Modulo creates discontinuities
    Value mod_signal = Value::t % Value(2.0);
    REQUIRE_FALSE(mod_signal.is_continuous()); // Jump discontinuities
    REQUIRE_FALSE(mod_signal.is_smooth());

    // Floor function creates step discontinuities
    Value floor_signal = Value::t.floor();
    REQUIRE_FALSE(floor_signal.is_continuous());
    REQUIRE_FALSE(floor_signal.is_smooth());
}

TEST_CASE("Symmetry metadata", "[signals][metadata][symmetry]")
{
    // t should be odd: f(-x) = -f(x)
    REQUIRE(Value::t.is_odd());
    REQUIRE_FALSE(Value::t.is_even());

    // t^2 should be even: f(-x) = f(x)
    Value t_squared = Value::t * Value::t;
    REQUIRE(t_squared.is_even());
    REQUIRE_FALSE(t_squared.is_odd());

    // t^3 should be odd
    Value t_cubed = t_squared * Value::t;
    REQUIRE(t_cubed.is_odd());
    REQUIRE_FALSE(t_cubed.is_even());

    // Constants should be even
    Value constant = Value(5);
    REQUIRE(constant.is_even());
    REQUIRE_FALSE(constant.is_odd());

    // abs(t) should be even
    Value abs_t = Value::t.abs();
    REQUIRE(abs_t.is_even());
    REQUIRE_FALSE(abs_t.is_odd());

    // Reflection symmetry tests
    Value shifted_abs = (Value::t - Value(3)).abs();
    REQUIRE(shifted_abs.has_reflection_symmetry());
    REQUIRE(shifted_abs.get_reflection_axis() == Approx(3.0).epsilon(1e-9));

    // sin(t) should be odd
    Value sin_t = Value::t.sin();
    REQUIRE(sin_t.is_odd());
    REQUIRE_FALSE(sin_t.is_even());

    // cos(t) should be even
    Value cos_t = Value::t.cos();
    REQUIRE(cos_t.is_even());
    REQUIRE_FALSE(cos_t.is_odd());
}

TEST_CASE("Rate and derivative metadata", "[signals][metadata][derivative]")
{
    // t has constant derivative = 1
    REQUIRE(Value::t.get_min_derivative() == Approx(1.0).epsilon(1e-9));
    REQUIRE(Value::t.get_max_derivative() == Approx(1.0).epsilon(1e-9));
    REQUIRE(Value::t.has_bounded_derivative());
    REQUIRE(Value::t.is_lipschitz_continuous());
    REQUIRE(Value::t.get_lipschitz_constant() == Approx(1.0).epsilon(1e-9));

    // t^2 has unbounded derivative (grows linearly)
    Value t_squared = Value::t * Value::t;
    REQUIRE_FALSE(t_squared.has_bounded_derivative());
    REQUIRE_FALSE(t_squared.is_lipschitz_continuous());

    // sin(t) has bounded derivative between -1 and 1
    Value sin_t = Value::t.sin();
    REQUIRE(sin_t.get_min_derivative() == Approx(-1.0).epsilon(1e-9));
    REQUIRE(sin_t.get_max_derivative() == Approx(1.0).epsilon(1e-9));
    REQUIRE(sin_t.has_bounded_derivative());
    REQUIRE(sin_t.is_lipschitz_continuous());
    REQUIRE(sin_t.get_lipschitz_constant() == Approx(1.0).epsilon(1e-9));

    // Constants have zero derivative
    Value constant = Value(42);
    REQUIRE(constant.get_min_derivative() == Approx(0.0).epsilon(1e-9));
    REQUIRE(constant.get_max_derivative() == Approx(0.0).epsilon(1e-9));
    REQUIRE(constant.has_bounded_derivative());

    // 2*t has derivative = 2
    Value scaled_t = Value::t * Value(2);
    REQUIRE(scaled_t.get_min_derivative() == Approx(2.0).epsilon(1e-9));
    REQUIRE(scaled_t.get_max_derivative() == Approx(2.0).epsilon(1e-9));
}

TEST_CASE("Frequency domain metadata", "[signals][metadata][frequency]")
{
    // DC signal (constant) should have zero frequency
    Value dc_signal = Value(10);
    REQUIRE(dc_signal.is_pure_tone());
    REQUIRE(dc_signal.get_fundamental_frequency() == Approx(0.0).epsilon(1e-9));
    REQUIRE(dc_signal.get_max_frequency() == Approx(0.0).epsilon(1e-9));
    REQUIRE(dc_signal.is_bandlimited());

    // Linear ramp (t) should be non-bandlimited
    REQUIRE_FALSE(Value::t.is_bandlimited());
    REQUIRE(Value::t.get_max_frequency() == Approx(INFINITY).epsilon(1e-9));
    REQUIRE_FALSE(Value::t.is_pure_tone());

    // Periodic sawtooth (t % period) should have harmonics
    Value sawtooth = Value::t % Value(2.0);
    REQUIRE_FALSE(sawtooth.is_pure_tone());
    REQUIRE(sawtooth.get_fundamental_frequency() ==
            Approx(0.5).epsilon(1e-9));       // freq = 1/period = 1/2
    REQUIRE_FALSE(sawtooth.is_bandlimited()); // Sawtooth has infinite harmonics

    // sin(2πft) should be pure tone
    Value sine_440 = (Value::t * Value(2 * M_PI * 440)).sin();
    REQUIRE(sine_440.is_pure_tone());
    REQUIRE(sine_440.get_fundamental_frequency() == Approx(440.0).epsilon(1e-6));
    REQUIRE(sine_440.get_max_frequency() == Approx(440.0).epsilon(1e-6));
    REQUIRE(sine_440.is_bandlimited());

    // Harmonics test for square wave approximation
    Value square_approx = sine_440 + (sine_440 * Value(3)).sin() / Value(3);
    REQUIRE_FALSE(square_approx.is_pure_tone());
    REQUIRE(square_approx.get_fundamental_frequency() ==
            Approx(440.0).epsilon(1e-6));
    std::vector<double> harmonics = square_approx.get_harmonics();
    REQUIRE(harmonics.size() >= 2); // Should contain 440Hz and 1320Hz
}

TEST_CASE("Statistical metadata", "[signals][metadata][statistics]")
{
    // Constants should have zero variance, mean = value
    Value constant = Value(5);
    REQUIRE(constant.get_mean_value() == Approx(5.0).epsilon(1e-9));
    REQUIRE(constant.get_variance() == Approx(0.0).epsilon(1e-9));
    REQUIRE(constant.get_rms_value() == Approx(5.0).epsilon(1e-9));
    REQUIRE_FALSE(constant.has_zero_mean());
    REQUIRE(constant.get_probability_positive() == Approx(1.0).epsilon(1e-9));

    // Zero constant should have zero mean
    Value zero = Value(0);
    REQUIRE(zero.has_zero_mean());
    REQUIRE(zero.get_probability_positive() == Approx(0.0).epsilon(1e-9));

    // sin(t) should have zero mean, RMS = 1/√2
    Value sin_t = Value::t.sin();
    REQUIRE(sin_t.has_zero_mean());
    REQUIRE(sin_t.get_mean_value() == Approx(0.0).epsilon(1e-9));
    REQUIRE(sin_t.get_rms_value() == Approx(1.0 / sqrt(2.0)).epsilon(1e-9));
    REQUIRE(sin_t.get_probability_positive() == Approx(0.5).epsilon(1e-9));

    // abs(sin(t)) should have positive mean = 2/π
    Value abs_sin_t = Value::t.sin().abs();
    REQUIRE_FALSE(abs_sin_t.has_zero_mean());
    REQUIRE(abs_sin_t.get_mean_value() == Approx(2.0 / M_PI).epsilon(1e-6));
    REQUIRE(abs_sin_t.get_probability_positive() == Approx(1.0).epsilon(1e-9));

    // Square wave should have zero mean if symmetric
    Value square_wave = Value::t.sin().sign();
    REQUIRE(square_wave.has_zero_mean());
    REQUIRE(square_wave.get_probability_positive() == Approx(0.5).epsilon(1e-9));

    // DC offset should shift mean
    Value offset_sin = Value::t.sin() + Value(3);
    REQUIRE_FALSE(offset_sin.has_zero_mean());
    REQUIRE(offset_sin.get_mean_value() == Approx(3.0).epsilon(1e-9));
}

TEST_CASE("Causality and memory metadata", "[signals][metadata][causality]")
{
    // Basic time parameter should be causal and memoryless
    REQUIRE(Value::t.is_causal());
    REQUIRE(Value::t.is_memoryless());
    REQUIRE(Value::t.get_memory_length() == Approx(0.0).epsilon(1e-9));
    REQUIRE(Value::t.is_time_invariant());

    // Constants should be causal and memoryless
    Value constant = Value(42);
    REQUIRE(constant.is_causal());
    REQUIRE(constant.is_memoryless());
    REQUIRE(constant.get_memory_length() == Approx(0.0).epsilon(1e-9));
    REQUIRE(constant.is_time_invariant());

    // Integration creates infinite memory
    Value integrated = Value::t.integrate();
    REQUIRE(integrated.is_causal());
    REQUIRE_FALSE(integrated.is_memoryless());
    REQUIRE(integrated.get_memory_length() == Approx(INFINITY).epsilon(1e-9));
    REQUIRE(integrated.is_time_invariant());

    // Delay operations
    Value delayed = Value::t.delay(Value(0.5));
    REQUIRE(delayed.is_causal()); // Positive delay is causal
    REQUIRE_FALSE(delayed.is_memoryless());
    REQUIRE(delayed.get_memory_length() == Approx(0.5).epsilon(1e-9));
    REQUIRE(delayed.is_time_invariant());

    // Advance (negative delay) should be non-causal
    Value advanced = Value::t.delay(Value(-0.3));
    REQUIRE_FALSE(advanced.is_causal()); // Negative delay looks into future
    REQUIRE_FALSE(advanced.is_memoryless());
    REQUIRE(advanced.get_memory_length() == Approx(0.3).epsilon(1e-9));

    // Moving average has finite memory
    Value moving_avg = Value::t.moving_average(Value(2.0));
    REQUIRE(moving_avg.is_causal());
    REQUIRE_FALSE(moving_avg.is_memoryless());
    REQUIRE(moving_avg.get_memory_length() == Approx(2.0).epsilon(1e-9));
}

TEST_CASE("Energy and power metadata", "[signals][metadata][energy]")
{
    // Constants should be neither energy nor power signals (except zero)
    Value constant = Value(5);
    REQUIRE_FALSE(constant.is_energy_signal());
    REQUIRE_FALSE(constant.is_power_signal());
    REQUIRE(constant.get_average_power() == Approx(INFINITY).epsilon(1e-9));
    REQUIRE(constant.get_total_energy() == Approx(INFINITY).epsilon(1e-9));

    // Zero signal should be energy signal with zero energy
    Value zero = Value(0);
    REQUIRE(zero.is_energy_signal());
    REQUIRE_FALSE(zero.is_power_signal());
    REQUIRE(zero.is_zero_energy());
    REQUIRE(zero.get_total_energy() == Approx(0.0).epsilon(1e-9));
    REQUIRE(zero.get_average_power() == Approx(0.0).epsilon(1e-9));

    // sin(t) should be power signal with average power = 0.5
    Value sin_t = Value::t.sin();
    REQUIRE_FALSE(sin_t.is_energy_signal());
    REQUIRE(sin_t.is_power_signal());
    REQUIRE_FALSE(sin_t.is_zero_energy());
    REQUIRE(sin_t.get_average_power() == Approx(0.5).epsilon(1e-9));
    REQUIRE(sin_t.get_total_energy() == Approx(INFINITY).epsilon(1e-9));

    // Exponential decay should be energy signal (if properly windowed)
    Value exp_decay = (Value::t * Value(-1)).exp();
    REQUIRE(exp_decay.is_energy_signal());
    REQUIRE_FALSE(exp_decay.is_power_signal());
    REQUIRE(std::isfinite(exp_decay.get_total_energy()));
    REQUIRE(exp_decay.get_average_power() == Approx(0.0).epsilon(1e-9));

    // Periodic signals should be power signals
    Value periodic = Value::t % Value(2.0);
    REQUIRE_FALSE(periodic.is_energy_signal());
    REQUIRE(periodic.is_power_signal());
    REQUIRE(std::isfinite(periodic.get_average_power()));
}

TEST_CASE("Quantization metadata", "[signals][metadata][quantization]")
{
    // Continuous signals should not be quantized
    REQUIRE_FALSE(Value::t.is_quantized());
    REQUIRE_FALSE(Value::t.is_integer_valued());
    REQUIRE_FALSE(Value::t.is_binary());
    REQUIRE(Value::t.get_quantum_step() == Approx(0.0).epsilon(1e-9));

    // Floor function creates integer quantization
    Value floor_t = Value::t.floor();
    REQUIRE(floor_t.is_quantized());
    REQUIRE(floor_t.is_integer_valued());
    REQUIRE_FALSE(floor_t.is_binary());
    REQUIRE(floor_t.get_quantum_step() == Approx(1.0).epsilon(1e-9));

    // Round to quarter creates 0.25 quantization
    Value quarter_round = (Value::t * Value(4)).round() / Value(4);
    REQUIRE(quarter_round.is_quantized());
    REQUIRE_FALSE(quarter_round.is_integer_valued());
    REQUIRE_FALSE(quarter_round.is_binary());
    REQUIRE(quarter_round.get_quantum_step() == Approx(0.25).epsilon(1e-9));

    // Sign function creates ternary quantization {-1, 0, 1}
    Value sign_t = Value::t.sign();
    REQUIRE(sign_t.is_quantized());
    REQUIRE(sign_t.is_integer_valued());
    REQUIRE_FALSE(sign_t.is_binary());
    std::set<double> allowed = sign_t.get_allowed_values();
    REQUIRE(allowed.size() == 3);
    REQUIRE(allowed.count(-1.0) > 0);
    REQUIRE(allowed.count(0.0) > 0);
    REQUIRE(allowed.count(1.0) > 0);

    // Step function creates binary quantization
    Value step_t = Value::t.step();
    REQUIRE(step_t.is_quantized());
    REQUIRE(step_t.is_integer_valued());
    REQUIRE(step_t.is_binary());
    std::set<double> binary_allowed = step_t.get_allowed_values();
    REQUIRE(binary_allowed.size() == 2);
    REQUIRE(binary_allowed.count(0.0) > 0);
    REQUIRE(binary_allowed.count(1.0) > 0);
}

TEST_CASE("Transcendental function metadata", "[signals][metadata][transcendental]")
{
    // sin(t) metadata
    Value sin_t = Value::t.sin();
    REQUIRE_FALSE(sin_t.is_constant());
    REQUIRE(sin_t.get_min() == Approx(-1.0).epsilon(1e-9));
    REQUIRE(sin_t.get_max() == Approx(1.0).epsilon(1e-9));
    REQUIRE(sin_t.is_periodic());
    REQUIRE(sin_t.get_period() == Approx(2 * M_PI).epsilon(1e-6));
    REQUIRE_FALSE(sin_t.is_monotonic());
    REQUIRE(sin_t.is_analytic());
    REQUIRE(sin_t.is_odd());
    REQUIRE_FALSE(sin_t.is_even());

    // cos(t) metadata
    Value cos_t = Value::t.cos();
    REQUIRE_FALSE(cos_t.is_constant());
    REQUIRE(cos_t.get_min() == Approx(-1.0).epsilon(1e-9));
    REQUIRE(cos_t.get_max() == Approx(1.0).epsilon(1e-9));
    REQUIRE(cos_t.is_periodic());
    REQUIRE(cos_t.get_period() == Approx(2 * M_PI).epsilon(1e-6));
    REQUIRE_FALSE(cos_t.is_monotonic());
    REQUIRE(cos_t.is_analytic());
    REQUIRE_FALSE(cos_t.is_odd());
    REQUIRE(cos_t.is_even());

    // exp(t) metadata
    Value exp_t = Value::t.exp();
    REQUIRE_FALSE(exp_t.is_constant());
    REQUIRE(exp_t.get_min() == Approx(0.0).epsilon(1e-9));
    REQUIRE(exp_t.get_max() == Approx(INFINITY).epsilon(1e-9));
    REQUIRE_FALSE(exp_t.is_periodic());
    REQUIRE(exp_t.is_monotonic());
    REQUIRE(exp_t.is_analytic());
    REQUIRE_FALSE(exp_t.is_even());
    REQUIRE_FALSE(exp_t.is_odd());

    // log(t) metadata (for positive t domain)
    Value positive_t = Value::t.abs() + Value(0.1); // Ensure positive domain
    Value log_t      = positive_t.log();
    REQUIRE_FALSE(log_t.is_constant());
    REQUIRE(log_t.is_monotonic());
    REQUIRE(log_t.is_analytic()); // On positive domain
    REQUIRE_FALSE(log_t.is_periodic());

    // sqrt(t) metadata (for non-negative t)
    Value sqrt_t = positive_t.sqrt();
    REQUIRE_FALSE(sqrt_t.is_constant());
    REQUIRE(sqrt_t.get_min() == Approx(sqrt(0.1)).epsilon(1e-9));
    REQUIRE(sqrt_t.is_monotonic());
    REQUIRE(sqrt_t.is_analytic()); // On positive domain
}

TEST_CASE("Composition and chaining metadata", "[signals][metadata][composition]")
{
    // sin(cos(t)) should be bounded [-1, 1] and non-monotonic
    Value sin_cos_t = Value::t.cos().sin();
    REQUIRE_FALSE(sin_cos_t.is_constant());
    REQUIRE(sin_cos_t.get_min() == Approx(-1.0).epsilon(1e-9));
    REQUIRE(sin_cos_t.get_max() == Approx(1.0).epsilon(1e-9));
    REQUIRE_FALSE(sin_cos_t.is_monotonic());
    REQUIRE(sin_cos_t.is_periodic()); // Should inherit periodicity

    // exp(sin(t)) should be bounded [e^(-1), e^1] and periodic
    Value exp_sin_t = Value::t.sin().exp();
    REQUIRE_FALSE(exp_sin_t.is_constant());
    REQUIRE(exp_sin_t.get_min() == Approx(exp(-1.0)).epsilon(1e-6));
    REQUIRE(exp_sin_t.get_max() == Approx(exp(1.0)).epsilon(1e-6));
    REQUIRE(exp_sin_t.is_periodic());
    REQUIRE(exp_sin_t.get_period() == Approx(2 * M_PI).epsilon(1e-6));

    // sin(t^2) should lose periodicity but remain bounded
    Value sin_t_squared = (Value::t * Value::t).sin();
    REQUIRE_FALSE(sin_t_squared.is_constant());
    REQUIRE(sin_t_squared.get_min() == Approx(-1.0).epsilon(1e-9));
    REQUIRE(sin_t_squared.get_max() == Approx(1.0).epsilon(1e-9));
    REQUIRE_FALSE(sin_t_squared.is_periodic()); // t^2 breaks periodicity
    REQUIRE_FALSE(sin_t_squared.is_monotonic());

    // abs(sin(t)) should be non-negative and preserve periodicity
    Value abs_sin_t = Value::t.sin().abs();
    REQUIRE_FALSE(abs_sin_t.is_constant());
    REQUIRE(abs_sin_t.get_min() == Approx(0.0).epsilon(1e-9));
    REQUIRE(abs_sin_t.get_max() == Approx(1.0).epsilon(1e-9));
    REQUIRE(abs_sin_t.is_periodic());
    REQUIRE(abs_sin_t.get_period() ==
            Approx(M_PI).epsilon(1e-6)); // Period halved by abs
    REQUIRE(abs_sin_t.is_even());

    // Integration of sin(t) should lose boundedness but remain periodic in envelope
    Value int_sin_t = Value::t.sin().integrate();
    REQUIRE_FALSE(int_sin_t.is_constant());
    REQUIRE_FALSE(int_sin_t.is_monotonic());
    // Integration of zero-mean periodic should remain bounded
    REQUIRE(std::isfinite(int_sin_t.get_min()));
    REQUIRE(std::isfinite(int_sin_t.get_max()));
}

TEST_CASE("Mathematical identity preservation", "[signals][metadata][identity]")
{
    // sin^2(t) + cos^2(t) = 1
    Value sin_t     = Value::t.sin();
    Value cos_t     = Value::t.cos();
    Value identity1 = sin_t * sin_t + cos_t * cos_t;
    REQUIRE(identity1.is_constant());
    REQUIRE(identity1.get_min() == Approx(1.0).epsilon(1e-9));
    REQUIRE(identity1.get_max() == Approx(1.0).epsilon(1e-9));
    REQUIRE(identity1.as_float() == Approx(1.0).epsilon(1e-9));

    // exp(log(x)) = x for positive x
    Value positive_signal  = Value::t.abs() + Value(1);
    Value exp_log_identity = positive_signal.log().exp();
    // Should preserve all metadata of positive_signal
    REQUIRE(exp_log_identity.get_min() ==
            Approx(positive_signal.get_min()).epsilon(1e-9));
    REQUIRE(exp_log_identity.get_max() ==
            Approx(positive_signal.get_max()).epsilon(1e-9));
    REQUIRE(exp_log_identity.is_monotonic() == positive_signal.is_monotonic());

    // log(exp(x)) = x
    Value log_exp_identity = Value::t.exp().log();
    REQUIRE(log_exp_identity.get_min() == Approx(Value::t.get_min()).epsilon(1e-9));
    REQUIRE(log_exp_identity.is_monotonic() == Value::t.is_monotonic());

    // sqrt(x^2) = abs(x)
    Value sqrt_square_identity = (Value::t * Value::t).sqrt();
    Value abs_t                = Value::t.abs();
    REQUIRE(sqrt_square_identity.get_min() == Approx(abs_t.get_min()).epsilon(1e-9));
    REQUIRE(sqrt_square_identity.get_max() == Approx(abs_t.get_max()).epsilon(1e-9));
    REQUIRE(sqrt_square_identity.is_even() == abs_t.is_even());

    // abs(abs(x)) = abs(x)
    Value double_abs = Value::t.abs().abs();
    Value single_abs = Value::t.abs();
    REQUIRE(double_abs.get_min() == Approx(single_abs.get_min()).epsilon(1e-9));
    REQUIRE(double_abs.get_max() == Approx(single_abs.get_max()).epsilon(1e-9));
    REQUIRE(double_abs.is_even() == single_abs.is_even());
}

TEST_CASE("Domain validation and error handling",
          "[signals][metadata][error_handling]")
{
    // log of negative values should create error metadata
    Value negative_signal = Value::t - Value(10); // Creates negative values
    Value log_negative    = negative_signal.log();
    REQUIRE(log_negative.has_domain_error());
    REQUIRE(log_negative.get_error_message() ==
            "log domain error: non-positive input");

    // sqrt of negative values should create error metadata
    Value sqrt_negative = negative_signal.sqrt();
    REQUIRE(sqrt_negative.has_domain_error());
    REQUIRE(sqrt_negative.get_error_message() ==
            "sqrt domain error: negative input");

    // Division by zero should create error metadata
    Value zero_signal = Value::t - Value::t; // Always zero
    Value div_by_zero = Value::t / zero_signal;
    REQUIRE(div_by_zero.has_domain_error());
    REQUIRE(div_by_zero.get_error_message() == "division by zero");

    // asin of values outside [-1, 1] should create error
    Value large_signal = Value::t + Value(2); // Values outside [-1, 1]
    Value asin_large   = large_signal.asin();
    REQUIRE(asin_large.has_domain_error());

    // Valid operations should not have errors
    Value valid_log = (Value::t.abs() + Value(1)).log();
    REQUIRE_FALSE(valid_log.has_domain_error());

    Value valid_sqrt = Value::t.abs().sqrt();
    REQUIRE_FALSE(valid_sqrt.has_domain_error());
}

TEST_CASE("Optimization validation metadata", "[signals][metadata][optimization]")
{
    // Constant folding should be detected
    Value const_expr = Value(3) + Value(4) * Value(2);
    REQUIRE(const_expr.is_constant());
    REQUIRE(const_expr.can_be_constant_folded());
    REQUIRE(const_expr.as_float() == Approx(11.0).epsilon(1e-9));

    // Dead code elimination: x * 0 = 0
    Value dead_code = Value::t * Value(0);
    REQUIRE(dead_code.is_constant());
    REQUIRE(dead_code.can_be_eliminated());
    REQUIRE(dead_code.as_float() == Approx(0.0).epsilon(1e-9));

    // Identity operations: x * 1 = x, x + 0 = x
    Value identity_mult = Value::t * Value(1);
    REQUIRE(identity_mult.can_be_simplified());
    REQUIRE(identity_mult.simplifies_to(Value::t));

    Value identity_add = Value::t + Value(0);
    REQUIRE(identity_add.can_be_simplified());
    REQUIRE(identity_add.simplifies_to(Value::t));

    // Strength reduction: x^2 can use multiplication instead of pow
    Value square_pow  = Value::t.pow(Value(2));
    Value square_mult = Value::t * Value::t;
    REQUIRE(square_pow.can_use_strength_reduction());
    REQUIRE(square_pow.strength_reduces_to(square_mult));

    // Common subexpression elimination
    Value expr1 = Value::t.sin() + Value::t.cos();
    Value expr2 = Value::t.sin() * Value(2) + Value::t.cos();
    REQUIRE(expr1.shares_subexpression_with(expr2));
    REQUIRE(expr1.get_common_subexpressions(expr2).size() > 0);
}

TEST_CASE("Property interaction matrix", "[signals][metadata][interaction]")
{
    // Constant × Periodic → Should be false (constants can't be periodic with period
    // > 0)
    Value constant = Value(5);
    REQUIRE(constant.is_constant());
    REQUIRE_FALSE(constant.is_periodic());
    // Validate this is impossible combination
    REQUIRE_FALSE(constant.is_constant() && constant.is_periodic() &&
                  constant.get_period() > 0);

    // Monotonic × Periodic → Only possible for constant signals
    Value periodic_mono = Value::t % Value(2.0);
    REQUIRE(periodic_mono.is_periodic());
    REQUIRE_FALSE(periodic_mono.is_monotonic());
    // Non-constant periodic signals cannot be monotonic
    REQUIRE_FALSE(!periodic_mono.is_constant() && periodic_mono.is_periodic() &&
                  periodic_mono.is_monotonic());

    // Even × Odd → Only possible for zero signal
    Value zero = Value(0);
    REQUIRE(zero.is_even());
    REQUIRE(zero.is_odd()); // Zero is both even and odd

    Value nonzero_even = Value::t * Value::t;
    REQUIRE(nonzero_even.is_even());
    REQUIRE_FALSE(nonzero_even.is_odd());

    Value odd = Value::t;
    REQUIRE(odd.is_odd());
    REQUIRE_FALSE(odd.is_even());

    // Smooth × Quantized → Generally incompatible except for constants
    Value smooth_signal = Value::t.sin();
    REQUIRE(smooth_signal.is_smooth());
    REQUIRE_FALSE(smooth_signal.is_quantized());

    Value quantized_signal = Value::t.floor();
    REQUIRE(quantized_signal.is_quantized());
    REQUIRE_FALSE(quantized_signal.is_smooth());

    // Constant can be both smooth and quantized
    REQUIRE(constant.is_smooth());
    REQUIRE(constant.is_quantized()); // Single value is quantized

    // Energy_signal × Power_signal → Mutually exclusive except zero
    REQUIRE(zero.is_energy_signal());
    REQUIRE_FALSE(zero.is_power_signal()); // Zero has no power

    Value power_signal = Value::t.sin();
    REQUIRE(power_signal.is_power_signal());
    REQUIRE_FALSE(power_signal.is_energy_signal());

    // Causal × Has_memory → Independent properties
    Value causal_memoryless = Value::t;
    REQUIRE(causal_memoryless.is_causal());
    REQUIRE(causal_memoryless.is_memoryless());

    Value causal_memory = Value::t.integrate();
    REQUIRE(causal_memory.is_causal());
    REQUIRE_FALSE(causal_memory.is_memoryless());
}
