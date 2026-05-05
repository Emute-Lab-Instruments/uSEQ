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

// ── Persistent projection fork (visualisation-projection.md §2) ──────────
//
// Cloned from live state on invalidation (reset-fill), then advanced
// incrementally at the frontier (extend-frontier). Live state is never
// mutated during projection.

struct ProjectionFork {
    double state_values[sig::MAX_STATE_SLOTS];
    uint16_t state_slot_count;
    sig::StateResourceRegistry registry;
    double prev_output_values[sig::MAX_OUTPUTS];
    double lkg_values[sig::MAX_OUTPUTS];
    double prev_tick_time;
    double cell_values[sig::MAX_CELLS];
    double hw_inputs[32];
    double frontier_time;
    double start_time;
    bool   valid;
};

static ProjectionFork g_projection_fork = {};

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

// Evaluate any signal expression at a given time via the cold eval scratch
// pool. Names that aren't output sinks (e.g. "bar", "beat", "(+ bar 0.5)")
// are compiled, executed once, and the numeric result is returned.
static double eval_expression_at_time(const char* expr, double t) {
    if (!g_engine) return std::numeric_limits<double>::quiet_NaN();
    double saved_time = g_engine->state.current_time;
    double saved_dt = g_engine->state.current_dt;
    g_engine->state.current_time = t;
    g_engine->state.current_dt = t - g_prev_tick_time;
    uint32_t len = (uint32_t)strlen(expr);
    sig::EvalResult result = sig::eval_expression(expr, len, *g_engine);
    g_engine->state.current_time = saved_time;
    g_engine->state.current_dt = saved_dt;
    if (result.kind == sig::EvalResult::Number) return result.number;
    return std::numeric_limits<double>::quiet_NaN();
}

static bool has_active_state() {
    return g_engine && g_engine->pool.state_slot_count > 0;
}

// Clone post-tick live state into the projection fork.
static void reset_projection_fork(double tick_time) {
    memcpy(g_projection_fork.state_values,
           g_engine->pool.state_values, sizeof(g_projection_fork.state_values));
    g_projection_fork.state_slot_count = g_engine->pool.state_slot_count;
    g_projection_fork.registry = g_engine->registry;
    memcpy(g_projection_fork.prev_output_values,
           g_engine->pool.prev_output_values, sizeof(g_projection_fork.prev_output_values));
    for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++)
        g_projection_fork.lkg_values[i] = g_engine->pool.outputs[i].lkg_value;
    g_projection_fork.prev_tick_time = tick_time;
    g_engine->cells.snapshot_values(g_projection_fork.cell_values, sig::MAX_CELLS);
    memcpy(g_projection_fork.hw_inputs, g_hw_inputs, sizeof(g_projection_fork.hw_inputs));
    g_projection_fork.frontier_time = tick_time;
    g_projection_fork.start_time    = tick_time;
    g_projection_fork.valid         = true;
}

