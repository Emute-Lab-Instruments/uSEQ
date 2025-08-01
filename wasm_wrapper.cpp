#include "uSEQ/src/uSEQ.h"
#include <cstring>
#include <cstdlib>

// Static instance of uSEQ interpreter
static uSEQ* useq_instance = nullptr;

extern "C" {
    // Initialize the uSEQ interpreter
    void useq_init() {
        if (!useq_instance) {
            useq_instance = new uSEQ();
            useq_instance->init();
        }
    }
    
    // Evaluate a LISP expression and return the result
    // Input: C string from JavaScript
    // Output: Dynamically allocated C string (Emscripten will handle cleanup)
    char* useq_eval(const char* input) {
        if (!useq_instance) {
            const char* error_msg = "Error: uSEQ not initialized. Call useq_init() first.";
            char* result = (char*)malloc(strlen(error_msg) + 1);
            strcpy(result, error_msg);
            return result;
        }
        
        try {
            // Convert C string to String type
            String code(input);
            
            // Evaluate the expression
            String result_str = useq_instance->eval(code);
            
            // Convert result back to C string
            const char* result_cstr = result_str.c_str();
            char* result = (char*)malloc(strlen(result_cstr) + 1);
            strcpy(result, result_cstr);
            
            return result;
        } catch (...) {
            const char* error_msg = "Error: Failed to evaluate expression";
            char* result = (char*)malloc(strlen(error_msg) + 1);
            strcpy(result, error_msg);
            return result;
        }
    }
}