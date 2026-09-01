#include "wasm_wrapper_internal.h"

// Evaluate any signal expression at a given time via the cold eval scratch
// pool. Names that aren't output sinks (e.g. "bar", "beat", "(+ bar 0.5)")
// are compiled, executed once, and the numeric result is returned.
double eval_expression_at_time(const char* expr, double t) {
    if (!g_engine) return std::numeric_limits<double>::quiet_NaN();
    sig::EngineState saved_state = g_engine->state;
    g_engine->state.current_wall_time = t;
    g_engine->state.current_time = g_engine->state.logical_time(t);
    g_engine->state.current_dt = g_engine->state.reset_dt_on_next_tick
        ? 0.0 : g_engine->state.current_time - g_prev_tick_time;
    uint32_t len = (uint32_t)strlen(expr);
    sig::EvalResult result = sig::eval_expression(expr, len, *g_engine);
    g_engine->state = saved_state;
    if (result.kind == sig::EvalResult::Number) return result.number;
    return std::numeric_limits<double>::quiet_NaN();
}

bool has_active_state() {
    return g_engine && g_engine->pool.state_slot_count > 0;
}

// True if any active output's graph needs the sample-sequential batch path:
//  - prev() feedback (e.g. (a2 (+ (prev a2) 0.01))) has no defstate slot, so
//    has_active_state() misses it, yet the fast batch path would read the
//    stale carried-forward scalar for every sample instead of the
//    immediately-preceding sample within the batch (prev.md §1.4/§1.6);
//  - bare dt reads (e.g. (a1 (* dt 1000))): the fast batch path evaluates
//    samples independently without advancing per-sample dt, so LoadDt renders
//    zero (A11). Sequential batching computes dt between consecutive samples.
bool needs_sequential_batch() {
    if (!g_engine) return false;
    const sig::NodePool& pool = g_engine->pool;
    // Scan all exec-ordered nodes once.
    for (uint16_t i = 0; i < pool.exec_count; i++) {
        uint16_t idx = pool.exec_order[i];
        if (idx >= pool.node_count) continue;
        sig::NodeOp op = pool.nodes[idx].op;
        if (op == sig::NodeOp::PrevOutputLoad || op == sig::NodeOp::LoadDt)
            return true;
    }
    return false;
}

// Both public live-tick APIs advance the same VM. A single strict wall-time
// frontier makes ownership fail closed: callers must choose one API as the
// authoritative owner for each instant, and accidental duplicate/cross-owner
// ticks cannot advance prev/state twice. Logical time may legitimately rewind
// through transport commands, so monotonicity is enforced on host wall time.
bool validate_execution_time(double wall_time,
                             ExecutionOwnership ownership,
                             double& logical_time) {
    if (!std::isfinite(wall_time)) {
        s_last_error = "authoritative tick time must be finite";
        return false;
    }

    logical_time = g_engine->state.logical_time(wall_time);
    if (!std::isfinite(logical_time)) {
        s_last_error = "authoritative logical tick time must be finite";
        return false;
    }

    if (ownership == ExecutionOwnership::Authoritative &&
        g_has_authoritative_wall_frontier &&
        wall_time <= g_authoritative_wall_frontier) {
        s_last_error =
            "authoritative tick time must strictly increase; use one live-tick API";
        return false;
    }
    return true;
}

// Sequential batch for stateful signals: ticks forward sample-by-sample,
// advancing state between each. Saves/restores real state so visualization
// doesn't corrupt the live signal.
void execute_batch_sequential(
    const double* t_array, int num_samples,
    const double* cell_values,
    uint16_t num_active, double* batch_buf)
{
    PreserveRuntimeHealth preserve_runtime_health(g_engine->pool);

    // Save all mutable state so visualization doesn't corrupt the live signal
    double saved_state[sig::MAX_STATE_SLOTS];
    bool saved_state_observed[sig::MAX_STATE_SLOTS];
    double saved_prev_outputs[sig::MAX_OUTPUTS];
    double saved_lkg[sig::MAX_OUTPUTS];
    bool saved_valid[sig::MAX_OUTPUTS];
    bool saved_has_lkg[sig::MAX_OUTPUTS];
    uint16_t saved_slot_count = g_engine->pool.state_slot_count;
    double saved_prev_t = g_prev_tick_time;
    memcpy(saved_state, g_engine->pool.state_values, sizeof(saved_state));
    memcpy(saved_state_observed, g_engine->pool.state_observed,
           sizeof(saved_state_observed));
    memcpy(saved_prev_outputs, g_engine->pool.prev_output_values, sizeof(saved_prev_outputs));
    for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
        saved_lkg[i] = g_engine->pool.outputs[i].lkg_value;
        saved_valid[i] = g_engine->pool.outputs[i].valid;
        saved_has_lkg[i] = g_engine->pool.outputs[i].has_lkg;
    }

    // Snapshot the active-row map ONCE before the sample loop (A2) — see
    // project_from_fork for the rationale (commit_outputs mutates `valid`
    // mid-batch, so a live per-sample scan can overflow batch_buf).
    uint16_t row_of[sig::MAX_OUTPUTS];
    {
        uint16_t r = 0;
        for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++)
            row_of[i] = (g_engine->pool.outputs[i].valid && r < num_active)
                            ? r++ : sig::NODE_NONE;
    }

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
        ctx.ground_t      = g_engine->state.current_wall_time
            + (t - g_engine->state.current_time);
        ctx.advance_state = t != g_prev_tick_time;

        sig::TickTransition transition;
        sig::prepare_tick(g_engine->pool, ctx, transition);
        sig::commit_tick(g_engine->pool, ctx, transition);
        g_prev_tick_time = t;

        for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
            if (row_of[i] != sig::NODE_NONE)
                batch_buf[row_of[i] * num_samples + s] = output_values[i];
        }
    }

    // Restore all state — visualization is read-only
    memcpy(g_engine->pool.state_values, saved_state, sizeof(saved_state));
    memcpy(g_engine->pool.state_observed, saved_state_observed,
           sizeof(saved_state_observed));
    memcpy(g_engine->pool.prev_output_values, saved_prev_outputs, sizeof(saved_prev_outputs));
    for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
        g_engine->pool.outputs[i].lkg_value = saved_lkg[i];
        g_engine->pool.outputs[i].valid = saved_valid[i];
        g_engine->pool.outputs[i].has_lkg = saved_has_lkg[i];
    }
    g_engine->pool.state_slot_count = saved_slot_count;
    g_prev_tick_time = saved_prev_t;
}

