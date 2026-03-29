#ifndef TEST_DSP_HELPERS_H
#define TEST_DSP_HELPERS_H

#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <limits>

namespace dsp {

// ── Basic Statistics ──────────────────────────────────────────────────────

inline double rms(const std::vector<double>& buf)
{
    if (buf.empty()) return 0.0;
    double sum_sq = 0.0;
    for (double v : buf) sum_sq += v * v;
    return std::sqrt(sum_sq / static_cast<double>(buf.size()));
}

inline double dc_offset(const std::vector<double>& buf)
{
    if (buf.empty()) return 0.0;
    double sum = 0.0;
    for (double v : buf) sum += v;
    return sum / static_cast<double>(buf.size());
}

inline double peak_to_peak(const std::vector<double>& buf)
{
    if (buf.empty()) return 0.0;
    auto [mn, mx] = std::minmax_element(buf.begin(), buf.end());
    return *mx - *mn;
}

inline double max_value(const std::vector<double>& buf)
{
    if (buf.empty()) return 0.0;
    return *std::max_element(buf.begin(), buf.end());
}

inline double min_value(const std::vector<double>& buf)
{
    if (buf.empty()) return 0.0;
    return *std::min_element(buf.begin(), buf.end());
}

// ── Range Checks ──────────────────────────────────────────────────────────

inline bool is_within_range(const std::vector<double>& buf, double lo, double hi)
{
    return std::all_of(buf.begin(), buf.end(),
                       [lo, hi](double v) { return v >= lo && v <= hi; });
}

inline bool all_finite(const std::vector<double>& buf)
{
    return std::all_of(buf.begin(), buf.end(),
                       [](double v) { return std::isfinite(v); });
}

inline bool is_constant(const std::vector<double>& buf, double tolerance = 1e-9)
{
    if (buf.size() < 2) return true;
    double first = buf[0];
    return std::all_of(buf.begin(), buf.end(),
                       [first, tolerance](double v) {
                           return std::abs(v - first) <= tolerance;
                       });
}

inline bool is_approximately(double actual, double expected, double tolerance = 0.01)
{
    return std::abs(actual - expected) <= tolerance;
}

// ── Frequency Analysis ────────────────────────────────────────────────────

// Count zero crossings (threshold crossings) in the buffer.
// Uses hysteresis to avoid counting noise.
inline int count_crossings(const std::vector<double>& buf, double threshold = 0.5,
                           double hysteresis = 0.01)
{
    if (buf.size() < 2) return 0;
    int crossings = 0;
    bool above = buf[0] > threshold;
    for (size_t i = 1; i < buf.size(); i++) {
        if (above && buf[i] < threshold - hysteresis) {
            above = false;
            crossings++;
        } else if (!above && buf[i] > threshold + hysteresis) {
            above = true;
            crossings++;
        }
    }
    return crossings;
}

// Estimate frequency from zero-crossing rate.
// Returns Hz given sample_rate in Hz.
inline double frequency_estimate(const std::vector<double>& buf, double sample_rate,
                                 double threshold = 0.5)
{
    int crossings = count_crossings(buf, threshold);
    if (crossings < 2) return 0.0;
    // Each full cycle has 2 crossings (up + down)
    double duration = static_cast<double>(buf.size()) / sample_rate;
    return static_cast<double>(crossings) / (2.0 * duration);
}

// Count rising edges (transitions from below to above threshold).
inline int count_rising_edges(const std::vector<double>& buf, double threshold = 0.5)
{
    if (buf.size() < 2) return 0;
    int edges = 0;
    for (size_t i = 1; i < buf.size(); i++) {
        if (buf[i - 1] <= threshold && buf[i] > threshold) edges++;
    }
    return edges;
}

// Count falling edges.
inline int count_falling_edges(const std::vector<double>& buf, double threshold = 0.5)
{
    if (buf.size() < 2) return 0;
    int edges = 0;
    for (size_t i = 1; i < buf.size(); i++) {
        if (buf[i - 1] > threshold && buf[i] <= threshold) edges++;
    }
    return edges;
}

// ── Correlation ───────────────────────────────────────────────────────────

// Normalized cross-correlation between two equal-length signals.
// Returns value in [-1, 1], where 1 = identical shape.
inline double correlate(const std::vector<double>& a, const std::vector<double>& b)
{
    if (a.size() != b.size() || a.empty()) return 0.0;
    double mean_a = dc_offset(a);
    double mean_b = dc_offset(b);
    double sum_ab = 0.0, sum_aa = 0.0, sum_bb = 0.0;
    for (size_t i = 0; i < a.size(); i++) {
        double da = a[i] - mean_a;
        double db = b[i] - mean_b;
        sum_ab += da * db;
        sum_aa += da * da;
        sum_bb += db * db;
    }
    double denom = std::sqrt(sum_aa * sum_bb);
    if (denom < 1e-15) return 0.0;
    return sum_ab / denom;
}

// ── Waveform Generation (for expected-value comparison) ───────────────────

inline std::vector<double> generate_sine(size_t length, double frequency,
                                         double sample_rate,
                                         double amplitude = 0.5,
                                         double offset = 0.5)
{
    std::vector<double> buf(length);
    for (size_t i = 0; i < length; i++) {
        double t = static_cast<double>(i) / sample_rate;
        buf[i] = offset + amplitude * std::sin(2.0 * M_PI * frequency * t);
    }
    return buf;
}

inline std::vector<double> generate_ramp(size_t length, double start, double end)
{
    std::vector<double> buf(length);
    for (size_t i = 0; i < length; i++) {
        double frac = static_cast<double>(i) / static_cast<double>(length);
        buf[i] = start + (end - start) * frac;
    }
    return buf;
}

// ── Segment Analysis ──────────────────────────────────────────────────────

// Check if a sub-range of the buffer is approximately constant at a given value.
inline bool segment_is(const std::vector<double>& buf, size_t from, size_t to,
                       double expected, double tolerance = 0.05)
{
    if (to > buf.size()) to = buf.size();
    for (size_t i = from; i < to; i++) {
        if (std::abs(buf[i] - expected) > tolerance) return false;
    }
    return true;
}

// Check if a buffer is monotonically increasing.
inline bool is_monotonic_increasing(const std::vector<double>& buf, double tolerance = 0.0)
{
    for (size_t i = 1; i < buf.size(); i++) {
        if (buf[i] < buf[i - 1] - tolerance) return false;
    }
    return true;
}

} // namespace dsp

#endif // TEST_DSP_HELPERS_H
