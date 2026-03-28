#include "cell_store.h"
#include <cstring>

namespace sig {

// ── SourceArena ─────────────────────────────────────────────────────────────

uint32_t SourceArena::store(const char* text, uint32_t length) {
    if (write_head + length > SOURCE_ARENA_SIZE) return UINT32_MAX;
    uint32_t offset = write_head;
    memcpy(data + write_head, text, length);
    write_head += length;
    return offset;
}

const char* SourceArena::read(uint32_t offset) const {
    if (offset >= SOURCE_ARENA_SIZE) return nullptr;
    return data + offset;
}

void SourceArena::reset() {
    write_head = 0;
}

// ── CellStore ───────────────────────────────────────────────────────────────

uint16_t CellStore::store_data_table(const double* values, uint16_t count) {
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

} // namespace sig
