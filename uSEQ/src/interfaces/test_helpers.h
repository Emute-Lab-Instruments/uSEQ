#pragma once

// Minimal test helper interface for faster test compilation
// Include this instead of full implementation headers in test files

#include "../utils/string.h"
#include <memory>

// Forward declaration from global namespace
class Interpreter;

namespace test_helpers {

// Simple test interpreter wrapper that hides implementation details
class QuickInterpreter {
private:
    std::unique_ptr<::Interpreter> impl;
    
public:
    QuickInterpreter();
    ~QuickInterpreter();
    
    // Simple evaluation methods
    String evalToString(const String& code);
    float evalToNumber(const String& code);
    bool evalToBool(const String& code);
    size_t evalListSize(const String& code);
    
    // Test assertions
    bool evalIsNumber(const String& code);
    bool evalIsString(const String& code);
    bool evalIsList(const String& code);
    bool evalIsNil(const String& code);
    bool evalIsTrue(const String& code);
};

// Common test utilities
namespace test_utils {
    // Create simple test expressions
    String makeAddExpr(float a, float b);
    String makeListExpr(float a, float b, float c);
    String makeIfExpr(const String& cond, const String& then_branch, const String& else_branch);
    
    // Validate results
    bool floatEquals(float a, float b, float epsilon = 0.0001f);
}

}