#ifndef SIGNAL_ENGINE_STATE_REGISTRY_H
#define SIGNAL_ENGINE_STATE_REGISTRY_H

#include "types.h"

namespace sig {

using StateID = SymbolID;

// ── Anonymous structural identity (state-identity.md §2.5) ─────────────────
// Stateful expressions without an explicit :id get a synthetic StateID derived
// from structural context: which program is being compiled (output index, or
// MAX_OUTPUTS + state slot for defstate update graphs) plus the ordinal
// position of the allocation within that compile. Compilation is
// deterministic over source text, so recompiling a program resolves to the
// same keys and REUSES its slots instead of leaking a fresh slot per
// recompile. Synthetic IDs live far above any interned SymbolID, so they can
// never collide with user-supplied :id symbols.
constexpr StateID  ANON_STATE_ID_BASE      = 0x80000000u;
constexpr uint16_t ANON_STATE_CONTEXT_NONE = 0xFFFF;

inline StateID make_anon_state_id(uint16_t context, uint16_t ordinal) {
    return ANON_STATE_ID_BASE | ((StateID)context << 12) | (StateID)ordinal;
}

enum class ResourceKind : uint8_t {
    OscillatorPhase,
    TriggerMemory,
    HeldValue,
    ToggleState,
    Counter,
    ResetLatch,
    SlewAccumulator,
    Integrator,
    OnePole,
    EnvelopeFollower,
    NoiseCounter,
};

struct StateResourceKey {
    StateID state_id;
    ResourceKind kind;
    uint8_t role;

    bool operator==(const StateResourceKey& other) const {
        return state_id == other.state_id &&
               kind == other.kind &&
               role == other.role;
    }
};

struct StateResourceEntry {
    StateResourceKey key;
    uint16_t slot_index;
    double init_value;
    bool active;
};

struct StateResourceRegistry {
    StateResourceEntry entries[MAX_STATE_SLOTS];
    uint16_t entry_count = 0;

    // Resolve a key to a dense state slot index. If the key already exists,
    // returns its slot and preserves accumulated state. If new, allocates a
    // fresh slot and writes init_value. Returns NODE_NONE on overflow.
    uint16_t resolve(const StateResourceKey& key, double init_value,
                     double* state_values, uint16_t& state_slot_count);

    // Mark all entries inactive. Used before recompilation — entries that
    // remain inactive after compilation are candidates for GC.
    void mark_all_inactive();

    // Full reset (useq-clear).
    void clear();
};

} // namespace sig

#endif // SIGNAL_ENGINE_STATE_REGISTRY_H
