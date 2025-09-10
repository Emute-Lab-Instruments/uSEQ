#include "uSeqGen_MT_DAC.h"

// Define static volatile variables for DAC value storage
volatile uint16_t uSeqGen_MT_DAC::latest_dac_left  = 0;
volatile uint16_t uSeqGen_MT_DAC::latest_dac_right = 0;