#ifndef _i2cHost_H_
#define _i2cHost_H_

#include "i2cUtils.h"
#ifdef ARDUINO
#include <Wire.h>
#else
// Desktop stubs
#include "../utils/string.h" // Arduino-compatible String class for desktop
class TwoWire; // Forward declaration for desktop
#endif

#define _talkingToMyslefBus_ Wire1
#define _defaultBus_ Wire

extern TwoWire* i2cHOST;
extern int aOutExpanderAddr[5];
extern int nOutExpander;

// Function declarations
void i2cScanForExpanders();
void setup_i2cHOST();
void i2cWriteString(int expander, String msg);
String getI2CResults(int expander);
void printAllI2CReports();
void syncAllI2c();

#endif // _i2cHost_H_