// Project from fork state. Installs fork state into engine, runs the
// sample loop with proper commit_state + commit_outputs between steps,
// saves updated fork state, and restores live state before returning.
// batch_buf is [num_active × num_samples] row-major (active-output rows).
static void project_from_fork(
    const double* t_array, int num_samples,
    uint16_t num_active, double* batch_buf)
{
    // Save live state
    double saved_state[sig::MAX_STATE_SLOTS];
    uint16_t saved_slot_count = g_engine->pool.state_slot_count;
    sig::StateResourceRegistry saved_registry = g_engine->registry;
    double saved_prev_outputs[sig::MAX_OUTPUTS];
    double saved_lkg[sig::MAX_OUTPUTS];
    double saved_prev_t = g_prev_tick_time;
    memcpy(saved_state, g_engine->pool.state_values, sizeof(saved_state));
    memcpy(saved_prev_outputs, g_engine->pool.prev_output_values, sizeof(saved_prev_outputs));
    for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++)
        saved_lkg[i] = g_engine->pool.outputs[i].lkg_value;

    // Install fork state
    memcpy(g_engine->pool.state_values,
           g_projection_fork.state_values, sizeof(saved_state));
    g_engine->pool.state_slot_count = g_projection_fork.state_slot_count;
    g_engine->registry = g_projection_fork.registry;
    memcpy(g_engine->pool.prev_output_values,
           g_projection_fork.prev_output_values, sizeof(saved_prev_outputs));
    for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++)
        g_engine->pool.outputs[i].lkg_value = g_projection_fork.lkg_values[i];
    g_prev_tick_time = g_projection_fork.prev_tick_time;

    double output_values[sig::MAX_OUTPUTS] = {};
    double node_values[sig::MAX_TOTAL_NODES];

    for (int s = 0; s < num_samples; s++) {
        double t = t_array[s];
        memset(output_values, 0, sizeof(output_values));

        sig::ExecutionContext ctx;
        ctx.t             = t;
        ctx.dt            = t - g_prev_tick_time;
        ctx.cell_values   = g_projection_fork.cell_values;
        ctx.hw_inputs     = g_projection_fork.hw_inputs;
        ctx.data_pool     = g_engine->cells.data_pool;
        ctx.data_offsets  = g_engine->cells.data_offsets;
        ctx.data_lengths  = g_engine->cells.data_lengths;
        ctx.prev_outputs  = g_engine->pool.prev_output_values;
        ctx.output_values = output_values;
        ctx.workspace     = node_values;
        sig::execute_all_outputs(g_engine->pool, ctx);

        sig::commit_state(g_engine->pool, node_values);
        sig::commit_outputs(g_engine->pool, output_values);
        g_prev_tick_time = t;

        uint16_t row = 0;
        for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
            if (g_engine->pool.outputs[i].valid) {
                batch_buf[row * num_samples + s] = output_values[i];
                row++;
            }
        }
    }

    // Save fork state (fork advances)
    memcpy(g_projection_fork.state_values,
           g_engine->pool.state_values, sizeof(saved_state));
    g_projection_fork.state_slot_count = g_engine->pool.state_slot_count;
    g_projection_fork.registry = g_engine->registry;
    memcpy(g_projection_fork.prev_output_values,
           g_engine->pool.prev_output_values, sizeof(saved_prev_outputs));
    for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++)
        g_projection_fork.lkg_values[i] = g_engine->pool.outputs[i].lkg_value;
    g_projection_fork.prev_tick_time = g_prev_tick_time;
    if (num_samples > 0)
        g_projection_fork.frontier_time = t_array[num_samples - 1];

    // Restore live state
    memcpy(g_engine->pool.state_values, saved_state, sizeof(saved_state));
    g_engine->pool.state_slot_count = saved_slot_count;
    g_engine->registry = saved_registry;
    memcpy(g_engine->pool.prev_output_values, saved_prev_outputs, sizeof(saved_prev_outputs));
    for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++)
        g_engine->pool.outputs[i].lkg_value = saved_lkg[i];
    g_prev_tick_time = saved_prev_t;
}

