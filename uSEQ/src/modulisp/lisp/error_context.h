#ifndef ERROR_CONTEXT_H_
#define ERROR_CONTEXT_H_

#include "../../utils/string.h"
#include <vector>
#include <optional>

// Forward declarations
class Value;
class Environment;

// Types of errors that can occur during evaluation
enum class ErrorCategory {
    SYNTAX_ERROR,           // Parsing errors (malformed expressions, unmatched parens)
    UNDEFINED_SYMBOL,       // Symbol not found in environment  
    UNDEFINED_FUNCTION,     // Function not found when calling
    ARITY_ERROR,           // Wrong number of arguments
    TYPE_ERROR,            // Type mismatch (e.g., string passed to numeric function)
    ARITHMETIC_ERROR,      // Division by zero, overflow, etc.
    INDEX_ERROR,           // Array/list index out of bounds
    RECURSION_ERROR,       // Stack overflow from infinite recursion
    TIMEOUT_ERROR,         // Evaluation took too long
    GENERIC_ERROR          // Catch-all for other errors
};

// Detailed information about an error occurrence
struct ErrorContext {
    ErrorCategory category = ErrorCategory::GENERIC_ERROR;
    String primary_message;           // Main error description
    String detailed_message;          // More verbose explanation
    String suggestion;                // Hint for how to fix
    
    // Location information (if available)
    int line = 0;
    int column = 0;
    String code_snippet;              // Relevant code that caused error
    
    // Context-specific information
    String symbol_name;               // For undefined symbols/functions
    String function_name;             // For function-related errors
    int expected_args = -1;           // For arity errors
    int received_args = -1;           // For arity errors
    String expected_type;             // For type errors
    String received_type;             // For type errors
    
    // Suggestions and alternatives
    std::vector<String> did_you_mean; // Similar symbols/functions
    std::vector<String> examples;     // Usage examples
    
    // Stack trace (for debugging)
    std::vector<String> stack_frames;
    
    // Constructor
    ErrorContext() = default;
    ErrorContext(ErrorCategory cat, const String& message) 
        : category(cat), primary_message(message) {}
};

// Manages error state and context during evaluation
class ErrorManager {
public:
    ErrorManager() = default;
    
    // Check if there are any errors
    bool has_error() const { return current_error.has_value(); }
    
    // Clear current error state
    void clear_error() { current_error.reset(); }
    
    // Get current error context (if any)
    const ErrorContext* get_current_error() const {
        return current_error.has_value() ? &current_error.value() : nullptr;
    }
    
    // Report different types of errors with context
    void report_undefined_symbol(const String& symbol_name, const String& code = "", int line = 0, int column = 0);
    void report_undefined_function(const String& function_name, const String& code = "", int line = 0, int column = 0);
    void report_arity_error(const String& function_name, int expected, int received);
    void report_type_error(const String& function_name, const String& expected_type, const String& received_type, int arg_index = -1);
    void report_arithmetic_error(const String& message);
    void report_syntax_error(const String& message, const String& code = "", int line = 0, int column = 0);
    void report_generic_error(const String& message);
    
    // Add contextual information to current error
    void add_suggestion(const String& suggestion);
    void add_did_you_mean(const String& alternative);
    void add_example(const String& example);
    void add_stack_frame(const String& frame);
    void set_location(int line, int column);
    void set_code_snippet(const String& snippet);
    
private:
    std::optional<ErrorContext> current_error;
    
    // Helper to ensure we have an active error context
    ErrorContext& ensure_error_context(ErrorCategory category, const String& message);
};

#endif // ERROR_CONTEXT_H_