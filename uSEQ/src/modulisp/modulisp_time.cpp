#include "../utils.h"
#include "modulisp_interpreter.h"

void ModuLispInterpreter::update_time() {
    DBG("ModuLispInterpreter::update_time");
    
    // Delegate to TimeManager
    m_time_manager->update();
    
    // Update logical time with the new value
    update_logical_time(m_time_manager->get_time_since_boot());
}

void ModuLispInterpreter::reset_logical_time() {
    // Delegate to TimeManager
    m_time_manager->reset_transport();
    
    // Update logical time
    update_logical_time(m_time_manager->get_time_since_boot());
}

void ModuLispInterpreter::update_logical_time(TimeValue actual_time) {
    DBG("ModuLispInterpreter::update_logical_time");
    
    // Update phasor state
    update_phasor_state();
    
    // Push them to the interpreter
    update_lisp_time_variables();
}

void ModuLispInterpreter::update_lisp_time_variables() {
    DBG("ModuLispInterpreter::update_lisp_time_variables");
    
    // These should appear as seconds in Lisp-land
    TimeValue time_s = m_time_manager->get_time_seconds();
    TimeValue t_s = m_time_manager->get_transport_seconds();
    get_environment()->set("time", Value(time_s));
    get_environment()->set("t", Value(t_s));
    
    // Update phasor variables
    get_environment()->set("beat", Value(m_current_phasor_state.beat_phase));
    get_environment()->set("bar", Value(m_current_phasor_state.bar_phase));
    get_environment()->set("phrase", Value(m_current_phasor_state.phrase_phase));
    get_environment()->set("section", Value(m_current_phasor_state.section_phase));
    
    // Update beat/bar numbers
    get_environment()->set("beatNum", Value(static_cast<int>(m_current_phasor_state.current_beat_num)));
    get_environment()->set("barNum", Value(static_cast<int>(m_current_phasor_state.current_bar_num)));
    
    // Update duration variables (in seconds)
    const auto& lengths = m_phasor_manager->get_lengths();
    get_environment()->set("beat-dur", Value(lengths.beat_length / 1000000.0));
    get_environment()->set("bar-dur", Value(lengths.bar_length / 1000000.0));
    get_environment()->set("phrase-dur", Value(lengths.phrase_length / 1000000.0));
    get_environment()->set("section-dur", Value(lengths.section_length / 1000000.0));
    
    // Aliases
    get_environment()->set("beatDur", Value(lengths.beat_length / 1000000.0));
    get_environment()->set("barDur", Value(lengths.bar_length / 1000000.0));
    get_environment()->set("phraseDur", Value(lengths.phrase_length / 1000000.0));
    get_environment()->set("sectionDur", Value(lengths.section_length / 1000000.0));
}

void ModuLispInterpreter::update_logical_time_variables(TimeValue t) {
    update_logical_time(t);
}

static inline double bpm_to_micros_per_beat(double bpm) {
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
        
        // Update LISP variables
        update_bpm_variables();
    }
}

void ModuLispInterpreter::update_bpm_variables() {
    DBG("ModuLispInterpreter::update_bpm_variables");
    
    // Update duration variables in LISP environment
    const auto& lengths = m_phasor_manager->get_lengths();
    
    get_environment()->set("beat-dur", Value(lengths.beat_length * 1e-6));
    get_environment()->set("bar-dur", Value(lengths.bar_length * 1e-6));
    get_environment()->set("phrase-dur", Value(lengths.phrase_length * 1e-6));
    get_environment()->set("section-dur", Value(lengths.section_length * 1e-6));
    
    // Aliases
    get_environment()->set("beatDur", Value(lengths.beat_length * 1e-6));
    get_environment()->set("barDur", Value(lengths.bar_length * 1e-6));
    get_environment()->set("phraseDur", Value(lengths.phrase_length * 1e-6));
    get_environment()->set("sectionDur", Value(lengths.section_length * 1e-6));
}

void ModuLispInterpreter::set_time_sig(double numerator, double denominator) {
    // Delegate to PhasorManager
    m_phasor_manager->set_time_signature(numerator, denominator);
    
    // Update LISP variables
    update_lisp_time_variables();
}

// Environment creation for time-based evaluation
Environment ModuLispInterpreter::make_env_for_time(TimeValue time) {
    Environment env(*get_environment());  // Start with current environment
    
    // Calculate phasor values at the specified time
    PhasorState state_at_time;
    m_phasor_manager->update_phasor_state(time, state_at_time);
    
    // Set time-specific variables
    env.set("t", Value(time / 1000000.0));
    env.set("beat", Value(state_at_time.beat_phase));
    env.set("bar", Value(state_at_time.bar_phase));
    env.set("phrase", Value(state_at_time.phrase_phase));
    env.set("section", Value(state_at_time.section_phase));
    env.set("beatNum", Value(static_cast<int>(state_at_time.current_beat_num)));
    env.set("barNum", Value(static_cast<int>(state_at_time.current_bar_num)));
    
    return env;
}

Environment ModuLispInterpreter::make_env_with_updated_time_durs(const Environment& env, TimeValue time) {
    Environment new_env(env);
    
    // Update duration variables based on current tempo settings
    const auto& lengths = m_phasor_manager->get_lengths();
    new_env.set("beatDur", Value(lengths.beat_length / 1000000.0));
    new_env.set("barDur", Value(lengths.bar_length / 1000000.0));
    new_env.set("phraseDur", Value(lengths.phrase_length / 1000000.0));
    new_env.set("sectionDur", Value(lengths.section_length / 1000000.0));
    
    return new_env;
}