// Sequential batch for stateful signals: ticks forward sample-by-sample,
// advancing state between each. Saves/restores real state so visualization
// doesn't corrupt the live signal.
static void execute_batch_sequential(
    const double* t_array, int num_samples,
    const double* cell_values,
    uint16_t num_active, double* batch_buf)
{
    // Save all mutable state so visualization doesn't corrupt the live signal
    double saved_state[sig::MAX_STATE_SLOTS];
    double saved_prev_outputs[sig::MAX_OUTPUTS];
    double saved_lkg[sig::MAX_OUTPUTS];
    uint16_t saved_slot_count = g_engine->pool.state_slot_count;
    double saved_prev_t = g_prev_tick_time;
    memcpy(saved_state, g_engine->pool.state_values, sizeof(saved_state));
    memcpy(saved_prev_outputs, g_engine->pool.prev_output_values, sizeof(saved_prev_outputs));
    for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++)
        saved_lkg[i] = g_engine->pool.outputs[i].lkg_value;

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
        sig::commit_outputs(g_engine->pool, output_values);
        g_prev_tick_time = t;

        uint16_t row = 0;
        for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
            if (g_engine->pool.outputs[i].valid) {
                batch_buf[row * num_samples + s] = output_values[i];
                row++;
            }
        }
    }

    // Restore all state — visualization is read-only
    memcpy(g_engine->pool.state_values, saved_state, sizeof(saved_state));
    memcpy(g_engine->pool.prev_output_values, saved_prev_outputs, sizeof(saved_prev_outputs));
    for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++)
        g_engine->pool.outputs[i].lkg_value = saved_lkg[i];
    g_engine->pool.state_slot_count = saved_slot_count;
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
            g_projection_fork.valid = false;

            g_engine->state.current_time = g_current_time;
            g_engine->state.current_dt = g_current_time - g_prev_tick_time;

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
        if (output_index == sig::NODE_NONE) {
            return eval_expression_at_time(name, time_seconds);
        }
        if (!g_engine->pool.outputs[output_index].valid) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        // Save all mutable state — useq_eval_output is read-only and must
        // not corrupt live engine state (execute_at_time advances everything).
        double saved_state[sig::MAX_STATE_SLOTS];
        double saved_prev_outputs[sig::MAX_OUTPUTS];
        double saved_lkg[sig::MAX_OUTPUTS];
        uint16_t saved_slot_count = g_engine->pool.state_slot_count;
        memcpy(saved_state, g_engine->pool.state_values, sizeof(saved_state));
        memcpy(saved_prev_outputs, g_engine->pool.prev_output_values, sizeof(saved_prev_outputs));
        for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++)
            saved_lkg[i] = g_engine->pool.outputs[i].lkg_value;
        double saved_prev_t = g_prev_tick_time;

        double output_values[sig::MAX_OUTPUTS] = {};
        execute_at_time(time_seconds, output_values);

        // Restore all state
        memcpy(g_engine->pool.state_values, saved_state, sizeof(saved_state));
        memcpy(g_engine->pool.prev_output_values, saved_prev_outputs, sizeof(saved_prev_outputs));
        for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++)
            g_engine->pool.outputs[i].lkg_value = saved_lkg[i];
        g_engine->pool.state_slot_count = saved_slot_count;
        g_prev_tick_time = saved_prev_t;

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
    // Live-edit slot API
    // ---------------------------------------------------------------

    /**
     * Set live-edit slot values from the frontend.
     *
     * JSON format: {"id1": 1.5, "id2": 0.7}
     * Keys are slot id strings, values are doubles.
     * Returns the count of successfully applied writes.
     */
    int useq_set_live_inputs(const char* json_str)
    {
        if (!g_engine || !json_str) return 0;

        int applied = 0;
        String json(json_str);
        int len = (int)json.length();

        // Walk the JSON object: find key-value pairs
        // Skip leading whitespace and opening brace
        int pos = 0;
        while (pos < len && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) pos++;
        if (pos >= len || json[pos] != '{') return 0;
        pos++; // skip '{'

        while (pos < len) {
            // Skip whitespace and commas
            while (pos < len && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == ',')) pos++;
            if (pos >= len || json[pos] == '}') break;

            // Expect a quoted key
            if (json[pos] != '"') break;
            pos++; // skip opening quote
            int key_start = pos;
            while (pos < len && json[pos] != '"') pos++;
            if (pos >= len) break;
            String key = json.substring(key_start, pos);
            pos++; // skip closing quote

            // Skip whitespace and colon
            while (pos < len && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) pos++;
            if (pos >= len || json[pos] != ':') break;
            pos++; // skip colon
            while (pos < len && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) pos++;

            // Parse number value (including negative and decimal)
            int val_start = pos;
            if (pos < len && (json[pos] == '-' || json[pos] == '+')) pos++;
            while (pos < len && ((json[pos] >= '0' && json[pos] <= '9') || json[pos] == '.' || json[pos] == 'e' || json[pos] == 'E' || json[pos] == '+' || json[pos] == '-')) {
                // Only allow +/- after e/E
                if ((json[pos] == '+' || json[pos] == '-') && pos > val_start && json[pos-1] != 'e' && json[pos-1] != 'E') break;
                pos++;
            }
            if (pos == val_start) break; // no number found

            String val_str = json.substring(val_start, pos);
            double value = atof(val_str.c_str());

            g_engine->pool.set_live_slot_value(key.c_str(), value);
            applied++;
        }

        return applied;
    }

    /**
     * Query all allocated live-edit slots and their metadata.
     *
     * Returns JSON array:
     *   [{"id":"x","value":0.5,"min":0,"max":1,"seed":0.5}, ...]
     */
    const char* useq_get_live_slots()
    {
        if (!g_engine || g_engine->pool.live_slot_count == 0) {
            return alloc_cstr("[]");
        }

        JsonBuilder json;
        json.array_begin_unkeyed();

        for (uint16_t i = 0; i < g_engine->pool.live_slot_count; i++) {
            const auto& slot = g_engine->pool.live_slots[i];
            json.object_begin();
            json.field("id", slot.id);

            // JsonBuilder lacks a double field — use field_raw with snprintf
            char numbuf[32];
            snprintf(numbuf, sizeof(numbuf), "%.15g", slot.value);
            json.field_raw("value", String(numbuf));

            snprintf(numbuf, sizeof(numbuf), "%.15g", slot.min_val);
            json.field_raw("min", String(numbuf));

            snprintf(numbuf, sizeof(numbuf), "%.15g", slot.max_val);
            json.field_raw("max", String(numbuf));

            snprintf(numbuf, sizeof(numbuf), "%.15g", slot.seed);
            json.field_raw("seed", String(numbuf));

            // Variant tag
            const char* variant_str = "numeric";
            if (slot.variant == sig::NodePool::SlotVariant::Boolean) variant_str = "boolean";
            else if (slot.variant == sig::NodePool::SlotVariant::Keyword) variant_str = "keyword";
            json.field("variant", variant_str);

            // Step
            snprintf(numbuf, sizeof(numbuf), "%.15g", slot.step);
            json.field_raw("step", String(numbuf));

            // Precision
            json.field("precision", slot.precision);

            // Options (array of keyword strings, for keyword slots)
            {
                String opts_arr = "[";
                if (slot.variant == sig::NodePool::SlotVariant::Keyword && slot.options_count > 0) {
                    for (uint8_t j = 0; j < slot.options_count; j++) {
                        if (j > 0) opts_arr += ",";
                        opts_arr += "\"";
                        opts_arr += slot.options[j];
                        opts_arr += "\"";
                    }
                }
                opts_arr += "]";
                json.field_raw("options", opts_arr);
            }

            json.object_end();
        }

        json.array_end();
        const String result = json.build();
        return alloc_string(result);
    }

    // ---------------------------------------------------------------
    // State snapshot application (state-sync.md §3)
    // ---------------------------------------------------------------

    // ── JSON helpers for state snapshot parsing ─────────────────────
    //
    // These skip strings properly (honouring \" escapes) so that braces
    // and key names inside string values don't confuse the structure walk.

    static int skip_json_string(const char* s, int len, int pos) {
        if (pos >= len || s[pos] != '"') return pos;
        pos++; // skip opening quote
        while (pos < len) {
            if (s[pos] == '\\') { pos += 2; continue; }
            if (s[pos] == '"') { pos++; return pos; }
            pos++;
        }
        return pos;
    }

    static int skip_json_value(const char* s, int len, int pos) {
        if (pos >= len) return pos;
        if (s[pos] == '"') return skip_json_string(s, len, pos);
        if (s[pos] == '{' || s[pos] == '[') {
            char open = s[pos], close = (open == '{') ? '}' : ']';
            int depth = 1;
            pos++;
            while (pos < len && depth > 0) {
                if (s[pos] == '"') { pos = skip_json_string(s, len, pos); continue; }
                if (s[pos] == open) depth++;
                else if (s[pos] == close) depth--;
                pos++;
            }
            return pos;
        }
        // number, bool, null — advance until delimiter
        while (pos < len && s[pos] != ',' && s[pos] != '}' && s[pos] != ']'
                        && s[pos] != ' ' && s[pos] != '\n' && s[pos] != '\r') pos++;
        return pos;
    }

    // Find a JSON key at the current object nesting depth (depth-1 keys
    // are skipped). Returns position of the value start, or -1.
    static int find_field_at_depth(const char* s, int len, int pos, const char* key) {
        size_t klen = strlen(key);
        // pos should be right after the opening '{' of the object
        while (pos < len) {
            // skip whitespace/commas
            while (pos < len && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n'
                              || s[pos] == '\r' || s[pos] == ',')) pos++;
            if (pos >= len || s[pos] == '}') return -1;
            if (s[pos] != '"') return -1;
            // read key
            int key_start = pos + 1;
            int key_end_pos = skip_json_string(s, len, pos);
            int key_end = key_end_pos - 1; // before closing quote
            pos = key_end_pos;
            // skip colon
            while (pos < len && (s[pos] == ' ' || s[pos] == ':')) pos++;
            // check if this key matches
            if ((key_end - key_start) == (int)klen && memcmp(s + key_start, key, klen) == 0) {
                return pos; // value starts here
            }
            // skip value
            pos = skip_json_value(s, len, pos);
        }
        return -1;
    }

    // Extract a JSON string value, unescaping \" and \\ sequences.
    // pos must point at the opening '"'. Returns the unescaped content.
    static String extract_json_string(const char* s, int len, int pos) {
        if (pos >= len || s[pos] != '"') return String();
        pos++; // skip opening quote
        String result;
        while (pos < len && s[pos] != '"') {
            if (s[pos] == '\\' && pos + 1 < len) {
                char next = s[pos + 1];
                switch (next) {
                    case '"':  result += '"';  break;
                    case '\\': result += '\\'; break;
                    case 'n':  result += '\n'; break;
                    case 'r':  result += '\r'; break;
                    case 't':  result += '\t'; break;
                    case '/':  result += '/';  break;
                    default:   result += next; break;
                }
                pos += 2;
            } else {
                result += s[pos];
                pos++;
            }
        }
        return result;
    }

    // Extract a JSON number value as double. pos must point at first digit/sign.
    static double extract_json_number(const char* s, int len, int pos) {
        int start = pos;
        if (pos < len && (s[pos] == '-' || s[pos] == '+')) pos++;
        while (pos < len && ((s[pos] >= '0' && s[pos] <= '9') || s[pos] == '.'
                          || s[pos] == 'e' || s[pos] == 'E'
                          || s[pos] == '+' || s[pos] == '-')) {
            if ((s[pos] == '+' || s[pos] == '-') && pos > start
                && s[pos-1] != 'e' && s[pos-1] != 'E') break;
            pos++;
        }
        char buf[64];
        int n = pos - start;
        if (n <= 0 || n >= (int)sizeof(buf)) return 0.0;
        memcpy(buf, s + start, n);
        buf[n] = '\0';
        return atof(buf);
    }

    int useq_apply_state_snapshot(const char* json_str)
    {
        if (!g_engine || !json_str) return -1;

        int len = (int)strlen(json_str);
        const char* s = json_str;

        // Find the opening '{' of the top-level object
        int pos = 0;
        while (pos < len && s[pos] != '{') pos++;
        if (pos >= len) return -1;
        pos++; // skip '{'

        // 1. Re-eval output source texts
        int outputs_pos = find_field_at_depth(s, len, pos, "outputs");
        if (outputs_pos >= 0 && outputs_pos < len && s[outputs_pos] == '{') {
            int op = outputs_pos + 1; // inside the outputs object
            while (op < len) {
                while (op < len && (s[op] == ' ' || s[op] == '\t' || s[op] == '\n'
                                  || s[op] == '\r' || s[op] == ',')) op++;
                if (op >= len || s[op] == '}') break;
                if (s[op] != '"') break;
                // Extract output name key
                String oname = extract_json_string(s, len, op);
                op = skip_json_string(s, len, op);
                // skip colon
                while (op < len && (s[op] == ' ' || s[op] == ':')) op++;
                // The value must be an object
                if (op >= len || s[op] != '{') {
                    op = skip_json_value(s, len, op);
                    continue;
                }
                int val_obj_start = op + 1;
                int val_obj_end = skip_json_value(s, len, op);
                // Find "source" inside this sub-object
                int src_pos = find_field_at_depth(s, len, val_obj_start, "source");
                if (src_pos >= 0 && src_pos < val_obj_end && s[src_pos] == '"') {
                    String source = extract_json_string(s, len, src_pos);
                    if (source.length() > 0) {
                        String cmd = "(" + oname + " " + source + ")";
                        sig::eval_cold(cmd.c_str(), (uint32_t)cmd.length(), *g_engine);
                    }
                }
                op = val_obj_end;
            }
        }

        // 2. Patch state slot values
        int slots_pos = find_field_at_depth(s, len, pos, "stateSlots");
        if (slots_pos >= 0 && slots_pos < len && s[slots_pos] == '[') {
            int p = slots_pos + 1;
            uint16_t slot_idx = 0;
            while (p < len && s[p] != ']' && slot_idx < sig::MAX_STATE_SLOTS) {
                while (p < len && (s[p] == ' ' || s[p] == ',' || s[p] == '\n')) p++;
                if (p >= len || s[p] == ']') break;
                if (s[p] != '{') break;
                int obj_start = p + 1;
                int obj_end = skip_json_value(s, len, p);
                int vpos = find_field_at_depth(s, len, obj_start, "value");
                if (vpos >= 0 && vpos < obj_end) {
                    double val = extract_json_number(s, len, vpos);
                    if (slot_idx < g_engine->pool.state_slot_count) {
                        g_engine->pool.state_values[slot_idx] = val;
                    }
                }
                p = obj_end;
                slot_idx++;
            }
        }

        // 3. Patch live-edit slots
        int live_pos = find_field_at_depth(s, len, pos, "liveSlots");
        if (live_pos >= 0 && live_pos < len && s[live_pos] == '[') {
            int p = live_pos + 1;
            while (p < len && s[p] != ']') {
                while (p < len && (s[p] == ' ' || s[p] == ',' || s[p] == '\n')) p++;
                if (p >= len || s[p] == ']') break;
                if (s[p] != '{') break;
                int obj_start = p + 1;
                int obj_end = skip_json_value(s, len, p);
                int id_pos = find_field_at_depth(s, len, obj_start, "id");
                int val_pos = find_field_at_depth(s, len, obj_start, "value");
                if (id_pos >= 0 && val_pos >= 0 && s[id_pos] == '"') {
                    String slot_id = extract_json_string(s, len, id_pos);
                    double val = extract_json_number(s, len, val_pos);
                    if (slot_id.length() > 0) {
                        g_engine->pool.set_live_slot_value(slot_id.c_str(), val);
                    }
                }
                p = obj_end;
            }
        }

        g_projection_fork.valid = false;
        return 0;
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
                if (idx == sig::NODE_NONE) {
                    for (int s = 0; s < num_samples; s++) {
                        if (s > 0) json_result += ",";
                        char val_buf[32];
                        snprintf(val_buf, sizeof(val_buf), "%.15g",
                                 eval_expression_at_time(outputs[c].c_str(), t_array[s]));
                        json_result += val_buf;
                    }
                } else if (g_engine->pool.outputs[idx].valid) {
                    uint16_t row = index_to_row[idx];
                    for (int s = 0; s < num_samples; s++) {
                        if (s > 0) json_result += ",";
                        char val_buf[32];
                        snprintf(val_buf, sizeof(val_buf), "%.15g",
                                 batch_buf[row * num_samples + s]);
                        json_result += val_buf;
                    }
                } else {
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

                if (idx == sig::NODE_NONE) {
                    for (int s = 0; s < num_samples; s++)
                        row[s] = eval_expression_at_time(outputs[c].c_str(), t_array[s]);
                } else if (g_engine->pool.outputs[idx].valid) {
                    uint16_t active_row = index_to_row[idx];
                    memcpy(row, &batch_buf[active_row * num_samples],
                           num_samples * sizeof(double));
                } else {
                    for (int s = 0; s < num_samples; s++)
                        row[s] = std::numeric_limits<double>::quiet_NaN();
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

    // ---------------------------------------------------------------
    // Output Classification (visualisation.md §7.3–7.4)
    // ---------------------------------------------------------------

    const char* useq_output_classifications()
    {
        if (!g_engine) return alloc_cstr("[]");

        String result = "[";
        for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
            if (i > 0) result += ",";
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", (int)g_engine->pool.output_class[i]);
            result += buf;
        }
        result += "]";
        return alloc_string(result);
    }

    uint32_t useq_output_dependencies(int output_index)
    {
        if (!g_engine || output_index < 0 || output_index >= (int)sig::MAX_OUTPUTS)
            return 0;
        return g_engine->pool.output_input_mask[output_index];
    }

    // ---------------------------------------------------------------
    // Combined tick + projection fork in a single boundary crossing.
    // See docs/specs/visualisation.md §5.2/§7.2 and
    // src-useq/docs/specs/visualisation-projection.md §7.
    //
    // Phase 1: state-advancing tick at tick_time.
    //
    // Phase 2 (projection_mode controls behaviour):
    //   0 — no projection (tick only).
    //   1 — reset-fill: clone post-tick state into fork, project from
    //       strictly after tick_time to projection_end.
    //   2 — extend-frontier: advance existing fork from its frontier
    //       toward projection_end. Fails (-1) if no valid fork exists.
    //
    // Buffer layout (doubles):
    //   [0 .. num_channels-1]                      tick values
    //   [num_channels .. num_channels*(N+1) - 1]   projection samples
    //
    // Returns num_channels on success, -1 on error.
    // ---------------------------------------------------------------
    int useq_tick_and_project(
        const char* outputs_json,
        double tick_time,
        int projection_mode,
        double projection_end,
        int num_future_samples,
        int buffer_ptr,
        int buffer_length)
    {
        if (!g_engine) {
            s_last_error = "uSEQ not initialized";
            return -1;
        }
        if (num_future_samples < 0) {
            s_last_error = "num_future_samples must be >= 0";
            return -1;
        }
        if (projection_mode < 0 || projection_mode > 2) {
            s_last_error = "projection_mode must be 0, 1, or 2";
            return -1;
        }

        try {
            std::vector<String> outputs;
            if (!parse_output_names(outputs_json, outputs)) {
                s_last_error = "Failed to parse outputs JSON array";
                return -1;
            }

            int num_channels = (int)outputs.size();
            int proj_count = (projection_mode == 0) ? 0 : num_future_samples;
            int required_slots = num_channels + (num_channels * proj_count);
            if (required_slots > buffer_length) {
                s_last_error = "Buffer too small";
                return -1;
            }

            std::vector<uint16_t> output_indices;
            output_indices.reserve(num_channels);
            for (const auto& name : outputs)
                output_indices.push_back(resolve_output_name(name.c_str()));

            double* buf = reinterpret_cast<double*>(buffer_ptr);

            // ── Phase 1: state-advancing tick ──────────────────────
            double tick_outputs[sig::MAX_OUTPUTS] = {};
            execute_at_time(tick_time, tick_outputs);

            for (int c = 0; c < num_channels; c++) {
                uint16_t idx = output_indices[c];
                if (idx == sig::NODE_NONE) {
                    buf[c] = eval_expression_at_time(outputs[c].c_str(), tick_time);
                } else {
                    buf[c] = g_engine->pool.outputs[idx].valid
                        ? tick_outputs[idx]
                        : std::numeric_limits<double>::quiet_NaN();
                }
            }

            // ── Phase 2: projection ────────────────────────────────
            if (projection_mode == 0 || proj_count == 0) {
                s_last_error = "";
                return num_channels;
            }

            if (projection_mode == 2 && !g_projection_fork.valid) {
                s_last_error = "extend-frontier requested but no valid projection fork";
                return -1;
            }

            // Reset-fill: clone post-tick live state into the fork
            if (projection_mode == 1)
                reset_projection_fork(tick_time);

            // Build time array — samples strictly after the fork origin.
            // step = (end - origin) / N, first sample at origin + step.
            double origin = g_projection_fork.frontier_time;
            if (!std::isfinite(projection_end) || projection_end <= origin) {
                s_last_error = "projection_end must be > fork frontier";
                return -1;
            }

            double step = (projection_end - origin) / (double)proj_count;
            std::vector<double> t_array(proj_count);
            for (int i = 0; i < proj_count; i++)
                t_array[i] = origin + step * (i + 1);

            // Active-output row mapping
            uint16_t index_to_row[sig::MAX_OUTPUTS];
            uint16_t num_active = 0;
            for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
                if (g_engine->pool.outputs[i].valid)
                    index_to_row[i] = num_active++;
                else
                    index_to_row[i] = sig::NODE_NONE;
            }

            std::vector<double> batch_buf(num_active * proj_count, 0.0);
            project_from_fork(t_array.data(), proj_count, num_active, batch_buf.data());

            // Copy requested channels into caller's buffer
            double* proj_buf = buf + num_channels;
            for (int c = 0; c < num_channels; c++) {
                double* row = proj_buf + (c * proj_count);
                uint16_t idx = output_indices[c];
                if (idx == sig::NODE_NONE) {
                    for (int s = 0; s < proj_count; s++)
                        row[s] = eval_expression_at_time(outputs[c].c_str(), t_array[s]);
                } else if (g_engine->pool.outputs[idx].valid) {
                    uint16_t active_row = index_to_row[idx];
                    memcpy(row, &batch_buf[active_row * proj_count],
                           proj_count * sizeof(double));
                } else {
                    for (int s = 0; s < proj_count; s++)
                        row[s] = std::numeric_limits<double>::quiet_NaN();
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
            s_last_error = "Unknown error during tick_and_project";
            return -1;
        }
    }
}
