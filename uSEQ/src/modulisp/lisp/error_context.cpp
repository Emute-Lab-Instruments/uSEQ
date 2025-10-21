#include "error_context.h"
#include "value.h"

// Report undefined symbol error
void ErrorManager::report_undefined_symbol(const String& symbol_name,
                                           const String& code, int line, int column)
{
    ErrorContext& ctx =
        ensure_error_context(ErrorCategory::UNDEFINED_SYMBOL,
                             "Variable '" + symbol_name + "' is not defined");

    ctx.symbol_name = symbol_name;
    ctx.detailed_message =
        "The symbol '" + symbol_name + "' was not found in the current environment.";
    ctx.suggestion = "Define the variable first using (def " + symbol_name +
                     " value) or check for typos.";
    ctx.line   = line;
    ctx.column = column;

    if (code.length() != 0)
    {
        ctx.code_snippet = code;
    }
}

// Report undefined function error
void ErrorManager::report_undefined_function(const String& function_name,
                                             const String& code, int line,
                                             int column)
{
    ErrorContext& ctx =
        ensure_error_context(ErrorCategory::UNDEFINED_FUNCTION,
                             "Function '" + function_name + "' is not defined");

    ctx.function_name    = function_name;
    ctx.detailed_message = "The function '" + function_name + "' was not found.";
    ctx.suggestion =
        "Check the function name spelling or ensure the function is defined.";
    ctx.line   = line;
    ctx.column = column;

    if (code.length() != 0)
    {
        ctx.code_snippet = code;
    }

    // TODO: Add fuzzy matching for "did you mean" suggestions
    // For now, add some common suggestions based on function name
    if (function_name == "sni")
    {
        add_did_you_mean("sin");
    }
    else if (function_name == "oscilator")
    {
        add_did_you_mean("oscillator");
    }
    else if (function_name == "Sin")
    {
        add_did_you_mean("sin");
        ctx.suggestion =
            "Did you mean 'sin'? Note: function names are case-sensitive";
    }
}

// Report arity error (wrong number of arguments)
void ErrorManager::report_arity_error(const String& function_name, int expected,
                                      int received)
{
    String message = "Function '" + function_name + "' expects ";
    if (expected == 0)
    {
        message += "no arguments";
    }
    else if (expected == 1)
    {
        message += "exactly 1 argument";
    }
    else
    {
        message += "exactly " + String(expected) + " arguments";
    }
    message += ", but got " + String(received);

    ErrorContext& ctx = ensure_error_context(ErrorCategory::ARITY_ERROR, message);
    ctx.function_name = function_name;
    ctx.expected_args = expected;
    ctx.received_args = received;

    // Add function-specific usage examples
    if (function_name == "sin")
    {
        add_example("(sin 0.5)");
        ctx.suggestion = "Usage: (sin phase)\\nExample: (sin 0.5) returns sine wave "
                         "value at phase 0.5";
    }
    else if (function_name == "+")
    {
        add_example("(+ 1 2)");
        add_example("(+ 1 2 3 4)");
        ctx.suggestion =
            "Usage: (+ number1 number2 ...)\\nExample: (+ 1 2 3) returns 6";
    }
}

// Report type error
void ErrorManager::report_type_error(const String& function_name,
                                     const String& expected_type,
                                     const String& received_type, int arg_index)
{
    String message = "Function '" + function_name + "' expects ";
    if (arg_index >= 0)
    {
        message += "argument " + String(arg_index + 1) + " to be ";
    }
    message += "a " + expected_type + ", but got " + received_type;

    ErrorContext& ctx = ensure_error_context(ErrorCategory::TYPE_ERROR, message);
    ctx.function_name = function_name;
    ctx.expected_type = expected_type;
    ctx.received_type = received_type;

    if (function_name == "sin" && received_type.indexOf("string") >= 0)
    {
        ctx.suggestion =
            "The sin function calculates the sine of a phase value (0.0 to "
            "1.0)\\nExample: (sin 0.25) returns 1.0 (peak of sine wave)";
    }
}

// Report arithmetic error
void ErrorManager::report_arithmetic_error(const String& message)
{
    ErrorContext& ctx =
        ensure_error_context(ErrorCategory::ARITHMETIC_ERROR, message);

    if (message.indexOf("division") >= 0 && message.indexOf("zero") >= 0)
    {
        ctx.suggestion =
            "Division by zero is undefined. Check your denominator values.";
    }
}

// Report syntax error
void ErrorManager::report_syntax_error(const String& message, const String& code,
                                       int line, int column)
{
    ErrorContext& ctx = ensure_error_context(ErrorCategory::SYNTAX_ERROR, message);
    ctx.line          = line;
    ctx.column        = column;

    if (code.length() != 0)
    {
        ctx.code_snippet = code;
    }

    if (message.indexOf("parenthesis") >= 0)
    {
        if (message.indexOf("missing") >= 0)
        {
            ctx.suggestion = "Add ')' at the end of your expression";
        }
        else if (message.indexOf("extra") >= 0)
        {
            ctx.suggestion = "Remove the extra ')' at position " + String(column);
        }
    }
    else if (message.indexOf("string") >= 0)
    {
        ctx.suggestion = "Add a closing '\"' to complete the string";
    }
}

// Report generic error
void ErrorManager::report_generic_error(const String& message)
{
    ensure_error_context(ErrorCategory::GENERIC_ERROR, message);
}

// Add suggestion to current error
void ErrorManager::add_suggestion(const String& suggestion)
{
    if (current_error.has_value())
    {
        current_error->suggestion = suggestion;
    }
}

// Add "did you mean" alternative
void ErrorManager::add_did_you_mean(const String& alternative)
{
    if (current_error.has_value())
    {
        current_error->did_you_mean.push_back(alternative);
    }
}

// Add usage example
void ErrorManager::add_example(const String& example)
{
    if (current_error.has_value())
    {
        current_error->examples.push_back(example);
    }
}

// Add stack frame
void ErrorManager::add_stack_frame(const String& frame)
{
    if (current_error.has_value())
    {
        current_error->stack_frames.push_back(frame);
    }
}

// Set location information
void ErrorManager::set_location(int line, int column)
{
    if (current_error.has_value())
    {
        current_error->line   = line;
        current_error->column = column;
    }
}

// Set code snippet
void ErrorManager::set_code_snippet(const String& snippet)
{
    if (current_error.has_value())
    {
        current_error->code_snippet = snippet;
    }
}

// Helper to ensure we have an active error context
ErrorContext& ErrorManager::ensure_error_context(ErrorCategory category,
                                                 const String& message)
{
    if (!current_error.has_value())
    {
        current_error = ErrorContext(category, message);
    }
    return current_error.value();
}