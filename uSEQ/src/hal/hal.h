// Minimal Hardware Abstraction Layer (HAL)
// Thin inline wrappers around platform functions to reduce #ifdef spread.

#ifndef USEQ_HAL_H_
#define USEQ_HAL_H_

#include "../hardware_includes.h"

namespace hal {

// PWM/analog output
inline void analog_write(int pin, int value) { analogWrite(pin, value); }

// Optional helpers (not yet used here, provided for future swaps)
inline void pin_mode(int pin, int mode) { pinMode(pin, mode); }

} // namespace hal

#endif // USEQ_HAL_H_

