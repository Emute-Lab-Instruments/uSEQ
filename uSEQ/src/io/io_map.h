// uSEQ IO mapping: describes inputs/outputs, names and pins, with simple helpers.
#ifndef USEQ_IO_MAP_H_
#define USEQ_IO_MAP_H_

#include "../uSEQ/configure.h"
#include "../hardware_includes.h"
#include "../hal/hal.h"
#include <cstddef>
#include <vector>
#include <string>

namespace useq {
namespace io {

enum class ChannelKind { Continuous, Binary, Serial };

struct OutputPins {
    int primary_pin; // PWM for continuous, GPIO for binary
    int led_pin;     // LED indicator pin if available, else -1
};

// Registry-backed helpers for IO mapping
void init_from_hardware();

// Counts
size_t num_continuous_outs();
size_t num_binary_outs();

// Pin accessors (1-based indices as in DSL)
int continuous_out_pwm_pin(int index);
int continuous_out_led_pin(int index);
int binary_out_pin(int index);
int binary_out_led_pin(int index);

// Names (canonical DSL symbols)
const char* continuous_out_name(int index); // e.g. "a1"
const char* binary_out_name(int index);     // e.g. "d1"

// Thin write helpers
inline void write_continuous_pwm(int index, int raw) {
    hal::analog_write(continuous_out_pwm_pin(index), raw);
}

inline void write_binary_level(int index, bool high) {
    // Keep inversion rules local to core for now; this wraps raw write only.
    hal::digital_write(binary_out_pin(index), high ? HIGH : LOW);
}

} // namespace io
} // namespace useq

#endif // USEQ_IO_MAP_H_
