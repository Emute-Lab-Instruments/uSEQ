#include "wasm_wrapper_internal.h"
#include <limits>

// ── Persistent projection fork (visualisation-projection.md §2) ──────────
//
// Cloned from live state on invalidation (reset-fill), then advanced
// incrementally at the frontier (extend-frontier). Live state is never
// mutated during projection.

struct ProjectionFork {
    double state_values[sig::MAX_STATE_SLOTS];
    bool state_observed[sig::MAX_STATE_SLOTS];
    uint16_t state_slot_count;
    sig::StateResourceRegistry registry;
    double prev_output_values[sig::MAX_OUTPUTS];
    double lkg_values[sig::MAX_OUTPUTS];
    bool output_valid[sig::MAX_OUTPUTS];
    bool output_has_lkg[sig::MAX_OUTPUTS];
    uint64_t runtime_fallback_mask;
    uint64_t state_update_failure_mask;
    double live_slot_values[sig::MAX_LIVE_SLOTS];
    uint16_t live_slot_count;
    double prev_tick_time;
    double ground_time;
    double cell_values[sig::MAX_CELLS];
    double hw_inputs[32];
    double frontier_time;
    double start_time;
    bool   valid;
};

static ProjectionFork g_projection_fork = {};

extern "C" double useq_projection_frontier_time() {
    return g_projection_fork.valid
        ? g_projection_fork.frontier_time
        : std::numeric_limits<double>::quiet_NaN();
}

// Evaluate selected synth-control roots at caller-supplied wall times on a
// restorable sequential scratch trajectory. This is the audio producer's
// dense sampling path; the live executor remains owned by the tick export.
extern "C" int useq_sample_synth_controls_into(
        int control_indices_ptr, int control_count,
        int sample_times_ptr, int sample_count,
        int out_values_ptr) {
    if (!g_engine || control_count < 0 || sample_count < 0 ||
        control_count > sig::MAX_SYNTH_CONTROLS || sample_count > 8192 ||
        (control_count > 0 && sample_count > 65536 / control_count) ||
        (control_count > 0 && (!control_indices_ptr || !out_values_ptr)) ||
        (sample_count > 0 && !sample_times_ptr)) {
        s_last_error = "Invalid synth-control sample buffer";
        return -1;
    }
    const uint16_t available = g_engine->synth_graph.control_count();
    const auto* indices = reinterpret_cast<const double*>(control_indices_ptr);
    const auto* times = reinterpret_cast<const double*>(sample_times_ptr);
    auto* values = reinterpret_cast<double*>(out_values_ptr);
    for (int row = 0; row < control_count; ++row) {
        if (!std::isfinite(indices[row]) || indices[row] < 0 ||
            indices[row] >= available || std::floor(indices[row]) != indices[row]) {
            s_last_error = "Synth control index out of range";
            return -1;
        }
    }

    double logical_times[8192];
    for (int sample = 0; sample < sample_count; ++sample) {
        if (!std::isfinite(times[sample]) ||
            !validate_execution_time(times[sample],
                                     ExecutionOwnership::Observational,
                                     logical_times[sample])) {
            s_last_error = "Invalid synth-control sample time";
            return -1;
        }
    }

    double cells[sig::MAX_CELLS];
    g_engine->cells.snapshot_values(cells, sig::MAX_CELLS);
    double output_values[sig::MAX_OUTPUTS] = {};
    double node_values[sig::MAX_TOTAL_NODES];
    double saved_state[sig::MAX_STATE_SLOTS];
    bool saved_state_observed[sig::MAX_STATE_SLOTS];
    double saved_prev_outputs[sig::MAX_OUTPUTS];
    double saved_lkg[sig::MAX_OUTPUTS];
    bool saved_valid[sig::MAX_OUTPUTS];
    bool saved_has_lkg[sig::MAX_OUTPUTS];
    const uint16_t saved_slot_count = g_engine->pool.state_slot_count;
    const double saved_prev_time = g_prev_tick_time;
    memcpy(saved_state, g_engine->pool.state_values, sizeof(saved_state));
    memcpy(saved_state_observed, g_engine->pool.state_observed,
           sizeof(saved_state_observed));
    memcpy(saved_prev_outputs, g_engine->pool.prev_output_values,
           sizeof(saved_prev_outputs));
    for (uint16_t i = 0; i < sig::MAX_OUTPUTS; ++i) {
        saved_lkg[i] = g_engine->pool.outputs[i].lkg_value;
        saved_valid[i] = g_engine->pool.outputs[i].valid;
        saved_has_lkg[i] = g_engine->pool.outputs[i].has_lkg;
    }
    PreserveRuntimeHealth preserve_runtime_health(g_engine->pool);
    for (int sample = 0; sample < sample_count; ++sample) {
        const double logical_time = logical_times[sample];
        sig::ExecutionContext ctx;
        memset(output_values, 0, sizeof(output_values));
        ctx.t = logical_time;
        ctx.dt = logical_time - g_prev_tick_time;
        ctx.cell_values = cells;
        ctx.hw_inputs = g_hw_inputs;
        ctx.data_pool = g_engine->cells.data_pool;
        ctx.data_offsets = g_engine->cells.data_offsets;
        ctx.data_lengths = g_engine->cells.data_lengths;
        ctx.prev_outputs = g_engine->pool.prev_output_values;
        ctx.output_values = output_values;
        ctx.workspace = node_values;
        ctx.ground_t = g_engine->state.current_wall_time +
            (logical_time - g_engine->state.current_time);
        ctx.advance_state = logical_time != g_prev_tick_time;
        sig::TickTransition transition;
        sig::prepare_tick(g_engine->pool, ctx, transition);
        sig::commit_tick(g_engine->pool, ctx, transition);
        g_prev_tick_time = logical_time;
        for (int row = 0; row < control_count; ++row) {
            const sig::SynthControlChannel& control =
                g_engine->synth_graph.controls[static_cast<uint16_t>(indices[row])];
            values[row * sample_count + sample] =
                control.root_node < g_engine->pool.node_count
                    ? node_values[control.root_node]
                    : std::numeric_limits<double>::quiet_NaN();
        }
    }
    memcpy(g_engine->pool.state_values, saved_state, sizeof(saved_state));
    memcpy(g_engine->pool.state_observed, saved_state_observed,
           sizeof(saved_state_observed));
    memcpy(g_engine->pool.prev_output_values, saved_prev_outputs,
           sizeof(saved_prev_outputs));
    for (uint16_t i = 0; i < sig::MAX_OUTPUTS; ++i) {
        g_engine->pool.outputs[i].lkg_value = saved_lkg[i];
        g_engine->pool.outputs[i].valid = saved_valid[i];
        g_engine->pool.outputs[i].has_lkg = saved_has_lkg[i];
    }
    g_engine->pool.state_slot_count = saved_slot_count;
    g_prev_tick_time = saved_prev_time;
    s_last_error = "";
    return 0;
}

