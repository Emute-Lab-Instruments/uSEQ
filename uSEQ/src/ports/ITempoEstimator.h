// Tempo estimation port interface for dependency injection and testing
#pragma once

#include <cstdint>

struct ITempoEstimator {
    virtual ~ITempoEstimator() = default;
    
    // Get the estimated BPM based on timing of input pulses
    virtual double averageBPM(double micros) = 0;
    
    // Get the standard deviation of the tempo estimation
    virtual double std() = 0;
    
    // Get the current average BPM value
    virtual double getAvgBPM() const = 0;
};