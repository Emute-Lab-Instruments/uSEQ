// Suppress all warnings for this file
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wpedantic"

#include "value.h"
#include "../../utils.h"
#include "../../utils/log.h"
#include "../../utils/logger_bridge.h"
#include "environment.h"
#include <cmath>
#include <limits>

#include "configure.h"

// TODO do we care to support this some_value.apply() and .eval()
// syntax and, if so, is there a better way?
class Interpreter;
#include "interpreter.h"

////DESTRUCTOR
Value::~Value() {}

//// STATIC INITIALIZATIONS

// Initialize static time parameter 't'
// Value Value::t = []() {
//     Value time_param;
//     time_param.type = SIGNAL;
//     time_param.stack_data.f = 0.0;       // Initialize time to 0.0
//     SignalMetadata metadata;
//     metadata.is_const = false;           // 't' varies with time
//     metadata.min_val = -INFINITY;        // unbounded
//     metadata.max_val = INFINITY;         // unbounded
//     metadata.min_inclusive = true;       // inclusive bounds
//     metadata.max_inclusive = true;       // inclusive bounds
//     metadata.is_periodic = false;        // monotonic, not periodic
//     metadata.period = 0.0;               // no period
//     metadata.is_monotonic = true;        // always increasing
//     metadata.is_monotonic_increasing = true;  // specifically increasing
//     metadata.is_monotonic_decreasing = false; // not decreasing
    
//     // Time properties
//     metadata.time_start = -INFINITY;     // time extends infinitely backward
//     metadata.time_end = INFINITY;        // time extends infinitely forward
//     metadata.is_causal = true;           // time is causal
//     metadata.is_memoryless = true;       // current time only
//     metadata.memory_length = 0.0;       // no memory
    
//     // Continuity properties 
//     metadata.is_continuous = true;       // time is continuous
//     metadata.is_smooth = true;          // time is smooth
//     metadata.is_stepped = false;        // not stepped
//     metadata.is_linear_segments = true; // linear progression
    
//     // Zero crossing analysis
//     metadata.has_zero_crossings = true; // crosses zero at t=0
//     metadata.zero_crossing_rate = 0.0;  // crosses zero once
//     metadata.zero_crossing_locations_known = true; // we know it crosses at t=0
    
//     // Other properties
//     metadata.interpolation_type = SignalMetadata::LINEAR;
//     metadata.phase_offset = 0.0;
//     metadata.phase_locked = false;
    
//     time_param.signal_metadata = metadata;
//     return time_param;
// }();*()

// Global reference to static t
// const Value& t = Value::t;

//// SIGNAL METADATA HELPERS

// Helper function to combine metadata for addition/subtraction operations
SignalMetadata combine_metadata_additive(const std::optional<SignalMetadata>& left_meta,
                                        const std::optional<SignalMetadata>& right_meta) {
    SignalMetadata result;
    
    // If neither has metadata, return default (constant)
    if (!left_meta && !right_meta) {
        return result;
    }
    
    // Extract metadata or use defaults
    SignalMetadata left = left_meta.value_or(SignalMetadata{});
    SignalMetadata right = right_meta.value_or(SignalMetadata{});
    
    // Result is non-constant if either operand is non-constant
    result.is_const = left.is_const && right.is_const;
    
    // Combine bounds: [a_min + b_min, a_max + b_max]
    result.min_val = left.min_val + right.min_val;
    result.max_val = left.max_val + right.max_val;
    result.min_inclusive = left.min_inclusive && right.min_inclusive;
    result.max_inclusive = left.max_inclusive && right.max_inclusive;
    
    // Conservative approach to periodicity - only periodic if both are with same period
    result.is_periodic = left.is_periodic && right.is_periodic && (left.period == right.period);
    result.period = result.is_periodic ? left.period : 0.0;
    result.phase_offset = result.is_periodic ? (left.phase_offset + right.phase_offset) : 0.0;
    result.phase_locked = left.phase_locked && right.phase_locked;
    
    // Monotonicity: only preserved if both are monotonic in same direction
    result.is_monotonic = left.is_monotonic && right.is_monotonic && 
                         (left.is_monotonic_increasing == right.is_monotonic_increasing) &&
                         (left.is_monotonic_decreasing == right.is_monotonic_decreasing);
    result.is_monotonic_increasing = result.is_monotonic && left.is_monotonic_increasing;
    result.is_monotonic_decreasing = result.is_monotonic && left.is_monotonic_decreasing;
    
    // Continuity and smoothness: preserved if both operands have these properties
    result.is_continuous = left.is_continuous && right.is_continuous;
    result.is_smooth = left.is_smooth && right.is_smooth;
    result.is_stepped = left.is_stepped || right.is_stepped; // addition can create steps
    result.is_linear_segments = left.is_linear_segments && right.is_linear_segments;
    
    // Time bounds: intersection of both time domains
    result.time_start = std::max(left.time_start, right.time_start);
    result.time_end = std::min(left.time_end, right.time_end);
    result.is_causal = left.is_causal && right.is_causal;
    result.is_memoryless = left.is_memoryless && right.is_memoryless;
    result.memory_length = std::max(left.memory_length, right.memory_length);
    
    // Zero crossing analysis becomes complex - conservative approach
    result.has_zero_crossings = true; // addition can create new zero crossings
    result.zero_crossing_rate = 0.0; // unknown without full analysis
    result.zero_crossing_locations_known = false;
    
    // Threshold and range analysis capabilities
    result.supports_threshold_queries = left.supports_threshold_queries && right.supports_threshold_queries;
    result.min_threshold_resolution = std::max(left.min_threshold_resolution, right.min_threshold_resolution);
    result.supports_range_queries = left.supports_range_queries && right.supports_range_queries;
    result.range_query_resolution = std::max(left.range_query_resolution, right.range_query_resolution);
    
    // Extrema analysis
    result.has_local_extrema = true; // addition typically creates new extrema
    result.extrema_locations_known = false; // would need recomputation
    result.extrema_detection_threshold = std::max(left.extrema_detection_threshold, right.extrema_detection_threshold);
    
    // Interpolation: use most restrictive type
    result.interpolation_type = static_cast<SignalMetadata::InterpolationType>(
        std::max(static_cast<int>(left.interpolation_type), static_cast<int>(right.interpolation_type))
    );
    
    // Quantization properties
    result.is_quantized = left.is_quantized || right.is_quantized;
    result.is_integer_valued = left.is_integer_valued && right.is_integer_valued;
    result.quantum_step = (left.is_quantized && right.is_quantized) ? 
                         std::min(left.quantum_step, right.quantum_step) : 0.0;
    
    // Domain validation
    result.has_domain_error = left.has_domain_error || right.has_domain_error;
    if (!result.has_domain_error && result.time_start > result.time_end) {
        result.has_domain_error = true;
        result.error_message = "Empty time domain after addition";
    }
    
    return result;
}

