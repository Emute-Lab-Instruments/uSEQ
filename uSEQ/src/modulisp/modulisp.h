#ifndef MODULISP_H_
#define MODULISP_H_

#include "modulisp_interpreter.h"
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <cctype>

// Simple facade class for easy LISP interaction
// This is the main public API for ModuLisp
class ModuLisp {
  public:
    // Error types for categorizing different kinds of errors
    enum class ErrorType {
        NONE,
        SYNTAX_ERROR,
        UNDEFINED_FUNCTION,
        UNDEFINED_VARIABLE,
        ARITY_ERROR,
        TYPE_ERROR,
        ARITHMETIC_ERROR,
        INDEX_ERROR,
        RECURSION_ERROR,
        TIMEOUT_ERROR,
        PARAMETER_ERROR,
        TIMING_ERROR,
        IMMUTABLE_ERROR,
        DESTRUCTURING_ERROR,
        GENERIC_ERROR
    };
    
    // Context for providing better suggestions
    enum class Context {
        GENERAL,
        AUDIO,
        PATTERN,
        VISUAL,
        TIMING
    };
    
    // Response wrapper class for LISP evaluation results
    class Response {
      public:
        Response(const Value& val) : value(val), error_type(ErrorType::NONE), line(0), column(0) {}
        
        // Check if the evaluation succeeded (not error or nil)
        bool okay() const {
            auto type_enum = value.get_type_enum();
            // Based on enum in value.h: NIL=13, ERROR=15
            return type_enum != 15 && type_enum != 13;
        }
        
        // Get the type name (returns capitalized strings like "Integer", "Float", "Symbol")
        std::string type() const {
            String type_name = value.get_type_name();
            std::string result(type_name.c_str());
            
            // Convert to capitalized format for consistency with test expectations
            if (result == "int") return "Integer";
            if (result == "float") return "Float";
            if (result == "atom") return "Symbol";
            if (result == "string") return "String";
            if (result == "list") return "List";
            if (result == "vector") return "Vector";
            if (result == "function") return "Function";
            if (result == "nil") return "Nil";
            if (result == "error") return "Error";
            if (result == "unit") return "Unit";
            
            // Default: capitalize first letter
            if (!result.empty()) {
                result[0] = std::toupper(result[0]);
            }
            return result;
        }
        
        // Direct mappings to Value methods
        int as_int() const { return value.as_int(); }
        double as_float() const { return value.as_float(); }
        std::string as_string() const { 
            String str = value.as_string();
            return std::string(str.c_str()); 
        }
        std::string as_symbol() const { 
            String atom = value.as_atom();
            return std::string(atom.c_str()); 
        }
        std::vector<Value> as_list() const { return value.as_list(); }
        
        // Access underlying value if needed
        const Value& get_value() const { return value; }
        
        // Error handling methods
        bool is_error() const { return error_type != ErrorType::NONE || value.get_type_enum() == 15; }
        ErrorType get_error_type() const { return error_type; }
        const std::string& get_error_message() const { return error_message; }
        const std::string& get_suggestion() const { return suggestion; }
        const std::vector<std::string>& get_did_you_mean() const { return did_you_mean; }
        const std::vector<std::string>& get_examples() const { return examples; }
        const std::string& get_context_snippet() const { return context_snippet; }
        const std::vector<std::string>& get_stack_trace() const { return stack_trace; }
        const std::vector<std::string>& get_warnings() const { return warnings; }
        const std::string& get_detailed_error() const { return detailed_error; }
        
        // Location information
        int get_line() const { return line; }
        int get_column() const { return column; }
        
        // Formatting methods
        std::string format_for_terminal() const;
        std::string to_json() const;
        std::string to_log_format() const;
        
        // Builder methods for setting error information
        Response& set_error_type(ErrorType type) { error_type = type; return *this; }
        Response& set_error_message(const std::string& msg) { error_message = msg; return *this; }
        Response& set_location(int l, int c) { line = l; column = c; return *this; }
        Response& set_suggestion(const std::string& s) { suggestion = s; return *this; }
        Response& add_did_you_mean(const std::string& s) { did_you_mean.push_back(s); return *this; }
        Response& add_example(const std::string& e) { examples.push_back(e); return *this; }
        Response& set_context_snippet(const std::string& c) { context_snippet = c; return *this; }
        Response& add_stack_frame(const std::string& f) { stack_trace.push_back(f); return *this; }
        Response& add_warning(const std::string& w) { warnings.push_back(w); return *this; }
        Response& set_detailed_error(const std::string& d) { detailed_error = d; return *this; }
        
      private:
        Value value;
        ErrorType error_type;
        std::string error_message;
        std::string detailed_error;
        std::string suggestion;
        std::string context_snippet;
        std::vector<std::string> did_you_mean;
        std::vector<std::string> examples;
        std::vector<std::string> stack_trace;
        std::vector<std::string> warnings;
        int line;
        int column;
    };
    
    // Constructor
    ModuLisp();
    ~ModuLisp();
    
    // Main API: send LISP code and get response
    Response send(const std::string& code);
    
    // Context management for better error suggestions
    void set_context(Context ctx) { current_context = ctx; }
    Context get_context() const { return current_context; }
    
    // Register deprecated functions
    void register_deprecated(const std::string& old_name, const std::string& new_name);
    
    // Direct access to interpreter if needed (for advanced use)
    ModuLispInterpreter* get_interpreter() { return interpreter.get(); }
    
  private:
    std::unique_ptr<ModuLispInterpreter> interpreter;
    Context current_context;
    std::map<std::string, std::string> deprecated_functions;
    
    // Helper methods for error detection
    Response analyze_error(const Value& result, const std::string& code);
    void detect_suggestions(Response& response, const std::string& code);
};

#endif // MODULISP_H_