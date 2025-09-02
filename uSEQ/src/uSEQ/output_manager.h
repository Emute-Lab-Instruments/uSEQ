#ifndef USEQ_OUTPUT_MANAGER_H
#define USEQ_OUTPUT_MANAGER_H

#include "../modulisp/lisp/value.h"
#include <vector>
#include <string>

// Forward declarations
class uSEQ;
class Environment;

/**
 * Unified Output Management System for uSEQ
 * 
 * This class eliminates the massive code duplication in the output functions
 * by providing a unified interface for analog, digital, and serial outputs.
 * Instead of having 24+ nearly identical functions, we use data-driven
 * configuration with pin mapping tables.
 */
class OutputManager {
public:
    enum class OutputType {
        CONTINUOUS,  // Analog outputs (a1-a8)
        BINARY,      // Digital outputs (d1-d8)  
        SERIAL_OUT   // Serial outputs (s1-s8)
    };

    struct OutputConfig {
        int id;                    // 1-based output number (1-8)
        OutputType type;          
        int pin_index;            // Index into hardware pin arrays
        const char* lisp_name;    // LISP function name ("a1", "d1", "s1")
        const char* getter_name;  // LISP getter name ("get-a1", "get-d1", etc.)
    };

    explicit OutputManager(uSEQ* useq_instance);
    
    // Unified LISP function handlers - replaces 24+ duplicated functions
    Value handle_output_setter(int output_id, OutputType type, std::vector<Value>& args, Environment& env);
    Value handle_output_getter(int output_id, OutputType type, std::vector<Value>& args, Environment& env);
    
    // Static configuration data - replaces hardcoded functions
    static const OutputConfig* get_config(int output_id, OutputType type);
    static const std::vector<OutputConfig>& get_all_configs();
    
    // Utility functions
    static bool is_output_available(int output_id, OutputType type);
    static const char* get_lisp_name(int output_id, OutputType type);
    static const char* get_getter_name(int output_id, OutputType type);

private:
    uSEQ* m_useq;
    
    // Static configuration table - single source of truth
    static const std::vector<OutputConfig> s_output_configs;
    
    // Helper functions
    Value set_continuous_output(int output_id, std::vector<Value>& args, Environment& env);
    Value set_binary_output(int output_id, std::vector<Value>& args, Environment& env);
    Value set_serial_output(int output_id, std::vector<Value>& args, Environment& env);
    
    Value get_continuous_output(int output_id, std::vector<Value>& args, Environment& env);
    Value get_binary_output(int output_id, std::vector<Value>& args, Environment& env);
    Value get_serial_output(int output_id, std::vector<Value>& args, Environment& env);
};

// Template-based LISP function generators - replaces macro duplication
template<int OutputId, OutputManager::OutputType Type>
class OutputFunctionGenerator {
public:
    static Value setter_function(uSEQ* useq, std::vector<Value>& args, Environment& env);
    static Value getter_function(uSEQ* useq, std::vector<Value>& args, Environment& env);
};

// Convenience macros for generating the LISP function wrappers
#define USEQ_OUTPUT_SETTER(id, type) \
    Value useq_##type##id(std::vector<Value>& args, Environment& env) { \
        return OutputFunctionGenerator<id, OutputManager::OutputType::type>::setter_function(this, args, env); \
    }

#define USEQ_OUTPUT_GETTER(id, type) \
    Value useq_get_##type##id(std::vector<Value>& args, Environment& env) { \
        return OutputFunctionGenerator<id, OutputManager::OutputType::type>::getter_function(this, args, env); \
    }

#endif // USEQ_OUTPUT_MANAGER_H