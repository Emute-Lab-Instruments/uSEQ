#include "wasm_wrapper_internal.h"

extern "C" {

    void useq_init()
    {
        if (g_engine) return;

        // Keep construction and initialization behind a local pointer. Some
        // initialization paths reset process-global compiler registries; if
        // the public pointer is published first, generated WASM can clear it
        // again and leave an exported but permanently uninitialized module.
        sig::SignalEngine* engine = new sig::SignalEngine();
        engine->init_defaults();

        // Allocate batch workspace for WASM visualization
        engine->pool.allocate_batch_workspace();

        g_engine = engine;
    }

    char* useq_eval(const char* input)
    {
        if (!g_engine) {
            return alloc_cstr("Error: uSEQ not initialized. Call useq_init() first.");
        }

        try {
            // Clear diagnostics from previous eval
            g_last_diagnostic_count = 0;
            invalidate_projection();

            g_engine->state.current_wall_time = g_current_time;
            g_engine->state.current_time =
                g_engine->state.logical_time(g_current_time);
            g_engine->state.current_dt = g_engine->state.reset_dt_on_next_tick
                ? 0.0 : g_engine->state.current_time - g_prev_tick_time;

            uint32_t length = (uint32_t)strlen(input);
            uint32_t generation_before = g_engine->session_generation;
            sig::EvalResult result = sig::eval_cold(input, length, *g_engine);

            if (g_engine->session_generation != generation_before)
                reset_host_session_state();

            // User eval may have changed cell definitions — invalidate
            // probe compilation cache so probes pick up the new state.
            if (g_engine->session_generation == generation_before)
                g_cell_revision++;

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

    // Advance the live VM exactly once and return ordered synth-control
    // samples for the published artefact revision. Returns the required
    // control count, or -1 when the caller buffer is invalid/too small.
    int useq_tick_synth_controls(double wall_time,
                                 int buffer_ptr,
                                 int buffer_length)
    {
        if (!g_engine) {
            s_last_error = "uSEQ not initialized";
            return -1;
        }
        uint16_t count = g_engine->synth_graph.control_count();
        if (buffer_length < 0 || (uint32_t)buffer_length < count) {
            s_last_error = "Synth control buffer too small";
            return -1;
        }
        if (count > 0 && buffer_ptr == 0) {
            s_last_error = "Synth control buffer must not be null";
            return -1;
        }
        double ignored_outputs[sig::MAX_OUTPUTS] = {};
        double* controls = reinterpret_cast<double*>(buffer_ptr);
        if (!execute_at_time(wall_time, ignored_outputs, controls, count,
                             ExecutionOwnership::Authoritative))
            return -1;
        s_last_error = "";
        return (int)count;
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

        PreserveRuntimeHealth preserve_runtime_health(g_engine->pool);

        // Save all mutable state — useq_eval_output is read-only and must
        // not corrupt live engine state (execute_at_time advances everything).
        double saved_state[sig::MAX_STATE_SLOTS];
        double saved_prev_outputs[sig::MAX_OUTPUTS];
        double saved_lkg[sig::MAX_OUTPUTS];
        bool saved_has_lkg[sig::MAX_OUTPUTS];
        uint16_t saved_slot_count = g_engine->pool.state_slot_count;
        sig::EngineState saved_engine_state = g_engine->state;
        memcpy(saved_state, g_engine->pool.state_values, sizeof(saved_state));
        memcpy(saved_prev_outputs, g_engine->pool.prev_output_values, sizeof(saved_prev_outputs));
        for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
            saved_lkg[i] = g_engine->pool.outputs[i].lkg_value;
            saved_has_lkg[i] = g_engine->pool.outputs[i].has_lkg;
        }
        double saved_prev_t = g_prev_tick_time;

        double output_values[sig::MAX_OUTPUTS] = {};
        bool executed = execute_at_time(
            time_seconds, output_values, nullptr, 0,
            ExecutionOwnership::Observational);

        // Restore all state
        memcpy(g_engine->pool.state_values, saved_state, sizeof(saved_state));
        memcpy(g_engine->pool.prev_output_values, saved_prev_outputs, sizeof(saved_prev_outputs));
        for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
            g_engine->pool.outputs[i].lkg_value = saved_lkg[i];
            g_engine->pool.outputs[i].has_lkg = saved_has_lkg[i];
        }
        g_engine->pool.state_slot_count = saved_slot_count;
        g_engine->state = saved_engine_state;
        g_prev_tick_time = saved_prev_t;

        return executed ? output_values[output_index]
                        : std::numeric_limits<double>::quiet_NaN();
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
        // Contract (diagnostics.md §4.2): a JSON array of live output,
        // named-state, and synth-control diagnostics; "[]" when every
        // subject is healthy.
        // Runtime masks describe the most recent authoritative execution;
        // rejected reactive candidates retain their own attributed compile
        // diagnostic while the previous program keeps running.
        if (!g_engine) {
            return alloc_cstr("[]");
        }

        bool has_active = g_engine->pool.runtime_fallback_mask != 0 ||
            g_engine->pool.state_update_failure_mask != 0;
        for (uint16_t i = 0; i < sig::MAX_OUTPUTS && !has_active; i++)
            has_active = g_engine->output_compile_diagnostics[i].active;
        for (uint16_t i = 0; i < sig::MAX_STATE_SLOTS && !has_active; i++)
            has_active = g_engine->state_compile_diagnostics[i].active;
        for (uint16_t i = 0;
             i < g_engine->synth_graph.control_count() && !has_active; i++) {
            has_active = g_engine->synth_graph.controls[i]
                             .compile_diagnostic.active();
        }
        if (!has_active) return alloc_cstr("[]");

        JsonBuilder json;
        json.array_begin_unkeyed();
        for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
            if (!(g_engine->pool.runtime_fallback_mask & ((uint64_t)1 << i)))
                continue;
            char name[3] = {0, 0, 0};
            if (!format_output_name(i, name)) continue;
            sig::OutputHealth health = sig::output_health(g_engine->pool, i);

            json.object_begin();
            json.field("output", name);
            json.field("severity",
                       health == sig::OutputHealth::Error
                           ? "error" : "warning");
            json.field("category", "arithmetic");
            json.field("state", sig::output_health_to_cstr(health));
            if (health == sig::OutputHealth::Error) {
                json.field("message",
                           "Non-finite value (NaN/Inf) reached this output "
                           "before any healthy sample; holding neutral zero");
            } else {
                json.field("message",
                           "Non-finite value (NaN/Inf) reached this output; "
                           "holding the last-known-good value");
            }
            json.object_end();
        }

        // Background output recompiles retain their previous program but
        // keep the rejected candidate's first diagnostic active until the
        // same dependency chain compiles cleanly.
        for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
            const sig::ActiveCompileDiagnostic& active =
                g_engine->output_compile_diagnostics[i];
            if (!active.active) continue;
            char name[3] = {0, 0, 0};
            if (!format_output_name(i, name)) continue;
            const sig::Diagnostic& d = active.diagnostic;
            json.object_begin();
            json.field("output", name);
            json.field("severity", sig::severity_to_cstr(d.severity));
            json.field("category", sig::category_to_cstr(d.category));
            json.field("state", sig::output_health_to_cstr(
                sig::output_health(g_engine->pool, i)));
            json.field("start", static_cast<int>(d.span_start));
            json.field("end", static_cast<int>(d.span_start + d.span_len));
            json.field("message", d.message ? d.message : "");
            if (d.suggestion) json.field("suggestion", d.suggestion);
            if (active.triggered_by != 0)
                json.field("triggered_by",
                           getSymbolString(active.triggered_by));
            json.object_end();
        }

        // Named-state health is its own subject: a defstate update can fail
        // before any output consumes it. Emit one state-attributed record so
        // that failure never disappears merely because the routing changed.
        for (uint16_t s = 0; s < g_engine->pool.state_slot_count; s++) {
            bool runtime_failed =
                (g_engine->pool.state_update_failure_mask &
                 ((uint64_t)1 << s)) != 0;
            const sig::ActiveCompileDiagnostic& active =
                g_engine->state_compile_diagnostics[s];
            if (!runtime_failed && !active.active) continue;
            sig::SymbolID state_symbol =
                g_engine->state_sources[s].state_symbol;
            if (state_symbol == 0) continue;
            const sig::Diagnostic* d = active.active
                ? &active.diagnostic : nullptr;
            json.object_begin();
            json.field("subject", "state");
            json.field("state", getSymbolString(state_symbol));
            json.field("severity", "error");
            json.field("category", d
                ? sig::category_to_cstr(d->category) : "arithmetic");
            json.field("status", "held");
            json.field("message", d && d->message
                ? d->message
                : "A declared state update produced NaN/Inf; holding its previous finite value");
            if (active.active && active.triggered_by != 0)
                json.field("triggered_by",
                           getSymbolString(active.triggered_by));
            json.object_end();
        }

        // Synth controls have one reactive slot and no derived runtime
        // diagnostic. Iterate the published control table directly so JSON
        // order is byte-for-byte the artifact controls[] order, including
        // after dense declaration replacement/removal.
        for (uint16_t i = 0; i < g_engine->synth_graph.control_count(); i++) {
            const sig::SynthControlChannel& control =
                g_engine->synth_graph.controls[i];
            const sig::SynthDeclaration* declaration =
                g_engine->synth_graph.declaration_for_control(i);
            const sig::NodeDefParam* parameter =
                g_engine->synth_graph.parameter_for_control(i);
            const auto& active = control.compile_diagnostic;
            if (!active.active()) continue;
            if (!declaration || !parameter) {
                json.object_begin();
                json.field("subject", "synth-control");
                json.field("identity", "");
                json.field("control", "");
                json.field("severity", "error");
                json.field("category", "runtime");
                json.field("status", "error");
                json.field("message",
                           "Synth control ownership metadata is inconsistent");
                json.object_end();
                continue;
            }
            json.object_begin();
            json.field("subject", "synth-control");
            json.field("identity", declaration->identity);
            json.field("control", parameter->name);
            json.field("severity", sig::severity_to_cstr(active.severity));
            json.field("category", sig::category_to_cstr(active.category));
            json.field("status", "retained");
            json.field("start", static_cast<int>(active.span_start));
            json.field("end", static_cast<int>(active.span_start +
                                                 active.span_len));
            json.field("message", active.message ? active.message : "");
            if (active.suggestion)
                json.field("suggestion", active.suggestion);
            if (active.triggered_by != 0)
                json.field("triggered_by",
                           getSymbolString(active.triggered_by));
            json.object_end();
        }
        json.array_end();
        return alloc_string(json.build());
    }

    // Stable scalar health query for actual-WASM conformance. Values match
    // sig::OutputHealth: 0 idle, 1 running, 2 fallback, 3 error; -1 denotes
    // an uninitialised runtime or invalid output name.
    int useq_output_health(const char* name)
    {
        if (!g_engine) return -1;
        uint16_t index = resolve_output_name(name);
        if (index == sig::NODE_NONE) return -1;
        return static_cast<int>(sig::output_health(g_engine->pool, index));
    }

    /**
     * Snapshot of the published synth artefacts (synth-nodes.md §7.2 /
     * VAL-COMP-009/012/015).
     *
     * Returns a versioned JSON object describing the current patch graph
     * (declarations keyed by stable identity) and the control channel
     * table. Both share one compiler revision so consumers can detect
     * coherent updates. Failed evals retain the previous snapshot and do
     * not advance the revision (VAL-COMP-010).
     *
     * Public identifiers are stable identity strings supplied by the
     * editor (hidden :id or explicit :name). Internal GC-remapped node
     * indices are never serialised (VAL-COMP-012).
     *
     * The schema declares an `abi` version field so future consumers can
     * reject incompatible bundles explicitly (VAL-COMP-015). The native
     * engine's canonical version is sig::SYNTH_ARTIFACT_ABI_VERSION; the
     * WASM wrapper uses the same constant via
     * sig::synth_artifacts_render_abi_wrapper() so native and WASM
     * consumers observe byte-equal payloads (VAL-COMP-016).
     */
    const char* useq_synth_artifacts()
    {
        // The wrapper renders the versioned body through the shared
        // synth_graph helper. Native tests exercise the same path.
        // Static storage avoids placing the maximum 64-node artefact buffer
        // on the WASM stack.
        static char buf[sig::SYNTH_ARTIFACT_JSON_CAP + 64];
        bool ok;
        if (g_engine) {
            ok = sig::synth_artifacts_render_abi_wrapper(
                *g_engine, sig::SYNTH_ARTIFACT_ABI_VERSION,
                buf, sizeof(buf));
        } else {
            // Engine not initialised — emit the canonical empty payload.
            std::snprintf(buf, sizeof(buf),
                "{\"abi\":%u,\"revision\":0,\"declarations\":[],\"controls\":[],\"connections\":[]}",
                (unsigned)sig::SYNTH_ARTIFACT_ABI_VERSION);
            ok = true;
        }
        if (!ok) {
            // Never turn a serialization failure into a valid-looking empty
            // revision: that would direct the host to tear down a healthy
            // graph. Missing declaration/control arrays make this envelope
            // fail closed in every ABI-2 consumer.
            std::snprintf(buf, sizeof(buf),
                "{\"abi\":%u,\"revision\":%lu,"
                "\"artifact_error\":\"serialization-capacity\"}",
                (unsigned)sig::SYNTH_ARTIFACT_ABI_VERSION,
                (unsigned long)(g_engine ? g_engine->synth_graph.revision : 0));
        }
        return alloc_cstr(buf);
    }

    /**
     * Set the runtime failure mode (failure-mode.md §3):
     *   0 = LKG fallback (default): non-finite at an output root falls back
     *       to the last-known-good value and raises a runtime diagnostic.
     *   1 = legacy zero-squash: every non-finite node result clamps to 0.0.
     * Returns the mode actually in effect, or -1 for an invalid argument.
     */
    int useq_set_failure_mode(int mode)
    {
        if (mode != 0 && mode != 1) return -1;
        sig::set_failure_mode(static_cast<sig::FailureMode>(mode));
        return mode;
    }

    /** Read the current runtime failure mode (0 = LKG, 1 = zero-squash). */
    int useq_get_failure_mode()
    {
        return static_cast<int>(sig::get_failure_mode());
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
        useq::json::ObjectCursor inputs(json_str, strlen(json_str));
        useq::json::Value key;
        useq::json::Value encoded;
        while (inputs.next(key, encoded)) {
            char id[sig::MAX_LIVE_SLOT_ID] = {};
            double value = 0.0;
            if (!useq::json::copy_string(key, id, sizeof(id)) ||
                !useq::json::number(encoded, value) || !std::isfinite(value)) {
                continue;
            }
            const int16_t slot_index = g_engine->pool.find_live_slot(id);
            if (slot_index < 0) continue;
            g_engine->pool.set_live_slot_value_by_index(
                static_cast<uint16_t>(slot_index), value);
            ++applied;
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

        invalidate_projection();
        return 0;
    }
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

} // extern "C"
