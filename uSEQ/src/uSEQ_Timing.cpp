#include "uSEQ_Timing.h"
#include "uSEQ.h"
#include "utils/log.h"
#include "uSEQ_Modules.h"

#if USE_NEW_MODULES && USE_TIMING_MODULE
// PWM control functions - only defined if the module is enabled
void pio_pwm_set_level(PIO pio, uint sm, uint32_t level)
{
#if USEQ_DEBUG
    debug("uSEQ::pio_pwm_set_level");
    debug(String(reinterpret_cast<size_t>(pio)));
    debug(String(sm));
    debug(String(level));
#endif
    pio_sm_put_blocking(pio, sm, level);
}

void pio_pwm_set_period(PIO pio, uint sm, uint32_t period)
{
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_put_blocking(pio, sm, period);
    pio_sm_exec(pio, sm, pio_encode_pull(false, false));
    pio_sm_exec(pio, sm, pio_encode_out(pio_isr, 32));
    pio_sm_set_enabled(pio, sm, true);
}
// Core timing implementations migrated from uSEQ.cpp
void uSEQ::update_time()
{
    if (!m_external_clock_source)
    {
        update_logical_time(micros());
    }
    update_lisp_time_variables();
}

void uSEQ::reset_logical_time()
{
    m_last_logical_time = micros();
    m_logical_time = 0;
    m_logical_time_int = 0;
}

void uSEQ::update_logical_time(TimeValue current_time)
{
    TimeValue delta = current_time - m_last_logical_time;
    m_last_logical_time = current_time;
    
    update_logical_time_variables(delta);
}

void uSEQ::update_logical_time_variables(TimeValue delta)
{
    m_logical_time += delta * m_time_scale;
    m_logical_time_int = m_logical_time;
    
    m_time_since_last_beat += delta * m_time_scale;
    
    if (m_time_since_last_beat >= m_beat_duration)
    {
        m_time_since_last_beat -= m_beat_duration;
        m_beat_counter++;
    }
}

void uSEQ::update_lisp_time_variables()
{
    set("t", Value(m_logical_time * 0.001));
    set("beat", Value(m_beat_counter));
    set("phase", Value(m_time_since_last_beat / m_beat_duration));
}

void uSEQ::update_bpm_variables()
{
    m_beat_duration = (60.0 / m_bpm) * 1000000;
    m_time_scale = m_bpm / m_nominal_bpm;
}

void uSEQ::set_bpm(double bpm, double nominal_bpm)
{
    m_bpm = bpm;
    m_nominal_bpm = nominal_bpm;
    update_bpm_variables();
}

void uSEQ::update_clock_from_external(double bpm)
{
    m_external_clock_source = true;
    set_bpm(bpm, m_nominal_bpm);
}
#else
// When the module is disabled, functions are provided by the original implementation
#endif // USE_NEW_MODULES && USE_TIMING_MODULE