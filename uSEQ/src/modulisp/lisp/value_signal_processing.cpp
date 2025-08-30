#include "value_signal_processing.h"
#include "value.h"
#include "signal_metadata.h"
#include "../../utils/error_messages.h"
#include "../../utils/log.h"
#include <cmath>
#include <algorithm>
#include <limits>

// Helper function for power metadata - complex bounds calculation
SignalMetadata combine_metadata_power(const std::optional<SignalMetadata>& base_meta,
                                     const std::optional<SignalMetadata>& exp_meta) {
    SignalMetadata result;
    
    // If neither has metadata, return default (constant)
    if (!base_meta && !exp_meta) {
        return result;
    }
    
    // Extract metadata or use defaults
    SignalMetadata base = base_meta.value_or(SignalMetadata{});
    SignalMetadata exp = exp_meta.value_or(SignalMetadata{});
    
    // Result is non-constant if either operand is non-constant
    result.is_const = base.is_const && exp.is_const;
    
    // Power bounds are complex - conservative approach
    // For positive bases: base^exp has predictable bounds
    // For negative bases with non-integer exponents: complex numbers (avoid)
    // For now, use conservative bounds
    if (base.min_val >= 0) {
        // Positive base case - more predictable
        std::vector<double> powers = {
            std::pow(base.min_val, exp.min_val),
            std::pow(base.min_val, exp.max_val),
            std::pow(base.max_val, exp.min_val),
            std::pow(base.max_val, exp.max_val)
        };
        result.min_val = *std::min_element(powers.begin(), powers.end());
        result.max_val = *std::max_element(powers.begin(), powers.end());
    } else {
        // Mixed sign base - conservative bounds
        result.min_val = -INFINITY;
        result.max_val = INFINITY;
    }
    
    result.min_inclusive = true; // Conservative
    result.max_inclusive = true; // Conservative
    
    // Power operations generally break periodicity and monotonicity
    result.is_periodic = false;
    result.period = 0.0;
    result.phase_offset = 0.0;
    result.phase_locked = false;
    result.is_monotonic = false;
    result.is_monotonic_increasing = false;
    result.is_monotonic_decreasing = false;
    
    // Power operations preserve smoothness for smooth operands (except for non-integer powers of negative bases)
    // For simplicity, we preserve smoothness if both operands are smooth and base is non-negative
    if (base.min_val >= 0) {
        result.is_continuous = base.is_continuous && exp.is_continuous;
        result.is_smooth = base.is_smooth && exp.is_smooth;
        result.is_stepped = base.is_stepped || exp.is_stepped;
        result.is_linear_segments = false; // power destroys linearity
    } else {
        // Conservative approach for potentially negative bases
        result.is_continuous = false;
        result.is_smooth = false;
        result.is_stepped = true;
        result.is_linear_segments = false;
    }
    
    // Time bounds: intersection of both time domains
    result.time_start = std::max(base.time_start, exp.time_start);
    result.time_end = std::min(base.time_end, exp.time_end);
    result.is_causal = base.is_causal && exp.is_causal;
    result.is_memoryless = base.is_memoryless && exp.is_memoryless;
    result.memory_length = std::max(base.memory_length, exp.memory_length);
    
    // Zero crossing analysis becomes very complex - conservative approach
    result.has_zero_crossings = true; // power can create new zero crossings
    result.zero_crossing_rate = 0.0; // unknown without full analysis
    result.zero_crossing_locations_known = false;
    
    // Threshold and range analysis capabilities
    result.supports_threshold_queries = base.supports_threshold_queries && exp.supports_threshold_queries;
    result.min_threshold_resolution = std::max(base.min_threshold_resolution, exp.min_threshold_resolution);
    result.supports_range_queries = base.supports_range_queries && exp.supports_range_queries;
    result.range_query_resolution = std::max(base.range_query_resolution, exp.range_query_resolution);
    
    // Extrema analysis
    result.has_local_extrema = true; // power typically creates new extrema
    result.extrema_locations_known = false; // would need recomputation
    result.extrema_detection_threshold = std::max(base.extrema_detection_threshold, exp.extrema_detection_threshold);
    
    // Interpolation: use most restrictive type
    result.interpolation_type = static_cast<SignalMetadata::InterpolationType>(
        std::max(static_cast<int>(base.interpolation_type), static_cast<int>(exp.interpolation_type))
    );
    
    // Quantization properties
    result.is_quantized = base.is_quantized || exp.is_quantized;
    result.is_integer_valued = base.is_integer_valued && exp.is_integer_valued;
    result.quantum_step = (base.is_quantized && exp.is_quantized) ? 
                         std::min(base.quantum_step, exp.quantum_step) : 0.0;
    
    // Domain validation
    result.has_domain_error = base.has_domain_error || exp.has_domain_error;
    if (!result.has_domain_error && result.time_start > result.time_end) {
        result.has_domain_error = true;
        result.error_message = "Empty time domain after power";
    } else if (base.min_val < 0 && !exp.is_integer_valued) {
        result.has_domain_error = true;
        result.error_message = "Complex result from negative base and non-integer exponent";
    }
    
    return result;
}

