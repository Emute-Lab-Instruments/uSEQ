// uSEQ firmware entry point
// Uses the new composition-based firmware architecture (firmware::Firmware).

#define USE_NOT_IN_FLASH 1

bool core1_separate_stack = true;

#include "src/firmware/firmware.h"

static firmware::Firmware fw;

void setup()
{
    Serial.begin();
    Serial.setTimeout(2);
    fw.init();
}

void __not_in_flash_func(loop)()
{
    fw.tick();
}

#ifdef ENABLE_DSP_ENGINE
void setup1()
{
    fw.dsp.init();
}

void __not_in_flash_func(loop1)()
{
    fw.dsp.tick();
}
#endif
