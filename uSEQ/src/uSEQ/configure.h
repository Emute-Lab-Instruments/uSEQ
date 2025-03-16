#ifndef CONFIGURE_H_
#define CONFIGURE_H_

// NOTE: change this to specify whichever
// hardware version we're building for
// NOTE: this ideally should be specified in uSEQ.ino
/* #define MUSICTHING */
/* #define USEQHARDWARE_0_1 */

// #define USEQHARDWARE_0_2
#define USEQHARDWARE_1_0 
/* #define MUSICTHING */

// NOTE: this needs to be included after the above define
#include "pinmap.h"

#define NUM_SERIAL_INS 32
#define NUM_SERIAL_OUTS 8

// TODO move definitions regarding num of IO here,
// leave mapping of pins to pinmap.h
#if defined(USEQHARDWARE_0_2)

// TODO restore num ins
/* #define NUM_CONTINUOUS_INS 0 */
/* #define NUM_BINARY_INS 0 */

#define NUM_CONTINUOUS_OUTS 2
#define NUM_BINARY_OUTS (6 - NUM_CONTINUOUS_OUTS)

#endif

#if defined(MUSICTHING) || defined(USEQHARDWARE_1_0)
#define ANALOG_INPUTS
#endif

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
#define BINARY_OUTPUT_VALUE_TYPE int
//#define SERIAL_OUTPUT_VALUE_TYPE double

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