// Execute the whole live graph once. Optional synth-control output is ordered
// exactly like the published controls[] artefact, so the host can use array
// position as its commit-plan channel index without evaluating a second VM.
bool execute_at_time(double wall_time, double* output_values,
                     double* synth_control_values,
                     uint16_t synth_control_capacity,
                     ExecutionOwnership ownership) {
    double t;
    if (!validate_execution_time(wall_time, ownership, t)) return false;

    g_engine->state.current_wall_time = wall_time;
    g_engine->state.current_time = t;

    if (!g_engine->state.is_playing) {
        for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
            output_values[i] = g_engine->pool.outputs[i].has_lkg
                ? g_engine->pool.outputs[i].lkg_value : 0.0;
        }
        if (synth_control_values) {
            uint16_t count = g_engine->synth_graph.control_count();
            if (count > synth_control_capacity) count = synth_control_capacity;
            for (uint16_t i = 0; i < count; i++) {
                const sig::SynthControlChannel& control =
                    g_engine->synth_graph.controls[i];
                synth_control_values[i] = control.has_lkg
                    ? control.lkg_value : 0.0;
            }
        }
        g_engine->state.current_dt = 0.0;
        if (ownership == ExecutionOwnership::Authoritative) {
            g_authoritative_wall_frontier = wall_time;
            g_has_authoritative_wall_frontier = true;
        }
        return true;
    }

    double cell_values[sig::MAX_CELLS];
    g_engine->cells.snapshot_values(cell_values, sig::MAX_CELLS);

    double node_values[sig::MAX_TOTAL_NODES];

    sig::ExecutionContext ctx;
    ctx.t             = t;
    ctx.dt            = g_engine->state.reset_dt_on_next_tick
        ? 0.0 : t - g_prev_tick_time;
    ctx.cell_values   = cell_values;
    ctx.hw_inputs     = g_hw_inputs;
    ctx.data_pool     = g_engine->cells.data_pool;
    ctx.data_offsets  = g_engine->cells.data_offsets;
    ctx.data_lengths  = g_engine->cells.data_lengths;
    ctx.prev_outputs  = g_engine->pool.prev_output_values;
    ctx.output_values = output_values;
    ctx.workspace     = node_values;
    ctx.ground_t      = wall_time;
    ctx.advance_state = !g_engine->state.reset_dt_on_next_tick;

    sig::TickTransition transition;
    sig::prepare_tick(g_engine->pool, ctx, transition);

    uint16_t failed_contexts[sig::MAX_SYNTH_CONTROLS] = {};
    uint16_t failed_count = 0;
    if (synth_control_values) {
        uint16_t count = g_engine->synth_graph.control_count();
        if (count > synth_control_capacity) count = synth_control_capacity;
        for (uint16_t i = 0; i < count; i++) {
            sig::SynthControlChannel& control =
                g_engine->synth_graph.controls[i];
            double value = control.root_node < g_engine->pool.node_count
                ? node_values[control.root_node]
                : std::numeric_limits<double>::quiet_NaN();
            if (std::isfinite(value)) {
                control.lkg_value = value;
                control.has_lkg = true;
            } else {
                value = control.has_lkg ? control.lkg_value : 0.0;
                bool already_failed = false;
                for (uint16_t f = 0; f < failed_count; f++) {
                    if (failed_contexts[f] == control.owner_context) {
                        already_failed = true;
                        break;
                    }
                }
                if (!already_failed && failed_count < sig::MAX_SYNTH_CONTROLS)
                    failed_contexts[failed_count++] = control.owner_context;
            }
            synth_control_values[i] = value;
        }
    }

    sig::commit_tick(g_engine->pool, ctx, transition,
                     failed_contexts, failed_count);
    g_engine->state.current_dt = ctx.dt;
    g_engine->state.reset_dt_on_next_tick = false;
    g_prev_tick_time = t;

    if (ownership == ExecutionOwnership::Authoritative) {
        g_authoritative_wall_frontier = wall_time;
        g_has_authoritative_wall_frontier = true;
    }
    return true;
}

// ── Probe pool management ─────────────────────────────────────────────────
extern "C" {

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

            PreserveRuntimeHealth preserve_runtime_health(g_engine->pool);

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

            if (has_active_state() || needs_sequential_batch()) {
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

            PreserveRuntimeHealth preserve_runtime_health(g_engine->pool);

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

            if (has_active_state() || needs_sequential_batch()) {
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

} // extern "C"