// Helper function to combine metadata for multiplication operations  
SignalMetadata combine_metadata_multiplicative(const std::optional<SignalMetadata>& left_meta,
                                              const std::optional<SignalMetadata>& right_meta,
                                              bool is_division = false) {
    SignalMetadata result;
    
    // If neither has metadata, return default (constant)
    if (!left_meta && !right_meta) {
        return result;
    }
    
    // Extract metadata or use defaults
    SignalMetadata left = left_meta.value_or(SignalMetadata{});
    SignalMetadata right = right_meta.value_or(SignalMetadata{});
    
    // Result is non-constant if either operand is non-constant
    result.is_const = left.is_const && right.is_const;
    
    // For bounds calculation, we need all possible combinations
    std::vector<double> products = {
        left.min_val * right.min_val,
        left.min_val * right.max_val,
        left.max_val * right.min_val,
        left.max_val * right.max_val
    };
    
    if (is_division) {
        // For division, replace right bounds with 1/bounds (watch for zero)
        if (right.min_val != 0.0 && right.max_val != 0.0) {
            products = {
                left.min_val / right.min_val,
                left.min_val / right.max_val,
                left.max_val / right.min_val,
                left.max_val / right.max_val
            };
        }
    }
    
    result.min_val = *std::min_element(products.begin(), products.end());
    result.max_val = *std::max_element(products.begin(), products.end());
    result.min_inclusive = true; // Conservative
    result.max_inclusive = true; // Conservative
    
    // Periodicity is complex for multiplication - conservative approach
    result.is_periodic = false;
    result.period = 0.0;
    result.is_monotonic = false;
    
    // Smoothness and continuity: preserved if both operands have these properties
    // Continuity and smoothness: preserved if both operands have these properties
    result.is_continuous = left.is_continuous && right.is_continuous;
    result.is_smooth = left.is_smooth && right.is_smooth;
    result.is_stepped = left.is_stepped || right.is_stepped; // multiplication can create steps
    result.is_linear_segments = false; // multiplication destroys linearity
    
    // Time bounds: intersection of both time domains  
    result.time_start = std::max(left.time_start, right.time_start);
    result.time_end = std::min(left.time_end, right.time_end);
    result.is_causal = left.is_causal && right.is_causal;
    result.is_memoryless = left.is_memoryless && right.is_memoryless;
    result.memory_length = std::max(left.memory_length, right.memory_length); 
    
    
    // Zero crossing analysis becomes complex - conservative approach
    result.has_zero_crossings = true; // multiplication can create new zero crossings
    result.zero_crossing_rate = 0.0; // unknown without full analysis
    result.zero_crossing_locations_known = false;
    
    // Threshold and range analysis capabilities
    result.supports_threshold_queries = left.supports_threshold_queries && right.supports_threshold_queries;
    result.min_threshold_resolution = std::max(left.min_threshold_resolution, right.min_threshold_resolution);
    result.supports_range_queries = left.supports_range_queries && right.supports_range_queries;
    result.range_query_resolution = std::max(left.range_query_resolution, right.range_query_resolution);
    
    // Extrema analysis
    result.has_local_extrema = true; // multiplication typically creates new extrema
    result.extrema_locations_known = false; // would need recomputation
    result.extrema_detection_threshold = std::max(left.extrema_detection_threshold, right.extrema_detection_threshold);
    
    // Interpolation: use most restrictive type
    result.interpolation_type = static_cast<SignalMetadata::InterpolationType>(
        std::max(static_cast<int>(left.interpolation_type), static_cast<int>(right.interpolation_type))
    );
    
    // Quantization properties
    result.is_quantized = left.is_quantized || right.is_quantized;
    result.is_integer_valued = left.is_integer_valued && right.is_integer_valued;
    result.quantum_step = (left.is_quantized && right.is_quantized) ? 
                         std::min(left.quantum_step, right.quantum_step) : 0.0;
    
    // Domain validation
    result.has_domain_error = left.has_domain_error || right.has_domain_error;
    if (!result.has_domain_error && result.time_start > result.time_end) {
        result.has_domain_error = true;
        result.error_message = is_division ? "Empty time domain after division" : "Empty time domain after multiplication";
    }
    
    return result;
}

