#include "modulisp.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

// Constructor
ModuLisp::ModuLisp()
    : m_interpreter(), current_context(Context::GENERAL)
{
    // Initialize the interpreter (owns diagnostics, Environment, Parser)
    m_interpreter.init();
    m_interpreter.init_builtinfuncs();
}

// Destructor
ModuLisp::~ModuLisp() = default;

// Main API: send LISP code and get response
ModuLisp::Response ModuLisp::send(const std::string& code)
{
    // Clear any previous diagnostics
    m_interpreter.clear_diagnostics();

    // Convert std::string to Arduino String
    String arduino_code(code.c_str());

    // Parse and evaluate the code
    Value result = m_interpreter.eval_v(arduino_code);

    // Create response
    Response response(result);

    // If it's an error, get detailed information from diagnostics
    if (result.get_type_enum() == 14)
    { // ERROR type
        const auto& diagnostics = m_interpreter.get_diagnostics();
        if (!diagnostics.empty())
        {
            response = create_response_from_diagnostic(diagnostics.back(), result);
        }
        else
        {
            // Fallback to old method if no diagnostic available
            response = analyze_error(result, code);
        }
    }

    // Check for deprecated functions
    // TODO: Parse code to detect deprecated function usage

    return response;
}

// Register deprecated functions
void ModuLisp::register_deprecated(const std::string& old_name,
                                   const std::string& new_name)
{
    deprecated_functions[old_name] = new_name;
}

// Analyze error and add helpful information
ModuLisp::Response ModuLisp::analyze_error(const Value& result,
                                           const std::string& code)
{
    Response response(result);

    // Parse the code to extract function name for undefined function errors
    std::string function_name;
    if (code.length() > 0 && code[0] == '(')
    {
        // Extract first atom after opening parenthesis
        size_t start = 1;
        while (start < code.length() && std::isspace(code[start]))
            start++;
        size_t end = start;
        while (end < code.length() && !std::isspace(code[end]) && code[end] != ')')
            end++;
        if (end > start)
        {
            function_name = code.substr(start, end - start);
        }
    }
    else
    {
        // For simple symbol lookup, the code is just the symbol name
        function_name = code;
        // Remove whitespace
        function_name.erase(
            std::remove_if(function_name.begin(), function_name.end(), ::isspace),
            function_name.end());
    }

    // Determine error type and message based on context
    if (!function_name.empty())
    {
        if (code[0] == '(')
        {
            // Function call - undefined function
            response.set_error_type(ErrorType::UNDEFINED_FUNCTION);
            response.set_error_message("Function '" + function_name +
                                       "' is not defined");
        }
        else
        {
            // Variable reference - undefined variable
            response.set_error_type(ErrorType::UNDEFINED_VARIABLE);
            response.set_error_message("Variable '" + function_name +
                                       "' is not defined");
        }
    }
    else
    {
        // Generic error
        response.set_error_type(ErrorType::GENERIC_ERROR);
        response.set_error_message("Evaluation error occurred");
    }

    // Add context-specific suggestions
    detect_suggestions(response, code);

    return response;
}

// Detect suggestions based on error and context
void ModuLisp::detect_suggestions(Response& response, const std::string& code)
{
    // This is a simplified implementation
    // In reality, we'd parse the code and provide specific suggestions

    switch (current_context)
    {
    case Context::AUDIO:
        response.set_suggestion("Check audio-related function names and parameters");
        break;
    case Context::PATTERN:
        response.set_suggestion("Check pattern sequencing syntax");
        break;
    default:
        response.set_suggestion("Check syntax and function names");
        break;
    }
}

