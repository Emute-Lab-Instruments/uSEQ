#include "utils.h"

#if defined(ARDUINO)
void flash_builtin(int sleep, int times = 10)
{
    for (int i = 0; i < times; i++)
    {
        digitalWrite(LED_BUILTIN, HIGH);
        // turn the LED on (HIGH is the voltage level)
        delay(sleep);
        // wait for a second
        digitalWrite(LED_BUILTIN, LOW);
        // turn the LED off by making the voltage LOW
        delay(sleep);
    }
}

#else

void flash_builtin(int sleep, int times = 10) {}
double millis() { return 0.0; }
double micros() { return 0.0; }

void delay(int x) {}
#endif
