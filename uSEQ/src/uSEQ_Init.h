#ifndef USEQ_INIT_H_
#define USEQ_INIT_H_

#include "uSEQ/configure.h"

/**
 * This file contains initialization and setup functions for uSEQ.
 * It handles hardware setup, pin configuration, and system initialization.
 */

// Initialize system LEDs
void setup_leds();

// Initialize PIO PWM if available
#ifdef USEQHARDWARE_1_0
void start_pdm();
#endif

// Forward declarations for initialization functions

#endif // USEQ_INIT_H_