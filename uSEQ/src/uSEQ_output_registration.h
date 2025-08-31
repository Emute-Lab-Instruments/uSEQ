#ifndef USEQ_OUTPUT_REGISTRATION_H_
#define USEQ_OUTPUT_REGISTRATION_H_

#include "uSEQ.h"
#include "uSEQ/output_manager.h"

/**
 * Helper class for data-driven output function registration
 * This eliminates the need for 40+ individual function implementations
 */
class OutputRegistrationHelper {
public:
    struct OutputConfig {
        const char* setter_name;
        const char* getter_name;  // nullptr if no getter
        int output_id;
        OutputManager::OutputType type;
    };
    
    // Register all output functions using a data table
    static void registerAllOutputs(uSEQ* instance);
    
private:
    static const OutputConfig output_configs[];
    static const size_t num_configs;
};

#endif // USEQ_OUTPUT_REGISTRATION_H_