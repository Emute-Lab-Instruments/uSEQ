/**
 * @file wasm_wrapper_sig.cpp
 * @brief WASM bindings for the signal engine (sig:: namespace)
 *
 * Drop-in replacement for wasm_wrapper.cpp using the new signal engine
 * instead of the ModuLisp bytecode VM interpreter.
 *
 * Provides JavaScript-callable functions for:
 * - Initializing the signal engine (cell store, node pool, source arena)
 * - Evaluating LISP expressions via sig::eval_cold()
 * - Updating transport time
 * - Evaluating individual outputs at specific times
 * - Batch evaluating multiple outputs across time windows
 */

#include "../uSEQ/src/signal_engine/signal_engine.h"
#include "../uSEQ/src/utils/json_builder.h"
#include "../uSEQ/src/modulisp/lisp/symbol_intern.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <limits>

// ── Static state ───────────────────────────────────────────────────────────

static sig::SignalEngine* g_engine = nullptr;

static double  g_hw_inputs[32]   = {};
static double  g_current_time    = 0.0;
static double  g_prev_tick_time  = 0.0;

static sig::Diagnostic g_last_diagnostics[16] = {};
static uint8_t         g_last_diagnostic_count = 0;

static String s_last_error;
static bool   g_init_called = false;

// ── Helpers ────────────────────────────────────────────────────────────────

static char* alloc_cstr(const char* s) {
    size_t len = strlen(s);
    char* buf = (char*)malloc(len + 1);
    memcpy(buf, s, len + 1);
    return buf;
}

static char* alloc_string(const String& s) {
    char* buf = (char*)malloc(s.length() + 1);
    memcpy(buf, s.c_str(), s.length() + 1);
    return buf;
}

// Parse a JSON array of output names (e.g. ["a1","a2","d1"])
static bool parse_output_names(const char* json, std::vector<String>& out) {
    String json_str(json);
    int start_pos = json_str.indexOf('[');
    int end_pos   = json_str.indexOf(']');
    if (start_pos < 0 || end_pos < 0 || end_pos <= start_pos)
        return false;

    String contents = json_str.substring(start_pos + 1, end_pos);
    int pos = 0;
    while (pos < (int)contents.length()) {
        int quote1 = contents.indexOf('"', pos);
        if (quote1 < 0) break;
        int quote2 = contents.indexOf('"', quote1 + 1);
        if (quote2 < 0) break;
        out.push_back(contents.substring(quote1 + 1, quote2));
        pos = quote2 + 1;
    }
    return true;
}

// Resolve an output name (e.g. "a1") to its index in the pool.
static uint16_t resolve_output_name(const char* name) {
    if (!name || strlen(name) != 2) return sig::NODE_NONE;
    char prefix = name[0];
    char digit  = name[1];
    if (digit < '1' || digit > '8') return sig::NODE_NONE;
    uint16_t num = (uint16_t)(digit - '1');
    switch (prefix) {
        case 'a': return num;
        case 'd': return 8 + num;
        case 's': return 16 + num;
        default:  return sig::NODE_NONE;
    }
}

static bool has_active_state() {
    return g_engine && g_engine->pool.state_slot_count > 0;
}

// Sequential batch for stateful signals: ticks forward sample-by-sample,
// advancing state between each. Saves/restores real state so visualization
// doesn't corrupt the live signal.
static void execute_batch_sequential(
    const double* t_array, int num_samples,
    const double* cell_values,
    uint16_t num_active, double* batch_buf)
{
    // Save state so visualization doesn't corrupt the live signal
    double saved_state[sig::MAX_STATE_SLOTS];
    double saved_prev_t = g_prev_tick_time;
    memcpy(saved_state, g_engine->pool.state_values, sizeof(saved_state));

    double output_values[sig::MAX_OUTPUTS] = {};
    double node_values[sig::MAX_TOTAL_NODES];

    for (int s = 0; s < num_samples; s++) {
        double t = t_array[s];
        memset(output_values, 0, sizeof(output_values));

        sig::ExecutionContext ctx;
        ctx.t             = t;
        ctx.dt            = t - g_prev_tick_time;
        ctx.cell_values   = cell_values;
        ctx.hw_inputs     = g_hw_inputs;
        ctx.data_pool     = g_engine->cells.data_pool;
        ctx.data_offsets  = g_engine->cells.data_offsets;
        ctx.data_lengths  = g_engine->cells.data_lengths;
        ctx.prev_outputs  = g_engine->pool.prev_output_values;
        ctx.output_values = output_values;
        ctx.workspace     = node_values;
        sig::execute_all_outputs(g_engine->pool, ctx);

        sig::commit_state(g_engine->pool, node_values);
        g_prev_tick_time = t;

        uint16_t row = 0;
        for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
            if (g_engine->pool.outputs[i].valid) {
                batch_buf[row * num_samples + s] = output_values[i];
                row++;
            }
        }
    }

    // Restore state — visualization is read-only
    memcpy(g_engine->pool.state_values, saved_state, sizeof(saved_state));
    g_prev_tick_time = saved_prev_t;
}

