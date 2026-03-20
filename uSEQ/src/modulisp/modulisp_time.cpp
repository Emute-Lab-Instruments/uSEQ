#include "../utils.h"
#include "modulisp_interpreter.h"

void ModuLispInterpreter::update_time()
{
    DBG("ModuLispInterpreter::update_time");

    // Delegate to TimeManager
    m_time_manager->update();

    // Update logical time with the new value
    update_logical_time(m_time_manager->get_time_since_boot());
}

void ModuLispInterpreter::reset_logical_time()
{
    // Delegate to TimeManager
    m_time_manager->reset_transport();

    // Update logical time
    update_logical_time(m_time_manager->get_time_since_boot());
}

void ModuLispInterpreter::update_logical_time(TimeValue actual_time)
{
    DBG("ModuLispInterpreter::update_logical_time");

    // Push them to the interpreter
    update_lisp_time_variables();
}

void ModuLispInterpreter::update_lisp_time_variables()
{
    DBG("ModuLispInterpreter::update_lisp_time_variables");

    TimeValue time_s = m_time_manager->get_time_seconds();
    TimeValue t_s    = m_time_manager->get_transport_seconds();
    TimeValue transport_micros = m_time_manager->get_transport_time();

    // Write directly to the temporal context struct — no map operations
    m_global_temporal_ctx.time_since_boot = time_s;
    m_global_temporal_ctx.t = t_s;
    m_global_temporal_ctx.beat = beat_at_time(transport_micros);
    m_global_temporal_ctx.bar = bar_at_time(transport_micros);
    m_global_temporal_ctx.phrase = phrase_at_time(transport_micros);
    m_global_temporal_ctx.section = section_at_time(transport_micros);
    m_global_temporal_ctx.beatNum = static_cast<int>(beat_num_at_time(transport_micros));
    m_global_temporal_ctx.barNum = static_cast<int>(bar_num_at_time(transport_micros));
    m_global_temporal_ctx.beatDur = m_beat_length / 1000000.0;
    m_global_temporal_ctx.barDur = m_bar_length / 1000000.0;
    m_global_temporal_ctx.phraseDur = m_phrase_length / 1000000.0;
    m_global_temporal_ctx.sectionDur = m_section_length / 1000000.0;
}

void ModuLispInterpreter::update_logical_time_variables(TimeValue t)
{
    update_logical_time(t);
}

static inline double bpm_to_micros_per_beat(double bpm)
{
    constexpr double micros_in_minute = 60e+6;
    return micros_in_minute / bpm;
}

void ModuLispInterpreter::set_bpm(double newBpm, double changeThreshold = 0.0)
{
    DBG("ModuLispInterpreter::setBPM");

    if (newBpm <= 0.0)
    {
        report_generic_error("Invalid BPM requested: " + String(newBpm));
    }
    else
    {
        // Update direct member variables like old uSEQ
        m_bpm = newBpm;

        // Derive phasor lengths (in microseconds)
        m_beat_length    = bpm_to_micros_per_beat(newBpm);
        m_bar_length     = m_beat_length * 4.0; // Assuming 4/4 time for now
        m_phrase_length  = m_bar_length * m_bars_per_phrase;
        m_section_length = m_phrase_length * m_phrases_per_section;

        // Update LISP variables
        update_bpm_variables();
    }
}

void ModuLispInterpreter::update_bpm_variables()
{
    DBG("ModuLispInterpreter::update_bpm_variables");

    // Update duration variables in LISP environment (convert microseconds to
    // seconds)
    get_environment()->set("beat-dur", Value(m_beat_length * 1e-6));
    get_environment()->set("bar-dur", Value(m_bar_length * 1e-6));
    get_environment()->set("phrase-dur", Value(m_phrase_length * 1e-6));
    get_environment()->set("section-dur", Value(m_section_length * 1e-6));

    // Aliases
    get_environment()->set("beatDur", Value(m_beat_length * 1e-6));
    get_environment()->set("barDur", Value(m_bar_length * 1e-6));
    get_environment()->set("phraseDur", Value(m_phrase_length * 1e-6));
    get_environment()->set("sectionDur", Value(m_section_length * 1e-6));
}

void ModuLispInterpreter::set_time_sig(double numerator, double denominator)
{
    // Store the time signature values
    m_meter_numerator   = numerator;
    m_meter_denominator = denominator;

    // Update bar length based on time signature
    // Assuming quarter note = beat, so bar_length = beat_length * numerator *
    // (4/denominator)
    m_bar_length     = m_beat_length * numerator * (4.0 / denominator);
    m_phrase_length  = m_bar_length * m_bars_per_phrase;
    m_section_length = m_phrase_length * m_phrases_per_section;

    // Update LISP variables
    update_bpm_variables();
}

// Environment creation for time-based evaluation
#ifdef WASM_BUILD
void ModuLispInterpreter::set_time_from_external_source(TimeValue actual_time)
{
    if (m_time_manager)
    {
        m_time_manager->set_external_time(actual_time);
    }
    update_logical_time(actual_time);
}
#endif

