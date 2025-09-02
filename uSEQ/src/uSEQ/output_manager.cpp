#include "output_manager.h"
#include "../uSEQ.h"
#include "../uSEQ/configure.h"

// Using declaration for OutputType enum class
using OutputType = OutputManager::OutputType;

// Static configuration table - single source of truth for all output functions
const std::vector<OutputManager::OutputConfig> OutputManager::s_output_configs = {
    // Continuous/Analog Outputs (a1-a8)
    {1, OutputType::CONTINUOUS, 0, "a1", "get-a1"},
    {2, OutputType::CONTINUOUS, 1, "a2", "get-a2"},
    {3, OutputType::CONTINUOUS, 2, "a3", "get-a3"},
    {4, OutputType::CONTINUOUS, 3, "a4", "get-a4"},
    {5, OutputType::CONTINUOUS, 4, "a5", "get-a5"},
    {6, OutputType::CONTINUOUS, 5, "a6", "get-a6"},
    {7, OutputType::CONTINUOUS, 6, "a7", "get-a7"},
    {8, OutputType::CONTINUOUS, 7, "a8", "get-a8"},
    
    // Binary/Digital Outputs (d1-d8)
    {1, OutputType::BINARY, 0, "d1", "get-d1"},
    {2, OutputType::BINARY, 1, "d2", "get-d2"},
    {3, OutputType::BINARY, 2, "d3", "get-d3"},
    {4, OutputType::BINARY, 3, "d4", "get-d4"},
    {5, OutputType::BINARY, 4, "d5", "get-d5"},
    {6, OutputType::BINARY, 5, "d6", "get-d6"},
    {7, OutputType::BINARY, 6, "d7", "get-d7"},
    {8, OutputType::BINARY, 7, "d8", "get-d8"},
    
    // Serial Outputs (s1-s8)
    {1, OutputType::SERIAL_OUT, 0, "s1", "get-s1"},
    {2, OutputType::SERIAL_OUT, 1, "s2", "get-s2"},
    {3, OutputType::SERIAL_OUT, 2, "s3", "get-s3"},
    {4, OutputType::SERIAL_OUT, 3, "s4", "get-s4"},
    {5, OutputType::SERIAL_OUT, 4, "s5", "get-s5"},
    {6, OutputType::SERIAL_OUT, 5, "s6", "get-s6"},
    {7, OutputType::SERIAL_OUT, 6, "s7", "get-s7"},
    {8, OutputType::SERIAL_OUT, 7, "s8", "get-s8"}
};

OutputManager::OutputManager(uSEQ* useq_instance) : m_useq(useq_instance) {
}

const OutputManager::OutputConfig* OutputManager::get_config(int output_id, OutputType type) {
    for (const auto& config : s_output_configs) {
        if (config.id == output_id && config.type == type) {
            return &config;
        }
    }
    return nullptr;
}

const std::vector<OutputManager::OutputConfig>& OutputManager::get_all_configs() {
    return s_output_configs;
}

bool OutputManager::is_output_available(int output_id, OutputType type) {
    switch (type) {
        case OutputType::CONTINUOUS:
            return output_id >= 1 && static_cast<size_t>(output_id) <= NUM_CONTINUOUS_OUTS;
        case OutputType::BINARY:
            return output_id >= 1 && static_cast<size_t>(output_id) <= NUM_BINARY_OUTS;
        case OutputType::SERIAL_OUT:
            return output_id >= 1 && static_cast<size_t>(output_id) <= NUM_SERIAL_OUTS;
    }
    return false;
}

const char* OutputManager::get_lisp_name(int output_id, OutputType type) {
    const auto* config = get_config(output_id, type);
    return config ? config->lisp_name : nullptr;
}

const char* OutputManager::get_getter_name(int output_id, OutputType type) {
    const auto* config = get_config(output_id, type);
    return config ? config->getter_name : nullptr;
}

Value OutputManager::handle_output_setter(int output_id, OutputType type, std::vector<Value>& args, Environment& env) {
    // Validate output is available for current hardware
    if (!is_output_available(output_id, type)) {
        return Value::nil(); // Silently ignore unavailable outputs
    }
    
    switch (type) {
        case OutputType::CONTINUOUS:
            return set_continuous_output(output_id, args, env);
        case OutputType::BINARY:
            return set_binary_output(output_id, args, env);
        case OutputType::SERIAL_OUT:
            return set_serial_output(output_id, args, env);
    }
    return Value::nil();
}

Value OutputManager::handle_output_getter(int output_id, OutputType type, std::vector<Value>& args, Environment& env) {
    // Validate output is available for current hardware
    if (!is_output_available(output_id, type)) {
        return Value(0.0); // Return 0 for unavailable outputs
    }
    
    switch (type) {
        case OutputType::CONTINUOUS:
            return get_continuous_output(output_id, args, env);
        case OutputType::BINARY:
            return get_binary_output(output_id, args, env);
        case OutputType::SERIAL_OUT:
            return get_serial_output(output_id, args, env);
    }
    return Value(0.0);
}

Value OutputManager::set_continuous_output(int output_id, std::vector<Value>& args, Environment& env) {
    const auto* config = get_config(output_id, OutputType::CONTINUOUS);
    if (!config) return Value::nil();
    
    // Set the expression in the environment
    m_useq->get_environment()->set_expr(config->lisp_name, args[0]);
    
    // Store in the AST array - convert to 0-based indexing
    m_useq->m_continuous_ASTs[config->pin_index] = args[0];
    
    return Value::atom(config->lisp_name);
}

