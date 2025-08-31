#include "modulisp.h"
#include <sstream>
#include <algorithm>
#include <iomanip>

// Constructor
ModuLisp::ModuLisp() : current_context(Context::GENERAL) {
    // Create the interpreter with default settings
    interpreter = std::make_unique<ModuLispInterpreter>();
    
    // Initialize the interpreter
    interpreter->init();
    interpreter->init_builtinfuncs();
}

// Destructor
ModuLisp::~ModuLisp() = default;

// Main API: send LISP code and get response
ModuLisp::Response ModuLisp::send(const std::string& code) {
    // Convert std::string to Arduino String
    String arduino_code(code.c_str());
    
    // Parse and evaluate the code
    Value result = interpreter->eval_v(arduino_code);
    
    // Create response and analyze for errors
    Response response(result);
    
    // If it's an error, add helpful information
    if (result.get_type_enum() == 15) { // ERROR type
        response = analyze_error(result, code);
    }
    
    // Check for deprecated functions
    // TODO: Parse code to detect deprecated function usage
    
    return response;
}

// Register deprecated functions
void ModuLisp::register_deprecated(const std::string& old_name, const std::string& new_name) {
    deprecated_functions[old_name] = new_name;
}

// Analyze error and add helpful information
ModuLisp::Response ModuLisp::analyze_error(const Value& result, const std::string& code) {
    Response response(result);
    
    // Try to determine error type from the error value
    // This is simplified - in reality we'd need more sophisticated parsing
    response.set_error_type(ErrorType::GENERIC_ERROR);
    response.set_error_message("Evaluation error occurred");
    
    // Add context-specific suggestions
    detect_suggestions(response, code);
    
    return response;
}

// Detect suggestions based on error and context
void ModuLisp::detect_suggestions(Response& response, const std::string& code) {
    // This is a simplified implementation
    // In reality, we'd parse the code and provide specific suggestions
    
    switch (current_context) {
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
std::string ModuLisp::Response::format_for_terminal() const {
    std::ostringstream out;
    
    if (is_error()) {
        out << "Error: ";
        if (line > 0) {
            out << "line " << line;
            if (column > 0) {
                out << ", column " << column;
            }
            out << "\n";
        }
        
        out << error_message << "\n";
        
        if (!context_snippet.empty()) {
            out << "\n" << context_snippet << "\n";
        }
        
        if (!suggestion.empty()) {
            out << "\nSuggestion: " << suggestion << "\n";
        }
        
        if (!did_you_mean.empty()) {
            out << "\nDid you mean:\n";
            for (const auto& s : did_you_mean) {
                out << "  • " << s << "\n";
            }
        }
        
        if (!examples.empty()) {
            out << "\nExamples:\n";
            for (const auto& e : examples) {
                out << "  " << e << "\n";
            }
        }
    } else {
        out << value.display().c_str();
        
        if (!warnings.empty()) {
            out << "\nWarnings:\n";
            for (const auto& w : warnings) {
                out << "  • " << w << "\n";
            }
        }
    }
    
    return out.str();
}

std::string ModuLisp::Response::to_json() const {
    std::ostringstream json;
    json << "{";
    
    json << "\"error\": " << (is_error() ? "true" : "false");
    
    if (is_error()) {
        json << ", \"type\": " << static_cast<int>(error_type);
        json << ", \"message\": \"" << error_message << "\"";
        
        if (line > 0) {
            json << ", \"line\": " << line;
        }
        if (column > 0) {
            json << ", \"column\": " << column;
        }
        
        if (!suggestion.empty()) {
            json << ", \"suggestion\": \"" << suggestion << "\"";
        }
        
        if (!did_you_mean.empty()) {
            json << ", \"suggestions\": [";
            for (size_t i = 0; i < did_you_mean.size(); ++i) {
                if (i > 0) json << ", ";
                json << "\"" << did_you_mean[i] << "\"";
            }
            json << "]";
        }
    } else {
        json << ", \"value\": \"" << value.display().c_str() << "\"";
    }
    
    if (!warnings.empty()) {
        json << ", \"warnings\": [";
        for (size_t i = 0; i < warnings.size(); ++i) {
            if (i > 0) json << ", ";
            json << "\"" << warnings[i] << "\"";
        }
        json << "]";
    }
    
    json << "}";
    return json.str();
}

std::string ModuLisp::Response::to_log_format() const {
    std::ostringstream log;
    
    if (is_error()) {
        log << "ERROR ";
        log << static_cast<int>(error_type) << " ";
        if (line > 0) {
            log << line << ":" << column << " ";
        }
        log << error_message;
    } else {
        log << "SUCCESS ";
        if (!warnings.empty()) {
            log << "WARNINGS:" << warnings.size() << " ";
        }
    }
    
    return log.str();
}