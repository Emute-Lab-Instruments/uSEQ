#pragma once

#include "../ports/ILogger.h"

// Default ILogger implementation that writes to console/Serial.
// Avoids recursion by not calling println()/report_*() functions.
class DefaultLogger : public ILogger
{
public:
    void info(const String& msg) override;
    void warn(const String& msg) override;
    void error(const String& msg) override;
    void debug(const String& msg) override;

private:
    void write_message(const char* prefix, const String& msg);
};