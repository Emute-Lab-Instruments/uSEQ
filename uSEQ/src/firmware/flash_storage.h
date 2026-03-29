#ifndef FIRMWARE_FLASH_STORAGE_H
#define FIRMWARE_FLASH_STORAGE_H

// Save and load signal engine state to on-chip flash (RP2040)
// or to a flat buffer via IStorage (desktop testing).

#include "../signal_engine/cold_eval.h"

#ifndef ARDUINO
#include "../ports/IStorage.h"
#endif

namespace firmware {

// ── Binary Format ──────────────────────────────────────────────────────────
// Magic: "uSEQ\0" (5 bytes)
// Version: uint16_t
// Cell count: uint16_t
// Output count: uint16_t
// Data size: uint32_t (total bytes after header, before checksum)
// Checksum: uint32_t (CRC32 of everything after the 17-byte header)
//
// For each cell:
//   symbol_name\0  (null-terminated string)
//   kind: uint8_t  (CellKind enum)
//   value: double  (8 bytes, IEEE 754)
//   source_text\0  (null-terminated string, empty string if none)
//
// For each output:
//   output_name\0  (null-terminated string, e.g. "a1")
//   source_text\0  (null-terminated string)

constexpr uint16_t FLASH_FORMAT_VERSION = 1;

// Header layout
struct FlashHeader {
    char magic[5];         // "uSEQ\0"
    uint16_t version;
    uint16_t cell_count;
    uint16_t output_count;
    uint32_t data_size;    // bytes of cell+output data (after header, before crc)
    uint32_t checksum;     // CRC32 of the data_size bytes
};
// Total header size: 5 + 2 + 2 + 2 + 4 + 4 = 19 bytes

constexpr size_t FLASH_HEADER_SIZE = 19;

// Maximum serialized state size (generous upper bound)
constexpr size_t FLASH_MAX_STATE_SIZE = 32 * 1024;

// ── FlashStorage ───────────────────────────────────────────────────────────

struct FlashStorage {
    void init();
    bool save(const sig::SignalEngine& engine);
    bool load(sig::SignalEngine& engine);
    bool has_saved_state();
    void erase();

#ifndef ARDUINO
    // Desktop: use IStorage for testability
    void set_storage(IStorage* storage);
private:
    IStorage* m_storage = nullptr;
#endif
};

// ── CRC32 ──────────────────────────────────────────────────────────────────
// Simple CRC32 (ISO 3309 / ITU-T V.42) for checksum validation.

uint32_t crc32(const uint8_t* data, size_t length);

} // namespace firmware

#endif // FIRMWARE_FLASH_STORAGE_H