namespace ValueSignalProcessing {

    ////////////////////////////////////////////////////////////////////////////////
    /// TRANSCENDENTAL AND MATHEMATICAL FUNCTIONS
    /// ////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

    Value sin(const Value& v) {
        // Only apply to numeric values and signals
        if (!v.is_number() && v.type != Value::SIGNAL) {
            println("sin: " + INVALID_BIN_OP);
            return Value::error();
        }
        
        // Get the numeric value
        double input_val = (v.type == Value::SIGNAL || v.type == Value::FLOAT) ? v.stack_data.f : static_cast<double>(v.stack_data.i);
        Value result = Value(std::sin(input_val));
        
        // Apply signal metadata if this is a signal or has signal metadata
        if (v.type == Value::SIGNAL || v.signal_metadata.has_value()) {
            result.type = Value::SIGNAL;
            
            SignalMetadata metadata;
            // Sin is non-constant if input is non-constant
            metadata.is_const = v.is_constant();
            
            // Sin output is always bounded [-1, 1]
            metadata.min_val = -1.0;
            metadata.max_val = 1.0;
            metadata.min_inclusive = true;
            metadata.max_inclusive = true;
            
            // Sin is periodic with period 2π
            metadata.is_periodic = true;
            metadata.period = 2.0 * M_PI;
            
            // Sin is not monotonic over its full period
            metadata.is_monotonic = false;
            
            // Energy and power properties for sin(t)
            // sin(t) is a periodic signal with regular zero crossings
            
            result.signal_metadata = metadata;
        }
        
        return result;
    }

    Value cos(const Value& v) {
        // Only apply to numeric values and signals
        if (!v.is_number() && v.type != Value::SIGNAL) {
            println("cos: " + INVALID_BIN_OP);
            return Value::error();
        }
        
        // Get the numeric value
        double input_val = (v.type == Value::SIGNAL || v.type == Value::FLOAT) ? v.stack_data.f : static_cast<double>(v.stack_data.i);
        Value result = Value(std::cos(input_val));
        
        // Apply signal metadata if this is a signal or has signal metadata
        if (v.type == Value::SIGNAL || v.signal_metadata.has_value()) {
            result.type = Value::SIGNAL;
            
            SignalMetadata metadata;
            // Cos is non-constant if input is non-constant
            metadata.is_const = v.is_constant();
            
            // Cos output is always bounded [-1, 1]
            metadata.min_val = -1.0;
            metadata.max_val = 1.0;
            metadata.min_inclusive = true;
            metadata.max_inclusive = true;
            
            // Cos is periodic with period 2π
            metadata.is_periodic = true;
            metadata.period = 2.0 * M_PI;
            
            // Cos is not monotonic over its full period
            metadata.is_monotonic = false;
            
            // Energy and power properties for cos(t)
            // cos(t) is a periodic signal with no zero crossings
            
            result.signal_metadata = metadata;
        }
        
        return result;
    }

    Value exp(const Value& v) {
        // Only apply to numeric values and signals
        if (!v.is_number() && v.type != Value::SIGNAL) {
            println("exp: " + INVALID_BIN_OP);
            return Value::error();
        }
        
        // Get the numeric value
        double input_val = (v.type == Value::SIGNAL || v.type == Value::FLOAT) ? v.stack_data.f : static_cast<double>(v.stack_data.i);
        Value result = Value(std::exp(input_val));
        
        // Apply signal metadata if this is a signal or has signal metadata
        if (v.type == Value::SIGNAL || v.signal_metadata.has_value()) {
            result.type = Value::SIGNAL;
            
            SignalMetadata metadata;
            // Exp is non-constant if input is non-constant
            metadata.is_const = v.is_constant();
            
            // Exp output is always positive, unbounded above
            metadata.min_val = 0.0;
            metadata.max_val = INFINITY;
            metadata.min_inclusive = false; // Never actually reaches 0
            metadata.max_inclusive = true;
            
            // Exp is not periodic
            metadata.is_periodic = false;
            metadata.period = 0.0;
            
            // Exp is monotonically increasing
            metadata.is_monotonic = true;
            
            // Energy and power properties for exp(t) depend on the input
            // For exp(-t) where t>=0: energy signal (decays to zero)
            // For exp(t): not an energy signal (grows to infinity)
            // For exp(constant): neither energy nor power signal (constant output)
            
            // Check if input represents a decaying exponential (negative coefficient)
            // This is a simplified heuristic - real implementation would need more analysis
            if (v.signal_metadata.has_value() && v.signal_metadata->min_val < 0 && v.signal_metadata->max_val <= 0) {
                // Likely exp(-at) where a > 0, which is an energy signal
                // exp(input) with negative coefficient decays to zero
            } else {
                // General case: growing exponential or mixed
                // exp(input) with positive or zero coefficient grows without bound
            }
            
            result.signal_metadata = metadata;
        }
        
        return result;
    }

