#include "executor.h"
#include <cmath>
#include <cstring>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace sig {

// ── Single-node evaluation ──────────────────────────────────────────────────

static inline double eval_node(
    const Node& n,
    double a, double b, double c,
    double t,
    double dt,
    const double* cell_values,
    const double* hw_inputs,
    const double* data_pool,
    const uint16_t* data_offsets,
    const uint16_t* data_lengths,
    const double* prev_output_values,
    const double* state_values
) {
    switch (n.op) {
        case NodeOp::Const:       return n.imm;
        case NodeOp::RawTimeLoad: return t;
        case NodeOp::LoadState:   return state_values[(uint16_t)n.imm];
        case NodeOp::LoadDt:      return dt;
        case NodeOp::CellLoad:    return cell_values[(uint16_t)n.imm];
        case NodeOp::InputLoad:   return hw_inputs[(uint16_t)n.imm];
        case NodeOp::PrevOutputLoad:
            return prev_output_values[(uint16_t)n.imm];
        case NodeOp::Add:  return a + b;
        case NodeOp::Sub:  return a - b;
        case NodeOp::Mul:  return a * b;
        case NodeOp::Div:  return (b != 0.0) ? a / b : 0.0;
        case NodeOp::Mod:  return (b != 0.0) ? fmod(a, b) : 0.0;
        case NodeOp::Expt: return pow(a, b);
        case NodeOp::Min:  return (a < b) ? a : b;
        case NodeOp::Max:  return (a > b) ? a : b;

        case NodeOp::Neg:   return -a;
        case NodeOp::Abs:   return fabs(a);
        case NodeOp::Floor: return floor(a);
        case NodeOp::Ceil:  return ceil(a);
        case NodeOp::Frac:  return a - floor(a);
        case NodeOp::Sqrt:  return sqrt(fabs(a));
        case NodeOp::Clamp: return (a < b) ? b : (a > c) ? c : a;

        case NodeOp::Sin:  return sin(a);
        case NodeOp::Cos:  return cos(a);
        case NodeOp::Tan:  return tan(a);

        case NodeOp::USin:   return (sin(a * 2.0 * M_PI) + 1.0) * 0.5;
        case NodeOp::UCos:   return (cos(a * 2.0 * M_PI) + 1.0) * 0.5;
        case NodeOp::Tri:    return 1.0 - fabs(2.0 * (a - floor(a)) - 1.0);
        case NodeOp::Sqr:    return ((a - floor(a)) < 0.5) ? 1.0 : 0.0;
        case NodeOp::Pulse:  return ((a - floor(a)) < b) ? 1.0 : 0.0;

        case NodeOp::CmpGt: return (a > b)  ? 1.0 : 0.0;
        case NodeOp::CmpLt: return (a < b)  ? 1.0 : 0.0;
        case NodeOp::CmpGe: return (a >= b) ? 1.0 : 0.0;
        case NodeOp::CmpLe: return (a <= b) ? 1.0 : 0.0;
        case NodeOp::CmpEq: return (a == b) ? 1.0 : 0.0;

        case NodeOp::Not: return (a == 0.0) ? 1.0 : 0.0;
        case NodeOp::And: return (a != 0.0 && b != 0.0) ? 1.0 : 0.0;
        case NodeOp::Or:  return (a != 0.0 || b != 0.0) ? 1.0 : 0.0;

        case NodeOp::Select: return (a != 0.0) ? b : c;

        case NodeOp::BiToUni: return (a + 1.0) * 0.5;
        case NodeOp::UniToBi: return a * 2.0 - 1.0;
        case NodeOp::Lerp:    return a + (b - a) * c;
        case NodeOp::Scale:   return c * (b - a) + a;
        case NodeOp::VecIndex: {
            uint16_t tid = (uint16_t)n.imm;
            uint16_t off = data_offsets[tid];
            uint16_t len = data_lengths[tid];
            if (len == 0) return 0.0;
            int index = ((int)floor(a) % len + len) % len;
            return data_pool[off + index];
        }
        case NodeOp::VecLerp: {
            uint16_t tid = (uint16_t)n.imm;
            uint16_t off = data_offsets[tid];
            uint16_t len = data_lengths[tid];
            if (len <= 1) return (len == 1) ? data_pool[off] : 0.0;
            double scaled = a * (len - 1);
            int i0 = (int)floor(scaled);
            if (i0 < 0) i0 = 0;
            if (i0 >= len - 1) i0 = len - 2;
            int i1 = i0 + 1;
            double frac = scaled - floor(scaled);
            return data_pool[off + i0] + (data_pool[off + i1] - data_pool[off + i0]) * frac;
        }

        case NodeOp::HashIndex: {
            // Deterministic hash matching old simple_hashing_function
            uint32_t v = (uint32_t)(int32_t)a;
            v = ((v >> 16) ^ v) * 0x45d9f3bu;
            v = ((v >> 16) ^ v) * 0x45d9f3bu;
            v = (v >> 16) ^ v;
            return (double)(v & 0x7fffffffu) / (double)0x7fffffffu;
        }

        case NodeOp::SlotLoad: return 0.0; // handled in execution loop

        default: return 0.0;
    }
}

