#ifndef PHASOR_MANAGER_H_
#define PHASOR_MANAGER_H_

#include <cstdint>

using TimeValue = double;
using PhaseValue = double;

// Settings structures for cleaner parameter passing
struct MeterSettings {
    double numerator = 4;
    double denominator = 4;
};

struct TempoSettings {
    double bpm = 130;
    double default_bpm = 130;
};

struct PhasorLengths {
    TimeValue beat_length = 0.0;    // in microseconds
    TimeValue bar_length = 0.0;     // in microseconds  
    TimeValue phrase_length = 0.0;  // in microseconds
    TimeValue section_length = 0.0; // in microseconds
};

struct PhasorState {
    PhaseValue beat_phase = 0.0;
    PhaseValue bar_phase = 0.0;
    PhaseValue phrase_phase = 0.0;
    PhaseValue section_phase = 0.0;
    uint32_t current_beat_num = 0;
    uint32_t current_bar_num = 0;
};

// Manages phasor calculations and musical time divisions
class PhasorManager {
public:
    PhasorManager() { update_from_tempo(); }
    
    // Update BPM and recalculate lengths
    void set_bpm(double new_bpm, double change_threshold);
    
    // Update time signature and recalculate lengths
    void set_time_signature(double numerator, double denominator);
    
    // Update structural parameters
    void set_bars_per_phrase(double bars) { m_bars_per_phrase = bars; update_from_tempo(); }
    void set_phrases_per_section(double phrases) { m_phrases_per_section = phrases; update_from_tempo(); }
    
    // Calculate phasor values at a given time
    PhaseValue beat_at_time(TimeValue time) const;
    uint32_t beat_num_at_time(TimeValue time) const;
    PhaseValue bar_at_time(TimeValue time) const;
    uint32_t bar_num_at_time(TimeValue time) const;
    PhaseValue phrase_at_time(TimeValue time) const;
    PhaseValue section_at_time(TimeValue time) const;
    
    // Update current phasor state for a given time
    void update_phasor_state(TimeValue time, PhasorState& state) const;
    
    // Getters
    const TempoSettings& get_tempo() const { return m_tempo; }
    const MeterSettings& get_meter() const { return m_meter; }
    const PhasorLengths& get_lengths() const { return m_lengths; }
    double get_bars_per_phrase() const { return m_bars_per_phrase; }
    double get_phrases_per_section() const { return m_phrases_per_section; }
    
private:
    TempoSettings m_tempo;
    MeterSettings m_meter;
    PhasorLengths m_lengths;
    
    double m_bars_per_phrase = 16;
    double m_phrases_per_section = 16;
    
    // Recalculate all lengths based on current tempo and meter
    void update_from_tempo();
};

#endif // PHASOR_MANAGER_H_