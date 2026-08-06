#ifndef SIGNAL_ENGINE_COMPILER_PIPELINE_H
#define SIGNAL_ENGINE_COMPILER_PIPELINE_H

#include "cell_store.h"
#include "diagnostics.h"
#include "state_registry.h"
#include "token.h"

namespace sig {

struct SignalEngine;

// Fixed-capacity parse IR. Tokenization writes directly into the retained
// stream, avoiding the old MAX_TOKENS temporary followed by a second copy.
// Diagnostics and token storage are bounded identically on every target.
struct ParsedProgram {
    static constexpr uint8_t MAX_PARSE_DIAGNOSTICS = 8;

    TokenStream stream = {};
    Diagnostic diagnostics[MAX_PARSE_DIAGNOSTICS] = {};
    uint8_t diagnostic_count = 0;

    void parse(const char* source, uint32_t length);
    bool ok() const { return diagnostic_count == 0; }
    bool empty() const { return stream.count == 0 || stream.at_end(); }
};

enum class SourcePlanStatus : uint8_t {
    NoSource,
    Ready,
    InvalidSpan,
    CapacityExceeded,
};

bool source_region_can_store(const SourceArena& arena,
                             uint32_t previous_offset,
                             uint32_t previous_length,
                             uint32_t new_length);

// Immutable plan for the source-arena part of a compiler publication. Span
// resolution and capacity validation happen before graph mutation; publish()
// is therefore the only write and has no policy decisions left to make.
struct SourceMutationPlan {
    SourcePlanStatus status = SourcePlanStatus::NoSource;
    const char* bytes = nullptr;
    uint32_t length = 0;
    uint32_t previous_offset = UINT32_MAX;
    uint32_t previous_length = 0;

    static SourceMutationPlan prepare(
        const SourceArena& arena,
        const char* source,
        uint32_t source_length,
        uint32_t span_start,
        uint32_t span_end,
        uint32_t previous_offset = UINT32_MAX,
        uint32_t previous_length = 0);

    bool has_source() const { return status == SourcePlanStatus::Ready; }
    uint32_t publish(SourceArena& arena) const;
};

// Bounded checkpoint for graph-building side effects. GraphBuilder interns
// directly into the live fixed-capacity pool, so candidate construction is a
// mutation stage, not a pure tree allocation. This transaction snapshots the
// mutable metadata and uses SignalEngine::scratch_pool for the fixed arrays.
// Unless accept() is called after publication, destruction restores the last
// published graph and capacity.
class GraphMutationTransaction {
public:
    GraphMutationTransaction() = default;
    explicit GraphMutationTransaction(SignalEngine& engine) { begin(engine); }
    ~GraphMutationTransaction();

    GraphMutationTransaction(const GraphMutationTransaction&) = delete;
    GraphMutationTransaction& operator=(const GraphMutationTransaction&) = delete;

    void begin(SignalEngine& engine);
    void accept() { active_ = false; }
    void rollback();
    bool active() const { return active_; }

private:
    SignalEngine* engine_ = nullptr;
    StateResourceRegistry registry_ = {};
    uint8_t data_table_count_ = 0;
    uint16_t state_slot_count_ = 0;
    uint16_t live_slot_count_ = 0;
    uint32_t arena_write_head_ = 0;
    bool active_ = false;
};

} // namespace sig

#endif // SIGNAL_ENGINE_COMPILER_PIPELINE_H
