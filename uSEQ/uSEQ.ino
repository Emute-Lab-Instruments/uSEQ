// NOTE: if this is set to 1, a /ton/ of debug info
// will be printed out through Serial.
// - Open with e.g. VSCode to get block collapse toggling
// - How should this be integrated with the editor so that it
//   doesn't pollute the console out?
#define USEQ_DEBUG 0
#define USE_NOT_IN_FLASH 1

// NOTE: this doesn't seem to carry over to "uSEQ/configure.h"
// (through "uSEQ.h") so it's being redefined there
// Seems like it should work since the headers are included after
// this define - maybe an issue with Arduino IDE?
// #define USEQHARDWARE_0_2

bool core1_separate_stack = true;

#include "src/uSEQ.h"

// NOTE: this has to be done here, as opposed to e.g. inside uSEQ::init,
// to prevent anything trying to write to serial before it's been set up
// (e.g. for debugging purposes)
void init_serial()
{
    Serial.begin();
    Serial.setTimeout(2);
}

void init_random()
{
#if USEQ_DEBUG
    // Fix random seed for debugging purposes
    randomSeed(123);
#else
    randomSeed(analogRead(0));
#endif
}

std::unique_ptr<uSEQ> __not_in_flash("useq") u;

void setup()
{
    init_serial();
    init_random();
    u = std::make_unique<uSEQ>();
    u->init();
}

void __not_in_flash_func(loop)() { u->tick(); }

// core 1
void setup1() { u->initDSP(); }

void __not_in_flash_func(loop1)() { u->tick_dsp(); }