// Execute all outputs at a given time, writing results into output_values.
static void execute_at_time(double t, double* output_values) {
    double cell_values[sig::MAX_CELLS];
    g_engine->cells.snapshot_values(cell_values, sig::MAX_CELLS);

    double node_values[sig::MAX_TOTAL_NODES];

    sig::ExecutionContext ctx;
    ctx.t             = t;
    ctx.dt            = t - g_prev_tick_time;
    ctx.cell_values   = cell_values;
    ctx.hw_inputs     = g_hw_inputs;
    ctx.data_pool     = g_engine->cells.data_pool;
    ctx.data_offsets  = g_engine->cells.data_offsets;
    ctx.data_lengths  = g_engine->cells.data_lengths;
    ctx.prev_outputs  = g_engine->pool.prev_output_values;
    ctx.output_values = output_values;
    ctx.workspace     = node_values;
    sig::execute_all_outputs(g_engine->pool, ctx);

    sig::commit_state(g_engine->pool, node_values);
    g_prev_tick_time = t;

    // Update previous output values for next tick
    for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
        if (g_engine->pool.outputs[i].valid) {
            g_engine->pool.prev_output_values[i] = output_values[i];
            g_engine->pool.outputs[i].lkg_value = output_values[i];
        }
    }
}

// ── Extern "C" ABI ────────────────────────────────────────────────────────

