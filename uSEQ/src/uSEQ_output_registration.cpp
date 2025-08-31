#include "uSEQ_output_registration.h"

// Data table for all output functions
const OutputRegistrationHelper::OutputConfig OutputRegistrationHelper::output_configs[] = {
    // Analog outputs (a1-a8)
    {"a1", "get-a1", 1, OutputManager::OutputType::CONTINUOUS},
    {"a2", "get-a2", 2, OutputManager::OutputType::CONTINUOUS},
    {"a3", "get-a3", 3, OutputManager::OutputType::CONTINUOUS},
    {"a4", "get-a4", 4, OutputManager::OutputType::CONTINUOUS},
    {"a5", "get-a5", 5, OutputManager::OutputType::CONTINUOUS},
    {"a6", "get-a6", 6, OutputManager::OutputType::CONTINUOUS},
    {"a7", "get-a7", 7, OutputManager::OutputType::CONTINUOUS},
    {"a8", "get-a8", 8, OutputManager::OutputType::CONTINUOUS},
    
    // Digital outputs (d1-d8)
    {"d1", "get-d1", 1, OutputManager::OutputType::BINARY},
    {"d2", "get-d2", 2, OutputManager::OutputType::BINARY},
    {"d3", "get-d3", 3, OutputManager::OutputType::BINARY},
    {"d4", "get-d4", 4, OutputManager::OutputType::BINARY},
    {"d5", "get-d5", 5, OutputManager::OutputType::BINARY},
    {"d6", "get-d6", 6, OutputManager::OutputType::BINARY},
    {"d7", "get-d7", 7, OutputManager::OutputType::BINARY},
    {"d8", "get-d8", 8, OutputManager::OutputType::BINARY},
    
    // Serial outputs (s1-s8) - no getters
    {"s1", nullptr, 1, OutputManager::OutputType::SERIAL},
    {"s2", nullptr, 2, OutputManager::OutputType::SERIAL},
    {"s3", nullptr, 3, OutputManager::OutputType::SERIAL},
    {"s4", nullptr, 4, OutputManager::OutputType::SERIAL},
    {"s5", nullptr, 5, OutputManager::OutputType::SERIAL},
    {"s6", nullptr, 6, OutputManager::OutputType::SERIAL},
    {"s7", nullptr, 7, OutputManager::OutputType::SERIAL},
    {"s8", nullptr, 8, OutputManager::OutputType::SERIAL},
};

const size_t OutputRegistrationHelper::num_configs = 
    sizeof(output_configs) / sizeof(output_configs[0]);

// Template helper to generate static wrapper functions
template<int OutputId, OutputManager::OutputType Type>
struct OutputFunctionWrapper {
    static Value setter(std::vector<Value>& args, Environment& env) {
        uSEQ* instance = uSEQ::instance;
        if (instance) {
            return instance->get_output_manager().handle_output_setter(OutputId, Type, args, env);
        }
        return Value::error();
    }
    
    static Value getter(std::vector<Value>& args, Environment& env) {
        uSEQ* instance = uSEQ::instance;
        if (instance) {
            return instance->get_output_manager().handle_output_getter(OutputId, Type, args, env);
        }
        return Value::error();
    }
};

// Helper macro to instantiate wrapper functions
#define INSTANTIATE_WRAPPER(id, type) \
    template struct OutputFunctionWrapper<id, OutputManager::OutputType::type>;

// Instantiate all needed wrappers
INSTANTIATE_WRAPPER(1, CONTINUOUS)
INSTANTIATE_WRAPPER(2, CONTINUOUS)
INSTANTIATE_WRAPPER(3, CONTINUOUS)
INSTANTIATE_WRAPPER(4, CONTINUOUS)
INSTANTIATE_WRAPPER(5, CONTINUOUS)
INSTANTIATE_WRAPPER(6, CONTINUOUS)
INSTANTIATE_WRAPPER(7, CONTINUOUS)
INSTANTIATE_WRAPPER(8, CONTINUOUS)

INSTANTIATE_WRAPPER(1, BINARY)
INSTANTIATE_WRAPPER(2, BINARY)
INSTANTIATE_WRAPPER(3, BINARY)
INSTANTIATE_WRAPPER(4, BINARY)
INSTANTIATE_WRAPPER(5, BINARY)
INSTANTIATE_WRAPPER(6, BINARY)
INSTANTIATE_WRAPPER(7, BINARY)
INSTANTIATE_WRAPPER(8, BINARY)

INSTANTIATE_WRAPPER(1, SERIAL)
INSTANTIATE_WRAPPER(2, SERIAL)
INSTANTIATE_WRAPPER(3, SERIAL)
INSTANTIATE_WRAPPER(4, SERIAL)
INSTANTIATE_WRAPPER(5, SERIAL)
INSTANTIATE_WRAPPER(6, SERIAL)
INSTANTIATE_WRAPPER(7, SERIAL)
INSTANTIATE_WRAPPER(8, SERIAL)

// Function pointer lookup table
typedef Value (*OutputFuncPtr)(std::vector<Value>&, Environment&);

