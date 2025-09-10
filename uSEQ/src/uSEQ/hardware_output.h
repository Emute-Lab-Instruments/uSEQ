#pragma once

#include "../uSEQ.h"
#include "../hardware_includes.h"
#include <cstdint>

// Centralized facade for all hardware output writes (digital/analog/LED/PWM).
// Goals:
// - Single chokepoint for debug/analytics
// - Preserve platform-specific behavior (USEQHARDWARE, MUSICTHING, expanders)
// - Seamless no-op on desktop builds
// - Optional callback hooks for tracing

struct OutputWriteEvent
{
    enum class Kind : uint8_t { Digital, Analog, PioPwm };
    Kind kind;
    uint8_t pin_or_sm;   // GPIO pin for Digital/Analog, SM index for PioPwm
    uint16_t value;      // 0/1 for Digital, 0..2^N-1 for Analog/PWM
};

using OutputWriteHook = void (*)(const OutputWriteEvent&);

class HardwareOutput
{
  public:
    // Configure a global hook to observe all writes (optional)
    static void set_hook(OutputWriteHook hook) { s_hook = hook; }

    // Digital write (boolean value encoded as 0/1)
    static inline void digital(uint8_t pin, uint8_t value)
    {
#ifdef ARDUINO
        digitalWrite(pin, value);
#else
        (void)pin; (void)value;
#endif
        emit_hook(OutputWriteEvent{OutputWriteEvent::Kind::Digital, pin, value});
    }

    // Analog/PWM write to a pin
    static inline void analog(uint8_t pin, uint16_t value)
    {
#ifdef ARDUINO
        analogWrite(pin, static_cast<int>(value));
#else
        (void)pin; (void)value;
#endif
        emit_hook(OutputWriteEvent{OutputWriteEvent::Kind::Analog, pin, value});
    }

    // Route LED via PIO PWM where applicable. When not available, fallback to analog().
    static inline void pio_pwm_level(uint8_t sm_index, uint16_t level)
    {
#if defined(ARDUINO) && !defined(USEQHARDWARE_EXPANDER_OUT_0_1) && !defined(MUSICTHING)
        // Default PIO instance choices follow existing convention: first 4 on pio0
        PIO pio = (sm_index < 4) ? pio0 : pio1;
        uint sm = sm_index % 4;
        pio_pwm_set_level(pio, sm, level);
#else
        (void)sm_index; (void)level; // Fallback/no-op on platforms without PIO routing
#endif
        emit_hook(OutputWriteEvent{OutputWriteEvent::Kind::PioPwm, sm_index, level});
    }

  private:
    static inline void emit_hook(const OutputWriteEvent& e)
    {
        if (s_hook)
            s_hook(e);
    }

    static OutputWriteHook s_hook;
};
