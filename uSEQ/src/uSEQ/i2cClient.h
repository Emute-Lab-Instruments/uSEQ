#ifdef ARDUINO
#include <Wire.h>
#include "i2cUtils.h"
#include "configure.h"
#include "pinmap.h"

#define ENABLEI2CCLIENT true

#define i2cCLIENT Wire

#define _i2c_req_none 0
#define _i2c_req_gettype 1
#define _i2c_req_getresponselength 2
#define _i2c_req_getresponse 3

static int i2cRequest = _i2c_req_none;
//************************
// Define a type for this module
//************************



static char i2cInBuff[500];   // buffer for incomming messages
static char i2cOutBuff[150];  // buffer for outgoing messages
extern String i2cPrintStr;

extern bool bNewI2CMessage;
extern int nI2CBytesRead;
static int nI2CResponseLen = 0;

// Function declarations
int generateChipAddress();
void i2cRecv(int len);
void i2cReq();
void setup_i2cCLIENT();

#else
// Desktop stubs for I2C functionality
#include "../utils/string.h" // Arduino-compatible String class for desktop
extern bool bNewI2CMessage;
extern int nI2CBytesRead;
static int nI2CResponseLen = 0;
static char i2cInBuff[500];
static char i2cOutBuff[150];
extern String i2cPrintStr;
#endif

