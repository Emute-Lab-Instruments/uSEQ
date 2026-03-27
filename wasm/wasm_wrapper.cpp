/**
 * @file wasm_wrapper.cpp
 * @brief WASM bindings for the ModuLisp interpreter
 *
 * Provides JavaScript-callable functions for:
 * - Initializing the interpreter
 * - Evaluating LISP expressions
 * - Updating transport time
 * - Evaluating individual outputs at specific times
 * - Batch evaluating multiple outputs across time windows (NEW)
 */

#include "../uSEQ/src/modulisp/modulisp_interpreter.h"
#include "../uSEQ/src/utils/json_builder.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <limits>

// Static instance of ModuLisp interpreter (simpler than full uSEQ)
static ModuLispInterpreter* useq_instance = nullptr;
static bool init_called = false;

extern "C"
{
    // Initialize the ModuLisp interpreter
    void useq_init()
    {
        if (init_called)
        {
            return;
        }

        if (!useq_instance)
        {
            useq_instance = new ModuLispInterpreter();
            useq_instance->init();
        }

        init_called = true;
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

            // Clear diagnostics from previous eval
            useq_instance->clear_diagnostics();

            // Evaluate the expression
            String result_str = useq_instance->eval(code);

            // Convert result back to C string
            const char* result_cstr = result_str.c_str();
            char* result            = (char*)malloc(strlen(result_cstr) + 1);
            strcpy(result, result_cstr);

            return result;
        }
        catch (const std::exception& e)
        {
            String error_msg = "Error: ";
            error_msg += e.what();
            char* result = (char*)malloc(error_msg.length() + 1);
            strcpy(result, error_msg.c_str());
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

    void useq_set_input_value(int channel, double value)
    {
        if (!useq_instance || channel < 0)
        {
            return;
        }

        useq_instance->set_input_value(static_cast<size_t>(channel), value);
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

    // ---------------------------------------------------------------
    // Last-error reporting
    // ---------------------------------------------------------------
    static String s_last_error;

    // Return the last error string (empty if no error)
    char* useq_last_error()
    {
        char* result = (char*)malloc(s_last_error.length() + 1);
        strcpy(result, s_last_error.c_str());
        return result;
    }

    // ---------------------------------------------------------------
    // Diagnostics from the most recent useq_eval() call
    // ---------------------------------------------------------------

    const char* useq_last_diagnostics()
    {
        if (!useq_instance)
        {
            char* buf = (char*)malloc(3);
            strcpy(buf, "[]");
            return buf;
        }

        const auto& diagnostics = useq_instance->get_diagnostics();

        JsonBuilder json;
        json.array_begin_unkeyed();

        for (const auto& d : diagnostics)
        {
            json.object_begin();
            json.field("severity", severity_to_cstr(d.severity));
            json.field("category", category_to_cstr(d.category));
            json.field("start", static_cast<int>(d.span.start));
            json.field("end", static_cast<int>(d.span.end));
            json.field("message", d.message);
            if (d.suggestion.length() > 0)
                json.field("suggestion", d.suggestion);
            if (d.example.length() > 0)
                json.field("example", d.example);
            if (d.triggered_by.length() > 0)
                json.field("triggered_by", d.triggered_by);
            json.object_end();
        }

        json.array_end();
        const String result = json.build();
        char* buf = (char*)malloc(result.length() + 1);
        strcpy(buf, result.c_str());
        return buf;
    }

    // ---------------------------------------------------------------
    // Active diagnostics across all output slots
    // ---------------------------------------------------------------

    // Return a JSON object keyed by output name, containing diagnostic
    // arrays for each output that has an active issue.
    // Returns "{}" when no outputs have diagnostics.
    // Caller must free() the returned pointer.
    const char* useq_active_diagnostics()
    {
        if (!useq_instance)
        {
            char* buf = (char*)malloc(3);
            strcpy(buf, "{}");
            return buf;
        }

        std::vector<std::pair<String, std::vector<Diagnostic>>> active;
        useq_instance->collect_active_diagnostics(active);

        if (active.empty())
        {
            char* buf = (char*)malloc(3);
            strcpy(buf, "{}");
            return buf;
        }

        JsonBuilder json;
        json.object_begin();

        for (const auto& entry : active)
        {
            const String& output_name = entry.first;
            const auto& diagnostics   = entry.second;

            json.array_begin(output_name);

            for (const auto& d : diagnostics)
            {
                json.object_begin();

                json.field("severity", severity_to_cstr(d.severity));
                json.field("category", category_to_cstr(d.category));
                json.field("start", static_cast<int>(d.span.start));
                json.field("end", static_cast<int>(d.span.end));
                json.field("message", d.message);

                if (d.suggestion.length() > 0)
                    json.field("suggestion", d.suggestion);
                if (d.example.length() > 0)
                    json.field("example", d.example);
                if (d.triggered_by.length() > 0)
                    json.field("triggered_by", d.triggered_by);

                json.object_end();
            }

            json.array_end();
        }

        json.object_end();

        const String& result_str = json.build();
        char* buf = (char*)malloc(result_str.length() + 1);
        strcpy(buf, result_str.c_str());
        return buf;
    }

    // ---------------------------------------------------------------
    // Batch evaluation helpers
    // ---------------------------------------------------------------

    // Parse a JSON array of output names (e.g. ["a1","a2","d1"])
    // Returns the parsed names in `out`. Returns false on parse error.
    static bool parse_output_names(const char* json, std::vector<String>& out)
    {
        String json_str(json);
        int start_pos = json_str.indexOf('[');
        int end_pos   = json_str.indexOf(']');
        if (start_pos < 0 || end_pos < 0 || end_pos <= start_pos)
            return false;

        String contents = json_str.substring(start_pos + 1, end_pos);
        int pos = 0;
        while (pos < (int)contents.length())
        {
            int quote1 = contents.indexOf('"', pos);
            if (quote1 < 0) break;
            int quote2 = contents.indexOf('"', quote1 + 1);
            if (quote2 < 0) break;
            out.push_back(contents.substring(quote1 + 1, quote2));
            pos = quote2 + 1;
        }
        return true;
    }

    // Evaluate multiple outputs across a time window
    // Returns a JSON string containing channel-indexed samples
    // Format: {"a1": [0.5, 0.6, ...], "a2": [0.3, 0.4, ...]}
    char* useq_eval_outputs_time_window(const char* outputs_json, double start_time, double end_time, int num_samples)
    {
        if (!useq_instance)
        {
            const char* error_msg = "{\"error\": \"uSEQ not initialized\"}";
            char* result = (char*)malloc(strlen(error_msg) + 1);
            strcpy(result, error_msg);
            return result;
        }

        if (num_samples < 1)
        {
            const char* error_msg = "{\"error\": \"num_samples must be >= 1\"}";
            char* result = (char*)malloc(strlen(error_msg) + 1);
            strcpy(result, error_msg);
            return result;
        }

        try
        {
            // Parse outputs array from JSON string (simple format: ["a1", "a2", "d1"])
            std::vector<String> outputs;
            if (!parse_output_names(outputs_json, outputs))
            {
                s_last_error = "Failed to parse outputs JSON array";
                const char* error_msg = "{\"error\": \"Failed to parse outputs JSON\"}";
                char* result = (char*)malloc(strlen(error_msg) + 1);
                strcpy(result, error_msg);
                return result;
            }

            // Call the batch evaluation API - now returns map<String, vector<double>>
            auto results = useq_instance->eval_outputs(start_time, end_time, num_samples, outputs);

            // Build JSON response - object with arrays
            // Format: {"a1": [0.5, 0.6], "a2": [0.3, 0.4]}
            String json_result = "{";
            bool first_channel = true;

            for (const auto& channel_pair : results)
            {
                if (!first_channel) json_result += ",";
                first_channel = false;

                // Output name as key
                json_result += "\"";
                json_result += channel_pair.first;
                json_result += "\":[";

                // Array of time samples for this channel
                const auto& samples = channel_pair.second;
                for (size_t i = 0; i < samples.size(); ++i)
                {
                    if (i > 0) json_result += ",";

                    // Convert double to string
                    char val_buf[32];
                    snprintf(val_buf, sizeof(val_buf), "%.15g", samples[i]);
                    json_result += val_buf;
                }

                json_result += "]";
            }
            json_result += "}";

            // Allocate and return result
            char* result = (char*)malloc(json_result.length() + 1);
            strcpy(result, json_result.c_str());
            return result;
        }
        catch (const std::exception& e)
        {
            s_last_error = e.what();
            String error_msg = "{\"error\": \"";
            error_msg += e.what();
            error_msg += "\"}";
            char* result = (char*)malloc(error_msg.length() + 1);
            strcpy(result, error_msg.c_str());
            return result;
        }
        catch (...)
        {
            s_last_error = "Unknown error during batch evaluation";
            const char* error_msg = "{\"error\": \"Unknown error during batch evaluation\"}";
            char* result = (char*)malloc(strlen(error_msg) + 1);
            strcpy(result, error_msg);
            return result;
        }
    }

    // ---------------------------------------------------------------
    // Batch evaluation into a caller-provided Float64 buffer
    // ---------------------------------------------------------------
    // Writes results into `buffer` in row-major order: channels × samples.
    // Returns the number of channels written, or -1 on error.
    //
    // ABI (from wasmAbi.ts):
    //   symbol: "useq_eval_outputs_time_window_into"
    //   returnType: "number"
    //   argTypes: ["string", "number", "number", "number", "number", "number"]
    //
    // Parameters:
    //   outputs_json  - JSON array of output names, e.g. '["a1","a2"]'
    //   start_time    - window start (seconds)
    //   end_time      - window end (seconds)
    //   num_samples   - number of time samples per channel
    //   buffer_ptr    - byte offset into the Emscripten HEAPF64 (from _malloc)
    //   buffer_length - total number of Float64 slots available in buffer
    int useq_eval_outputs_time_window_into(
        const char* outputs_json,
        double start_time,
        double end_time,
        int num_samples,
        int buffer_ptr,
        int buffer_length)
    {
        if (!useq_instance)
        {
            s_last_error = "uSEQ not initialized";
            return -1;
        }
        if (num_samples < 1)
        {
            s_last_error = "num_samples must be >= 1";
            return -1;
        }

        try
        {
            std::vector<String> outputs;
            if (!parse_output_names(outputs_json, outputs))
            {
                s_last_error = "Failed to parse outputs JSON array";
                return -1;
            }

            int num_channels = (int)outputs.size();
            int required_slots = num_channels * num_samples;
            if (required_slots > buffer_length)
            {
                s_last_error = "Buffer too small: need " +
                    String(std::to_string(required_slots).c_str()) +
                    " slots, got " +
                    String(std::to_string(buffer_length).c_str());
                return -1;
            }

            // Interpret buffer_ptr as a byte offset into the Emscripten heap.
            // Emscripten HEAPF64 views the same memory; buffer_ptr is the
            // byte address returned by _malloc. Convert to double* pointer.
            double* buf = reinterpret_cast<double*>(buffer_ptr);

            // Use the interpreter's batch API
            auto results = useq_instance->eval_outputs(start_time, end_time, num_samples, outputs);

            // Write into the buffer in row-major order: channel 0 samples, channel 1 samples, ...
            int channel_idx = 0;
            for (const auto& name : outputs)
            {
                auto it = results.find(name);
                double* row = buf + (channel_idx * num_samples);

                if (it != results.end())
                {
                    const auto& samples = it->second;
                    int count = std::min((int)samples.size(), num_samples);
                    for (int i = 0; i < count; ++i)
                        row[i] = samples[i];
                    // Zero-fill if fewer samples than requested
                    for (int i = count; i < num_samples; ++i)
                        row[i] = 0.0;
                }
                else
                {
                    // Channel not found — fill with NaN
                    for (int i = 0; i < num_samples; ++i)
                        row[i] = std::numeric_limits<double>::quiet_NaN();
                }
                ++channel_idx;
            }

            s_last_error = "";
            return num_channels;
        }
        catch (const std::exception& e)
        {
            s_last_error = e.what();
            return -1;
        }
        catch (...)
        {
            s_last_error = "Unknown error during batch evaluation";
            return -1;
        }
    }
}