extern "C"
{
    void useq_init()
    {
        if (g_init_called) return;

        g_engine = new sig::SignalEngine();
        g_engine->init_defaults();

        // Allocate batch workspace for WASM visualization
        g_engine->pool.allocate_batch_workspace();

        g_init_called = true;
    }

    char* useq_eval(const char* input)
    {
        if (!g_engine) {
            return alloc_cstr("Error: uSEQ not initialized. Call useq_init() first.");
        }

        try {
            // Clear diagnostics from previous eval
            g_last_diagnostic_count = 0;

            uint32_t length = (uint32_t)strlen(input);
            sig::EvalResult result = sig::eval_cold(input, length, *g_engine);

            // Copy diagnostics
            g_last_diagnostic_count = result.diagnostic_count;
            if (result.diagnostic_count > 0) {
                memcpy(g_last_diagnostics, result.diagnostics,
                       result.diagnostic_count * sizeof(sig::Diagnostic));
            }

            // Convert result to string
            switch (result.kind) {
                case sig::EvalResult::Number: {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.15g", result.number);
                    return alloc_cstr(buf);
                }
                case sig::EvalResult::Text:
                    if (result.text && result.text_length > 0) {
                        char* buf = (char*)malloc(result.text_length + 1);
                        memcpy(buf, result.text, result.text_length);
                        buf[result.text_length] = '\0';
                        return buf;
                    }
                    return alloc_cstr("");
                case sig::EvalResult::Ok:
                    return alloc_cstr("ok");
                case sig::EvalResult::Error: {
                    // Return first diagnostic message as error string
                    if (result.diagnostic_count > 0 && result.diagnostics[0].message) {
                        String msg = "Error: ";
                        msg += result.diagnostics[0].message;
                        return alloc_string(msg);
                    }
                    return alloc_cstr("Error: evaluation failed");
                }
                case sig::EvalResult::DataRef:
                    return alloc_cstr("[data]");
            }
            return alloc_cstr("");
        }
        catch (const std::exception& e) {
            String msg = "Error: ";
            msg += e.what();
            return alloc_string(msg);
        }
        catch (...) {
            return alloc_cstr("Error: Failed to evaluate expression");
        }
    }

    void useq_update_time(double time_seconds)
    {
        g_current_time = time_seconds;
    }

    void useq_set_input_value(int channel, double value)
    {
        if (channel >= 0 && channel < 32) {
            g_hw_inputs[channel] = value;
        }
    }

    double useq_eval_output(const char* name, double time_seconds)
    {
        if (!g_engine) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        uint16_t output_index = resolve_output_name(name);
        if (output_index == sig::NODE_NONE ||
            !g_engine->pool.outputs[output_index].valid) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        double output_values[sig::MAX_OUTPUTS] = {};
        execute_at_time(time_seconds, output_values);

        return output_values[output_index];
    }

    char* useq_last_error()
    {
        return alloc_string(s_last_error);
    }

    const char* useq_last_diagnostics()
    {
        if (g_last_diagnostic_count == 0) {
            return alloc_cstr("[]");
        }

        JsonBuilder json;
        json.array_begin_unkeyed();

        for (uint8_t i = 0; i < g_last_diagnostic_count; i++) {
            const auto& d = g_last_diagnostics[i];
            json.object_begin();
            json.field("severity", sig::severity_to_cstr(d.severity));
            json.field("category", sig::category_to_cstr(d.category));
            json.field("start", static_cast<int>(d.span_start));
            json.field("end", static_cast<int>(d.span_start + d.span_len));
            json.field("message", d.message ? d.message : "");
            if (d.suggestion)
                json.field("suggestion", d.suggestion);
            json.object_end();
        }

        json.array_end();
        const String result = json.build();
        return alloc_string(result);
    }

    const char* useq_active_diagnostics()
    {
        // TODO: per-output active diagnostics
        return alloc_cstr("{}");
    }

    // ---------------------------------------------------------------
    // Batch evaluation helpers
    // ---------------------------------------------------------------

    char* useq_eval_outputs_time_window(
        const char* outputs_json,
        double start_time,
        double end_time,
        int num_samples)
    {
        if (!g_engine) {
            return alloc_cstr("{\"error\": \"uSEQ not initialized\"}");
        }
        if (num_samples < 1) {
            return alloc_cstr("{\"error\": \"num_samples must be >= 1\"}");
        }

        try {
            std::vector<String> outputs;
            if (!parse_output_names(outputs_json, outputs)) {
                s_last_error = "Failed to parse outputs JSON array";
                return alloc_cstr("{\"error\": \"Failed to parse outputs JSON\"}");
            }

            // Resolve output indices
            std::vector<uint16_t> output_indices;
            for (const auto& name : outputs) {
                output_indices.push_back(resolve_output_name(name.c_str()));
            }

            // Build time array
            double dt = (num_samples > 1)
                ? (end_time - start_time) / (double)(num_samples - 1)
                : 0.0;

            // Snapshot cell values once
            double cell_values[sig::MAX_CELLS];
            g_engine->cells.snapshot_values(cell_values, sig::MAX_CELLS);

            // Use batch execution if workspace is available
            uint16_t num_active_outputs = 0;
            for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
                if (g_engine->pool.outputs[i].valid) num_active_outputs++;
            }

            // Allocate output buffer: all outputs x num_samples
            std::vector<double> batch_buf(num_active_outputs * num_samples, 0.0);

            // Build time array
            std::vector<double> t_array(num_samples);
            for (int i = 0; i < num_samples; i++) {
                t_array[i] = start_time + dt * i;
            }

            if (has_active_state()) {
                execute_batch_sequential(t_array.data(), num_samples,
                                          cell_values, num_active_outputs, batch_buf.data());
            } else if (g_engine->pool.batch_workspace) {
                sig::execute_batch(
                    g_engine->pool, t_array.data(), (size_t)num_samples,
                    cell_values, g_hw_inputs,
                    g_engine->cells.data_pool, g_engine->cells.data_offsets, g_engine->cells.data_lengths,
                    batch_buf.data(), num_active_outputs
                );
            } else {
                execute_batch_sequential(t_array.data(), num_samples,
                                          cell_values, num_active_outputs, batch_buf.data());
            }

            // Build JSON: map from name to sample array
            // batch_buf is indexed by active output row, but we need to map
            // from requested output names to their row indices.
            // Build a mapping from output_index -> active row.
            uint16_t index_to_row[sig::MAX_OUTPUTS];
            {
                uint16_t row = 0;
                for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
                    if (g_engine->pool.outputs[i].valid) {
                        index_to_row[i] = row++;
                    } else {
                        index_to_row[i] = sig::NODE_NONE;
                    }
                }
            }

            String json_result = "{";
            bool first_channel = true;

            for (size_t c = 0; c < outputs.size(); c++) {
                if (!first_channel) json_result += ",";
                first_channel = false;

                json_result += "\"";
                json_result += outputs[c];
                json_result += "\":[";

                uint16_t idx = output_indices[c];
                if (idx != sig::NODE_NONE && g_engine->pool.outputs[idx].valid) {
                    uint16_t row = index_to_row[idx];
                    for (int s = 0; s < num_samples; s++) {
                        if (s > 0) json_result += ",";
                        char val_buf[32];
                        snprintf(val_buf, sizeof(val_buf), "%.15g",
                                 batch_buf[row * num_samples + s]);
                        json_result += val_buf;
                    }
                } else {
                    // Output not active — fill with NaN
                    for (int s = 0; s < num_samples; s++) {
                        if (s > 0) json_result += ",";
                        json_result += "null";
                    }
                }

                json_result += "]";
            }
            json_result += "}";

            return alloc_string(json_result);
        }
        catch (const std::exception& e) {
            s_last_error = e.what();
            String msg = "{\"error\": \"";
            msg += e.what();
            msg += "\"}";
            return alloc_string(msg);
        }
        catch (...) {
            s_last_error = "Unknown error during batch evaluation";
            return alloc_cstr("{\"error\": \"Unknown error during batch evaluation\"}");
        }
    }

    int useq_eval_outputs_time_window_into(
        const char* outputs_json,
        double start_time,
        double end_time,
        int num_samples,
        int buffer_ptr,
        int buffer_length)
    {
        if (!g_engine) {
            s_last_error = "uSEQ not initialized";
            return -1;
        }
        if (num_samples < 1) {
            s_last_error = "num_samples must be >= 1";
            return -1;
        }

        try {
            std::vector<String> outputs;
            if (!parse_output_names(outputs_json, outputs)) {
                s_last_error = "Failed to parse outputs JSON array";
                return -1;
            }

            int num_channels = (int)outputs.size();
            int required_slots = num_channels * num_samples;
            if (required_slots > buffer_length) {
                s_last_error = "Buffer too small: need " +
                    String(std::to_string(required_slots).c_str()) +
                    " slots, got " +
                    String(std::to_string(buffer_length).c_str());
                return -1;
            }

            // Resolve output indices
            std::vector<uint16_t> output_indices;
            for (const auto& name : outputs) {
                output_indices.push_back(resolve_output_name(name.c_str()));
            }

            double* buf = reinterpret_cast<double*>(buffer_ptr);

            // Build time array
            double dt = (num_samples > 1)
                ? (end_time - start_time) / (double)(num_samples - 1)
                : 0.0;

            // Snapshot cell values
            double cell_values[sig::MAX_CELLS];
            g_engine->cells.snapshot_values(cell_values, sig::MAX_CELLS);

            // Build active-output row mapping
            uint16_t index_to_row[sig::MAX_OUTPUTS];
            uint16_t num_active = 0;
            for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
                if (g_engine->pool.outputs[i].valid) {
                    index_to_row[i] = num_active++;
                } else {
                    index_to_row[i] = sig::NODE_NONE;
                }
            }

            // Build time array
            std::vector<double> t_array(num_samples);
            for (int i = 0; i < num_samples; i++) {
                t_array[i] = start_time + dt * i;
            }

            // Batch buffer: active_outputs x num_samples
            std::vector<double> batch_buf(num_active * num_samples, 0.0);

            if (has_active_state()) {
                execute_batch_sequential(t_array.data(), num_samples,
                                          cell_values, num_active, batch_buf.data());
            } else if (g_engine->pool.batch_workspace && num_active > 0) {
                sig::execute_batch(
                    g_engine->pool, t_array.data(), (size_t)num_samples,
                    cell_values, g_hw_inputs,
                    g_engine->cells.data_pool, g_engine->cells.data_offsets, g_engine->cells.data_lengths,
                    batch_buf.data(), num_active
                );
            } else {
                execute_batch_sequential(t_array.data(), num_samples,
                                          cell_values, num_active, batch_buf.data());
            }

            // Copy requested channels into caller's buffer
            for (int c = 0; c < num_channels; c++) {
                double* row = buf + (c * num_samples);
                uint16_t idx = output_indices[c];

                if (idx != sig::NODE_NONE && g_engine->pool.outputs[idx].valid) {
                    uint16_t active_row = index_to_row[idx];
                    memcpy(row, &batch_buf[active_row * num_samples],
                           num_samples * sizeof(double));
                } else {
                    for (int s = 0; s < num_samples; s++) {
                        row[s] = std::numeric_limits<double>::quiet_NaN();
                    }
                }
            }

            s_last_error = "";
            return num_channels;
        }
        catch (const std::exception& e) {
            s_last_error = e.what();
            return -1;
        }
        catch (...) {
            s_last_error = "Unknown error during batch evaluation";
            return -1;
        }
    }
}
