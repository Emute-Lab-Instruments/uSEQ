#include "uSEQ.h"

#include <cmath>

double bpm_to_micros_per_beat(double bpm)
{
    // 60 seconds * 1e+6 microseconds
    constexpr double micros_in_minute = 60e+6;
    return micros_in_minute / bpm;
}

void uSEQ::set_bpm(double newBpm, double changeThreshold)
{
    DBG("uSEQ::setBPM");

    if (newBpm <= 0.0)
    {
        report_generic_error("Invalid BPM requested: " + String(newBpm));
    }
    else if (fabs(newBpm - m_bpm) >= changeThreshold)
    {
        m_bpm = newBpm;
        // Derive phasor lengths (in micros)
        m_beat_length = bpm_to_micros_per_beat(newBpm);
        m_bar_length  = m_beat_length * (4.0 / meter_denominator) * meter_numerator;
        m_bar_length  = m_beat_length * (4.0 / meter_denominator) * meter_numerator;
        m_phrase_length  = m_bar_length * m_bars_per_phrase;
        m_section_length = m_phrase_length * m_phrases_per_section;

        update_bpm_variables();
    }
}

void uSEQ::update_bpm_variables()
{
    DBG("uSEQ::update_bpm_variables");

    set("bpm", Value(m_bpm));
    set("bps", Value(m_bpm / 60.0));
    // These should appear as seconds in Lisp-land
    set("beatDur", Value(m_beat_length * 1e-6));
    set("barDur", Value(m_bar_length * 1e-6));
    set("phraseDur", Value(m_phrase_length * 1e-6));
    set("sectionDur", Value(m_section_length * 1e-6));
}

void uSEQ::set_time_sig(double numerator, double denominator)
{
    meter_denominator = denominator;
    meter_numerator   = numerator;
    // This will refresh the phasor durations
    // with the new meter
    set_bpm(m_bpm, 0.0);
}