    Value log(const Value& v) {
        // Only apply to numeric values and signals
        if (!v.is_number() && v.type != Value::SIGNAL) {
            println("log: " + INVALID_BIN_OP);
            return Value::error();
        }
        
        // Get the numeric value and check domain
        double input_val = (v.type == Value::SIGNAL || v.type == Value::FLOAT) ? v.stack_data.f : static_cast<double>(v.stack_data.i);
        
        // Log requires positive input
        if (input_val <= 0.0) {
            println("log: domain error - non-positive input");
            return Value::error();
        }
        
        Value result = Value(std::log(input_val));
        
        // Apply signal metadata if this is a signal or has signal metadata
        if (v.type == Value::SIGNAL || v.signal_metadata.has_value()) {
            result.type = Value::SIGNAL;
            
            SignalMetadata metadata;
            // Log is non-constant if input is non-constant
            metadata.is_const = v.is_constant();
            
            // Log output is unbounded
            metadata.min_val = -INFINITY;
            metadata.max_val = INFINITY;
            metadata.min_inclusive = true;
            metadata.max_inclusive = true;
            
            // Log is not periodic
            metadata.is_periodic = false;
            metadata.period = 0.0;
            
            // Log is monotonically increasing
            metadata.is_monotonic = true;
            
            result.signal_metadata = metadata;
        }
        
        return result;
    }

    Value sqrt(const Value& v) {
        // Only apply to numeric values and signals
        if (!v.is_number() && v.type != Value::SIGNAL) {
            println("sqrt: " + INVALID_BIN_OP);
            return Value::error();
        }
        
        // Get the numeric value and check domain
        double input_val = (v.type == Value::SIGNAL || v.type == Value::FLOAT) ? v.stack_data.f : static_cast<double>(v.stack_data.i);
        
        // Sqrt requires non-negative input
        if (input_val < 0.0) {
            println("sqrt: domain error - negative input");
            return Value::error();
        }
        
        Value result = Value(std::sqrt(input_val));
        
        // Apply signal metadata if this is a signal or has signal metadata
        if (v.type == Value::SIGNAL || v.signal_metadata.has_value()) {
            result.type = Value::SIGNAL;
            
            SignalMetadata metadata;
            // Sqrt is non-constant if input is non-constant
            metadata.is_const = v.is_constant();
            
            // Sqrt output is non-negative, unbounded above
            metadata.min_val = 0.0;
            metadata.max_val = INFINITY;
            metadata.min_inclusive = true;
            metadata.max_inclusive = true;
            
            // Sqrt is not periodic
            metadata.is_periodic = false;
            metadata.period = 0.0;
            
            // Sqrt is monotonically increasing
            metadata.is_monotonic = true;
            
            result.signal_metadata = metadata;
        }
        
        return result;
    }

    Value abs(const Value& v) {
        // Only apply to numeric values and signals
        if (!v.is_number() && v.type != Value::SIGNAL) {
            println("abs: " + INVALID_BIN_OP);
            return Value::error();
        }
        
        // Get the numeric value
        double input_val = (v.type == Value::SIGNAL || v.type == Value::FLOAT) ? v.stack_data.f : static_cast<double>(v.stack_data.i);
        Value result = Value(std::abs(input_val));
        
        // Apply signal metadata if this is a signal or has signal metadata
        if (v.type == Value::SIGNAL || v.signal_metadata.has_value()) {
            result.type = Value::SIGNAL;
            
            SignalMetadata metadata;
            // Abs is non-constant if input is non-constant
            metadata.is_const = v.is_constant();
            
            // Abs output is always non-negative
            metadata.min_val = 0.0;
            metadata.max_val = INFINITY;
            metadata.min_inclusive = true;
            metadata.max_inclusive = true;
            
            // Abs generally breaks periodicity (except for even functions)
            metadata.is_periodic = false;
            metadata.period = 0.0;
            
            // Abs is not monotonic (creates V-shape)
            metadata.is_monotonic = false;
            
            result.signal_metadata = metadata;
        }
        
        return result;
    }

