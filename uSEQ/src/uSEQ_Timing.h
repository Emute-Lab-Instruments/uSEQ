#ifndef USEQ_TIMING_H_
#define USEQ_TIMING_H_

#include "hardware/pio.h"

/**
 * This file contains timing-related functions for uSEQ.
 * It handles clock management, BPM calculation, and phase computation.
 */

// Utility function for PWM control
void pio_pwm_set_level(PIO pio, uint sm, uint32_t level);
void pio_pwm_set_period(PIO pio, uint sm, uint32_t period);

#endif // USEQ_TIMING_H_