#ifndef OUTPUT_FUNCTIONS_H_
#define OUTPUT_FUNCTIONS_H_

#include "../modulisp/function_module.h"
#include "../uSEQ/output_manager.h"

// Forward declaration
class uSEQ;

/**
 * Module providing all output functions (a1-a8, d1-d8, s1-s8, etc.)
 * Uses a data-driven approach to eliminate code duplication
 */
class OutputFunctionsModule : public FunctionModule {
public:
    explicit OutputFunctionsModule(uSEQ* useq_instance);
    ~OutputFunctionsModule() override = default;
    
    void registerFunctions(FunctionRegistry& registry) override;
    
private:
    uSEQ* useq_;
    
    // Data-driven configuration for all output functions
    struct OutputFunctionConfig {
        const char* name;              // Function name (e.g., "a1")
        const char* getter_name;       // Getter function name (e.g., "get-a1")
        int output_id;                 // Output number (1-8)
        OutputManager::OutputType type; // CONTINUOUS, BINARY, or SERIAL
    };
    
    // Static configuration data for all outputs
    static const OutputFunctionConfig output_configs[];
    static const size_t num_output_configs;
    
    // Generic handlers that work for all output types
    Value handleOutputSetter(int output_id, OutputManager::OutputType type, 
                            std::vector<Value>& args, Environment& env);
    Value handleOutputGetter(int output_id, OutputManager::OutputType type,
                            std::vector<Value>& args, Environment& env);
};

#endif // OUTPUT_FUNCTIONS_H_