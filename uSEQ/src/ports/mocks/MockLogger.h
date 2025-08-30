// Simple message-capturing logger for tests
#pragma once

#include "../ILogger.h"
#include <vector>

struct MockLogger : public ILogger {
    struct Entry { String level; String msg; };
    std::vector<Entry> entries;

    void info(const String& msg) override { entries.push_back({"info", msg}); }
    void warn(const String& msg) override { entries.push_back({"warn", msg}); }
    void error(const String& msg) override { entries.push_back({"error", msg}); }
    void debug(const String& msg) override { entries.push_back({"debug", msg}); }
};

