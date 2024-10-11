#include "i2cUtils.h"
#include <Wire.h>

#define _talkingToMyslefBus_ Wire1
#define _defaultBus_ Wire

TwoWire* i2cHOST;

int aOutExpanderAddr[] = {
    0, 0, 0, 0, 0
}; // array for any analog out expanders - future we could load this array from FFS
   // as currently the order is also the channel number i.e. entry +10 A11 is output
   // 1 on aOutExpander[0]

int nOutExpander = 0; // counter for number of analog out expanders found

// scan for I2C devices on WIRE! (simulated i2cHOST)
void i2cScanForExpanders()
{
    byte error, address;
    int nDevices;
    char b[10]; // buffer for sending/reveiving messages to found devices
    int nRecdChars;

    println("Scanning...");

    nDevices = 0;
    for (address = 1; address < 127; address++)
    {
        // The i2c_scanner uses the return value of
        // the Write.endTransmisstion to see if
        // a device did acknowledge to the address.
        i2cHOST->beginTransmission(address);
        error = i2cHOST->endTransmission();

        if (error == 0)
        {
            println("I2C device found at address 0x");
            println(String(address));

            // found a device so request its type
            i2cHOST->beginTransmission(address);
            sprintf(b, "$gettype");
            i2cHOST->write(b, strlen(b));
            i2cHOST->endTransmission();

            // Read type response from the i2cCLIENT and print out
            // expander ID string arbitarily set at 7 to allow for range of options
            i2cHOST->requestFrom(address, 7);

            // read 7 (hopefully) chars to b[]
            nRecdChars = 0;
            while (i2cHOST->available())
            {
                b[nRecdChars] = (char)i2cHOST->read();
                nRecdChars++;
            }
            // add null termination
            b[nRecdChars] = 0;
            // look for known strings here to id tify expanders and set them up
            println(b);

            // analog out expander "aoutxx" where xx is going to be version
            if (strstr(b, "aout"))
            {
                // save address
                aOutExpanderAddr[nOutExpander] = address;
                nOutExpander++;
                println("aout found");
            }

            nDevices++;
        }
        else if (error == 4)
        {
            // error handling not added as yet!
           println("Unknown error at address 0x");
            if (address < 16)
               println("0");
            println(String(address));
        }
    }
    if (nDevices == 0)
        println("No I2C devices found\n");
    else
        println("done\n");
}

void setup_i2cHOST()
{
    bI2CclientMode = false;
    bI2ChostMode = true;
    if (bI2ChostMode && bI2CclientMode)
    {
        i2cHOST = &_talkingToMyslefBus_;
        i2cHOST->setSDA(2); // simulated i2ci2cHOST SDA
        i2cHOST->setSCL(3); // simulated i2cHOST CLK
        i2cHOST->begin();
    }
    else if (bI2ChostMode && !bI2CclientMode)
    {
        i2cHOST = &_defaultBus_;
        i2cHOST->end();
        //i2cHOST->setSDA(4); // ELI2040 SDA
        //i2cHOST->setSCL(1); // ELI2040 CLK
        i2cHOST->setSDA(0); // uSEQ SDA
        i2cHOST->setSCL(1); // uSEQ CLK
        i2cHOST->begin();
    }

    i2cScanForExpanders();
}

// ********************************************
// Analog Expander Specific functions
// ********************************************
void i2cWriteString(int expander, String msg)
{
    msg.concat(char(3));
    int packetStart    = 0;
    int packetSize     = 250;
    int maxMessageSize = 500;
    while (packetStart < msg.length())
    {
        i2cHOST->beginTransmission(aOutExpanderAddr[expander - 1]);
        if ((msg.length() - packetStart) > (packetSize - 1))
        {
            i2cHOST->write(
                msg.substring(packetStart, packetStart + packetSize).c_str(),
                packetSize);
        }
        else
        {
            i2cHOST->write(msg.substring(packetStart).c_str(),
                           msg.length() - packetStart + 1);
        }
        i2cHOST->endTransmission();
        packetStart += packetSize;
        delayMicroseconds(5);
    }
}

String getI2CResults(int expander)
{
    // println("request results");
    String res = "";
    i2cWriteString(expander, "$getreslen");
    i2cHOST->requestFrom(aOutExpanderAddr[expander - 1], 1);
    int c = i2cHOST->read();
    // println(c, DEC);
    i2cWriteString(expander, "$getresponse");
    int nRecdChars     = 0;
    int nCharRequested = 0;
    int maxPacketLen   = 17;
    while (nRecdChars < c)
    {
        if ((c - nRecdChars) > maxPacketLen)
            nCharRequested = maxPacketLen;
        else
            nCharRequested = c - nRecdChars;
        i2cHOST->requestFrom(aOutExpanderAddr[expander - 1], nCharRequested);
        while (i2cHOST->available())
        {
            // b[nRecdChars] = (char)i2cHOST->read();
            res += String((char)i2cHOST->read());
            //println(b[nRecdChars]);
            nRecdChars++;
        }
    }

    return res;
}
void printAllI2CReports()
{
    for (int i = 0; i < nOutExpander; i++)
    {
        println(getI2CResults(i));
    }
}

// send a sync cloock message to all I2C uSEQ devices
void syncAllI2c()
{
    for (int i = 0; i < nOutExpander; i++)
        i2cWriteString(i, "$sync");
}