// Helper function for subtraction metadata (similar to addition but with right operand negated)
SignalMetadata combine_metadata_subtractive(const std::optional<SignalMetadata>& left_meta,
                                           const std::optional<SignalMetadata>& right_meta) {
    SignalMetadata result;
    
    // If neither has metadata, return default (constant)
    if (!left_meta && !right_meta) {
        return result;
    }
    
    // Extract metadata or use defaults
    SignalMetadata left = left_meta.value_or(SignalMetadata{});
    SignalMetadata right = right_meta.value_or(SignalMetadata{});
    
    // Result is non-constant if either operand is non-constant
    result.is_const = left.is_const && right.is_const;
    
    // Combine bounds: [a_min - b_max, a_max - b_min] (note the swap for subtraction)
    result.min_val = left.min_val - right.max_val;
    result.max_val = left.max_val - right.min_val;
    result.min_inclusive = left.min_inclusive && right.max_inclusive;
    result.max_inclusive = left.max_inclusive && right.min_inclusive;
    
    // Conservative approach to periodicity - only periodic if both are with same period
    result.is_periodic = left.is_periodic && right.is_periodic && (left.period == right.period);
    result.period = result.is_periodic ? left.period : 0.0;
    result.phase_offset = result.is_periodic ? (left.phase_offset - right.phase_offset) : 0.0;
    result.phase_locked = left.phase_locked && right.phase_locked;
    
    // Monotonicity: generally not preserved (conservative)
    result.is_monotonic = false;
    result.is_monotonic_increasing = false;
    result.is_monotonic_decreasing = false;
    
    // Continuity and smoothness: preserved if both operands have these properties
    result.is_continuous = left.is_continuous && right.is_continuous;
    result.is_smooth = left.is_smooth && right.is_smooth;
    result.is_stepped = left.is_stepped || right.is_stepped; // subtraction can create steps
    result.is_linear_segments = left.is_linear_segments && right.is_linear_segments;
    
    // Time bounds: intersection of both time domains
    result.time_start = std::max(left.time_start, right.time_start);
    result.time_end = std::min(left.time_end, right.time_end);
    result.is_causal = left.is_causal && right.is_causal;
    result.is_memoryless = left.is_memoryless && right.is_memoryless;
    result.memory_length = std::max(left.memory_length, right.memory_length);
    
    // Zero crossing analysis becomes complex - conservative approach
    result.has_zero_crossings = true; // subtraction can create new zero crossings
    result.zero_crossing_rate = 0.0; // unknown without full analysis
    result.zero_crossing_locations_known = false;
    
    // Threshold and range analysis capabilities
    result.supports_threshold_queries = left.supports_threshold_queries && right.supports_threshold_queries;
    result.min_threshold_resolution = std::max(left.min_threshold_resolution, right.min_threshold_resolution);
    result.supports_range_queries = left.supports_range_queries && right.supports_range_queries;
    result.range_query_resolution = std::max(left.range_query_resolution, right.range_query_resolution);
    
    // Extrema analysis
    result.has_local_extrema = true; // subtraction typically creates new extrema
    result.extrema_locations_known = false; // would need recomputation
    result.extrema_detection_threshold = std::max(left.extrema_detection_threshold, right.extrema_detection_threshold);
    
    // Interpolation: use most restrictive type
    result.interpolation_type = static_cast<SignalMetadata::InterpolationType>(
        std::max(static_cast<int>(left.interpolation_type), static_cast<int>(right.interpolation_type))
    );
    
    // Quantization properties
    result.is_quantized = left.is_quantized || right.is_quantized;
    result.is_integer_valued = left.is_integer_valued && right.is_integer_valued;
    result.quantum_step = (left.is_quantized && right.is_quantized) ? 
                         std::min(left.quantum_step, right.quantum_step) : 0.0;
    
    // Domain validation
    result.has_domain_error = left.has_domain_error || right.has_domain_error;
    if (!result.has_domain_error && result.time_start > result.time_end) {
        result.has_domain_error = true;
        result.error_message = "Empty time domain after subtraction";
    }
    
    return result;
}

