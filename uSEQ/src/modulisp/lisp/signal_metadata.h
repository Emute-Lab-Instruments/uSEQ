#ifndef SIGNAL_METADATA_H_
#define SIGNAL_METADATA_H_

#include <cmath>
#include <string>

// Metadata for control-rate automation signals
struct SignalMetadata
{
    // Core Properties
    bool is_const                = true;      // false if value depends on time
    double min_val               = -INFINITY; // minimum possible value
    double max_val               = INFINITY;  // maximum possible value
    bool min_inclusive           = true;      // whether min_val is inclusive
    bool max_inclusive           = true;      // whether max_val is inclusive
    bool is_periodic             = false;     // whether signal repeats
    double period                = 0.0;       // repetition period (if periodic)
    bool is_monotonic            = false;     // whether always increasing/decreasing
    bool is_monotonic_increasing = false;     // specifically increasing
    bool is_monotonic_decreasing = false;     // specifically decreasing

    // Continuity and Smoothness
    bool is_continuous      = true;  // no jump discontinuities
    bool is_smooth          = true;  // smooth interpolation possible
    bool is_stepped         = false; // discrete step changes only
    bool is_linear_segments = false; // piecewise linear

    // Time-Based Properties
    double time_start    = -INFINITY; // earliest defined time
    double time_end      = INFINITY;  // latest defined time
    bool is_causal       = true;      // output depends only on current/past
    bool is_memoryless   = true;      // output depends only on current input
    double memory_length = 0.0;       // length of memory (seconds)

    // Zero Crossing Analysis
    bool has_zero_crossings            = false; // signal crosses zero
    double zero_crossing_rate          = 0.0;   // average zero crossings per second
    bool zero_crossing_locations_known = false; // exact crossings computed

    // Threshold Analysis
    bool supports_threshold_queries = false; // can find threshold crossings
    double min_threshold_resolution = 0.0;   // smallest detectable change

    // Range Analysis
    bool supports_range_queries   = false; // can find value ranges over time
    double range_query_resolution = 0.0;   // time resolution for range queries

    // Extrema Analysis
    bool has_local_extrema             = false; // has peaks/valleys
    bool extrema_locations_known       = false; // exact extrema computed
    double extrema_detection_threshold = 0.0;   // minimum change for extrema

    // Interpolation Properties
    enum InterpolationType
    {
        NONE    = 0, // no interpolation support
        LINEAR  = 1, // linear interpolation
        SMOOTH  = 2, // smooth/spline interpolation
        STEPPED = 3  // hold previous value
    } interpolation_type = LINEAR;

    // Periodicity Details
    double phase_offset = 0.0;   // phase offset for periodic signals
    bool phase_locked   = false; // phase relationship maintained

    // Quantization (kept for discrete automation)
    bool is_quantized      = false; // takes discrete values only
    bool is_integer_valued = false; // all values are integers
    double quantum_step    = 0.0;   // quantization step size

    // Domain validation
    bool has_domain_error     = false; // domain/range errors present
    std::string error_message = "";    // error description

    // Optimization hints
    bool can_constant_fold        = false; // can be evaluated at compile time
    bool can_cache_results        = false; // results can be cached
    bool is_expensive_to_evaluate = false; // computation cost hint
};

#endif // SIGNAL_METADATA_H_