Value OutputManager::set_binary_output(int output_id, std::vector<Value>& args, Environment& env) {
    const auto* config = get_config(output_id, OutputType::BINARY);
    if (!config) return Value::nil();
    
    // Set the expression in the environment
    m_useq->get_environment()->set_expr(config->lisp_name, args[0]);
    
    // Store in the AST array - convert to 0-based indexing  
    m_useq->m_binary_ASTs[config->pin_index] = args[0];
    
    return Value::atom(config->lisp_name);
}

Value OutputManager::set_serial_output(int output_id, std::vector<Value>& args, Environment& env) {
    const auto* config = get_config(output_id, OutputType::SERIAL_OUT);
    if (!config) return Value::nil();
    
    // Set the expression in the environment
    m_useq->get_environment()->set_expr(config->lisp_name, args[0]);
    
    // Store in the AST array - convert to 0-based indexing
    m_useq->m_serial_ASTs[config->pin_index] = args[0];
    
    return Value::atom(config->lisp_name);
}

Value OutputManager::get_continuous_output(int output_id, std::vector<Value>& args, Environment& env) {
    const auto* config = get_config(output_id, OutputType::CONTINUOUS);
    if (!config) return Value(0.0);
    
    // Return current output value - convert to 0-based indexing
    return Value(m_useq->m_continuous_vals[config->pin_index]);
}

Value OutputManager::get_binary_output(int output_id, std::vector<Value>& args, Environment& env) {
    const auto* config = get_config(output_id, OutputType::BINARY);
    if (!config) return Value(0.0);
    
    // Return current output value - convert to 0-based indexing
    return Value(static_cast<double>(m_useq->m_binary_vals[config->pin_index]));
}

Value OutputManager::get_serial_output(int output_id, std::vector<Value>& args, Environment& env) {
    const auto* config = get_config(output_id, OutputType::SERIAL_OUT);
    if (!config) return Value(0.0);
    
    // Return current output value - convert to 0-based indexing
    const auto& serial_val = m_useq->m_serial_vals[config->pin_index];
    if (serial_val.has_value()) {
        return Value(serial_val.value());
    }
    return Value(0.0);
}

// Template method implementations (moved from header to avoid incomplete type warnings)
template<int OutputId, OutputManager::OutputType Type>
Value OutputFunctionGenerator<OutputId, Type>::setter_function(uSEQ* useq, std::vector<Value>& args, Environment& env) {
    return useq->get_output_manager().handle_output_setter(OutputId, Type, args, env);
}

template<int OutputId, OutputManager::OutputType Type>
Value OutputFunctionGenerator<OutputId, Type>::getter_function(uSEQ* useq, std::vector<Value>& args, Environment& env) {
    return useq->get_output_manager().handle_output_getter(OutputId, Type, args, env);
}

// Explicit template instantiations for all combinations used
// Analog outputs (a1-a8)
template class OutputFunctionGenerator<1, OutputManager::OutputType::CONTINUOUS>;
template class OutputFunctionGenerator<2, OutputManager::OutputType::CONTINUOUS>;
template class OutputFunctionGenerator<3, OutputManager::OutputType::CONTINUOUS>;
template class OutputFunctionGenerator<4, OutputManager::OutputType::CONTINUOUS>;
template class OutputFunctionGenerator<5, OutputManager::OutputType::CONTINUOUS>;
template class OutputFunctionGenerator<6, OutputManager::OutputType::CONTINUOUS>;
template class OutputFunctionGenerator<7, OutputManager::OutputType::CONTINUOUS>;
template class OutputFunctionGenerator<8, OutputManager::OutputType::CONTINUOUS>;

// Digital outputs (d1-d8)
template class OutputFunctionGenerator<1, OutputManager::OutputType::BINARY>;
template class OutputFunctionGenerator<2, OutputManager::OutputType::BINARY>;
template class OutputFunctionGenerator<3, OutputManager::OutputType::BINARY>;
template class OutputFunctionGenerator<4, OutputManager::OutputType::BINARY>;
template class OutputFunctionGenerator<5, OutputManager::OutputType::BINARY>;
template class OutputFunctionGenerator<6, OutputManager::OutputType::BINARY>;
template class OutputFunctionGenerator<7, OutputManager::OutputType::BINARY>;
template class OutputFunctionGenerator<8, OutputManager::OutputType::BINARY>;

// Serial outputs (s1-s8) - setters only
template class OutputFunctionGenerator<1, OutputType::SERIAL_OUT>;
template class OutputFunctionGenerator<2, OutputType::SERIAL_OUT>;
template class OutputFunctionGenerator<3, OutputType::SERIAL_OUT>;
template class OutputFunctionGenerator<4, OutputType::SERIAL_OUT>;
template class OutputFunctionGenerator<5, OutputType::SERIAL_OUT>;
template class OutputFunctionGenerator<6, OutputType::SERIAL_OUT>;
template class OutputFunctionGenerator<7, OutputType::SERIAL_OUT>;
template class OutputFunctionGenerator<8, OutputType::SERIAL_OUT>;