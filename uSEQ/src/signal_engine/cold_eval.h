#ifndef SIGNAL_ENGINE_COLD_EVAL_H
#define SIGNAL_ENGINE_COLD_EVAL_H

#include "types.h"
#include "cell_store.h"
#include "node_pool.h"
#include "state_registry.h"
#include "synth_graph.h"
#include "diagnostics.h"

namespace sig {

// ── Engine State ───────────────────────────────────────────────────────────
// Transport and timing state managed by cold-path commands.

struct EngineState {
    double time_offset   = 0.0;
    // Internal transport-origin correction. Kept separate from the
    // user-visible time_offset reported over the wire.
    double transport_offset = 0.0;
    bool is_playing      = true;
    double current_time  = 0.0;
    double current_dt    = 0.0;
    double current_wall_time = 0.0;
    double paused_time   = 0.0;
    bool has_pause_anchor = false;
    bool reset_dt_on_next_tick = true;

    // Translate a host/hardware monotonic clock into session-logical time.
    // Paused time is an anchor, not a wall clock that continues invisibly.
    double logical_time(double wall_time) const {
        return (!is_playing && has_pause_anchor)
            ? paused_time : wall_time + time_offset + transport_offset;
    }

    void pause() {
        if (is_playing) {
            paused_time = current_time;
            has_pause_anchor = true;
        }
        is_playing = false;
        current_dt = 0.0;
    }

    void play() {
        if (!is_playing) {
            if (has_pause_anchor) {
                transport_offset =
                    paused_time - current_wall_time - time_offset;
                current_time = paused_time;
            }
            is_playing = true;
            reset_dt_on_next_tick = true;
            current_dt = 0.0;
        }
    }

    void rewind() {
        // At the current wall instant, wall + user offset + transport-origin
        // correction must equal logical zero.
        transport_offset = -current_wall_time - time_offset;
        current_time = 0.0;
        current_dt = 0.0;
        reset_dt_on_next_tick = true;
        if (!is_playing) {
            paused_time = 0.0;
            has_pause_anchor = true;
        }
    }

    void stop() {
        rewind();
        paused_time = 0.0;
        has_pause_anchor = true;
        is_playing = false;
    }
};

// ── Output Source Storage ───────────────────────────────────────────────────
// Stores the source text for each output expression (needed for recompilation).

struct OutputSource {
    uint32_t arena_offset = 0;
    uint32_t arena_length = 0;
    bool has_source = false;
};

// ── Eval Result ─────────────────────────────────────────────────────────────

struct EvalResult {
    enum Kind : uint8_t { Number, Text, DataRef, Ok, Error } kind = Ok;
    double number     = 0.0;
    const char* text  = nullptr;
    uint16_t text_length = 0;
    Diagnostic diagnostics[8] = {};
    uint8_t diagnostic_count  = 0;
};

// ── SignalEngine ────────────────────────────────────────────────────────────
// Bundles all persistent engine state that was previously spread across
// separate globals and local variables.

// ── State Update Source ─────────────────────────────────────────────────────
// Stores the source text and dependency info for a state update expression
// (needed for recompilation when cells change).

struct StateUpdateSource {
    uint32_t arena_offset = 0;
    uint32_t arena_length = 0;
    bool has_source = false;
    SymbolID dep_cells[MAX_OUTPUT_DEPS] = {};
    uint8_t dep_count = 0;
};

struct SignalEngine {
    CellStore cells;
    SourceArena arena;
    NodePool pool;
    EngineState state;
    OutputSource output_sources[MAX_OUTPUTS] = {};
    StateUpdateSource state_sources[MAX_STATE_SLOTS] = {};

    StateResourceRegistry registry;
    NodePool scratch_pool;
    char eval_text_buf[512] = {};

    // ── Synth compiler domain (synth-nodes.md) ──────────────────────────
    // Published synth artefacts: identity-keyed declarations + control
    // channel table, sharing one compiler revision. Each top-level synth form
    // advances atomically; earlier forms survive a later sibling's failure.
    SynthGraph synth_graph;

    // Pending wrapper-injected state identity (state-identity.md §2.2:
    // `with-state-id` and `:id` normalise to the same internal identity
    // annotation). Set by the cold-path `with-state-id` handler before it
    // evaluates its wrapped form, consumed first-synth-wins by do_synth
    // when the form carries no explicit :name/:id, and always restored
    // when the wrapper handler returns. Never inspected on the hot path.
    char pending_state_identity[MAX_SYNTH_IDENTITY] = {};
    bool has_pending_state_identity = false;

    // Ordinal of anonymous synth declarations within the current cold
    // eval, reset at eval_cold entry. The anonymous fallback identity is
    // "::anon-<ordinal>" so recompiling the same program reuses its
    // identity instead of leaking one per eval (state-identity.md §2.5).
    uint16_t eval_anon_synth_ordinal = 0;

    // Increments on each user-visible full-session clear. Wrappers with
    // compiler caches use this to discard references outside SignalEngine.
    uint32_t session_generation = 0;

    void init_defaults(double bpm = 120.0, int beats_per_bar = 4,
                       int bars_per_phrase = 4, int phrases_per_section = 4);

    // Reset all compiler/runtime storage owned by a livecoding session while
    // leaving transport state and any persistent flash image untouched.
    void reset_session_storage(double bpm = 120.0, int beats_per_bar = 4,
                               int bars_per_phrase = 4,
                               int phrases_per_section = 4,
                               bool publish_synth_clear = true);
};

// ── Cold-Path Evaluation ────────────────────────────────────────────────────
// Handles everything that isn't signal sampling: define, defn, set-bpm, etc.

EvalResult eval_cold(const char* source, uint32_t length, SignalEngine& engine);

// ── Top-Level Expression Evaluation ────────────────────────────────────────
// Compile and execute a signal expression in scratch isolation. Used for
// top-level queries (bare symbols, unknown forms, vector eval). Does not
// commit state or mutate live output programs.

EvalResult eval_expression(const char* source, uint32_t length,
                           SignalEngine& engine);

// ── Bulk Recompilation ──────────────────────────────────────────────────────
// Recompile all outputs that have stored source text.  Used after flash load
// to rebuild signal graphs from persisted source.
//
// Idempotent: the node pool uses hash-consing (CSE), so recompiling the same
// expression twice yields the same node indices.  A gc pass after rebuild
// reclaims any stale nodes left from a previous compilation.

void recompile_all_outputs(SignalEngine& engine);

// ── Dependency Tracking ─────────────────────────────────────────────────────
// When a cell changes, recompile outputs that depend on it.

void on_cell_changed(SymbolID cell_id, SignalEngine& engine);

// ── Synth GC integration ────────────────────────────────────────────────────
// Register every committed synth control root with the NodePool's external
// roots array so the next GC pass keeps them reachable and remaps their
// indices. Callers that invoke pool.gc_unreachable_nodes() directly must
// call this first. Safe to call repeatedly; clears and re-registers each
// time so stale indices do not linger after a synth-graph edit.

void register_synth_external_roots(SignalEngine& engine);

// After a GC pass, copy the remapped external root indices back into the
// synth_graph.controls[] table. Pairs with register_synth_external_roots().

void commit_synth_external_roots(SignalEngine& engine);

} // namespace sig

#endif // SIGNAL_ENGINE_COLD_EVAL_H
