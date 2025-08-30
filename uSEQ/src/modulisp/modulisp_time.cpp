#include "../utils.h"
#include "modulisp.h"

// Max value that size_t can hold before overflow
constexpr TimeValue max_size_t = static_cast<TimeValue>((size_t)-1);

void ModuLispInterpreter::update_time() {
    DBG("ModuLispInterpreter::update_time");

    // Cache previous values
    m_micros_raw_last = m_micros_raw;
    m_last_known_time_since_boot = m_time_since_boot;

    // 1. Get time-since-boot reading from the board (prefer injected clock)
    if (clock != nullptr) {
        m_micros_raw = static_cast<size_t>(clock->micros());
    } else {
        m_micros_raw = static_cast<size_t>(micros());
    }

    // 2. Check if it has overflowed
    if (m_micros_raw < m_micros_raw_last) {
        dbg("INFO: overflow occurred, incrementing counter.");
        m_overflow_counter++;
    }

    // 3. Add an offset according to how many overflows we've had so far
    m_time_since_boot =
        static_cast<TimeValue>(m_micros_raw) +
        (max_size_t * static_cast<TimeValue>(m_overflow_counter));

    update_logical_time(m_time_since_boot);
}

void ModuLispInterpreter::reset_logical_time() {
    m_last_transport_reset_time = m_time_since_boot;
    update_logical_time(m_time_since_boot);
}

void ModuLispInterpreter::update_logical_time(TimeValue actual_time) {
    DBG("ModuLispInterpreter::set_time");

    // Update the main UI timekeeping variable
    m_transport_time = actual_time - m_last_transport_reset_time;

    // Phasors
    m_beat_phase = beat_at_time(m_transport_time);
    m_current_beat_num = beat_num_at_time(m_transport_time);
    m_bar_phase = bar_at_time(m_transport_time);
    m_current_bar_num = bar_num_at_time(m_transport_time);
    m_phrase_phase = phrase_at_time(m_transport_time);
    m_section_phase = section_at_time(m_transport_time);

    // Push them to the interpreter
    update_lisp_time_variables();
}

void ModuLispInterpreter::update_lisp_time_variables() {
    DBG("ModuLispInterpreter::update_lisp_time_variables");

    // These should appear as seconds in Lisp-land
    TimeValue time_s = m_time_since_boot * 1e-6;
    TimeValue t_s = m_transport_time * 1e-6;
    set("time", Value(time_s));
    set("t", Value(t_s));

    // dbg("time_s = " + String(time_s));
    // dbg("t_s = " + String(t_s));
    // dbg("norm_beat = " + String(norm_beat));
    // dbg("norm_bar = " + String(norm_bar));
    // dbg("norm_phrase = " + String(norm_phrase));
    // dbg("norm_section = " + String(norm_section));

    set("beat", Value(m_beat_phase));
    set("beat-num", Value(static_cast<int>(m_current_beat_num)));
    set("bar", Value(m_bar_phase));
    set("bar-num", Value(static_cast<int>(m_current_bar_num)));
    set("phrase", Value(m_phrase_phase));
    set("section", Value(m_section_phase));
}

double bpm_to_micros_per_beat(double bpm) {
    // 60 seconds * 1e+6 microseconds
    constexpr double micros_in_minute = 60e+6;
    return micros_in_minute / bpm;
}

void ModuLispInterpreter::set_bpm(double newBpm, double changeThreshold = 0.0) {
    DBG("ModuLispInterpreter::setBPM");

    if (newBpm <= 0.0) {
        report_generic_error("Invalid BPM requested: " + String(newBpm));
    } else if (fabs(newBpm - m_bpm) >= changeThreshold) {
        m_bpm = newBpm;
        // Derive phasor lengths (in micros)
        m_beat_length = bpm_to_micros_per_beat(newBpm);
        m_bar_length =
            m_beat_length * (4.0 / meter_denominator) * meter_numerator;
        m_bar_length =
            m_beat_length * (4.0 / meter_denominator) * meter_numerator;
        m_phrase_length = m_bar_length * m_bars_per_phrase;
        m_section_length = m_phrase_length * m_phrases_per_section;

        update_bpm_variables();
    }
}

void ModuLispInterpreter::update_bpm_variables() {
    DBG("ModuLispInterpreter::update_bpm_variables");

    set("bpm", Value(m_bpm));
    set("bps", Value(m_bpm / 60.0));
    // These should appear as seconds in Lisp-land
    set("beat-dur", Value(m_beat_length * 1e-6));
    set("bar-dur", Value(m_bar_length * 1e-6));
    set("phrase-dur", Value(m_phrase_length * 1e-6));
    set("section-dur", Value(m_section_length * 1e-6));
}

void ModuLispInterpreter::set_time_sig(double numerator, double denominator) {
    meter_denominator = denominator;
    meter_numerator = numerator;
    // This will refresh the phasor durations
    // with the new meter
    set_bpm(m_bpm, 0.0);
}

PhaseValue ModuLispInterpreter::beat_at_time(TimeValue time) {
    return fmod(time, m_beat_length) / m_beat_length;
}

uint32_t ModuLispInterpreter::beat_num_at_time(TimeValue time) {
    return static_cast<uint32_t>(time / m_beat_length);
}

PhaseValue ModuLispInterpreter::bar_at_time(TimeValue time) {
    // println("time: " + String(time));
    // println("m_bar_length: " + String(time));
    return fmod(time, m_bar_length) / m_bar_length;
}

uint32_t ModuLispInterpreter::bar_num_at_time(TimeValue time) {
    return static_cast<uint32_t>(time / m_bar_length);
}

PhaseValue ModuLispInterpreter::phrase_at_time(TimeValue time) {
    return fmod(time, m_phrase_length) / m_phrase_length;
}

PhaseValue ModuLispInterpreter::section_at_time(TimeValue time) {
    return fmod(time, m_section_length) / m_section_length;
}

// SYNC FUNCTIONS