void reset_projection_session()
{
    g_projection_fork = ProjectionFork{};
}

void invalidate_projection()
{
    g_projection_fork.valid = false;
}

static void reset_projection_fork(double tick_time) {
    memcpy(g_projection_fork.state_values,
           g_engine->pool.state_values, sizeof(g_projection_fork.state_values));
    memcpy(g_projection_fork.state_observed,
           g_engine->pool.state_observed,
           sizeof(g_projection_fork.state_observed));
    g_projection_fork.state_slot_count = g_engine->pool.state_slot_count;
    g_projection_fork.registry = g_engine->registry;
    memcpy(g_projection_fork.prev_output_values,
           g_engine->pool.prev_output_values, sizeof(g_projection_fork.prev_output_values));
    for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
        g_projection_fork.lkg_values[i] = g_engine->pool.outputs[i].lkg_value;
        g_projection_fork.output_valid[i] = g_engine->pool.outputs[i].valid;
        g_projection_fork.output_has_lkg[i] =
            g_engine->pool.outputs[i].has_lkg;
    }
    g_projection_fork.runtime_fallback_mask =
        g_engine->pool.runtime_fallback_mask;
    g_projection_fork.state_update_failure_mask =
        g_engine->pool.state_update_failure_mask;
    g_projection_fork.live_slot_count = g_engine->pool.live_slot_count;
    for (uint16_t i = 0; i < g_projection_fork.live_slot_count; i++)
        g_projection_fork.live_slot_values[i] =
            g_engine->pool.live_slots[i].value;
    g_projection_fork.prev_tick_time = tick_time;
    g_projection_fork.ground_time = g_engine->state.current_wall_time;
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
    PreserveRuntimeHealth preserve_runtime_health(g_engine->pool);

    // Save live state
    double saved_state[sig::MAX_STATE_SLOTS];
    bool saved_state_observed[sig::MAX_STATE_SLOTS];
    uint16_t saved_slot_count = g_engine->pool.state_slot_count;
    sig::StateResourceRegistry saved_registry = g_engine->registry;
    double saved_prev_outputs[sig::MAX_OUTPUTS];
    double saved_lkg[sig::MAX_OUTPUTS];
    bool saved_valid[sig::MAX_OUTPUTS];
    bool saved_has_lkg[sig::MAX_OUTPUTS];
    double saved_live_slot_values[sig::MAX_LIVE_SLOTS];
    uint16_t saved_live_slot_count = g_engine->pool.live_slot_count;
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
    for (uint16_t i = 0; i < saved_live_slot_count; i++)
        saved_live_slot_values[i] = g_engine->pool.live_slots[i].value;

    // Snapshot the active-row map ONCE before the sample loop (A2). The loop
    // below runs commit_outputs, which sets valid=true for any output with a
    // root node — re-scanning live `valid` per sample would grow the packed
    // row count mid-batch and write past the end of batch_buf (the caller
    // sized it from ITS pre-loop scan). Same pattern as
    // useq_eval_outputs_time_window_into.
    uint16_t row_of[sig::MAX_OUTPUTS];
    {
        uint16_t r = 0;
        for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++)
            row_of[i] = (g_projection_fork.output_valid[i] && r < num_active)
                            ? r++ : sig::NODE_NONE;
    }

    // Install fork state
    memcpy(g_engine->pool.state_values,
           g_projection_fork.state_values, sizeof(saved_state));
    memcpy(g_engine->pool.state_observed,
           g_projection_fork.state_observed,
           sizeof(g_projection_fork.state_observed));
    g_engine->pool.state_slot_count = g_projection_fork.state_slot_count;
    g_engine->registry = g_projection_fork.registry;
    memcpy(g_engine->pool.prev_output_values,
           g_projection_fork.prev_output_values, sizeof(saved_prev_outputs));
    for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
        g_engine->pool.outputs[i].lkg_value = g_projection_fork.lkg_values[i];
        g_engine->pool.outputs[i].valid = g_projection_fork.output_valid[i];
        g_engine->pool.outputs[i].has_lkg =
            g_projection_fork.output_has_lkg[i];
    }
    g_engine->pool.runtime_fallback_mask =
        g_projection_fork.runtime_fallback_mask;
    g_engine->pool.state_update_failure_mask =
        g_projection_fork.state_update_failure_mask;
    g_engine->pool.live_slot_count = g_projection_fork.live_slot_count;
    for (uint16_t i = 0; i < g_projection_fork.live_slot_count; i++)
        g_engine->pool.live_slots[i].value =
            g_projection_fork.live_slot_values[i];
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
        ctx.ground_t      = g_projection_fork.ground_time
            + (t - g_prev_tick_time);
        ctx.advance_state = t != g_prev_tick_time;

        sig::TickTransition transition;
        sig::prepare_tick(g_engine->pool, ctx, transition);
        sig::commit_tick(g_engine->pool, ctx, transition);
        g_projection_fork.ground_time = ctx.ground_t;
        g_prev_tick_time = t;

        for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
            if (row_of[i] != sig::NODE_NONE)
                batch_buf[row_of[i] * num_samples + s] = output_values[i];
        }
    }

    // Save fork state (fork advances)
    memcpy(g_projection_fork.state_values,
           g_engine->pool.state_values, sizeof(saved_state));
    memcpy(g_projection_fork.state_observed,
           g_engine->pool.state_observed,
           sizeof(g_projection_fork.state_observed));
    g_projection_fork.state_slot_count = g_engine->pool.state_slot_count;
    g_projection_fork.registry = g_engine->registry;
    memcpy(g_projection_fork.prev_output_values,
           g_engine->pool.prev_output_values, sizeof(saved_prev_outputs));
    for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
        g_projection_fork.lkg_values[i] = g_engine->pool.outputs[i].lkg_value;
        g_projection_fork.output_valid[i] = g_engine->pool.outputs[i].valid;
        g_projection_fork.output_has_lkg[i] =
            g_engine->pool.outputs[i].has_lkg;
    }
    g_projection_fork.runtime_fallback_mask =
        g_engine->pool.runtime_fallback_mask;
    g_projection_fork.state_update_failure_mask =
        g_engine->pool.state_update_failure_mask;
    g_projection_fork.live_slot_count = g_engine->pool.live_slot_count;
    for (uint16_t i = 0; i < g_projection_fork.live_slot_count; i++)
        g_projection_fork.live_slot_values[i] =
            g_engine->pool.live_slots[i].value;
    g_projection_fork.prev_tick_time = g_prev_tick_time;
    if (num_samples > 0)
        g_projection_fork.frontier_time = t_array[num_samples - 1];

    // Restore live state
    memcpy(g_engine->pool.state_values, saved_state, sizeof(saved_state));
    memcpy(g_engine->pool.state_observed, saved_state_observed,
           sizeof(saved_state_observed));
    g_engine->pool.state_slot_count = saved_slot_count;
    g_engine->registry = saved_registry;
    memcpy(g_engine->pool.prev_output_values, saved_prev_outputs, sizeof(saved_prev_outputs));
    for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
        g_engine->pool.outputs[i].lkg_value = saved_lkg[i];
        g_engine->pool.outputs[i].valid = saved_valid[i];
        g_engine->pool.outputs[i].has_lkg = saved_has_lkg[i];
    }
    g_engine->pool.live_slot_count = saved_live_slot_count;
    for (uint16_t i = 0; i < saved_live_slot_count; i++)
        g_engine->pool.live_slots[i].value = saved_live_slot_values[i];
    g_prev_tick_time = saved_prev_t;
}

