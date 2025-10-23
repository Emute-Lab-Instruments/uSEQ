#include "../uSEQ/src/modulisp/modulisp_interpreter.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <limits>
#include <iostream>

// Static instance of ModuLisp interpreter (simpler than full uSEQ)
static ModuLispInterpreter* useq_instance = nullptr;
static bool init_called = false;
static int eval_count = 0;

extern "C"
{
    // Initialize the ModuLisp interpreter
    void useq_init()
    {
        std::cout << "[WASM] useq_init() called. init_called=" << init_called
                  << ", useq_instance=" << (void*)useq_instance << std::endl;

        if (init_called)
        {
            std::cout << "[WASM] WARNING: useq_init() called multiple times!" << std::endl;
            return;
        }

        if (!useq_instance)
        {
            std::cout << "[WASM] Creating new ModuLispInterpreter instance..." << std::endl;
            // Create with nullptr for ErrorManager - let the constructor handle defaults
            useq_instance = new ModuLispInterpreter(nullptr);
            std::cout << "[WASM] Calling init()..." << std::endl;
            useq_instance->init();
            std::cout << "[WASM] Initialization complete. Environment ptr="
                      << (void*)useq_instance->get_environment() << std::endl;
        }

        init_called = true;
    }

    // Evaluate a LISP expression and return the result
    // Input: C string from JavaScript
    // Output: Dynamically allocated C string (Emscripten will handle cleanup)
    char* useq_eval(const char* input)
    {
        eval_count++;
        std::cout << "[WASM] useq_eval() call #" << eval_count
                  << ". Input: " << (input ? input : "<null>") << std::endl;

        if (!useq_instance)
        {
            std::cout << "[WASM] ERROR: useq_instance is nullptr!" << std::endl;
            const char* error_msg =
                "Error: uSEQ not initialized. Call useq_init() first.";
            char* result = (char*)malloc(strlen(error_msg) + 1);
            strcpy(result, error_msg);
            return result;
        }

        std::cout << "[WASM] useq_instance=" << (void*)useq_instance
                  << ", environment=" << (void*)useq_instance->get_environment() << std::endl;

        // Check environment has some vars defined
        auto env = useq_instance->get_environment();
        if (env)
        {
            std::cout << "[WASM] Environment has " << env->get_defs().size() << " defs" << std::endl;
        }

        try
        {
            // Convert C string to String type
            String code(input);

            // Evaluate the expression
            std::cout << "[WASM] Calling eval()..." << std::endl;
            String result_str = useq_instance->eval(code);
            std::cout << "[WASM] Eval complete. Result: " << result_str.c_str() << std::endl;

            // Check environment again after eval
            if (env)
            {
                std::cout << "[WASM] After eval, environment has " << env->get_defs().size() << " defs" << std::endl;
            }

            // Convert result back to C string
            const char* result_cstr = result_str.c_str();
            char* result            = (char*)malloc(strlen(result_cstr) + 1);
            strcpy(result, result_cstr);

            return result;
        }
        catch (const std::exception& e)
        {
            std::cout << "[WASM] Exception during eval: " << e.what() << std::endl;
            String error_msg = "Error: ";
            error_msg += e.what();
            char* result = (char*)malloc(error_msg.length() + 1);
            strcpy(result, error_msg.c_str());
            return result;
        }
        catch (...)
        {
            std::cout << "[WASM] Unknown exception during eval" << std::endl;
            const char* error_msg = "Error: Failed to evaluate expression";
            char* result          = (char*)malloc(strlen(error_msg) + 1);
            strcpy(result, error_msg);
            return result;
        }
    }

    void useq_update_time(double time_seconds)
    {
        if (!useq_instance)
        {
            return;
        }

        const double micros = time_seconds * 1e6;
        useq_instance->set_time_from_external_source(micros);
    }

    double useq_eval_output(const char* name, double time_seconds)
    {
        if (!useq_instance)
        {
            return std::numeric_limits<double>::quiet_NaN();
        }

        bool ok      = false;
        double value = useq_instance->eval_output_at_time(name, time_seconds, &ok);

        if (!ok && !std::isfinite(value))
        {
            return std::numeric_limits<double>::quiet_NaN();
        }

        return value;
    }
}
