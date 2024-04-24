#ifndef CONFIGURE_H_
#define CONFIGURE_H_

#define NUM_CONTINUOUS_OUTS 3
#define NUM_BINARY_OUTS (6 - NUM_CONTINUOUS_OUTS)
#define NUM_MISC_OUTS 0

#define NUM_CONTINUOUS_INS 0
#define NUM_BINARY_INS 0
#define NUM_MISC_INS 0

#define CONTINUOUS_OUTPUT_VALUE_TYPE double
#define BINARY_OUTPUT_VALUE_TYPE int
#define MISC_OUTPUT_VALUE_TYPE int

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