extern "C" {

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
        if (!outputs_json) {
            s_last_error = "outputs_json must not be null";
            return -1;
        }
        double logical_tick_time;
        if (!validate_execution_time(tick_time,
                                     ExecutionOwnership::Authoritative,
                                     logical_tick_time)) {
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
        if (buffer_length < 0) {
            s_last_error = "buffer_length must be >= 0";
            return -1;
        }

        try {
            std::vector<String> outputs;
            if (!parse_output_names(outputs_json, outputs)) {
                s_last_error = "Failed to parse outputs JSON array";
                return -1;
            }

            if (outputs.size() > (size_t)std::numeric_limits<int>::max()) {
                s_last_error = "Too many output channels";
                return -1;
            }

            int num_channels = (int)outputs.size();
            int proj_count = (projection_mode == 0) ? 0 : num_future_samples;
            size_t required_slots = (size_t)num_channels * ((size_t)proj_count + 1);
            if (required_slots > (size_t)buffer_length) {
                s_last_error = "Buffer too small";
                return -1;
            }
            if (required_slots > 0 && buffer_ptr == 0) {
                s_last_error = "buffer_ptr must not be null";
                return -1;
            }

            // Projection validation is part of the call's precondition, not
            // phase 2: an invalid projection request must not commit phase 1.
            double logical_projection_end =
                g_engine->state.logical_time(projection_end);
            double origin = logical_tick_time;
            double step = 0.0;
            if (proj_count > 0) {
                if (projection_mode == 2) {
                    if (!g_projection_fork.valid) {
                        s_last_error =
                            "extend-frontier requested but no valid projection fork";
                        return -1;
                    }
                    origin = g_projection_fork.frontier_time;
                }
                if (!std::isfinite(logical_projection_end) ||
                    logical_projection_end <= origin) {
                    s_last_error = "projection_end must be > fork frontier";
                    return -1;
                }
                step = (logical_projection_end - origin) /
                    (double)proj_count;
                if (!std::isfinite(step) || step <= 0.0) {
                    s_last_error = "projection interval must be finite";
                    return -1;
                }
            }

            std::vector<uint16_t> output_indices;
            output_indices.reserve(num_channels);
            for (const auto& name : outputs)
                output_indices.push_back(resolve_output_name(name.c_str()));

            // Allocate every projection buffer before the live tick. A bad
            // request or allocation failure therefore cannot half-commit the
            // combined operation.
            std::vector<double> t_array(proj_count);
            for (int i = 0; i < proj_count; i++)
                t_array[i] = origin + step * (i + 1);

            uint16_t index_to_row[sig::MAX_OUTPUTS];
            uint16_t num_active = 0;
            for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
                if (g_engine->pool.outputs[i].valid)
                    index_to_row[i] = num_active++;
                else
                    index_to_row[i] = sig::NODE_NONE;
            }
            std::vector<double> batch_buf((size_t)num_active * proj_count, 0.0);

            double* buf = reinterpret_cast<double*>(buffer_ptr);

            // ── Phase 1: state-advancing tick ──────────────────────
            double tick_outputs[sig::MAX_OUTPUTS] = {};
            if (!execute_at_time(tick_time, tick_outputs, nullptr, 0,
                                 ExecutionOwnership::Authoritative))
                return -1;

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

            // Reset-fill: clone post-tick live state into the fork
            if (projection_mode == 1)
                reset_projection_fork(g_engine->state.current_time);

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

} // extern "C"