    Value floor(const Value& v) {
        // Only apply to numeric values and signals
        if (!v.is_number() && v.type != Value::SIGNAL) {
            println("floor: " + INVALID_BIN_OP);
            return Value::error();
        }
        
        // Get the numeric value
        double input_val = (v.type == Value::SIGNAL || v.type == Value::FLOAT) ? v.stack_data.f : static_cast<double>(v.stack_data.i);
        Value result = Value(std::floor(input_val));
        
        // Apply signal metadata if this is a signal or has signal metadata
        if (v.type == Value::SIGNAL || v.signal_metadata.has_value()) {
            result.type = Value::SIGNAL;
            
            SignalMetadata metadata;
            // Floor is non-constant if input is non-constant
            metadata.is_const = v.is_constant();
            
            // Floor output bounds depend on input bounds
            if (v.signal_metadata.has_value()) {
                metadata.min_val = std::floor(v.signal_metadata->min_val);
                metadata.max_val = std::floor(v.signal_metadata->max_val);
            } else {
                metadata.min_val = -INFINITY;
                metadata.max_val = INFINITY;
            }
            metadata.min_inclusive = true;
            metadata.max_inclusive = true;
            
            // Floor generally breaks periodicity
            metadata.is_periodic = false;
            metadata.period = 0.0;
            
            // Floor creates step function (not strictly monotonic)
            metadata.is_monotonic = false;
            
            result.signal_metadata = metadata;
        }
        
        return result;
    }

    Value round(const Value& v) {
        // Only apply to numeric values and signals
        if (!v.is_number() && v.type != Value::SIGNAL) {
            println("round: " + INVALID_BIN_OP);
            return Value::error();
        }
        
        // Get the numeric value
        double input_val = (v.type == Value::SIGNAL || v.type == Value::FLOAT) ? v.stack_data.f : static_cast<double>(v.stack_data.i);
        Value result = Value(std::round(input_val));
        
        // Apply signal metadata if this is a signal or has signal metadata
        if (v.type == Value::SIGNAL || v.signal_metadata.has_value()) {
            result.type = Value::SIGNAL;
            
            SignalMetadata metadata;
            // Round is non-constant if input is non-constant
            metadata.is_const = v.is_constant();
            
            // Round output bounds depend on input bounds
            if (v.signal_metadata.has_value()) {
                metadata.min_val = std::round(v.signal_metadata->min_val);
                metadata.max_val = std::round(v.signal_metadata->max_val);
            } else {
                metadata.min_val = -INFINITY;
                metadata.max_val = INFINITY;
            }
            metadata.min_inclusive = true;
            metadata.max_inclusive = true;
            
            // Round generally breaks periodicity
            metadata.is_periodic = false;
            metadata.period = 0.0;
            
            // Round creates step function (not strictly monotonic)
            metadata.is_monotonic = false;
            
            result.signal_metadata = metadata;
        }
        
        return result;
    }

    Value sign(const Value& v) {
        // Only apply to numeric values and signals
        if (!v.is_number() && v.type != Value::SIGNAL) {
            println("sign: " + INVALID_BIN_OP);
            return Value::error();
        }
        
        // Get the numeric value
        double input_val = (v.type == Value::SIGNAL || v.type == Value::FLOAT) ? v.stack_data.f : static_cast<double>(v.stack_data.i);
        double sign_val = (input_val > 0.0) ? 1.0 : (input_val < 0.0) ? -1.0 : 0.0;
        Value result = Value(sign_val);
        
        // Apply signal metadata if this is a signal or has signal metadata
        if (v.type == Value::SIGNAL || v.signal_metadata.has_value()) {
            result.type = Value::SIGNAL;
            
            SignalMetadata metadata;
            // Sign is non-constant if input is non-constant
            metadata.is_const = v.is_constant();
            
            // Sign output is bounded to {-1, 0, 1}
            metadata.min_val = -1.0;
            metadata.max_val = 1.0;
            metadata.min_inclusive = true;
            metadata.max_inclusive = true;
            
            // Sign generally breaks periodicity
            metadata.is_periodic = false;
            metadata.period = 0.0;
            
            // Sign is not monotonic (step function)
            metadata.is_monotonic = false;
            
            result.signal_metadata = metadata;
        }
        
        return result;
    }

