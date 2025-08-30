#ifndef VALUE_SIGNAL_PROCESSING_H_
#define VALUE_SIGNAL_PROCESSING_H_

#include <vector>
#include <utility>

// Forward declaration to avoid circular dependency
class Value;

namespace ValueSignalProcessing {
    ////////////////////////////////////////////////////////////////////////////////
    /// TRANSCENDENTAL AND MATHEMATICAL FUNCTIONS
    /// ////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    
    // Trigonometric functions
    Value sin(const Value& v);
    Value cos(const Value& v);
    
    // Exponential and logarithmic functions
    Value exp(const Value& v);
    Value log(const Value& v);
    
    // Root functions
    Value sqrt(const Value& v);
    
    // Other mathematical functions
    Value abs(const Value& v);
    Value floor(const Value& v);
    Value round(const Value& v);
    Value sign(const Value& v);
    Value step(const Value& v);
    
    // Power operation with metadata support
    Value pow(const Value& base, const Value& exponent);
    
    ////////////////////////////////////////////////////////////////////////////////
    /// SIGNAL PROCESSING FUNCTIONS
    /// ////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    
    // Signal processing operations
    Value integrate(const Value& v);
    Value delay(const Value& v, const Value& time);
    Value moving_average(const Value& v, const Value& window);

    ////////////////////////////////////////////////////////////////////////////////
    /// AUTOMATION ANALYSIS METHODS
    /// ////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    
    // Zero Crossing Analysis
    std::vector<double> find_zero_crossings(const Value& v, double start_time, double end_time);
    double get_next_zero_crossing(const Value& v, double from_time);
    bool has_zero_crossings_in_range(const Value& v, double start_time, double end_time);
    
    // Threshold Analysis
    std::vector<double> find_threshold_crossings(const Value& v, double threshold, double start_time, double end_time);
    double get_next_threshold_crossing(const Value& v, double threshold, double from_time, bool rising_edge = true);
    
    // Range Analysis
    std::vector<std::pair<double, double>> find_value_ranges(const Value& v, double min_val, double max_val, double start_time, double end_time);
    bool is_in_range(const Value& v, double min_val, double max_val, double at_time);
    double get_time_in_range(const Value& v, double min_val, double max_val, double start_time, double end_time);
    
    // Extrema Analysis
    std::vector<double> find_local_maxima(const Value& v, double start_time, double end_time);
    std::vector<double> find_local_minima(const Value& v, double start_time, double end_time);
    double get_global_maximum_time(const Value& v, double start_time, double end_time);
    double get_global_minimum_time(const Value& v, double start_time, double end_time);
    
    // Monotonicity Analysis
    bool is_monotonic_in_range(const Value& v, double start_time, double end_time);
    bool is_increasing_in_range(const Value& v, double start_time, double end_time);
    bool is_decreasing_in_range(const Value& v, double start_time, double end_time);

} // namespace ValueSignalProcessing

#endif // VALUE_SIGNAL_PROCESSING_H_