// Helper function for modulo metadata - creates periodic signals with bounded output
SignalMetadata combine_metadata_modulo(const std::optional<SignalMetadata>& left_meta,
                                      const std::optional<SignalMetadata>& right_meta,
                                      double right_constant_value = 0.0) {
    SignalMetadata result;
    
    // If neither has metadata, return default (constant)
    if (!left_meta && !right_meta) {
        return result;
    }
    
    // Extract metadata or use defaults
    SignalMetadata left = left_meta.value_or(SignalMetadata{});
    SignalMetadata right = right_meta.value_or(SignalMetadata{});
    
    // Result is non-constant if either operand is non-constant
    result.is_const = left.is_const && right.is_const;
    
    // Determine the modulus value
    double modulus_value;
    bool is_positive_constant;
    
    if (right_meta.has_value()) {
        // Right operand has signal metadata
        is_positive_constant = right.is_const && right.min_val > 0 && right.min_val == right.max_val;
        modulus_value = is_positive_constant ? right.min_val : 0.0;
    } else {
        // Right operand is a simple constant (no signal metadata)
        is_positive_constant = right_constant_value > 0;
        modulus_value = right_constant_value;
    }
    
    // For modulo, the result is bounded by [0, modulus) when modulus is positive
    if (!is_positive_constant || modulus_value <= 0) {
        // Conservative bounds if modulus is not a positive constant
        result.min_val = -INFINITY;
        result.max_val = INFINITY;
        result.min_inclusive = true;
        result.max_inclusive = true;
    } else {
        // Modulus is positive constant - result is [0, modulus)
        result.min_val = 0.0;
        result.max_val = modulus_value;
        result.min_inclusive = true;
        result.max_inclusive = false;  // modulo result is exclusive of the modulus
        
        // Modulo creates periodicity with period equal to the modulus
        result.is_periodic = true;
        result.period = modulus_value;
        result.phase_offset = 0.0;
        result.phase_locked = true;
    }
    
    // Monotonicity: modulo breaks monotonicity (saw-tooth pattern)
    result.is_monotonic = false;
    result.is_monotonic_increasing = false;
    result.is_monotonic_decreasing = false;
    
    // Modulo operations create jump discontinuities, breaking continuity and smoothness
    result.is_continuous = false;
    result.is_smooth = false;
    result.is_stepped = true; // modulo creates discrete jumps
    result.is_linear_segments = false;
    
    // Time bounds: use left operand's time domain
    result.time_start = left.time_start;
    result.time_end = left.time_end;
    result.is_causal = left.is_causal;
    result.is_memoryless = left.is_memoryless;
    result.memory_length = left.memory_length;
    
    // Zero crossing analysis
    if (is_positive_constant && modulus_value > 0) {
        result.has_zero_crossings = true; // modulo creates regular zero crossings
        result.zero_crossing_rate = 1.0 / modulus_value; // one crossing per period
        result.zero_crossing_locations_known = true;
    } else {
        result.has_zero_crossings = false;
        result.zero_crossing_rate = 0.0;
        result.zero_crossing_locations_known = false;
    }
    
    // Threshold and range analysis capabilities
    result.supports_threshold_queries = true; // modulo has predictable structure
    result.min_threshold_resolution = is_positive_constant ? modulus_value / 1000.0 : 0.0;
    result.supports_range_queries = true;
    result.range_query_resolution = is_positive_constant ? modulus_value / 1000.0 : 0.0;
    
    // Extrema analysis  
    if (is_positive_constant && modulus_value > 0) {
        result.has_local_extrema = true;
        result.extrema_locations_known = true;
        result.extrema_detection_threshold = modulus_value / 1000.0;
    } else {
        result.has_local_extrema = false;
        result.extrema_locations_known = false;
        result.extrema_detection_threshold = 0.0;
    }
    
    // Interpolation: stepped due to discontinuities
    result.interpolation_type = SignalMetadata::InterpolationType::STEPPED;
    
    // Quantization properties
    result.is_quantized = left.is_quantized;
    result.is_integer_valued = left.is_integer_valued && is_positive_constant && 
                              (modulus_value == std::floor(modulus_value));
    result.quantum_step = left.quantum_step;
    
    // Domain validation
    result.has_domain_error = left.has_domain_error || (!is_positive_constant && modulus_value <= 0);
    if (!result.has_domain_error && result.time_start > result.time_end) {
        result.has_domain_error = true;
        result.error_message = "Empty time domain after modulo";
    } else if (modulus_value <= 0) {
        result.has_domain_error = true;
        result.error_message = "Modulo by zero or negative value";
    }
    
    return result;
}


//// CONSTRUCTORS
// static Value Value::error()
Value Value::error() {
    Value result;
    result.type = ERROR;
    return result;
}

Value Value::nil() {
    Value result;
    result.type = NIL;
    return result;
}

// LAMBDA
Value::Value(std::vector<Value> params, Value ret, Environment const &env)
    : type(LAMBDA) {
    DBG("Value::Value (LAMBDA)");

    lambda_scope = std::make_shared<Environment>();

    // We store the params and the result in the list member
    // instead of having dedicated members. This is to save memory.
    list.push_back(Value::vector(params));
    list.push_back(ret);

    // Lambdas capture only variables that they know they will use.
    std::set<String> used_atoms = ret.get_used_atoms();

    for (const String &atom : used_atoms) {
        // Don't capture the current value of time variables
        // TODO: there should be some globally-accessible set
        // of the special time-varying values

        if (atom == "time" || atom == "t" || atom == "bar" || atom == "beat" ||
            atom == "section" || atom == "phrase") {
            continue;
        }

        // Ignore atoms that are known to be args
        if (std::find(params.begin(), params.end(), atom) != params.end()) {
            continue;
        }

        // Expr
        std::optional<Value> def_expr = env.get_expr(atom);
        // If the environment has a symbol that this lambda uses, capture it.
        if (def_expr) {
            lambda_scope->set_expr(atom, *def_expr);
        }

        // Static def
        std::optional<Value> def = env.get(atom);
        // If the environment has a symbol that this lambda uses, capture it.
        if (def) {
            lambda_scope->set(atom, *def);
        }
    }
}

// BUILTIN
Value::Value(String name, BuiltinFuncRawPtr ptr) : type(BUILTIN) {
    // Store the name of the builtin function in the str member
    // to save memory, and use the builtin function slot in the union
    // to store the function pointer.
    str = name;
    stack_data.builtin = ptr;
}

// BUILTIN_METHOD
Value::Value(String name, uSEQ_Method_Ptr ptr) : type(BUILTIN_METHOD) {
    str = name;
    stack_data.builtin_method = ptr;
}

// BUILTIN_MODULISP_METHOD
Value::Value(String name, ModuLispInterpreter_Method_Ptr ptr) : type(BUILTIN_MODULISP_METHOD) {
    str = name;
    stack_data.builtin_modulisp_method = ptr;
}

