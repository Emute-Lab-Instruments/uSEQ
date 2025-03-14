#ifndef USEQ_HARDWARE_CONTROL_H_
#define USEQ_HARDWARE_CONTROL_H_

#include "uSEQ/configure.h"

/**
 * This file contains hardware-specific functions for uSEQ.
 * It handles direct hardware control that's unique to specific
 * hardware versions of the uSEQ.
 */

// External variables
extern float pdm_y;
extern float pdm_err;
extern float pdm_w;

#ifdef USEQHARDWARE_1_0
// Forward declaration - timer callback function will be implemented later
// Only declared during active migration
// bool timer_callback(repeating_timer_t* mst);
#endif

#endif // USEQ_HARDWARE_CONTROL_H_