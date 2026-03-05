#include "time_manager.h"
#include "../utils.h"

// Max value that size_t can hold before overflow
constexpr TimeValue max_size_t = static_cast<TimeValue>((size_t)-1);

void TimeManager::update()
{
    // Cache previous values
    m_micros_raw_last            = m_micros_raw;
    m_last_known_time_since_boot = m_time_since_boot;

    // 1. Get time-since-boot reading from the board (prefer injected clock)
    if (clock != nullptr)
    {
        m_micros_raw = static_cast<size_t>(clock->micros());
    }
    else
    {
        m_micros_raw = static_cast<size_t>(micros());
    }

    // 2. Check if it has overflowed
    if (m_micros_raw < m_micros_raw_last)
    {
        dbg("INFO: overflow occurred, incrementing counter.");
        m_overflow_counter++;
    }

    // 3. Add an offset according to how many overflows we've had so far
    m_time_since_boot = static_cast<TimeValue>(m_micros_raw) +
                        (max_size_t * static_cast<TimeValue>(m_overflow_counter));

    // 4. Update transport time
    update_transport_time(m_time_since_boot);
}

void TimeManager::reset_transport()
{
    m_last_transport_reset_time = m_time_since_boot;
    m_total_paused_duration     = 0.0;
    m_transport_paused_at       = m_time_since_boot;
    m_last_transport_time       = m_transport_time;
    m_transport_time            = m_transport_time_offset;
}

void TimeManager::play_transport()
{
    if (m_transport_playing)
    {
        return;
    }

    m_total_paused_duration += (m_time_since_boot - m_transport_paused_at);
    m_transport_playing = true;
    update_transport_time(m_time_since_boot);
}

void TimeManager::pause_transport()
{
    if (!m_transport_playing)
    {
        return;
    }

    m_transport_playing = false;
    m_transport_paused_at = m_time_since_boot;
    m_last_transport_time = m_transport_time;
}

void TimeManager::update_transport_time(TimeValue actual_time)
{
    m_last_transport_time = m_transport_time;
    if (!m_transport_playing)
    {
        return;
    }

    m_transport_time = actual_time - m_last_transport_reset_time -
                       m_total_paused_duration + m_transport_time_offset;
}

void TimeManager::set_external_time(TimeValue actual_time)
{
    // Drive internal bookkeeping directly from supplied time source.
    m_last_known_time_since_boot = m_time_since_boot;
    m_time_since_boot            = actual_time;
    m_micros_raw_last            = m_micros_raw;
    m_micros_raw                 = static_cast<size_t>(actual_time);
    update_transport_time(actual_time);
}