// METHODS
Value Value::quote(Value quoted) {
    Value result;
    result.type = QUOTE;
    result.list.push_back(quoted);
    return result;
}

Value Value::atom(String s) {
    Value result;
    result.type = ATOM;
    result.str = s;
    return result;
}

Value Value::string(String s) {
    Value result;
    result.type = STRING;
    result.str = s;
    return result;
}

Value Value::vector(std::vector<Value> vec) {
    Value result;
    result.type = VECTOR;
    result.list = vec;
    return result;
}

std::set<String> Value::get_used_atoms() const {
    std::set<String> result, tmp;
    switch (type) {
    case QUOTE:
        // The data for a quote is stored in the
        // first slot of the list member.
        return list[0].get_used_atoms();
    case ATOM:
        // If this is an atom, add it to the list
        // of used atoms in this expression.
        result.insert(as_atom());
        return result;
    case LAMBDA:
        // If this is a lambda, get the list of used atoms in the body
        // of the expression.
        return list[1].get_used_atoms();
    case LIST:
    case VECTOR:
        // If this is a list, add each of the atoms used in all
        // of the elements in the list.
        for (size_t i = 0; static_cast<size_t>(i) < list.size(); i++) {
            // Get the atoms used in the element
            tmp = list[i].get_used_atoms();
            // Add the used atoms to the current list of used atoms
            result = set_union(result, tmp);
        }
        return result;
    default:
        return result;
    }
}

bool Value::is_nil() const { return type == NIL; }
bool Value::is_builtin() const {
    return type == BUILTIN || type == BUILTIN_METHOD;
}

Value Value::apply(std::vector<Value> &args, Environment &env) {
    return Interpreter::apply(*this, args, env);
}

Value Value::eval(Environment &env) { return Interpreter::eval_in(*this, env); }

bool Value::is_number() const { return type == INT || type == FLOAT; }
bool Value::is_int() const { return type == INT; }
bool Value::is_float() const { return type == FLOAT; }

// FIXME
bool Value::is_negative_number() const { return is_number() && *this < 0.0; }
bool Value::is_positive_number() const { return is_number() && *this > 0.0; }
bool Value::is_non_zero_number() const {
    return is_number() && (*this < 0.0 || *this > 0.0);
}

bool Value::is_error() const { return type == ERROR; }

bool Value::is_list() const { return type == LIST; }
bool Value::is_vector() const { return type == VECTOR; }
bool Value::is_sequential() const { return is_list() || is_vector(); }

bool Value::is_signal() const {
    // A value is a signal if:
    // 1. It has the SIGNAL type, OR
    // 2. It has signal metadata (meaning it was derived from signal operations)
    return type == SIGNAL || signal_metadata.has_value();
}

// Signal metadata query methods
double Value::get_min() const {
    if (signal_metadata.has_value()) {
        return signal_metadata->min_val;
    }
    // For non-signals, return the actual value if numeric
    if (is_number()) {
        return as_float();
    }
    return -INFINITY; // Conservative default
}

double Value::get_max() const {
    if (signal_metadata.has_value()) {
        return signal_metadata->max_val;
    }
    // For non-signals, return the actual value if numeric
    if (is_number()) {
        return as_float();
    }
    return INFINITY; // Conservative default
}

bool Value::is_periodic() const {
    if (signal_metadata.has_value()) {
        return signal_metadata->is_periodic;
    }
    return false; // Non-signals are not periodic
}

double Value::get_period() const {
    if (signal_metadata.has_value() && signal_metadata->is_periodic) {
        return signal_metadata->period;
    }
    return 0.0; // No period for non-periodic signals
}

bool Value::is_monotonic() const {
    if (signal_metadata.has_value()) {
        return signal_metadata->is_monotonic;
    }
    return true; // Constants are trivially monotonic
}

bool Value::is_constant() const {
    if (signal_metadata.has_value()) {
        return signal_metadata->is_const;
    }
    return true; // Non-signals are constant by default
}

bool Value::is_smooth() const {
    if (signal_metadata.has_value()) {
        return signal_metadata->is_smooth;
    }
    return true; // Non-signals (constants) are smooth by default
}

bool Value::is_continuous() const {
    if (signal_metadata.has_value()) {
        return signal_metadata->is_continuous;
    }
    return true; // Non-signals (constants) are continuous by default
}

// Note: Removed is_analytic(), is_odd(), and is_even() methods
// These methods depended on DSP metadata fields that were removed
// in favor of automation-focused metadata

bool Value::is_empty() const { return list.empty(); }

bool Value::is_list_and_empty() const { return type == LIST && list.empty(); }

bool Value::is_string() const { return type == STRING; }

bool Value::is_symbol() const { return type == ATOM; }

bool Value::as_bool() const { return *this != Value(0); }

int Value::as_int() const { return cast_to_int().stack_data.i; }

double Value::as_float() const { return cast_to_float().stack_data.f; }

// FIXME @correctness these should probably change to std::optionals
String Value::as_string() const {
    if (type != STRING) {
        println("str: " + BAD_CAST);
        return "std::nullopt";
    }
    return str;
}

String Value::as_atom() const {
    if (type != ATOM) {
        println("atom: " + BAD_CAST);
        return "std::nullopt";
    }
    return str;
}

std::vector<Value> Value::as_list() const {
    if (type != LIST && type != VECTOR) {
        println("list: " + BAD_CAST);
        return {};
    }
    return list;
}

std::vector<Value> Value::as_vector() const {
    if (type != VECTOR) {
        println("vector: " + BAD_CAST);
        return {};
    }
    return list;
}

