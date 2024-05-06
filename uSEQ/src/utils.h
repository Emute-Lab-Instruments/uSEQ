#ifndef UTILS_H_
#define UTILS_H_

#if USEQ_DEBUG
// instantiates a local debugger object
#define DBG(__name__) DEBUGGER local_debugger(__name__)
// can be used within that scope to add to debug log
#define dbg(__contents__) local_debugger.log(__contents__)

/* EXAMPLE USAGE
int foo(int x)
{
    DBG("foo"); // First, constructor will log
    int result = x + 1;
    dbg(result); // then, this will log
    return result; // finally, destructor will log
}
*/

#else

// empty macros
#define DBG(__name__)
#define dbg(__contents__)

#endif // USEQ_DEBUG

// TODO
#if defined(ARDUINO)
#include <Arduino.h>

void flash_builtin(int, int);

#else

void flash_builtin(int, int);
double millis();
double micros();

void delay(int);

/* class PIO */
/* { */
/* }; */

/* void pio_pwm_set_level(PIO, uint, uint32_t); */
/* void pio_pwm_set_period(PIO, uint, uint32_t); */

#endif // defined(ARDUINO)

#endif // UTILS_H_
