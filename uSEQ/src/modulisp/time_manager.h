#ifndef TIME_MANAGER_H_
#define TIME_MANAGER_H_

#include <cstdint>
#include "../ports/IClock.h"

using TimeValue = double;

// Manages all time-related state and calculations
class TimeManager {
public:
    explicit TimeManager(IClock* clk = nullptr) : clock(clk) {}

    // Main update function - call this to update all time values
    void update();
    
    // Reset transport time to current time
    void reset_transport();
    
    // Set transport time offset for nudging
    void set_transport_offset(TimeValue offset) { 
        m_transport_time_offset = offset; 
    }
    
    // Getters for various time values
    TimeValue get_time_since_boot() const { return m_time_since_boot; }
    TimeValue get_transport_time() const { return m_transport_time; }
    TimeValue get_last_transport_time() const { return m_last_transport_time; }
    TimeValue get_transport_offset() const { return m_transport_time_offset; }
    
    // Get time in seconds (for LISP environment)
    TimeValue get_time_seconds() const { return m_time_since_boot / 1000000.0; }
    TimeValue get_transport_seconds() const { return m_transport_time / 1000000.0; }

private:
    // Clock interface for dependency injection
    IClock* clock = nullptr;
    
    // Raw time tracking with overflow handling
    uint8_t m_overflow_counter = 0;
    size_t m_micros_raw = 0;
    size_t m_micros_raw_last = 0;
    
    // Time values in microseconds
    TimeValue m_time_since_boot = 0.0;
    TimeValue m_last_known_time_since_boot = -1;
    TimeValue m_last_transport_reset_time = 0.0;
    TimeValue m_transport_time = 0.0;
    TimeValue m_transport_time_offset = 0.0;
    TimeValue m_last_transport_time = 0.0;
    
    // Helper to update transport time based on current time
    void update_transport_time(TimeValue actual_time);
};

#endif // TIME_MANAGER_H_