std::vector<Value> Value::as_sequential() const {
    if (type != VECTOR && type != LIST) {
        println(BAD_CAST);
        return {};
    }
    return list;
}

void Value::push(Value val) {
    if (type == LIST || type == VECTOR) {
        list.push_back(val);
    } else {
        println(MISMATCHED_TYPES);
    }
}

Value Value::pop() {
    Value result = Value::nil();

    if (type == LIST || type == VECTOR) {
        result = list[list.size() - 1];
        list.pop_back();
    } else {
        println(MISMATCHED_TYPES);
    }

    return result;
}

Value Value::cast_to_int() const {
    switch (type) {
    case INT:
        return *this;
    case FLOAT:
        return Value(int(stack_data.f));
    case SIGNAL:
        // For signals, cast their current time value to int
        return Value(int(stack_data.f));
    default:
        println(BAD_CAST + " (int)");
        return Value::error();
    }
}

Value Value::cast_to_float() const {
    switch (type) {
    case FLOAT:
        return *this;
    case INT:
        return Value(double(stack_data.i));
    case SIGNAL:
        // For signals, return their current time value
        // For now, use the stored float value (defaults to 0.0 for time parameter 't')
        return Value(stack_data.f);
    default:
        println(BAD_CAST + " (float)");
        return Value::error();
    }
}

bool Value::operator==(const String &other) const { return str == other; }

bool Value::operator==(Value other) const {
    // If either of these values are floats, promote the
    // other to a float, and then compare for equality.
    if (type == FLOAT && other.type == INT)
        return *this == other.cast_to_float();
    else if (type == INT && other.type == FLOAT)
        return this->cast_to_float() == other;
    // If the values types aren't equal, then they cannot be equal.
    else if (type != other.type)
        return false;

    switch (type) {
    case FLOAT:
        return stack_data.f == other.stack_data.f;
    case INT:
        return stack_data.i == other.stack_data.i;
    case BUILTIN:
        // FIXME how do we compare functions? compare if they point to the same
        // thing?
        return str == other.str;
    case STRING:
    case ATOM:
        // Both atoms and strings store their
        // data in the str member.
        return str == other.str;
    case LAMBDA:
    case LIST:
    case VECTOR:
        // Both lambdas and lists store their
        // data in the list member.
        return list == other.list;
    case QUOTE:
        // The values for quotes are stored in the
        // first slot of the list member.
        return list[0] == other.list[0];
    default:
        return true;
    }
}

bool Value::operator!=(Value other) const { return !(*this == other); }

bool Value::operator>=(Value other) const { return !(*this < other); }

bool Value::operator<=(Value other) const {
    return (*this == other) || (*this < other);
}

bool Value::operator>(Value other) const { return !(*this <= other); }

bool Value::operator<(Value other) const {
    // Other type must be a float or an int
    if (other.type != FLOAT && other.type != INT) {
        if (auto L = get_global_logger()) L->error(INVALID_BIN_OP);
        println(INVALID_BIN_OP);
    }
    // throw Error(*this, Environment(), INVALID_BIN_OP);

    switch (type) {
    case FLOAT:
        // If this is a float, promote the other value to a float and compare.
        return stack_data.f < other.cast_to_float().stack_data.f;
    case INT:
        // If the other value is a float, promote this value to a float and
        // compare.
        if (other.type == FLOAT)
            return cast_to_float().stack_data.f < other.stack_data.f;
        // Otherwise compare the integer values
        else
            return stack_data.i < other.stack_data.i;
    default:
        // Only allow comparisons between integers and floats
        println(INVALID_ORDER);
        return false;
        // throw Error(*this, Environment(), INVALID_ORDER);
    }
}

Value Value::operator+(Value other) const {
    if (other.type == UNIT)
        return other;

    if ((is_number() || other.is_number()) &&
        !(is_number() && other.is_number())) {
        if (auto L = get_global_logger()) L->error(INVALID_BIN_OP);
        println(INVALID_BIN_OP);
    }

    // Check if we need signal metadata (either operand has metadata)
    bool has_signal_metadata = signal_metadata.has_value() || other.signal_metadata.has_value();
    
    Value result;
    
    switch (type) {
    case SIGNAL:
    case FLOAT:
        result = Value(stack_data.f + other.cast_to_float().stack_data.f);
        break;
    case INT:
        if (other.type == FLOAT || other.type == SIGNAL)
            result = Value(cast_to_float() + other.cast_to_float().stack_data.f);
        else if (other.type == STRING)
            result = Value::string(as_string() + other.as_string());
        else
            result = Value(stack_data.i + other.stack_data.i);
        break;
    case STRING:
        if (other.type == STRING)
            result = Value::string(str + other.str);
        else
            println(INVALID_BIN_OP);
        break;
    case LIST:
        if (other.type == LIST) {
            result = *this;
            for (size_t i = 0; i < other.list.size(); i++)
                result.push(other.list[i]);
        } else
            println(INVALID_BIN_OP);
        break;
    case UNIT:
        result = *this;
        break;
    default:
        println(INVALID_BIN_OP);
        return Value();
    }
    
    // Apply signal metadata if needed for numeric operations
    if (has_signal_metadata && (result.is_number() || type == SIGNAL || other.type == SIGNAL)) {
        result.type = SIGNAL;
        result.signal_metadata = combine_metadata_additive(signal_metadata, other.signal_metadata);
    }
    
    return result;
}

