#include "compiler_pipeline.h"

#include "cold_eval.h"
#include "executor.h"

#include <cstring>

namespace sig {

bool source_region_can_store(const SourceArena& arena,
                             uint32_t previous_offset,
                             uint32_t previous_length,
                             uint32_t new_length) {
    const bool existing_region_fits =
        previous_offset <= SOURCE_ARENA_SIZE &&
        previous_length <= SOURCE_ARENA_SIZE - previous_offset;
    if (existing_region_fits && new_length <= previous_length) return true;
    return arena.write_head <= SOURCE_ARENA_SIZE &&
           new_length <= SOURCE_ARENA_SIZE - arena.write_head;
}

void ParsedProgram::parse(const char* source, uint32_t length) {
    stream.count = 0;
    stream.pos = 0;
    diagnostic_count = 0;
    stream.count = TokenStream::tokenize(
        source, length, stream.tokens, MAX_TOKENS,
        diagnostics, &diagnostic_count);
    stream.pos = 0;
}

SourceMutationPlan SourceMutationPlan::prepare(
        const SourceArena& arena,
        const char* source,
        uint32_t source_length,
        uint32_t span_start,
        uint32_t span_end,
        uint32_t previous_offset,
        uint32_t previous_length) {
    SourceMutationPlan plan;
    plan.previous_offset = previous_offset;
    plan.previous_length = previous_length;

    if (!source) return plan;
    if (span_end <= span_start || span_start > source_length ||
        span_end > source_length) {
        plan.status = SourcePlanStatus::InvalidSpan;
        return plan;
    }

    plan.bytes = source + span_start;
    plan.length = span_end - span_start;

    plan.status = source_region_can_store(
                      arena, previous_offset, previous_length, plan.length)
        ? SourcePlanStatus::Ready
        : SourcePlanStatus::CapacityExceeded;
    return plan;
}

uint32_t SourceMutationPlan::publish(SourceArena& arena) const {
    if (status != SourcePlanStatus::Ready) return UINT32_MAX;
    return arena.store_reuse(
        previous_offset, previous_length, bytes, length);
}

GraphMutationTransaction::~GraphMutationTransaction() {
    rollback();
}

void GraphMutationTransaction::begin(SignalEngine& engine) {
    if (active_) rollback();
    engine_ = &engine;
    registry_ = engine.registry;
    data_table_count_ = engine.cells.data_table_count;
    state_slot_count_ = engine.pool.state_slot_count;
    live_slot_count_ = engine.pool.live_slot_count;
    arena_write_head_ = engine.arena.write_head;

    std::memcpy(engine.scratch_pool.state_values, engine.pool.state_values,
                sizeof(engine.pool.state_values));
    std::memcpy(engine.scratch_pool.state_observed,
                engine.pool.state_observed,
                sizeof(engine.pool.state_observed));
    std::memcpy(engine.scratch_pool.state_update_roots,
                engine.pool.state_update_roots,
                sizeof(engine.pool.state_update_roots));
    std::memcpy(engine.scratch_pool.state_owner_context,
                engine.pool.state_owner_context,
                sizeof(engine.pool.state_owner_context));
    std::memcpy(engine.scratch_pool.live_slots, engine.pool.live_slots,
                sizeof(engine.pool.live_slots));
    active_ = true;
}

void GraphMutationTransaction::rollback() {
    if (!active_ || !engine_) return;
    SignalEngine& engine = *engine_;
    engine.registry = registry_;
    engine.cells.data_table_count = data_table_count_;
    engine.pool.state_slot_count = state_slot_count_;
    engine.pool.live_slot_count = live_slot_count_;
    engine.arena.write_head = arena_write_head_;
    std::memcpy(engine.pool.state_values, engine.scratch_pool.state_values,
                sizeof(engine.pool.state_values));
    std::memcpy(engine.pool.state_observed,
                engine.scratch_pool.state_observed,
                sizeof(engine.pool.state_observed));
    std::memcpy(engine.pool.state_update_roots,
                engine.scratch_pool.state_update_roots,
                sizeof(engine.pool.state_update_roots));
    std::memcpy(engine.pool.state_owner_context,
                engine.scratch_pool.state_owner_context,
                sizeof(engine.pool.state_owner_context));
    std::memcpy(engine.pool.live_slots, engine.scratch_pool.live_slots,
                sizeof(engine.pool.live_slots));

#if USEQ_HAS_SYNTH_ENGINE
    register_synth_external_roots(engine);
#endif
    engine.pool.gc_unreachable_nodes();
#if USEQ_HAS_SYNTH_ENGINE
    commit_synth_external_roots(engine);
#endif
    engine.pool.rebuild_execution_order();
    classify_outputs(engine.pool);

    active_ = false;
}

} // namespace sig
