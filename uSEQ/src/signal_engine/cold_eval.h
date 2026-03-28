#ifndef SIGNAL_ENGINE_COLD_EVAL_H
#define SIGNAL_ENGINE_COLD_EVAL_H

#include "types.h"
#include "cell_store.h"
#include "node_pool.h"
#include "diagnostics.h"

namespace sig {

// ── Eval Result ─────────────────────────────────────────────────────────────

struct EvalResult {
    enum Kind : uint8_t { Number, Text, DataRef, Ok, Error } kind = Ok;
    double number     = 0.0;
    const char* text  = nullptr;
    uint16_t text_length = 0;
    Diagnostic diagnostics[8] = {};
    uint8_t diagnostic_count  = 0;
};

// ── Cold-Path Evaluation ────────────────────────────────────────────────────
// Handles everything that isn't signal sampling: define, defn, set-bpm, etc.

EvalResult eval_cold(const char* source, uint32_t length,
                     CellStore& cells, SourceArena& arena, NodePool& pool);

// ── Dependency Tracking ─────────────────────────────────────────────────────
// When a cell changes, recompile outputs that depend on it.

void on_cell_changed(SymbolID cell_id, CellStore& cells,
                     SourceArena& arena, NodePool& pool);

// ── Output Source Storage ───────────────────────────────────────────────────
// Stores the source text for each output expression (needed for recompilation).

struct OutputSource {
    uint32_t arena_offset = 0;
    uint32_t arena_length = 0;
    bool has_source = false;
};

extern OutputSource output_sources[MAX_OUTPUTS];

} // namespace sig

#endif // SIGNAL_ENGINE_COLD_EVAL_H
