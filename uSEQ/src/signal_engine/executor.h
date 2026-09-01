#ifndef SIGNAL_ENGINE_EXECUTOR_H
#define SIGNAL_ENGINE_EXECUTOR_H

#include "node_pool.h"
#include "cell_store.h"

namespace sig {

// ── Failure Mode (failure-model.md §2.1/§3.1) ───────────────────────────────
// Global runtime policy for non-finite values (NaN/Inf) during execution.
//
//   LkgFallback (default, spec-mandated): values propagate freely through
//     the node graph; a non-finite value reaching an OUTPUT ROOT makes that
//     output substitute its last-known-good value (or 0 if none) and marks
//     it as being in fallback (see NodePool::runtime_fallback_mask).
//   ZeroSquash (legacy): every node's result is clamped to 0.0 when
//     non-finite. No fallback, no diagnostic — pre-v1.2 behaviour.
//
// The mode is engine-global (not per-output); configurable over the serial
// wire protocol ("set-failure-mode") and the WASM export
// useq_set_failure_mode(). See docs/specs/failure-model.md §3.

enum class FailureMode : uint8_t {
    LkgFallback = 0,
    ZeroSquash  = 1,
};

void set_failure_mode(FailureMode mode);
FailureMode get_failure_mode();

// Output assignment and last-good existence are independent.  In
// particular, an active program whose first sample is non-finite is Error,
// while the same failure after a finite commit is Fallback.
enum class OutputHealth : uint8_t {
    Idle,
    Running,
    Fallback,
    Error,
};

OutputHealth output_health(const NodePool& pool, uint16_t output_index);
const char* output_health_to_cstr(OutputHealth health);

// ── Execution Context ──────────────────────────────────────────────────────
// Bundles every per-tick datum the executor needs, replacing the previous
// 10-parameter execute_all_outputs signature.

struct ExecutionContext {
    Sample t;
    Sample dt;                       // time delta since last tick
    const Sample* cell_values;
    const Sample* hw_inputs;
    const Sample* data_pool;
    const uint16_t* data_offsets;
    const uint16_t* data_lengths;
    const Sample* prev_outputs;
    Sample* output_values;           // out  [MAX_OUTPUTS]
    Sample* workspace;               // scratch [MAX_TOTAL_NODES]
    Sample ground_t = 0.0;           // monotonic time since adapter/session boot
    const Sample* state_values = nullptr; // optional immutable state snapshot
    bool advance_state = true;       // false on a first/discontinuous tick
};

// ── Single-Sample Execution ─────────────────────────────────────────────────
// One forward pass through topologically-sorted nodes.
// Uses LKG fallback for outputs with a current graph and a finite committed
// previous value; a first-ever failure emits neutral zero and remains Error.

void execute_all_outputs(const NodePool& pool, ExecutionContext& ctx);

// ── Authoritative Tick Transition ─────────────────────────────────────────
// Stateful update expressions read one immutable previous-state snapshot.
// Output expressions then read the simultaneous finite next-state candidates,
// as required by state.md §2.3-2.4.  Adapters may inspect other published
// roots between prepare_tick() and commit_tick() (notably synth controls), but
// must commit the returned transition exactly once for an authoritative tick.

struct TickTransition {
    Sample state_values[MAX_STATE_SLOTS] = {};
    uint64_t start_mask = 0;
    uint64_t attempted_mask = 0;
    uint64_t failure_mask = 0;
};

void prepare_tick(const NodePool& pool, ExecutionContext& ctx,
                  TickTransition& transition);

void commit_tick(NodePool& pool, const ExecutionContext& ctx,
                 const TickTransition& transition,
                 const uint16_t* failed_owner_contexts = nullptr,
                 uint16_t failed_owner_count = 0);

// ── Post-Tick Commit ────────────────────────────────────────────────────────
// After execute_all_outputs, call this to:
//   1. Copy output_values → pool.prev_output_values (for next tick's PrevOutputLoad)
//   2. Promote only finite, non-substituted roots to lkg_value / has_lkg
// This mutates pool state, so it is NOT used in the batch/visualization path.

void commit_outputs(NodePool& pool, const Sample* output_values);

// ── Post-Tick State Commit ─────────────────────────────────────────────────
// After execute_all_outputs, call this to update state slots from their
// update graphs.  State update roots must already have been executed as
// part of the node graph (they share the workspace). Non-finite candidates
// retain the previous value and publish state_update_failure_mask until a
// later finite candidate clears it.

void commit_state(NodePool& pool, const Sample* workspace,
                  const uint16_t* failed_owner_contexts = nullptr,
                  uint16_t failed_owner_count = 0);

// ── Batched Execution (WASM Visualization) ──────────────────────────────────
// SOA execution across a time window for efficient visualization.

void execute_batch(
    const NodePool& pool,
    const Sample* t_array,
    size_t sample_count,
    const Sample* cell_values,
    const Sample* hw_inputs,
    const Sample* data_pool,
    const uint16_t* data_offsets,
    const uint16_t* data_lengths,
    Sample* output_buffer,  // [num_outputs × sample_count], row-major
    uint16_t num_outputs
);

// ── Output Classification ───────────────────────────────────────────────────
// Walk each output's node graph to determine OutputClass and input dependency
// bitmask. Call after compilation or graph changes. Writes directly into
// pool.output_class[] and pool.output_input_mask[].

void classify_outputs(NodePool& pool);

} // namespace sig

#endif // SIGNAL_ENGINE_EXECUTOR_H
