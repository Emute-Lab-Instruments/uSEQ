#include <Wire.h>
#include "i2cUtils.h"

#define ENABLEI2CCLIENT true

#define i2cCLIENT Wire

#define _i2c_req_none 0
#define _i2c_req_gettype 1
#define _i2c_req_getresponselength 2
#define _i2c_req_getresponse 3

int i2cRequest = _i2c_req_none;
//************************
// Define a type for this module
//************************
String expanderTypeID = "aout01";  //used to distinguish between expanders


static char i2cInBuff[500];   // buffer for incomming messages
static char i2cOutBuff[150];  // buffer for outgoing messages
String i2cPrintStr = "";

bool bNewI2CMessage = false;
int nI2CBytesRead = 0;
int nI2CResponseLen = 0;

//generate Chip specific I2C address
int generateChipAddress() {
  const char* chipID = rp2040.getChipID();
  //Serial.print("Unique Id: ");
  //Serial.println(chipID);
  //Serial.println((hex2int(chipID[strlen(chipID)-2]) * 16) + hex2int(chipID[strlen(chipID)-1]));
  int id = (hex2int(chipID[strlen(chipID) - 2]) * 16) + hex2int(chipID[strlen(chipID) - 1]);
  // new ID might be out of range
  if (id > 127) {
    id -= 127;
  }

  return id;
}

// Called when the I2C i2cCLIENT gets written to
void i2cRecv(int len) {
  int i;
  // Just stuff the sent bytes into a global the main routine can pick up and use
  for (i = 0; i < len; i++) {
    i2cInBuff[nI2CBytesRead] = i2cCLIENT.read();
    // detect end of message and trigger parsing in main routine
   if (i2cInBuff[nI2CBytesRead] == 3) {
    bNewI2CMessage = true;
    i2cInBuff[nI2CBytesRead] == 0;
   }
   nI2CBytesRead++;
  }
  // check for i2c command strings and set up for i2cReq call to be be processed
  if (strstr(i2cInBuff, "$gettype")) {
    i2cRequest = _i2c_req_gettype;
    bNewI2CMessage = false;
    nI2CBytesRead = 0;
  }
  if (strstr(i2cInBuff, "$getreslen")) {
    i2cRequest = _i2c_req_getresponselength;
    bNewI2CMessage = false;
    nI2CBytesRead = 0;
  }
  if (strstr(i2cInBuff, "$getresponse")) {
    i2cRequest = _i2c_req_getresponse;
    bNewI2CMessage = false;
    nI2CBytesRead = 0;
  }

}

// Called when the I2C i2cCLIENT is read from - should be following a Receive -
void i2cReq() {
  //Serial.print("i2c req  ");
  //Serial.println(i2cRequest);
  //respond to gettype request
  if (i2cRequest == _i2c_req_gettype) {
    // Serial.println("Type ID request");
    expanderTypeID.toCharArray(i2cOutBuff, 7);
    i2cCLIENT.write(i2cOutBuff, 7);
    i2cRequest = _i2c_req_none;
  }

  if (i2cRequest == _i2c_req_getresponselength) {
  

    i2cOutBuff[0] = i2cPrintStr.length();
    i2cCLIENT.write(i2cOutBuff , 1);
    i2cRequest = _i2c_req_none;
      // Serial.println("response length request");
     //Serial.println(i2cOutBuff[0],DEC);
  }

  if (i2cRequest == _i2c_req_getresponse) {
    //Serial.println("response request");
    int maxPacketLen = 17;
    int nBytesToSend = maxPacketLen; 
    if (i2cPrintStr.length() < maxPacketLen) nBytesToSend = maxPacketLen - i2cPrintStr.length();
    i2cPrintStr.toCharArray(i2cOutBuff, nBytesToSend);
    i2cCLIENT.write(i2cOutBuff, nBytesToSend);
    i2cPrintStr.remove(0,nBytesToSend);
    if (i2cPrintStr.length() <1) i2cRequest = _i2c_req_none;
    //i2cCLIENT.write(i2cOutBuff, 3);
    //i2cPrintStr = "";
  }

  // TO DO add a default req error?
  // request dealt with so clear
  
}

void setup_i2cCLIENT() {
  if (!bI2CclientMode) return;
  i2cCLIENT.setSDA(_USEQ_SDA_PIN_);  //ELI2040 SDA
  i2cCLIENT.setSCL(_USEQ_SCL_PIN_);
  //i2cCLIENT.setSDA(4);  //ELI2040 SDA
  //i2cCLIENT.setSCL(1);  //ELI2040 CLK
  //i2cCLIENT.setSDA(0);  //uSEQ SDA
  //i2cCLIENT.setSCL(1);  //uSEQ CLK
  i2cCLIENT.begin(generateChipAddress());
  i2cCLIENT.onReceive(i2cRecv);
  i2cCLIENT.onRequest(i2cReq);
}