    Value step(const Value& v) {
        // Only apply to numeric values and signals
        if (!v.is_number() && v.type != Value::SIGNAL) {
            println("step: " + INVALID_BIN_OP);
            return Value::error();
        }
        
        // Get the numeric value
        double input_val = (v.type == Value::SIGNAL || v.type == Value::FLOAT) ? v.stack_data.f : static_cast<double>(v.stack_data.i);
        double step_val = (input_val >= 0.0) ? 1.0 : 0.0;
        Value result = Value(step_val);
        
        // Apply signal metadata if this is a signal or has signal metadata
        if (v.type == Value::SIGNAL || v.signal_metadata.has_value()) {
            result.type = Value::SIGNAL;
            
            SignalMetadata metadata;
            // Step is non-constant if input is non-constant
            metadata.is_const = v.is_constant();
            
            // Step output is bounded to {0, 1}
            metadata.min_val = 0.0;
            metadata.max_val = 1.0;
            metadata.min_inclusive = true;
            metadata.max_inclusive = true;
            
            // Step generally breaks periodicity
            metadata.is_periodic = false;
            metadata.period = 0.0;
            
            // Step is not monotonic (step function)
            metadata.is_monotonic = false;
            
            result.signal_metadata = metadata;
        }
        
        return result;
    }

