#include "uSEQ.h"

// Max value that size_t can hold before overflow
constexpr TimeValue max_size_t = static_cast<TimeValue>((size_t)-1);

void uSEQ::update_time()
{
    DBG("uSEQ::update_time");

    // Cache previous values
    m_micros_raw_last            = m_micros_raw;
    m_last_known_time_since_boot = m_time_since_boot;

    // 1. Get time-since-boot reading from the board
    m_micros_raw = micros();

    // 2. Check if it has overflowed
    if (m_micros_raw < m_micros_raw_last)
    {
        dbg("INFO: overflow occurred, incrementing counter.");
        m_overflow_counter++;
    }

    // 3. Add an offset according to how many overflows we've had so far
    m_time_since_boot = static_cast<TimeValue>(m_micros_raw) +
                        (max_size_t * static_cast<TimeValue>(m_overflow_counter));

    update_logical_time(m_time_since_boot);
}

void uSEQ::reset_logical_time()
{
    m_last_transport_reset_time = m_time_since_boot;
    update_logical_time(m_time_since_boot);
}

void uSEQ::update_logical_time(TimeValue actual_time)
{
    DBG("uSEQ::set_time");

    // Update the main UI timekeeping variable
    m_transport_time = actual_time - m_last_transport_reset_time;

    // Phasors
    m_beat_phase    = beat_at_time(m_transport_time);
    m_bar_phase     = bar_at_time(m_transport_time);
    m_phrase_phase  = phrase_at_time(m_transport_time);
    m_section_phase = section_at_time(m_transport_time);

    // Push them to the interpreter
    update_lisp_time_variables();
}

PhaseValue uSEQ::beat_at_time(TimeValue time)
{
    return fmod(time, m_beat_length) / m_beat_length;
}

PhaseValue uSEQ::bar_at_time(TimeValue time)
{
    // println("time: " + String(time));
    // println("m_bar_length: " + String(time));
    return fmod(time, m_bar_length) / m_bar_length;
}

PhaseValue uSEQ::phrase_at_time(TimeValue time)
{
    return fmod(time, m_phrase_length) / m_phrase_length;
}

PhaseValue uSEQ::section_at_time(TimeValue time)
{
    return fmod(time, m_section_length) / m_section_length;
}