// Create lookup tables for function pointers
static OutputFuncPtr get_setter_func(int id, OutputManager::OutputType type) {
    if (type == OutputManager::OutputType::CONTINUOUS) {
        switch(id) {
            case 1: return OutputFunctionWrapper<1, OutputManager::OutputType::CONTINUOUS>::setter;
            case 2: return OutputFunctionWrapper<2, OutputManager::OutputType::CONTINUOUS>::setter;
            case 3: return OutputFunctionWrapper<3, OutputManager::OutputType::CONTINUOUS>::setter;
            case 4: return OutputFunctionWrapper<4, OutputManager::OutputType::CONTINUOUS>::setter;
            case 5: return OutputFunctionWrapper<5, OutputManager::OutputType::CONTINUOUS>::setter;
            case 6: return OutputFunctionWrapper<6, OutputManager::OutputType::CONTINUOUS>::setter;
            case 7: return OutputFunctionWrapper<7, OutputManager::OutputType::CONTINUOUS>::setter;
            case 8: return OutputFunctionWrapper<8, OutputManager::OutputType::CONTINUOUS>::setter;
        }
    } else if (type == OutputManager::OutputType::BINARY) {
        switch(id) {
            case 1: return OutputFunctionWrapper<1, OutputManager::OutputType::BINARY>::setter;
            case 2: return OutputFunctionWrapper<2, OutputManager::OutputType::BINARY>::setter;
            case 3: return OutputFunctionWrapper<3, OutputManager::OutputType::BINARY>::setter;
            case 4: return OutputFunctionWrapper<4, OutputManager::OutputType::BINARY>::setter;
            case 5: return OutputFunctionWrapper<5, OutputManager::OutputType::BINARY>::setter;
            case 6: return OutputFunctionWrapper<6, OutputManager::OutputType::BINARY>::setter;
            case 7: return OutputFunctionWrapper<7, OutputManager::OutputType::BINARY>::setter;
            case 8: return OutputFunctionWrapper<8, OutputManager::OutputType::BINARY>::setter;
        }
    } else if (type == OutputManager::OutputType::SERIAL) {
        switch(id) {
            case 1: return OutputFunctionWrapper<1, OutputManager::OutputType::SERIAL>::setter;
            case 2: return OutputFunctionWrapper<2, OutputManager::OutputType::SERIAL>::setter;
            case 3: return OutputFunctionWrapper<3, OutputManager::OutputType::SERIAL>::setter;
            case 4: return OutputFunctionWrapper<4, OutputManager::OutputType::SERIAL>::setter;
            case 5: return OutputFunctionWrapper<5, OutputManager::OutputType::SERIAL>::setter;
            case 6: return OutputFunctionWrapper<6, OutputManager::OutputType::SERIAL>::setter;
            case 7: return OutputFunctionWrapper<7, OutputManager::OutputType::SERIAL>::setter;
            case 8: return OutputFunctionWrapper<8, OutputManager::OutputType::SERIAL>::setter;
        }
    }
    return nullptr;
}

static OutputFuncPtr get_getter_func(int id, OutputManager::OutputType type) {
    if (type == OutputManager::OutputType::CONTINUOUS) {
        switch(id) {
            case 1: return OutputFunctionWrapper<1, OutputManager::OutputType::CONTINUOUS>::getter;
            case 2: return OutputFunctionWrapper<2, OutputManager::OutputType::CONTINUOUS>::getter;
            case 3: return OutputFunctionWrapper<3, OutputManager::OutputType::CONTINUOUS>::getter;
            case 4: return OutputFunctionWrapper<4, OutputManager::OutputType::CONTINUOUS>::getter;
            case 5: return OutputFunctionWrapper<5, OutputManager::OutputType::CONTINUOUS>::getter;
            case 6: return OutputFunctionWrapper<6, OutputManager::OutputType::CONTINUOUS>::getter;
            case 7: return OutputFunctionWrapper<7, OutputManager::OutputType::CONTINUOUS>::getter;
            case 8: return OutputFunctionWrapper<8, OutputManager::OutputType::CONTINUOUS>::getter;
        }
    } else if (type == OutputManager::OutputType::BINARY) {
        switch(id) {
            case 1: return OutputFunctionWrapper<1, OutputManager::OutputType::BINARY>::getter;
            case 2: return OutputFunctionWrapper<2, OutputManager::OutputType::BINARY>::getter;
            case 3: return OutputFunctionWrapper<3, OutputManager::OutputType::BINARY>::getter;
            case 4: return OutputFunctionWrapper<4, OutputManager::OutputType::BINARY>::getter;
            case 5: return OutputFunctionWrapper<5, OutputManager::OutputType::BINARY>::getter;
            case 6: return OutputFunctionWrapper<6, OutputManager::OutputType::BINARY>::getter;
            case 7: return OutputFunctionWrapper<7, OutputManager::OutputType::BINARY>::getter;
            case 8: return OutputFunctionWrapper<8, OutputManager::OutputType::BINARY>::getter;
        }
    }
    // Serial outputs don't have getters
    return nullptr;
}

void OutputRegistrationHelper::registerAllOutputs(uSEQ* instance) {
    // Register all output functions using the data table
    for (size_t i = 0; i < num_configs; ++i) {
        const auto& config = output_configs[i];
        
        // Register setter
        OutputFuncPtr setter = get_setter_func(config.output_id, config.type);
        if (setter) {
            Environment::builtindefs()[config.setter_name] = 
                Value(config.setter_name, setter);
        }
        
        // Register getter if available
        if (config.getter_name) {
            OutputFuncPtr getter = get_getter_func(config.output_id, config.type);
            if (getter) {
                Environment::builtindefs()[config.getter_name] = 
                    Value(config.getter_name, getter);
            }
        }
    }
}