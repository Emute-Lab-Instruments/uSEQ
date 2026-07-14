#ifndef SIGNAL_ENGINE_CELL_STORE_H
#define SIGNAL_ENGINE_CELL_STORE_H

#include "types.h"

namespace sig {

// ── Cell Table ──────────────────────────────────────────────────────────────
// Replaces Value (88 bytes) + Environment (string-keyed map) with
// Cell (16 bytes) + flat array indexed by SymbolID.

enum class CellKind : uint8_t {
    Empty,       // unused slot
    Number,      // double constant
    Data,        // numeric array (vectors, step patterns, scale tables)
    Callable,    // user function template (params + body token offset)
    Nil          // explicitly nil
};

struct Cell {
    CellKind kind       = CellKind::Empty;
    uint8_t flags       = 0;       // 0x01 = frozen
    uint16_t data_table_id = 0;    // for Data cells: index into shared data pool
    uint32_t revision   = 0;       // bumped on every change; dirty detection
    double value        = 0.0;     // Number: the value. Data: length as double.
};
// sizeof(Cell) == 16 bytes

struct CallableInfo {
    SymbolID params[MAX_CALLABLE_PARAMS] = {};
    uint8_t param_count = 0;
    uint8_t pad[3]      = {};
    uint32_t source_offset = 0;    // byte offset into source arena
    uint32_t source_length = 0;    // byte length of body source text
};
// sizeof(CallableInfo) == 24 bytes (with 4-byte SymbolID: 32+4+4+4 = 44... adjust)

// ── Source Arena ────────────────────────────────────────────────────────────
// Append-only string buffer for callable body text.

struct SourceArena {
    char data[SOURCE_ARENA_SIZE] = {};
    uint32_t write_head = 0;

    // Store source text, return offset. Returns UINT32_MAX on overflow.
    uint32_t store(const char* text, uint32_t length);

    // Read back source text.
    const char* read(uint32_t offset) const;

    // Reset the arena (e.g. after useq-clear).
    void reset();
};

// ── Cell Store ──────────────────────────────────────────────────────────────

struct CellStore {
    Cell cells[MAX_CELLS]               = {};
    CallableInfo callables[MAX_CELLS]   = {}; // parallel array; valid when kind==Callable

    // Store-wide revision (A12). Bumped whenever cell values may have changed
    // (every cold eval, timing init, flash load). Lets per-tick consumers skip
    // re-snapshotting all MAX_CELLS values when nothing changed — measured at
    // ~40% of the firmware engine tick. Overcounting (bumping without an
    // actual change) is safe; missing a bump is not, so bumps happen at the
    // coarse mutation entry points rather than per cell write.
    uint32_t store_revision = 1;

    // Shared data pool
    double data_pool[MAX_DATA_ENTRIES]    = {};
    uint16_t data_offsets[MAX_DATA_TABLES] = {};
    uint16_t data_lengths[MAX_DATA_TABLES] = {};
    uint8_t data_table_count              = 0;

    // Store a new data table. Returns table ID, or UINT8_MAX on overflow.
    uint16_t store_data_table(const double* values, uint16_t count);

    // Get data table pointer and length.
    const double* get_data_table(uint16_t table_id, uint16_t& out_length) const;

    // Snapshot cell numeric values for executor (copies cell[i].value for all).
    void snapshot_values(double* out, size_t max_count) const;

    // Convenience: initialise the four well-known timing cells.
    // bpm (default 120), beats-per-bar (default 4),
    // bars-per-phrase (default 4), phrases-per-section (default 4).
    void init_timing_defaults(double bpm = 120.0, int beats_per_bar = 4,
                              int bars_per_phrase = 4, int phrases_per_section = 4);
};

} // namespace sig

#endif // SIGNAL_ENGINE_CELL_STORE_H