// ── Single-Sample Execution ─────────────────────────────────────────────────

void execute_all_outputs(const NodePool& pool, ExecutionContext& ctx) {
    for (uint16_t i = 0; i < pool.exec_count; i++) {
        uint16_t idx = pool.exec_order[i];
        const Node& n = pool.nodes[idx];

        double result;
        if (n.op == NodeOp::SlotLoad) {
            uint16_t slot_idx = (uint16_t)n.imm;
            result = (slot_idx < pool.live_slot_count)
                ? pool.live_slots[slot_idx].value : 0.0;
        } else {
            double a = (n.input_a != NODE_NONE) ? ctx.workspace[n.input_a] : 0.0;
            double b = (n.input_b != NODE_NONE) ? ctx.workspace[n.input_b] : 0.0;
            double c = (n.input_c != NODE_NONE) ? ctx.workspace[n.input_c] : 0.0;

            result = eval_node(n, a, b, c, ctx.t, ctx.dt,
                               ctx.cell_values, ctx.hw_inputs,
                               ctx.data_pool, ctx.data_offsets, ctx.data_lengths,
                               ctx.prev_outputs, pool.state_values);
        }

        // NaN/Inf guard
        if (!std::isfinite(result)) result = 0.0;

        ctx.workspace[idx] = result;
    }

    // Read output values with LKG fallback
    for (uint16_t i = 0; i < MAX_OUTPUTS; i++) {
        if (pool.outputs[i].root_node != NODE_NONE) {
            ctx.output_values[i] = ctx.workspace[pool.outputs[i].root_node];
        } else if (pool.outputs[i].valid) {
            // No graph assigned but we have a last-known-good value — use it
            ctx.output_values[i] = pool.outputs[i].lkg_value;
        }
        // else: output was never assigned, leave at caller's init (typically 0)
    }
}

// ── Post-Tick Commit ───────────────────────────────────────────────────────

void commit_outputs(NodePool& pool, const double* output_values) {
    for (uint16_t i = 0; i < MAX_OUTPUTS; i++) {
        pool.prev_output_values[i] = output_values[i];
        if (pool.outputs[i].root_node != NODE_NONE) {
            pool.outputs[i].lkg_value = output_values[i];
            pool.outputs[i].valid = true;
        }
    }
}

// ── Post-Tick State Commit ────────────────────────────────────────────────

void commit_state(NodePool& pool, const double* workspace) {
    for (uint16_t s = 0; s < pool.state_slot_count; ++s) {
        if (pool.state_update_roots[s] != NODE_NONE) {
            pool.state_values[s] = workspace[pool.state_update_roots[s]];
        }
    }
}

// Legacy 10-parameter overload

void execute_all_outputs(
    const NodePool& pool,
    double t,
    const double* cell_values,
    const double* hw_inputs,
    const double* data_pool,
    const uint16_t* data_offsets,
    const uint16_t* data_lengths,
    const double* prev_output_values,
    double* output_values,
    double* node_values
) {
    ExecutionContext ctx;
    ctx.t             = t;
    ctx.dt            = 0.0;
    ctx.cell_values   = cell_values;
    ctx.hw_inputs     = hw_inputs;
    ctx.data_pool     = data_pool;
    ctx.data_offsets  = data_offsets;
    ctx.data_lengths  = data_lengths;
    ctx.prev_outputs  = prev_output_values;
    ctx.output_values = output_values;
    ctx.workspace     = node_values;
    execute_all_outputs(pool, ctx);
}

// ── Batched Execution ───────────────────────────────────────────────────────

