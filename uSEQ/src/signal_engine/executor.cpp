#include "executor.h"
#include "eval_ops.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace sig {

// ── Failure Mode ────────────────────────────────────────────────────────────

static FailureMode g_failure_mode = FailureMode::LkgFallback;

void set_failure_mode(FailureMode mode) { g_failure_mode = mode; }
FailureMode get_failure_mode() { return g_failure_mode; }

// ── Single-node evaluation ──────────────────────────────────────────────────
// Load ops and data ops need runtime context and are handled here directly.
// Pure-math ops delegate to the shared eval_ops.h functions.

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
        // ── Load ops (runtime context required) ─────────────────────────
        case NodeOp::Const:       return n.imm;
        case NodeOp::RawTimeLoad: return t;
        case NodeOp::LoadState:   return state_values[(uint16_t)n.imm];
        case NodeOp::LoadDt:      return dt;
        case NodeOp::CellLoad:    return cell_values[(uint16_t)n.imm];
        case NodeOp::InputLoad:   return hw_inputs[(uint16_t)n.imm];
        case NodeOp::PrevOutputLoad:
            return prev_output_values[(uint16_t)n.imm];
        case NodeOp::SlotLoad: return 0.0; // handled in execution loop

        // ── Data ops (need data_pool arrays) ────────────────────────────
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
            // frac relative to the (possibly clamped) i0, not floor(scaled).
            // At integral phase == 1.0, scaled == len-1 and i0 is clamped to
            // len-2, so frac == 1.0 and we correctly return the LAST element
            // (data[len-1]); using floor(scaled) here yielded frac == 0 and the
            // wrong element (data[len-2]). Also gracefully clamps phase > 1.
            double frac = scaled - i0;
            if (frac < 0.0) frac = 0.0;
            if (frac > 1.0) frac = 1.0;
            return data_pool[off + i0] + (data_pool[off + i1] - data_pool[off + i0]) * frac;
        }

        // ── Pure-math ops (shared with constant folding) ────────────────
        // Unary
        case NodeOp::Neg:
        case NodeOp::Abs:
        case NodeOp::Floor:
        case NodeOp::Ceil:
        case NodeOp::Frac:
        case NodeOp::Sqrt:
        case NodeOp::Sin:
        case NodeOp::Cos:
        case NodeOp::Tan:
        case NodeOp::USin:
        case NodeOp::UCos:
        case NodeOp::Tri:
        case NodeOp::Sqr:
        case NodeOp::Not:
        case NodeOp::BiToUni:
        case NodeOp::UniToBi:
        case NodeOp::HashIndex:
            return eval_unary_op(n.op, a);

        // Binary
        case NodeOp::Add:
        case NodeOp::Sub:
        case NodeOp::Mul:
        case NodeOp::Div:
        case NodeOp::Mod:
        case NodeOp::Expt:
        case NodeOp::Min:
        case NodeOp::Max:
        case NodeOp::Pulse:
        case NodeOp::CmpGt:
        case NodeOp::CmpLt:
        case NodeOp::CmpGe:
        case NodeOp::CmpLe:
        case NodeOp::CmpEq:
        case NodeOp::And:
        case NodeOp::Or:
            return eval_binary_op(n.op, a, b);

        // Ternary
        case NodeOp::Clamp:
        case NodeOp::Lerp:
        case NodeOp::Scale:
        case NodeOp::Select:
            return eval_ternary_op(n.op, a, b, c);

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

        // Per-node NaN/Inf guard — legacy ZeroSquash mode only. In
        // LkgFallback mode non-finite values propagate to the output root,
        // where the LKG substitution below declares the failure
        // (failure-model.md §3.1).
        if (g_failure_mode == FailureMode::ZeroSquash && !std::isfinite(result))
            result = 0.0;

        ctx.workspace[idx] = result;
    }

    // Read output values with LKG fallback
    uint64_t fallback_mask = 0;
    for (uint16_t i = 0; i < MAX_OUTPUTS; i++) {
        if (pool.outputs[i].root_node != NODE_NONE) {
            double v = ctx.workspace[pool.outputs[i].root_node];
            if (g_failure_mode == FailureMode::LkgFallback &&
                !std::isfinite(v)) {
                // Non-finite at the root: substitute the last-known-good
                // value (or the neutral default when no LKG exists —
                // failure-model.md §2.4) and record the fallback.
                v = pool.outputs[i].valid ? pool.outputs[i].lkg_value : 0.0;
                fallback_mask |= (uint64_t)1 << i;
            }
            ctx.output_values[i] = v;
        } else if (pool.outputs[i].valid) {
            // No graph assigned but we have a last-known-good value — use it
            ctx.output_values[i] = pool.outputs[i].lkg_value;
        }
        // else: output was never assigned, leave at caller's init (typically 0)
    }
    pool.runtime_fallback_mask = fallback_mask;
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
            double v = workspace[pool.state_update_roots[s]];
            // Never commit non-finite state: in LkgFallback mode NaN/Inf can
            // flow through the workspace, and a poisoned state slot would
            // never recover. Keep the previous (finite) value instead.
            if (std::isfinite(v)) pool.state_values[s] = v;
        }
    }
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
    uint64_t fallback_mask = 0;

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
                if (g_failure_mode == FailureMode::ZeroSquash &&
                    !std::isfinite(val)) val = 0.0;
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
                    if (g_failure_mode == FailureMode::ZeroSquash &&
                        !std::isfinite(result)) result = 0.0;
                    reg_out[s] = result;
                }
            }
        }

        // Copy output values for this chunk.
        // Row-packing MUST match how callers count active outputs and build
        // their index_to_row map. Callers (wasm_wrapper.cpp) and the other
        // batch paths (execute_batch_sequential, project_from_fork) all key on
        // outputs[o].valid, so we do too. Keying on root_node != NODE_NONE here
        // diverged from that during the post-compile-fail window (valid==false
        // but root_node preserved), which mislabelled one output's samples as
        // another's. An output with valid && root_node==NODE_NONE holds only an
        // LKG scalar and has no register row, so fall back to its lkg_value.
        uint16_t out_idx = 0;
        for (uint16_t o = 0; o < MAX_OUTPUTS && out_idx < num_outputs; o++) {
            if (pool.outputs[o].valid) {
                double* dst = output_buffer + (size_t)out_idx * sample_count + chunk_start;
                if (pool.outputs[o].root_node != NODE_NONE) {
                    double* src = regs + (size_t)pool.outputs[o].root_node * CHUNK;
                    if (g_failure_mode == FailureMode::LkgFallback) {
                        // Non-finite at the root → substitute LKG per sample
                        // and record the fallback (failure-model.md §10.2).
                        double lkg = pool.outputs[o].lkg_value;
                        for (size_t s = 0; s < chunk_size; s++) {
                            double v = src[s];
                            if (!std::isfinite(v)) {
                                v = lkg;
                                fallback_mask |= (uint64_t)1 << o;
                            }
                            dst[s] = v;
                        }
                    } else {
                        memcpy(dst, src, chunk_size * sizeof(double));
                    }
                } else {
                    double lkg = pool.outputs[o].lkg_value;
                    for (size_t s = 0; s < chunk_size; s++) dst[s] = lkg;
                }
                out_idx++;
            }
        }
    }
    pool.runtime_fallback_mask = fallback_mask;
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
