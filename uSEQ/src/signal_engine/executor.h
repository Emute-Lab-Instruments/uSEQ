#ifndef SIGNAL_ENGINE_EXECUTOR_H
#define SIGNAL_ENGINE_EXECUTOR_H

#include "node_pool.h"
#include "cell_store.h"

namespace sig {

// ── Single-Sample Execution ─────────────────────────────────────────────────
// One forward pass through topologically-sorted nodes.

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
    double* node_values    // workspace [MAX_TOTAL_NODES], can be stack-allocated
);

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

} // namespace sig

#endif // SIGNAL_ENGINE_EXECUTOR_H
