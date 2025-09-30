#include <cassert>
#include <iostream>

#include "../uSEQ/src/ports/mocks/MockLogger.h"
#include "../uSEQ/src/utils/log.h"
#include "../uSEQ/src/utils/logger_bridge.h"

void test_global_logger_bridge()
{
    std::cout << "Testing global logger bridge..." << std::endl;

    // Initialize mock logger
    MockLogger mock;
    set_global_logger(&mock);

    // Test that the global logger is set
    assert(get_global_logger() != nullptr);
    std::cout << "✓ Global logger set successfully" << std::endl;

    // Clear any existing entries
    mock.entries.clear();

    // Test report_generic_error
    report_generic_error("Test error message");
    assert(mock.entries.size() >= 1);
    bool found_error = false;
    for (const auto& entry : mock.entries)
    {
        if (entry.level == "error" &&
            entry.msg.indexOf("**Error**: Test error message") >= 0)
        {
            found_error = true;
            break;
        }
    }
    assert(found_error);
    std::cout << "✓ report_generic_error logs to global logger" << std::endl;

    // Clear entries for next test
    mock.entries.clear();

    // Test report_user_warning
    report_user_warning("Test warning message");
    assert(mock.entries.size() >= 1);
    bool found_warning = false;
    for (const auto& entry : mock.entries)
    {
        if (entry.level == "warn" &&
            entry.msg.indexOf("**Warning**: Test warning message") >= 0)
        {
            found_warning = true;
            break;
        }
    }
    assert(found_warning);
    std::cout << "✓ report_user_warning logs to global logger" << std::endl;

    // Clear entries for next test
    mock.entries.clear();

    // Test println
    println("Test info message");
    assert(mock.entries.size() >= 1);
    bool found_info = false;
    for (const auto& entry : mock.entries)
    {
        if (entry.level == "info" && entry.msg == "Test info message")
        {
            found_info = true;
            break;
        }
    }
    assert(found_info);
    std::cout << "✓ println logs to global logger" << std::endl;

    // Clean up: restore null logger to avoid cross-test leakage
    set_global_logger(nullptr);
    assert(get_global_logger() == nullptr);
    std::cout << "✓ Global logger reset to null" << std::endl;

    std::cout << "✅ All logging bridge tests passed!" << std::endl;
}

int main()
{
    test_global_logger_bridge();
    return 0;
}