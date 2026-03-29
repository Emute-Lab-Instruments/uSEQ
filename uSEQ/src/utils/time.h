#ifndef TIME_H_
#define TIME_H_

// Platform-independent system time utility.
// Returns seconds since boot as a double.

#if defined(__EMSCRIPTEN__)

#include <emscripten.h>

inline double get_system_time_seconds()
{
    return emscripten_get_now() / 1000.0;
}

#elif defined(USE_STD_IO)

#include <chrono>

// Test override: when non-null, get_system_time_seconds() returns *this
// instead of the real clock. Set from test harnesses for deterministic time.
inline double* g_test_time_ptr = nullptr;

inline double get_system_time_seconds()
{
    if (g_test_time_ptr) return *g_test_time_ptr;
    static const auto start = std::chrono::steady_clock::now();
    auto now                = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - start;
    return elapsed.count();
}

#else // Arduino / RP2040

#include <Arduino.h>

inline double get_system_time_seconds()
{
    return micros() / 1e6;
}

#endif

#endif // TIME_H_
