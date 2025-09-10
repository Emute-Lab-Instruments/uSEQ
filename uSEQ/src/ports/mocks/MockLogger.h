// Mock logger for testing
#pragma once

#include "../ILogger.h"
#include <vector>

class MockLogger : public ILogger
{
public:
    struct LogEntry
    {
        String level;
        String msg;
    };

    void info(const String& msg) override
    {
        entries.push_back({ "info", msg });
        info_messages.push_back(msg);
        info_count++;
    }

    void warn(const String& msg) override
    {
        entries.push_back({ "warn", msg });
        warn_messages.push_back(msg);
        warn_count++;
    }

    void error(const String& msg) override
    {
        entries.push_back({ "error", msg });
        error_messages.push_back(msg);
        error_count++;
    }

    void debug(const String& msg) override
    {
        entries.push_back({ "debug", msg });
        debug_messages.push_back(msg);
        debug_count++;
    }

    // Test helpers
    void clear()
    {
        entries.clear();
        info_messages.clear();
        warn_messages.clear();
        error_messages.clear();
        debug_messages.clear();
        info_count = warn_count = error_count = debug_count = 0;
    }

    // Unified entry list for tests that expect it
    std::vector<LogEntry> entries;

    // Counters and message storage for verification
    int info_count  = 0;
    int warn_count  = 0;
    int error_count = 0;
    int debug_count = 0;

    std::vector<String> info_messages;
    std::vector<String> warn_messages;
    std::vector<String> error_messages;
    std::vector<String> debug_messages;
};