#include "itoa.h"
/*
 * Copyright (c) 2020 Arduino.  All rights reserved.
 */

#ifdef ARDUINO

#else

/**************************************************************************************
 * INCLUDE
 **************************************************************************************/

#include "itoa.h"

#include <stdexcept>
#include <string>

#include <stdio.h>

/**************************************************************************************
 * FUNCTION IMPLEMENTATION
 **************************************************************************************/

std::string radixToFmtString(int const radix)
{
    if (radix == 8)
        return std::string("%o");
    else if (radix == 10)
        return std::string("%d");
    else if (radix == 16)
        return std::string("%X");
    else
    {
#ifndef ARDUINO
        throw std::runtime_error("Invalid radix.");
#endif
        return "";
    }
}

char* itoa(int value, char* str, int radix)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    sprintf(str, radixToFmtString(radix).c_str(), value);
#pragma GCC diagnostic pop
    return str;
}

char* ltoa(long value, char* str, int radix)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    sprintf(str, radixToFmtString(radix).c_str(), value);
#pragma GCC diagnostic pop
    return str;
}

char* utoa(unsigned value, char* str, int radix)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    sprintf(str, radixToFmtString(radix).c_str(), value);
#pragma GCC diagnostic pop
    return str;
}

char* ultoa(unsigned long value, char* str, int radix)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    sprintf(str, radixToFmtString(radix).c_str(), value);
#pragma GCC diagnostic pop
    return str;
}

#endif // ifdef ARDUINO
