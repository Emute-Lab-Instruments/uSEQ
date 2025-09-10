// Lightweight clock port interface for dependency injection and testing
#pragma once

#include <cstdint>

struct IClock
{
    virtual ~IClock() = default;
    // Microseconds and milliseconds since boot (monotonic)
    virtual uint64_t micros() = 0;
    virtual uint64_t millis() = 0;
};
