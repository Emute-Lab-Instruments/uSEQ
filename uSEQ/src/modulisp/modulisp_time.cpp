#include "../utils.h"
#include "modulisp.h"

// Max value that size_t can hold before overflow
constexpr TimeValue max_size_t = static_cast<TimeValue>((size_t)-1);

void ModuLispInterpreter::update_time() {
    DBG("ModuLispInterpreter::update_time");
    
    // Delegate to TimeManager
    m_time_manager->update();
    
    // Update logical time with the new value
    update_logical_time(m_time_manager->get_time_since_boot());
    
    // Sync compatibility layer
    sync_compatibility_layer();
}

void ModuLispInterpreter::reset_logical_time() {
    // Delegate to TimeManager
    m_time_manager->reset_transport();
    
    // Update logical time
    update_logical_time(m_time_manager->get_time_since_boot());
    
    // Sync compatibility layer
    sync_compatibility_layer();
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
    TimeValue time_s = m_time_manager->get_time_seconds();
    TimeValue t_s = m_time_manager->get_transport_seconds();
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
    } else {
        // Delegate to PhasorManager
        m_phasor_manager->set_bpm(newBpm, changeThreshold);
        
        // Sync compatibility layer  
        sync_compatibility_layer();

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
    // Delegate to PhasorManager
    m_phasor_manager->set_time_signature(numerator, denominator);
    
    // Sync compatibility layer
    sync_compatibility_layer();
    
    // Update LISP variables
    update_lisp_time_variables();
}

// Phasor calculations are now delegated to PhasorManager via inline functions in the header

// SYNC FUNCTIONS