Value Value::operator-(Value other) const {
    // If the other value's type is the unit type,
    // don't even bother continuing.
    // Unit types consume all arithmetic operations.
    if (other.type == UNIT)
        return other;

    // Other type must be a float, int, or signal
    if (other.type != FLOAT && other.type != INT && other.type != SIGNAL)
        println(INVALID_BIN_OP);
    // throw Error(*this, Environment(), INVALID_BIN_OP);

    // Check if we need signal metadata (either operand has metadata)
    bool has_signal_metadata = signal_metadata.has_value() || other.signal_metadata.has_value();
    
    Value result;
    
    switch (type) {
    case SIGNAL:
    case FLOAT:
        // If one is a float, promote the other by default and do
        // float subtraction.
        result = Value(stack_data.f - other.cast_to_float().stack_data.f);
        break;
    case INT:
        // If the other type is a float or signal, go ahead and promote this expression
        // before continuing with the subtraction
        if (other.type == FLOAT || other.type == SIGNAL)
            result = Value(cast_to_float().stack_data.f - other.cast_to_float().stack_data.f);
        // Otherwise, do integer subtraction.
        else
            result = Value(stack_data.i - other.stack_data.i);
        break;
    case UNIT:
        // Unit types consume all arithmetic operations.
        result = *this;
        break;
    default:
        // This operation was done on an unsupported type
        println(INVALID_BIN_OP);
        return Value();
        // throw Error(*this, Environment(), INVALID_BIN_OP);
    }
    
    // Apply signal metadata if needed for numeric operations
    if (has_signal_metadata && (result.is_number() || type == SIGNAL || other.type == SIGNAL)) {
        result.type = SIGNAL;
        result.signal_metadata = combine_metadata_subtractive(signal_metadata, other.signal_metadata);
    }
    
    return result;
}

Value Value::operator*(Value other) const {

    // If the other value's type is the unit type,
    // don't even bother continuing.
    // Unit types consume all arithmetic operations.
    if (other.type == UNIT)
        return other;

    // Other type must be a float, int, or signal
    if (other.type != FLOAT && other.type != INT && other.type != SIGNAL)
        println(INVALID_BIN_OP);
    // throw Error(*this, Environment(), INVALID_BIN_OP);

    // Check if we need signal metadata (either operand has metadata)
    bool has_signal_metadata = signal_metadata.has_value() || other.signal_metadata.has_value();
    
    Value result;
    
    switch (type) {
    case SIGNAL:
    case FLOAT:
        result = Value(stack_data.f * other.cast_to_float().stack_data.f);
        break;
    case INT:
        // If the other type is a float or signal, go ahead and promote this expression
        // before continuing with the product
        if (other.type == FLOAT || other.type == SIGNAL)
            result = Value(cast_to_float().stack_data.f * other.cast_to_float().stack_data.f);
        // Otherwise, do integer multiplication.
        else
            result = Value(stack_data.i * other.stack_data.i);
        break;
    case UNIT:
        // Unit types consume all arithmetic operations.
        result = *this;
        break;
    default:
        // This operation was done on an unsupported type
        println(INVALID_BIN_OP);
        return Value();
        // throw Error(*this, Environment(), INVALID_BIN_OP);
    }
    
    // Apply signal metadata if needed for numeric operations
    if (has_signal_metadata && (result.is_number() || type == SIGNAL || other.type == SIGNAL)) {
        result.type = SIGNAL;
        result.signal_metadata = combine_metadata_multiplicative(signal_metadata, other.signal_metadata);
    }
    
    return result;
}

Value Value::operator/(Value other) const {

    // If the other value's type is the unit type,
    // don't even bother continuing.
    // Unit types consume all arithmetic operations.
    if (other.type == UNIT)
        return other;

    // Other type must be a float, int, or signal
    if (other.type != FLOAT && other.type != INT && other.type != SIGNAL)
        println(INVALID_BIN_OP);
    //             throw Error(*this, Environment(), INVALID_BIN_OP);

    // Check if we need signal metadata (either operand has metadata)
    bool has_signal_metadata = signal_metadata.has_value() || other.signal_metadata.has_value();
    
    Value result;
    
    switch (type) {
    case SIGNAL:
    case FLOAT: {
        result = Value(stack_data.f / other.cast_to_float().stack_data.f);
        break;
    }
    case INT: {
        // Division always promotes to float to preserve precision
        result = Value(cast_to_float().stack_data.f / other.cast_to_float().stack_data.f);
        break;
    }
    case UNIT:
        // Unit types consume all arithmetic operations.
        result = *this;
        break;
    default:
        // This operation was done on an unsupported type
        println(INVALID_BIN_OP);
        return Value();
        //  throw Error(*this, Environment(), INVALID_BIN_OP);
    }
    
    // Apply signal metadata if needed for numeric operations
    if (has_signal_metadata && (result.is_number() || type == SIGNAL || other.type == SIGNAL)) {
        result.type = SIGNAL;
        result.signal_metadata = combine_metadata_multiplicative(signal_metadata, other.signal_metadata, true);
    }
    
    return result;
}