void execute_batch(
    const NodePool& pool,
    const double* t_array,
    size_t sample_count,
    const double* cell_values,
    const double* hw_inputs,
    const double* data_pool,
    const uint16_t* data_offsets,
    const uint16_t* data_lengths,
    double* output_buffer,
    uint16_t num_outputs
) {
    if (!pool.batch_workspace) return;

    const size_t CHUNK = pool.batch_chunk_size;
    double* regs = pool.batch_workspace.get();

    for (size_t chunk_start = 0; chunk_start < sample_count; chunk_start += CHUNK) {
        size_t chunk_size = std::min(CHUNK, sample_count - chunk_start);

        for (uint16_t ni = 0; ni < pool.exec_count; ni++) {
            uint16_t idx = pool.exec_order[ni];
            const Node& n = pool.nodes[idx];
            double* reg_out = regs + (size_t)idx * CHUNK;

            if (n.op == NodeOp::SlotLoad) {
                uint16_t slot_idx = (uint16_t)n.imm;
                double val = (slot_idx < pool.live_slot_count)
                    ? pool.live_slots[slot_idx].value : 0.0;
                for (size_t s = 0; s < chunk_size; s++) reg_out[s] = val;
            } else if (n.flags & FLAG_TIME_INVARIANT) {
                // Compute once, broadcast.
                // Read input values from registers — time-invariant inputs
                // were already computed and are the same for every sample,
                // so reading at index 0 is sufficient.
                double a = (n.input_a != NODE_NONE) ? regs[(size_t)n.input_a * CHUNK] : 0.0;
                double b = (n.input_b != NODE_NONE) ? regs[(size_t)n.input_b * CHUNK] : 0.0;
                double c = (n.input_c != NODE_NONE) ? regs[(size_t)n.input_c * CHUNK] : 0.0;
                double val = eval_node(n, a, b, c, 0.0, 0.0,
                                       cell_values, hw_inputs,
                                       data_pool, data_offsets, data_lengths,
                                       pool.prev_output_values,
                                       pool.state_values);
                if (!std::isfinite(val)) val = 0.0;
                for (size_t s = 0; s < chunk_size; s++) reg_out[s] = val;
            } else {
                for (size_t s = 0; s < chunk_size; s++) {
                    double t = t_array[chunk_start + s];

                    double a = (n.input_a != NODE_NONE) ? regs[(size_t)n.input_a * CHUNK + s] : 0.0;
                    double b = (n.input_b != NODE_NONE) ? regs[(size_t)n.input_b * CHUNK + s] : 0.0;
                    double c = (n.input_c != NODE_NONE) ? regs[(size_t)n.input_c * CHUNK + s] : 0.0;

                    double result = eval_node(n, a, b, c, t, 0.0,
                                              cell_values, hw_inputs,
                                              data_pool, data_offsets, data_lengths,
                                              pool.prev_output_values,
                                              pool.state_values);
                    if (!std::isfinite(result)) result = 0.0;
                    reg_out[s] = result;
                }
            }
        }

        // Copy output values for this chunk
        uint16_t out_idx = 0;
        for (uint16_t o = 0; o < MAX_OUTPUTS && out_idx < num_outputs; o++) {
            if (pool.outputs[o].root_node != NODE_NONE) {
                double* src = regs + (size_t)pool.outputs[o].root_node * CHUNK;
                double* dst = output_buffer + (size_t)out_idx * sample_count + chunk_start;
                memcpy(dst, src, chunk_size * sizeof(double));
                out_idx++;
            }
        }
    }
}

// ── Output Classification ───────────────────────────────────────────────────

struct ClassifyResult {
    bool has_state;      // LoadState, LoadDt, or PrevOutputLoad
    bool has_input;      // InputLoad
    uint32_t input_mask; // bitmask of hw input channels
};

static void classify_node_tree(const NodePool& pool, uint16_t root, ClassifyResult& result) {
    if (root == NODE_NONE || root >= pool.node_count) return;

    bool visited[MAX_TOTAL_NODES] = {};
    uint16_t stack[MAX_TOTAL_NODES];
    uint16_t sp = 0;
    stack[sp++] = root;

    while (sp > 0) {
        uint16_t idx = stack[--sp];
        if (idx == NODE_NONE || idx >= pool.node_count) continue;
        if (visited[idx]) continue;
        visited[idx] = true;

        const Node& n = pool.nodes[idx];
        switch (n.op) {
            case NodeOp::LoadState:
            case NodeOp::LoadDt:
            case NodeOp::PrevOutputLoad:
                result.has_state = true;
                break;
            case NodeOp::InputLoad:
                result.has_input = true;
                if ((uint16_t)n.imm < 32)
                    result.input_mask |= (1u << (uint16_t)n.imm);
                break;
            default:
                break;
        }

        // Check visited before pushing to avoid stack overflow in dense DAGs
        if (n.input_a != NODE_NONE && !visited[n.input_a] && sp < MAX_TOTAL_NODES) stack[sp++] = n.input_a;
        if (n.input_b != NODE_NONE && !visited[n.input_b] && sp < MAX_TOTAL_NODES) stack[sp++] = n.input_b;
        if (n.input_c != NODE_NONE && !visited[n.input_c] && sp < MAX_TOTAL_NODES) stack[sp++] = n.input_c;
    }
}

void classify_outputs(NodePool& pool) {
    // First pass: classify each output by its own node tree only
    for (uint16_t i = 0; i < MAX_OUTPUTS; i++) {
        if (!pool.outputs[i].valid || pool.outputs[i].root_node == NODE_NONE) {
            pool.output_class[i] = OutputClass::Inactive;
            pool.output_input_mask[i] = 0;
            continue;
        }

        ClassifyResult cr = {};
        classify_node_tree(pool, pool.outputs[i].root_node, cr);

        if (cr.has_state) {
            pool.output_class[i] = OutputClass::Stateful;
        } else if (cr.has_input) {
            pool.output_class[i] = OutputClass::InputDep;
        } else {
            pool.output_class[i] = OutputClass::Pure;
        }
        pool.output_input_mask[i] = cr.input_mask;
    }

    // If any state slots exist, outputs that reference LoadState/LoadDt are
    // already marked Stateful. State update roots are part of the stateful
    // outputs' computation — they don't pollute pure outputs.
}

} // namespace sig
