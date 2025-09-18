#ifndef CONFIGURE_H_
#define CONFIGURE_H_

// NOTE: change this to specify whichever
// hardware version we're building for
// NOTE: this ideally should be specified in uSEQ.ino
/* #define MUSICTHING */
/* #define USEQHARDWARE_0_1 */

// #define USEQHARDWARE_0_2
// #define USEQHARDWARE_1_0
// #define USEQHARDWARE_EXPANDER_OUT_0_1
#define MUSICTHING
// #define DESKTOP

// ===== FEATURE FLAGS FOR CONDITIONAL COMPILATION =====
// These flags allow selective compilation of features to reduce binary size
// and compilation time for specific build configurations

// DSP Features (audio processing, tempo estimation)
#ifdef ARDUINO
#define ENABLE_DSP_ENGINE
#define ENABLE_TEMPO_ESTIMATOR
#endif

// I2C Communication (multi-module networking)
#if defined(ARDUINO) && !defined(DESKTOP)
#define ENABLE_I2C_NETWORKING
#define ENABLE_I2C_HOST
#define ENABLE_I2C_CLIENT
#endif

// Flash Storage (persistent data storage)
#ifdef ARDUINO
#define ENABLE_FLASH_STORAGE
#endif

// LED Control (visual feedback)
#if defined(ARDUINO) && !defined(DESKTOP)
#define ENABLE_LED_CONTROL
#define ENABLE_RGB_LED
#endif

// Hardware I/O Features
#ifdef ARDUINO
#define ENABLE_ANALOG_INPUTS
#define ENABLE_DIGITAL_IO
#define ENABLE_ENCODER_INPUT
#endif

// Advanced Features (can be disabled for minimal builds)
#ifndef MINIMAL_BUILD
#define ENABLE_SERIAL_DEBUG
#define ENABLE_DIAGNOSTICS
#define ENABLE_PROFILING
#endif

// Memory Management Features
#ifdef ARDUINO
#define USE_FIXED_MEMORY_POOLS
#else
#define USE_DYNAMIC_ALLOCATION
#endif

#ifdef MUSICTHING
#define DIGI_OUT_INVERTED
#define ANALOG_OUT_INVERTED
#define AUDIO_OUT_INVERTED
#endif

// NOTE: this needs to be included after the above define
#ifdef ARDUINO
#include "pinmap.h"
#endif

#define NUM_SERIAL_INS 32
#define NUM_SERIAL_OUTS 9

// #if defined(MUSICTHING) || defined(USEQHARDWARE_1_0)
#define ANALOG_INPUTS
// #endif

/* // NUM OUTS */
/* #if !defined(NUM_CONTINUOUS_OUTS) */
/* #define NUM_CONTINUOUS_OUTS 3 */
/* #endif */
/* #define NUM_BINARY_OUTS (6 - NUM_CONTINUOUS_OUTS) */
/* #define NUM_SERIAL_OUTS 0 */

/* // NUM INS */
/* #if !defined(NUM_CONTINUOUS_INS) */
/* #define NUM_CONTINUOUS_INS 0 */
/* #endif */
/* #define NUM_BINARY_INS 0 */
/* #define NUM_SERIAL_INS 0 */

#define CONTINUOUS_OUTPUT_VALUE_TYPE double
#define BINARY_OUTPUT_VALUE_TYPE double
#define SERIAL_OUTPUT_VALUE_TYPE double

#define CONTINUOUS_INPUT_VALUE_TYPE double
#define BINARY_INPUT_VALUE_TYPE int
#define MISC_INPUT_VALUE_TYPE int

enum useqInputNames
{
    // signals
    USEQI1 = 0,
    USEQI2 = 1,
    // momentary
    USEQM1 = 2,
    USEQM2 = 3,
    // toggle
    USEQT1 = 4,
    USEQT2 = 5,
    // rotary enoder
    USEQRS1 = 6, // switch
    USEQR1  = 7, // encoder
                 // analog ins
    USEQAI1 = 8,
    USEQAI2 = 9,
    //
    MTMAINKNOB = 10,
    MTXKNOB    = 11,
    MTYKNOB    = 12,
    MTZSWITCH  = 13,
};

#endif // CONFIGURE_H_
