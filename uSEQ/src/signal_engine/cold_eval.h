#ifndef SIGNAL_ENGINE_COLD_EVAL_H
#define SIGNAL_ENGINE_COLD_EVAL_H

#include "types.h"
#include "cell_store.h"
#include "node_pool.h"
#include "state_registry.h"
#include "diagnostics.h"

namespace sig {

// ── Engine State ───────────────────────────────────────────────────────────
// Transport and timing state managed by cold-path commands.

struct EngineState {
    double time_offset   = 0.0;
    bool is_playing      = true;
    double current_time  = 0.0;
    double current_dt    = 0.0;
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

    void init_defaults(double bpm = 120.0, int beats_per_bar = 4,
                       int bars_per_phrase = 4, int phrases_per_section = 4);
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

} // namespace sig

#endif // SIGNAL_ENGINE_COLD_EVAL_H
