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

#include "src/uSEQ.h"

// NOTE: this has to be done here, as opposed to e.g. inside uSEQ::init,
// to prevent anything trying to write to serial before it's been set up
// (e.g. for debugging purposes)
void init_serial()
{
    Serial.begin(115200);
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

uSEQ u;


void setup()
{
    init_serial();
    init_random();
    u.init();
}

void loop() { u.tick(); }