Value Value::operator%(Value other) const {
    // If the other value's type is the unit type,
    // don't even bother continuing.
    // Unit types consume all arithmetic operations.
    if (other.type == UNIT)
        return other;

    // Other type must be a float, int, or signal
    if (other.type != FLOAT && other.type != INT && other.type != SIGNAL)
        println(INVALID_BIN_OP);
    // throw Error(*this, Environment(), INVALID_BIN_OP);

    // Check if we need signal metadata (either operand has metadata)
    bool has_signal_metadata = signal_metadata.has_value() || other.signal_metadata.has_value();
    
    Value result;
    
    switch (type) {
    // If we support libm, we can find the remainder of floating point values.
    case SIGNAL:
    case FLOAT:
        result = Value(fmod(stack_data.f, other.cast_to_float().stack_data.f));
        break;
    case INT:
        if (other.type == FLOAT || other.type == SIGNAL)
            result = Value(fmod(cast_to_float().stack_data.f, other.cast_to_float().stack_data.f));
        else
            result = Value(stack_data.i % other.stack_data.i);
        break;
    case UNIT:
        // Unit types consume all arithmetic operations.
        result = *this;
        break;
    default:
        // This operation was done on an unsupported type
        println(INVALID_BIN_OP);
        return Value();
        // throw Error(*this, Environment(), INVALID_BIN_OP);
    }
    
    // Apply signal metadata if needed for numeric operations
    if (has_signal_metadata && (result.is_number() || type == SIGNAL || other.type == SIGNAL)) {
        result.type = SIGNAL;
        // Pass the right operand's value for proper modulus calculation
        double right_value = other.is_number() ? other.as_float() : 0.0;
        result.signal_metadata = combine_metadata_modulo(signal_metadata, other.signal_metadata, right_value);
    }
    
    return result;
}


int Value::get_type_enum() const { return type; }

// Get the name of the type of this value
String Value::get_type_name() const {
    switch (type) {
    case NIL:
        return "nil";
    case QUOTE:
        return QUOTE_TYPE;
    case ATOM:
        return ATOM_TYPE;
    case INT:
        return INT_TYPE;
    case FLOAT:
        return FLOAT_TYPE;
    case LIST:
        return LIST_TYPE;
    case VECTOR:
        return VECTOR_TYPE;
    case STRING:
        return STRING_TYPE;
    case BUILTIN:
    case LAMBDA:
        // Instead of differentiating between
        // lambda and builtin types, we group them together.
        // This is because they are both callable.
        return FUNCTION_TYPE;
    case UNIT:
        return UNIT_TYPE;
    case ERROR:
        return ERROR_TYPE;
    default:
        // We don't know the name of this type.
        // This isn't the users fault, this is just unhandled.
        // This should never be reached.
        ::report_generic_error("(get_type_name) UNKNOWN TYPE");
        return "";
        // throw Error(*this, Environment(), INTERNAL_ERROR);
    }
}

String Value::display() const {
    // DBG("Value::display");
    // dbg("type: " + String(type));

    String result;
    switch (type) {
    case STRING:
        return str;
    case LAMBDA:
        for (size_t i = 0; static_cast<size_t>(i) < list.size(); i++) {
            result += list[i].display();
            if (static_cast<size_t>(i) < list.size() - 1)
                result += " ";
        }
        return "(lambda " + result + ")";
    case LIST:
        for (size_t i = 0; static_cast<size_t>(i) < list.size(); i++) {
            result += list[i].display();
            if (static_cast<size_t>(i) < list.size() - 1)
                result += " ";
        }
        return "(" + result + ")";
    case VECTOR:
        for (size_t i = 0; static_cast<size_t>(i) < list.size(); i++) {
            result += list[i].display();
            if (static_cast<size_t>(i) < list.size() - 1)
                result += " ";
        }
        return "[" + result + "]";
    case BUILTIN:
        // NOTE: should this print the address of the unique
        // pointer or of the thing it's pointing to?
        return "{builtin " + str + " at " +
               String(size_t(&stack_data.builtin)) + "}";
    case BUILTIN_METHOD:
        // NOTE: should this print the address of the unique
        // pointer or of the thing it's pointing to?
        return "{builtin method " + str + " at " +
               String(size_t(&stack_data.builtin_method)) + "}";
    case UNIT:
        return "";
    case ERROR:
        return "{error}";
    default:
        return to_lisp_src();
    }
}

String Value::to_lisp_src() const {
    String result;
    switch (type) {
    case NIL:
        return "nil";
    case QUOTE:
        return "'" + list[0].to_lisp_src();
    case ATOM:
        return str;
    case INT:
        return String(stack_data.i);
    case FLOAT:
        return String(stack_data.f);
    case STRING:
        for (size_t i = 0; i < str.length(); i++) {
            if (str[i] == '"')
                result += "\\\"";
            else
                result += str[i];
        }
        return "\"" + result + "\"";
    case LAMBDA:
        for (size_t i = 0; static_cast<size_t>(i) < list.size(); i++) {
            result += list[i].to_lisp_src();
            if (static_cast<size_t>(i) < list.size() - 1)
                result += " ";
        }
        return "(lambda " + result + ")";
    case LIST:
        for (size_t i = 0; static_cast<size_t>(i) < list.size(); i++) {
            result += list[i].to_lisp_src();
            if (static_cast<size_t>(i) < list.size() - 1)
                result += " ";
        }
        return "(" + result + ")";
    case VECTOR:
        for (size_t i = 0; static_cast<size_t>(i) < list.size(); i++) {
            result += list[i].to_lisp_src();
            if (static_cast<size_t>(i) < list.size() - 1)
                result += " ";
        }
        return "[" + result + "]";
    case UNIT:
        return "@";
        // TODO @correctness this needs to be lexically sound,
        // i.e. referring to the variable that the original did
        // and not to any potential shadowings in the current scope
    case BUILTIN:
    case BUILTIN_METHOD:
        return str;
    default:
        // We don't know how to display whatever type this is.
        // This isn't the users fault, this is just unhandled.
        return "";
    }
}



#pragma GCC diagnostic pop
