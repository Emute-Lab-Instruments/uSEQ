#include "cell_store.h"
#include "../modulisp/lisp/symbol_intern.h"
#include <cstring>

namespace sig {

// ── SourceArena ─────────────────────────────────────────────────────────────

uint32_t SourceArena::store(const char* text, uint32_t length) {
    if (write_head > SOURCE_ARENA_SIZE ||
        length > SOURCE_ARENA_SIZE - write_head) return UINT32_MAX;
    uint32_t offset = write_head;
    memcpy(data + write_head, text, length);
    write_head += length;
    return offset;
}

uint32_t SourceArena::store_reuse(uint32_t existing_offset,
                                  uint32_t existing_length,
                                  const char* text, uint32_t length) {
    // A source slot owns its whole previous region. A shorter replacement can
    // occupy that same region without consuming any more arena space.
    bool existing_region_fits =
        existing_offset <= SOURCE_ARENA_SIZE &&
        existing_length <= SOURCE_ARENA_SIZE - existing_offset;
    if (existing_region_fits && length <= existing_length) {
        if (length > 0) {
            // Recompilation can read the old source directly from the arena,
            // so use memmove for the same-region case as well.
            memmove(data + existing_offset, text, length);
        }
        return existing_offset;
    }

    return store(text, length);
}

const char* SourceArena::read(uint32_t offset) const {
    if (offset >= SOURCE_ARENA_SIZE) return nullptr;
    return data + offset;
}

void SourceArena::reset() {
    memset(data, 0, sizeof(data));
    write_head = 0;
}

// ── CellStore ───────────────────────────────────────────────────────────────

uint16_t CellStore::store_data_table(const double* values, uint16_t count) {
    // Content-intern: identical source text compiles to identical tables, and
    // tables are immutable after storage, so recompiles (on_cell_changed,
    // output reassign) must reuse the existing table instead of appending a
    // duplicate — otherwise every recompile of a program containing a vector
    // literal leaks a table until the pool is exhausted (MAX_DATA_TABLES is
    // 32 on firmware). Cold path only; linear scan over <= MAX_DATA_TABLES.
    for (uint16_t t = 0; t < data_table_count; t++) {
        if (data_lengths[t] != count) continue;
        if (memcmp(data_pool + data_offsets[t], values,
                   count * sizeof(double)) == 0) {
            return t;
        }
    }

    if (data_table_count >= MAX_DATA_TABLES) return UINT8_MAX;

    // Find where to append in the pool
    uint16_t pool_offset = 0;
    if (data_table_count > 0) {
        uint16_t last = data_table_count - 1;
        pool_offset = data_offsets[last] + data_lengths[last];
    }

    if (pool_offset + count > MAX_DATA_ENTRIES) return UINT8_MAX;

    uint16_t table_id = data_table_count;
    data_offsets[table_id] = pool_offset;
    data_lengths[table_id] = count;
    memcpy(data_pool + pool_offset, values, count * sizeof(double));
    data_table_count++;

    return table_id;
}

const double* CellStore::get_data_table(uint16_t table_id, uint16_t& out_length) const {
    if (table_id >= data_table_count) {
        out_length = 0;
        return nullptr;
    }
    out_length = data_lengths[table_id];
    return data_pool + data_offsets[table_id];
}

void CellStore::snapshot_values(double* out, size_t max_count) const {
    size_t n = max_count < MAX_CELLS ? max_count : MAX_CELLS;
    for (size_t i = 0; i < n; i++) {
        out[i] = cells[i].value;
    }
}

void CellStore::reset(double bpm, int beats_per_bar,
                      int bars_per_phrase, int phrases_per_section) {
    // Keep the monotonic store revision across resets so cached snapshots can
    // never mistake a freshly-cleared store for their previous generation.
    uint32_t next_revision = store_revision + 1;
    if (next_revision == 0) next_revision = 1;

    for (size_t i = 0; i < MAX_CELLS; i++) {
        cells[i] = Cell{};
        callables[i] = CallableInfo{};
    }
    memset(data_pool, 0, sizeof(data_pool));
    memset(data_offsets, 0, sizeof(data_offsets));
    memset(data_lengths, 0, sizeof(data_lengths));
    data_table_count = 0;
    store_revision = next_revision;

    init_timing_defaults(bpm, beats_per_bar, bars_per_phrase,
                         phrases_per_section);
}

void CellStore::init_timing_defaults(double bpm, int beats_per_bar,
                                     int bars_per_phrase, int phrases_per_section) {
    store_revision++; // A12: cell values change below
    auto& si = SymbolIntern::getInstance();

    SymbolID bpm_sym = si.intern("bpm");
    cells[bpm_sym].kind     = CellKind::Number;
    cells[bpm_sym].value    = bpm;
    cells[bpm_sym].revision = 1;

    SymbolID bpb_sym = si.intern("beats-per-bar");
    cells[bpb_sym].kind     = CellKind::Number;
    cells[bpb_sym].value    = (double)beats_per_bar;
    cells[bpb_sym].revision = 1;

    SymbolID bpp_sym = si.intern("bars-per-phrase");
    cells[bpp_sym].kind     = CellKind::Number;
    cells[bpp_sym].value    = (double)bars_per_phrase;
    cells[bpp_sym].revision = 1;

    SymbolID pps_sym = si.intern("phrases-per-section");
    cells[pps_sym].kind     = CellKind::Number;
    cells[pps_sym].value    = (double)phrases_per_section;
    cells[pps_sym].revision = 1;
}

} // namespace sig
