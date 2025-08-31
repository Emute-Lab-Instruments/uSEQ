#include "phasor_manager.h"
#include "../utils.h"
#include <cmath>

void PhasorManager::set_bpm(double new_bpm, double change_threshold) {
    // Only update if change is significant
    if (std::abs(new_bpm - m_tempo.bpm) > change_threshold) {
        m_tempo.bpm = new_bpm;
        update_from_tempo();
    }
}

void PhasorManager::set_time_signature(double numerator, double denominator) {
    m_meter.numerator = numerator;
    m_meter.denominator = denominator;
    update_from_tempo();
}

void PhasorManager::update_from_tempo() {
    // Calculate beat length in microseconds
    // 60 seconds / BPM * 1,000,000 microseconds/second
    m_lengths.beat_length = (60.0 / m_tempo.bpm) * 1000000.0;
    
    // Bar length depends on meter
    m_lengths.bar_length = m_lengths.beat_length * m_meter.numerator;
    
    // Phrase and section lengths
    m_lengths.phrase_length = m_lengths.bar_length * m_bars_per_phrase;
    m_lengths.section_length = m_lengths.phrase_length * m_phrases_per_section;
}

PhaseValue PhasorManager::beat_at_time(TimeValue time) const {
    if (m_lengths.beat_length <= 0) return 0.0;
    return std::fmod(time / m_lengths.beat_length, 1.0);
}

uint32_t PhasorManager::beat_num_at_time(TimeValue time) const {
    if (m_lengths.beat_length <= 0) return 0;
    return static_cast<uint32_t>(time / m_lengths.beat_length);
}

PhaseValue PhasorManager::bar_at_time(TimeValue time) const {
    if (m_lengths.bar_length <= 0) return 0.0;
    return std::fmod(time / m_lengths.bar_length, 1.0);
}

uint32_t PhasorManager::bar_num_at_time(TimeValue time) const {
    if (m_lengths.bar_length <= 0) return 0;
    return static_cast<uint32_t>(time / m_lengths.bar_length);
}

PhaseValue PhasorManager::phrase_at_time(TimeValue time) const {
    if (m_lengths.phrase_length <= 0) return 0.0;
    return std::fmod(time / m_lengths.phrase_length, 1.0);
}

PhaseValue PhasorManager::section_at_time(TimeValue time) const {
    if (m_lengths.section_length <= 0) return 0.0;
    return std::fmod(time / m_lengths.section_length, 1.0);
}

void PhasorManager::update_phasor_state(TimeValue time, PhasorState& state) const {
    state.beat_phase = beat_at_time(time);
    state.bar_phase = bar_at_time(time);
    state.phrase_phase = phrase_at_time(time);
    state.section_phase = section_at_time(time);
    state.current_beat_num = beat_num_at_time(time);
    state.current_bar_num = bar_num_at_time(time);
}