// Response formatting methods
std::string ModuLisp::Response::format_for_terminal() const
{
    std::ostringstream out;

    if (is_error())
    {
        out << "Error: ";
        if (line > 0)
        {
            out << "line " << line;
            if (column > 0)
            {
                out << ", column " << column;
            }
            out << "\n";
        }

        out << error_message << "\n";

        if (!context_snippet.empty())
        {
            out << "\n" << context_snippet << "\n";
        }

        if (!suggestion.empty())
        {
            out << "\nSuggestion: " << suggestion << "\n";
        }

        if (!did_you_mean.empty())
        {
            out << "\nDid you mean:\n";
            for (const auto& s : did_you_mean)
            {
                out << "  • " << s << "\n";
            }
        }

        if (!examples.empty())
        {
            out << "\nExamples:\n";
            for (const auto& e : examples)
            {
                out << "  " << e << "\n";
            }
        }
    }
    else
    {
        out << value.display().c_str();

        if (!warnings.empty())
        {
            out << "\nWarnings:\n";
            for (const auto& w : warnings)
            {
                out << "  • " << w << "\n";
            }
        }
    }

    return out.str();
}

std::string ModuLisp::Response::to_json() const
{
    std::ostringstream json;
    json << "{";

    json << "\"error\": " << (is_error() ? "true" : "false");

    if (is_error())
    {
        json << ", \"type\": " << static_cast<int>(error_type);
        json << ", \"message\": \"" << error_message << "\"";

        if (line > 0)
        {
            json << ", \"line\": " << line;
        }
        if (column > 0)
        {
            json << ", \"column\": " << column;
        }

        if (!suggestion.empty())
        {
            json << ", \"suggestion\": \"" << suggestion << "\"";
        }

        if (!did_you_mean.empty())
        {
            json << ", \"suggestions\": [";
            for (size_t i = 0; i < did_you_mean.size(); ++i)
            {
                if (i > 0)
                    json << ", ";
                json << "\"" << did_you_mean[i] << "\"";
            }
            json << "]";
        }
    }
    else
    {
        json << ", \"value\": \"" << value.display().c_str() << "\"";
    }

    if (!warnings.empty())
    {
        json << ", \"warnings\": [";
        for (size_t i = 0; i < warnings.size(); ++i)
        {
            if (i > 0)
                json << ", ";
            json << "\"" << warnings[i] << "\"";
        }
        json << "]";
    }

    json << "}";
    return json.str();
}

std::string ModuLisp::Response::to_log_format() const
{
    std::ostringstream log;

    if (is_error())
    {
        log << "ERROR ";
        log << static_cast<int>(error_type) << " ";
        if (line > 0)
        {
            log << line << ":" << column << " ";
        }
        log << error_message;
    }
    else
    {
        log << "SUCCESS ";
        if (!warnings.empty())
        {
            log << "WARNINGS:" << warnings.size() << " ";
        }
    }

    return log.str();
}

// Create response from a Diagnostic
ModuLisp::Response
ModuLisp::create_response_from_diagnostic(const Diagnostic& diag,
                                          const Value& result)
{
    Response response(result);

    // Map DiagnosticCategory to ModuLisp::ErrorType
    ModuLisp::ErrorType error_type = ErrorType::GENERIC_ERROR;
    switch (diag.category)
    {
    case DiagnosticCategory::Syntax:
        error_type = ErrorType::SYNTAX_ERROR;
        break;
    case DiagnosticCategory::UndefinedName:
        error_type = ErrorType::UNDEFINED_VARIABLE;
        break;
    case DiagnosticCategory::Arity:
        error_type = ErrorType::ARITY_ERROR;
        break;
    case DiagnosticCategory::Type:
        error_type = ErrorType::TYPE_ERROR;
        break;
    case DiagnosticCategory::Arithmetic:
        error_type = ErrorType::ARITHMETIC_ERROR;
        break;
    case DiagnosticCategory::Runtime:
    case DiagnosticCategory::Boundary:
    case DiagnosticCategory::Overflow:
    default:
        error_type = ErrorType::GENERIC_ERROR;
        break;
    }

    // Set error information
    response.set_error_type(error_type);
    response.set_error_message(std::string(diag.message.c_str()));

    // Set suggestion
    if (diag.suggestion.length() != 0)
    {
        response.set_suggestion(std::string(diag.suggestion.c_str()));
    }

    // Set example
    if (diag.example.length() != 0)
    {
        response.add_example(std::string(diag.example.c_str()));
    }

    return response;
}
