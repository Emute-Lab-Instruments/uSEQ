#include "../uSEQ/src/modulisp/modulisp_interpreter.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <limits>

// Static instance of ModuLisp interpreter (simpler than full uSEQ)
static ModuLispInterpreter* useq_instance = nullptr;

extern "C"
{
    // Initialize the ModuLisp interpreter
    void useq_init()
    {
        if (!useq_instance)
        {
            // Create with nullptr for ErrorManager - let the constructor handle defaults
            useq_instance = new ModuLispInterpreter(nullptr);
            useq_instance->init();
        }
    }

    // Evaluate a LISP expression and return the result
    // Input: C string from JavaScript
    // Output: Dynamically allocated C string (Emscripten will handle cleanup)
    char* useq_eval(const char* input)
    {
        if (!useq_instance)
        {
            const char* error_msg =
                "Error: uSEQ not initialized. Call useq_init() first.";
            char* result = (char*)malloc(strlen(error_msg) + 1);
            strcpy(result, error_msg);
            return result;
        }

        try
        {
            // Convert C string to String type
            String code(input);

            // Evaluate the expression
            String result_str = useq_instance->eval(code);

            // Convert result back to C string
            const char* result_cstr = result_str.c_str();
            char* result            = (char*)malloc(strlen(result_cstr) + 1);
            strcpy(result, result_cstr);

            return result;
        }
        catch (...)
        {
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
