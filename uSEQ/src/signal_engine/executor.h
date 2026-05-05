#ifndef SIGNAL_ENGINE_EXECUTOR_H
#define SIGNAL_ENGINE_EXECUTOR_H

#include "node_pool.h"
#include "cell_store.h"

namespace sig {

// ── Execution Context ──────────────────────────────────────────────────────
// Bundles every per-tick datum the executor needs, replacing the previous
// 10-parameter execute_all_outputs signature.

struct ExecutionContext {
    double t;
    double dt;                       // time delta since last tick
    const double* cell_values;
    const double* hw_inputs;
    const double* data_pool;
    const uint16_t* data_offsets;
    const uint16_t* data_lengths;
    const double* prev_outputs;
    double* output_values;           // out  [MAX_OUTPUTS]
    double* workspace;               // scratch [MAX_TOTAL_NODES]
};

// ── Single-Sample Execution ─────────────────────────────────────────────────
// One forward pass through topologically-sorted nodes.
// Uses LKG fallback for outputs with no current graph but a valid previous value.

void execute_all_outputs(const NodePool& pool, ExecutionContext& ctx);

// ── Post-Tick Commit ────────────────────────────────────────────────────────
// After execute_all_outputs, call this to:
//   1. Copy output_values → pool.prev_output_values (for next tick's PrevOutputLoad)
//   2. Update lkg_value / valid on each active output slot
// This mutates pool state, so it is NOT used in the batch/visualization path.

void commit_outputs(NodePool& pool, const double* output_values);

// ── Post-Tick State Commit ─────────────────────────────────────────────────
// After execute_all_outputs, call this to update state slots from their
// update graphs.  State update roots must already have been executed as
// part of the node graph (they share the workspace).

void commit_state(NodePool& pool, const double* workspace);

// ── Batched Execution (WASM Visualization) ──────────────────────────────────
// SOA execution across a time window for efficient visualization.

void execute_batch(
    const NodePool& pool,
    const double* t_array,
    size_t sample_count,
    const double* cell_values,
    const double* hw_inputs,
    const double* data_pool,
    const uint16_t* data_offsets,
    const uint16_t* data_lengths,
    double* output_buffer,  // [num_outputs × sample_count], row-major
    uint16_t num_outputs
);

// ── Output Classification ───────────────────────────────────────────────────
// Walk each output's node graph to determine OutputClass and input dependency
// bitmask. Call after compilation or graph changes. Writes directly into
// pool.output_class[] and pool.output_input_mask[].

void classify_outputs(NodePool& pool);

} // namespace sig

#endif // SIGNAL_ENGINE_EXECUTOR_H