    Value pow(const Value& base, const Value& exponent) {
        // Handle unit types
        if (base.type == Value::UNIT || exponent.type == Value::UNIT) {
            return exponent.type == Value::UNIT ? exponent : base;
        }

        // Both operands must be numeric or signal
        if (!base.is_number() && base.type != Value::SIGNAL) {
            println(INVALID_BIN_OP);
            return Value();
        }
        if (!exponent.is_number() && exponent.type != Value::SIGNAL) {
            println(INVALID_BIN_OP);
            return Value();
        }

        // Check if we need signal metadata (either operand has metadata)
        bool has_signal_metadata = base.signal_metadata.has_value() || exponent.signal_metadata.has_value();
        
        // Compute the power using std::pow with float promotion
        double base_val = (base.type == Value::SIGNAL || base.type == Value::FLOAT) ? base.stack_data.f : static_cast<double>(base.stack_data.i);
        double exp_val = (exponent.type == Value::SIGNAL || exponent.type == Value::FLOAT) ? 
                         exponent.stack_data.f : static_cast<double>(exponent.stack_data.i);
        
        Value result = Value(std::pow(base_val, exp_val));
        
        // Apply signal metadata if needed
        if (has_signal_metadata) {
            result.type = Value::SIGNAL;
            result.signal_metadata = combine_metadata_power(base.signal_metadata, exponent.signal_metadata);
        }
        
        return result;
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// SIGNAL PROCESSING FUNCTIONS
    /// ////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

    Value integrate(const Value& v) {
        // Only apply to numeric values and signals
        if (!v.is_number() && v.type != Value::SIGNAL) {
            println("integrate: " + INVALID_BIN_OP);
            return Value::error();
        }
        
        // Get the numeric value
        double input_val = (v.type == Value::SIGNAL || v.type == Value::FLOAT) ? v.stack_data.f : static_cast<double>(v.stack_data.i);
        
        // For constants, integration results in a linear ramp: integral(c) = c*t
        // For time-varying signals, this is a conceptual integration
        Value result;
        
        if (v.is_constant()) {
            // For constants, create a linear ramp signal: c*t
            result = Value::t * Value(input_val);
        } else {
            // For time-varying signals, represent as integrated signal
            result = Value(input_val); // Placeholder - actual integration would need history
            result.type = Value::SIGNAL;
            
            SignalMetadata metadata;
            // Integration makes non-constant signals
            metadata.is_const = false;
            
            // Integration affects bounds - for bounded input, may become unbounded
            if (v.signal_metadata.has_value()) {
                // Conservative approach: integration can grow without bound
                metadata.min_val = -INFINITY;
                metadata.max_val = INFINITY;
            } else {
                metadata.min_val = -INFINITY;
                metadata.max_val = INFINITY;
            }
            metadata.min_inclusive = true;
            metadata.max_inclusive = true;
            
            // Integration typically breaks periodicity (except for zero-mean periodic signals)
            metadata.is_periodic = false;
            metadata.period = 0.0;
            
            // Integration generally breaks monotonicity
            metadata.is_monotonic = false;
            
            // Integration increases smoothness
            metadata.is_smooth = true;
            metadata.is_continuous = true;
            
            // Integration creates infinite memory and preserves causality
            metadata.is_causal = true;
            metadata.is_memoryless = false;
            metadata.memory_length = INFINITY;
            
            result.signal_metadata = metadata;
        }
        
        return result;
    }

    Value delay(const Value& v, const Value& time) {
        // Only apply to numeric values and signals
        if (!v.is_number() && v.type != Value::SIGNAL) {
            println("delay: " + INVALID_BIN_OP);
            return Value::error();
        }
        
        // Time parameter must be numeric
        if (!time.is_number()) {
            println("delay: time parameter must be numeric");
            return Value::error();
        }
        
        double delay_time = time.as_float();
        
        // Get the input value
        double input_val = (v.type == Value::SIGNAL || v.type == Value::FLOAT) ? v.stack_data.f : static_cast<double>(v.stack_data.i);
        
        // For constants, delay doesn't change the value
        if (v.is_constant()) {
            return v; // Constants are unaffected by delay
        }
        
        // For time-varying signals, create delayed version
        Value result = Value(input_val); 
        result.type = Value::SIGNAL;
        
        SignalMetadata metadata;
        
        // Inherit basic properties from input
        if (v.signal_metadata.has_value()) {
            metadata = *v.signal_metadata;
        } else {
            // Default metadata for numeric input being treated as signal
            metadata.is_const = false;
            metadata.min_val = -INFINITY;
            metadata.max_val = INFINITY;
            metadata.is_smooth = true;
            metadata.is_continuous = true;
        }
        
        // Delay affects causality
        metadata.is_causal = (delay_time >= 0.0); // Positive delay is causal, negative is not
        metadata.is_memoryless = false;
        metadata.memory_length = std::abs(delay_time);
        
        // Delay preserves all other signal properties (bounds, periodicity, smoothness, etc.)
        // No changes needed to these properties
        
        result.signal_metadata = metadata;
        
        return result;
    }

    Value moving_average(const Value& v, const Value& window) {
        // Only apply to numeric values and signals
        if (!v.is_number() && v.type != Value::SIGNAL) {
            println("moving_average: " + INVALID_BIN_OP);
            return Value::error();
        }
        
        // Window parameter must be numeric and positive
        if (!window.is_number()) {
            println("moving_average: window parameter must be numeric");
            return Value::error();
        }
        
        double window_size = window.as_float();
        if (window_size <= 0.0) {
            println("moving_average: window size must be positive");
            return Value::error();
        }
        
        // Get the input value
        double input_val = (v.type == Value::SIGNAL || v.type == Value::FLOAT) ? v.stack_data.f : static_cast<double>(v.stack_data.i);
        
        // For constants, moving average returns the same constant
        if (v.is_constant()) {
            return v; // Moving average of constant is the constant
        }
        
        // For time-varying signals, create averaged version
        Value result = Value(input_val);
        result.type = Value::SIGNAL;
        
        SignalMetadata metadata;
        
        // Moving average affects signal properties
        metadata.is_const = false;
        
        // Moving average preserves bounds for bounded signals
        if (v.signal_metadata.has_value()) {
            metadata.min_val = v.signal_metadata->min_val;
            metadata.max_val = v.signal_metadata->max_val;
            metadata.min_inclusive = v.signal_metadata->min_inclusive;
            metadata.max_inclusive = v.signal_metadata->max_inclusive;
        } else {
            metadata.min_val = -INFINITY;
            metadata.max_val = INFINITY;
            metadata.min_inclusive = true;
            metadata.max_inclusive = true;
        }
        
        // Moving average generally breaks periodicity (unless window is multiple of period)
        metadata.is_periodic = false;
        metadata.period = 0.0;
        
        // Moving average generally breaks monotonicity
        metadata.is_monotonic = false;
        
        // Moving average increases smoothness (smoothing operation)
        metadata.is_smooth = true;
        metadata.is_continuous = true;
        
        // Moving average creates finite memory and preserves causality
        metadata.is_causal = true;
        metadata.is_memoryless = false;
        metadata.memory_length = window_size;
        
        result.signal_metadata = metadata;
        
        return result;
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// AUTOMATION ANALYSIS METHODS
    /// ////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

    // Zero Crossing Analysis

    std::vector<double> find_zero_crossings(const Value& v, double start_time, double end_time) {
        std::vector<double> crossings;
        
        // Constants: only cross zero if they are exactly zero
        if (v.is_constant()) {
            double val = v.as_float();
            if (std::abs(val) < 1e-12) { // Treat very small values as zero
                // Zero constant is always at zero - but doesn't "cross"
                return crossings; // Empty vector - no crossings
            }
            return crossings; // Non-zero constants never cross zero
        }
        
        // For the time parameter 't', it crosses zero at t=0
        if (v.type == Value::SIGNAL && v.signal_metadata.has_value()) {
            const auto& meta = *v.signal_metadata;
            if (meta.has_zero_crossings && meta.zero_crossing_locations_known) {
                // Special case for 't' - crosses zero at t=0
                if (&v == &Value::t && start_time <= 0.0 && end_time >= 0.0) {
                    crossings.push_back(0.0);
                }
            }
        }
        
        // For other time-varying signals, this would require actual signal evaluation
        // For now, return empty vector as we don't have signal evaluation infrastructure
        return crossings;
    }

    double get_next_zero_crossing(const Value& v, double from_time) {
        // Constants never have zero crossings after any time
        if (v.is_constant()) {
            return std::numeric_limits<double>::infinity(); // No future zero crossings
        }
        
        // For the time parameter 't'
        if (v.type == Value::SIGNAL && v.signal_metadata.has_value()) {
            const auto& meta = *v.signal_metadata;
            if (meta.has_zero_crossings && meta.zero_crossing_locations_known) {
                // Special case for 't' - crosses zero at t=0
                if (&v == &Value::t) {
                    return (from_time < 0.0) ? 0.0 : std::numeric_limits<double>::infinity();
                }
            }
        }
        
        // For other signals, would require signal evaluation
        return std::numeric_limits<double>::infinity();
    }

    bool has_zero_crossings_in_range(const Value& v, double start_time, double end_time) {
        // Constants: only if they are exactly zero (but that's not a "crossing")
        if (v.is_constant()) {
            return false; // Constants don't cross zero
        }
        
        // Check metadata for zero crossing information
        if (v.type == Value::SIGNAL && v.signal_metadata.has_value()) {
            const auto& meta = *v.signal_metadata;
            if (meta.has_zero_crossings) {
                // Special case for 't' - crosses at t=0
                if (&v == &Value::t) {
                    return (start_time <= 0.0 && end_time >= 0.0);
                }
                // For other signals with zero crossings, be conservative
                return true;
            }
            return false;
        }
        
        return false;
    }

    // Threshold Analysis

    std::vector<double> find_threshold_crossings(const Value& v, double threshold, double start_time, double end_time) {
        std::vector<double> crossings;
        
        // Constants never cross thresholds
        if (v.is_constant()) {
            return crossings;
        }
        
        // For time parameter 't', it crosses any threshold at that time value
        if (v.type == Value::SIGNAL && v.signal_metadata.has_value()) {
            if (&v == &Value::t) {
                // 't' crosses threshold at t = threshold
                if (start_time <= threshold && end_time >= threshold) {
                    crossings.push_back(threshold);
                }
            }
        }
        
        // For other signals, would need evaluation infrastructure
        return crossings;
    }

    double get_next_threshold_crossing(const Value& v, double threshold, double from_time, bool rising_edge) {
        // Constants never have threshold crossings
        if (v.is_constant()) {
            return std::numeric_limits<double>::infinity();
        }
        
        // For time parameter 't'
        if (v.type == Value::SIGNAL && v.signal_metadata.has_value()) {
            if (&v == &Value::t) {
                // 't' is monotonically increasing
                if (rising_edge) {
                    // Rising edge: 't' crosses threshold at t = threshold
                    return (from_time < threshold) ? threshold : std::numeric_limits<double>::infinity();
                } else {
                    // Falling edge: 't' never falls, so no falling crossings
                    return std::numeric_limits<double>::infinity();
                }
            }
        }
        
        return std::numeric_limits<double>::infinity();
    }

    // Range Analysis

    std::vector<std::pair<double, double>> find_value_ranges(const Value& v, double min_val, double max_val, double start_time, double end_time) {
        std::vector<std::pair<double, double>> ranges;
        
        // Constants: if in range, entire time window is valid
        if (v.is_constant()) {
            double val = v.as_float();
            if (val >= min_val && val <= max_val) {
                ranges.push_back({start_time, end_time});
            }
            return ranges;
        }
        
        // For time parameter 't'
        if (v.type == Value::SIGNAL && v.signal_metadata.has_value()) {
            if (&v == &Value::t) {
                // 't' is in range [min_val, max_val] when t ∈ [min_val, max_val]
                double range_start = std::max(start_time, min_val);
                double range_end = std::min(end_time, max_val);
                if (range_start <= range_end) {
                    ranges.push_back({range_start, range_end});
                }
            }
        }
        
        return ranges;
    }

    bool is_in_range(const Value& v, double min_val, double max_val, double at_time) {
        // Get value at specified time
        if (v.is_constant()) {
            double val = v.as_float();
            return (val >= min_val && val <= max_val);
        }
        
        // For time parameter 't'
        if (v.type == Value::SIGNAL && v.signal_metadata.has_value()) {
            if (&v == &Value::t) {
                // Value of 't' at time at_time is at_time itself
                return (at_time >= min_val && at_time <= max_val);
            }
        }
        
        // For other signals, would need evaluation
        return false;
    }

    double get_time_in_range(const Value& v, double min_val, double max_val, double start_time, double end_time) {
        // Constants: if in range, entire time window counts
        if (v.is_constant()) {
            double val = v.as_float();
            if (val >= min_val && val <= max_val) {
                return end_time - start_time;
            }
            return 0.0;
        }
        
        // For time parameter 't'
        if (v.type == Value::SIGNAL && v.signal_metadata.has_value()) {
            if (&v == &Value::t) {
                // 't' is in range when t ∈ [min_val, max_val] ∩ [start_time, end_time]
                double range_start = std::max({start_time, min_val});
                double range_end = std::min({end_time, max_val});
                return std::max(0.0, range_end - range_start);
            }
        }
        
        return 0.0;
    }

    // Extrema Analysis

    std::vector<double> find_local_maxima(const Value& v, double start_time, double end_time) {
        std::vector<double> maxima;
        
        // Constants have no local extrema (every point is both max and min)
        if (v.is_constant()) {
            return maxima;
        }
        
        // For monotonic signals, extrema only at endpoints
        if (v.type == Value::SIGNAL && v.signal_metadata.has_value()) {
            const auto& meta = *v.signal_metadata;
            if (meta.is_monotonic_increasing) {
                // Maximum at end of interval
                maxima.push_back(end_time);
            } else if (meta.is_monotonic_decreasing) {
                // Maximum at start of interval
                maxima.push_back(start_time);
            }
            // For non-monotonic signals, would need signal evaluation to find extrema
        }
        
        return maxima;
    }

    std::vector<double> find_local_minima(const Value& v, double start_time, double end_time) {
        std::vector<double> minima;
        
        // Constants have no local extrema
        if (v.is_constant()) {
            return minima;
        }
        
        // For monotonic signals, extrema only at endpoints
        if (v.type == Value::SIGNAL && v.signal_metadata.has_value()) {
            const auto& meta = *v.signal_metadata;
            if (meta.is_monotonic_increasing) {
                // Minimum at start of interval
                minima.push_back(start_time);
            } else if (meta.is_monotonic_decreasing) {
                // Minimum at end of interval
                minima.push_back(end_time);
            }
            // For non-monotonic signals, would need signal evaluation to find extrema
        }
        
        return minima;
    }

    double get_global_maximum_time(const Value& v, double start_time, double end_time) {
        // Constants: no meaningful maximum time
        if (v.is_constant()) {
            return start_time; // Arbitrary choice - could be any time in range
        }
        
        // For monotonic signals
        if (v.type == Value::SIGNAL && v.signal_metadata.has_value()) {
            const auto& meta = *v.signal_metadata;
            if (meta.is_monotonic_increasing) {
                return end_time; // Maximum at end
            } else if (meta.is_monotonic_decreasing) {
                return start_time; // Maximum at start
            }
        }
        
        return start_time; // Default fallback
    }

    double get_global_minimum_time(const Value& v, double start_time, double end_time) {
        // Constants: no meaningful minimum time
        if (v.is_constant()) {
            return start_time; // Arbitrary choice - could be any time in range
        }
        
        // For monotonic signals
        if (v.type == Value::SIGNAL && v.signal_metadata.has_value()) {
            const auto& meta = *v.signal_metadata;
            if (meta.is_monotonic_increasing) {
                return start_time; // Minimum at start
            } else if (meta.is_monotonic_decreasing) {
                return end_time; // Minimum at end
            }
        }
        
        return start_time; // Default fallback
    }

    // Monotonicity Analysis

    bool is_monotonic_in_range(const Value& v, double start_time, double end_time) {
        // Constants are trivially monotonic (but also trivially both increasing and decreasing)
        if (v.is_constant()) {
            return true;
        }
        
        // Check signal metadata
        if (v.type == Value::SIGNAL && v.signal_metadata.has_value()) {
            const auto& meta = *v.signal_metadata;
            return meta.is_monotonic;
        }
        
        return false; // Conservative default
    }

    bool is_increasing_in_range(const Value& v, double start_time, double end_time) {
        // Constants are neither increasing nor decreasing (flat)
        if (v.is_constant()) {
            return false;
        }
        
        // Check signal metadata
        if (v.type == Value::SIGNAL && v.signal_metadata.has_value()) {
            const auto& meta = *v.signal_metadata;
            return meta.is_monotonic_increasing;
        }
        
        return false;
    }

    bool is_decreasing_in_range(const Value& v, double start_time, double end_time) {
        // Constants are neither increasing nor decreasing (flat)
        if (v.is_constant()) {
            return false;
        }
        
        // Check signal metadata
        if (v.type == Value::SIGNAL && v.signal_metadata.has_value()) {
            const auto& meta = *v.signal_metadata;
            return meta.is_monotonic_decreasing;
        }
        
        return false;
    }

} // namespace ValueSignalProcessing