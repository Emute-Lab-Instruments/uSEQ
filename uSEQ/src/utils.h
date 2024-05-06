#ifndef UTILS_H_
#define UTILS_H_

#if USEQ_DEBUG
// instantiates a local debugger object
#define DBG(__name__) DebugLogger local_debugger(__name__)
// can be used within that scope to add to debug log
#define dbg(__contents__) local_debugger.filtered_log(__contents__)

/* EXAMPLE USAGE

// (optional) specify mutes or solos

DebugLogger::solos.insert("foo");
// or
DebugLogger::mutes.insert("bar");

// Add local debuggers to functions/methods
int foo(int x)
{
    DBG("foo"); // First, constructor will log
    int result = x + 1;
    dbg("result: " + String(result)); // then, this will log
    return result; // finally, destructor will log
}

int bar()
{
    DBG("bar"); // this won't print
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

#endif // defined(ARDUINO)

#endif // UTILS_H_
