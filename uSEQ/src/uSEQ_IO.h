#ifndef USEQ_IO_H_
#define USEQ_IO_H_

#include "uSEQ/configure.h"

/**
 * This file contains I/O handling functions for uSEQ.
 * It manages physical inputs and outputs, including rotary encoders,
 * buttons, analog inputs, and various output types.
 */

// Helper functions for pin mapping
int analog_out_LED_pin(int out);
int digital_out_LED_pin(int out);
int analog_out_pin(int out);
int digital_out_pin(int out);

// Rotary encoder functions
#ifdef USEQHARDWARE_0_2
int8_t read_rotary();
#endif

#endif // USEQ_IO_H_