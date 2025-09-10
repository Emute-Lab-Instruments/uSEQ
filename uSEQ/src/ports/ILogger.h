// Lightweight logger port interface for dependency injection and testing
#pragma once

#include "../utils/string.h"

struct ILogger
{
    virtual ~ILogger()                    = default;
    virtual void info(const String& msg)  = 0;
    virtual void warn(const String& msg)  = 0;
    virtual void error(const String& msg) = 0;
    virtual void debug(const String& msg) = 0;
};
