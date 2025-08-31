#include "output_functions.h"
#include "../modulisp/function_registry.h"
#include "../uSEQ.h"
#include "../utils/log.h"

// Static configuration data for all output functions
const OutputFunctionsModule::OutputFunctionConfig OutputFunctionsModule::output_configs[] = {
    // Analog/Continuous outputs (a1-a8)
    {"a1", "get-a1", 1, OutputManager::OutputType::CONTINUOUS},
    {"a2", "get-a2", 2, OutputManager::OutputType::CONTINUOUS},
    {"a3", "get-a3", 3, OutputManager::OutputType::CONTINUOUS},
    {"a4", "get-a4", 4, OutputManager::OutputType::CONTINUOUS},
    {"a5", "get-a5", 5, OutputManager::OutputType::CONTINUOUS},
    {"a6", "get-a6", 6, OutputManager::OutputType::CONTINUOUS},
    {"a7", "get-a7", 7, OutputManager::OutputType::CONTINUOUS},
    {"a8", "get-a8", 8, OutputManager::OutputType::CONTINUOUS},
    
    // Digital/Binary outputs (d1-d8)
    {"d1", "get-d1", 1, OutputManager::OutputType::BINARY},
    {"d2", "get-d2", 2, OutputManager::OutputType::BINARY},
    {"d3", "get-d3", 3, OutputManager::OutputType::BINARY},
    {"d4", "get-d4", 4, OutputManager::OutputType::BINARY},
    {"d5", "get-d5", 5, OutputManager::OutputType::BINARY},
    {"d6", "get-d6", 6, OutputManager::OutputType::BINARY},
    {"d7", "get-d7", 7, OutputManager::OutputType::BINARY},
    {"d8", "get-d8", 8, OutputManager::OutputType::BINARY},
    
    // Serial outputs (s1-s8)
    {"s1", nullptr, 1, OutputManager::OutputType::SERIAL},
    {"s2", nullptr, 2, OutputManager::OutputType::SERIAL},
    {"s3", nullptr, 3, OutputManager::OutputType::SERIAL},
    {"s4", nullptr, 4, OutputManager::OutputType::SERIAL},
    {"s5", nullptr, 5, OutputManager::OutputType::SERIAL},
    {"s6", nullptr, 6, OutputManager::OutputType::SERIAL},
    {"s7", nullptr, 7, OutputManager::OutputType::SERIAL},
    {"s8", nullptr, 8, OutputManager::OutputType::SERIAL},
};

const size_t OutputFunctionsModule::num_output_configs = 
    sizeof(output_configs) / sizeof(output_configs[0]);

OutputFunctionsModule::OutputFunctionsModule(uSEQ* useq_instance) 
    : FunctionModule("output"), useq_(useq_instance) {
}

void OutputFunctionsModule::registerFunctions(FunctionRegistry& registry) {
    // Register all output functions using the configuration table
    for (size_t i = 0; i < num_output_configs; ++i) {
        const auto& config = output_configs[i];
        
        // Register setter function
        registry.registerFunction(
            config.name,
            module_name_,
            [this, config](std::vector<Value>& args, Environment& env) -> Value {
                return handleOutputSetter(config.output_id, config.type, args, env);
            },
            false  // Not a special form
        );
        
        // Register getter function if it has one
        if (config.getter_name) {
            registry.registerFunction(
                config.getter_name,
                module_name_,
                [this, config](std::vector<Value>& args, Environment& env) -> Value {
                    return handleOutputGetter(config.output_id, config.type, args, env);
                },
                false  // Not a special form
            );
        }
    }
    
    DBG("OutputFunctionsModule: Registered " + String(std::to_string(num_output_configs)) + 
        " output functions with their getters");
}

Value OutputFunctionsModule::handleOutputSetter(int output_id, OutputManager::OutputType type,
                                               std::vector<Value>& args, Environment& env) {
    if (!useq_) {
        return Value::error();
    }
    
    // Delegate to the OutputManager for actual implementation
    return useq_->get_output_manager().handle_output_setter(output_id, type, args, env);
}

Value OutputFunctionsModule::handleOutputGetter(int output_id, OutputManager::OutputType type,
                                               std::vector<Value>& args, Environment& env) {
    if (!useq_) {
        return Value::error();
    }
    
    // Delegate to the OutputManager for actual implementation
    return useq_->get_output_manager().handle_output_getter(output_id, type, args, env);
}