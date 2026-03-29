// FlashStorage test suite
// Tests serialization, deserialization, CRC32 validation, and round-trip
// save/load of signal engine state through the IStorage interface.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "src/firmware/flash_storage.h"
#include "src/signal_engine/signal_engine.h"
#include "src/ports/mocks/MockStorage.h"
#include <cstring>

using namespace sig;
using namespace firmware;

// ── Helpers ────────────────────────────────────────────────────────────────

static FlashStorage make_flash(MockStorage& storage) {
    FlashStorage fs;
    fs.init();
    fs.set_storage(&storage);
    return fs;
}

// ── CRC32 Tests ────────────────────────────────────────────────────────────

TEST_CASE("CRC32: known test vectors", "[flash][crc32]") {
    SECTION("Empty data") {
        uint32_t c = crc32(nullptr, 0);
        // CRC32 of empty data is 0x00000000
        REQUIRE(c == 0x00000000);
    }

    SECTION("ASCII string '123456789'") {
        const char* data = "123456789";
        uint32_t c = crc32(reinterpret_cast<const uint8_t*>(data), 9);
        REQUIRE(c == 0xCBF43926);
    }
}

// ── Empty State ────────────────────────────────────────────────────────────

TEST_CASE("FlashStorage: no saved state initially", "[flash]") {
    MockStorage storage;
    FlashStorage fs = make_flash(storage);

    REQUIRE(fs.has_saved_state() == false);
}

TEST_CASE("FlashStorage: save and detect saved state", "[flash]") {
    MockStorage storage;
    FlashStorage fs = make_flash(storage);
    SignalEngine engine;
    engine.init_defaults();

    REQUIRE(fs.save(engine) == true);
    REQUIRE(fs.has_saved_state() == true);
}

TEST_CASE("FlashStorage: erase clears saved state", "[flash]") {
    MockStorage storage;
    FlashStorage fs = make_flash(storage);
    SignalEngine engine;
    engine.init_defaults();

    fs.save(engine);
    REQUIRE(fs.has_saved_state() == true);

    fs.erase();
    REQUIRE(fs.has_saved_state() == false);
}

// ── Round-Trip: Number Cells ───────────────────────────────────────────────

TEST_CASE("FlashStorage: round-trip number cells", "[flash]") {
    MockStorage storage;
    SignalEngine engine;
    engine.init_defaults(140.0); // non-default BPM

    // Define some numeric cells
    const char* src1 = "(define freq 440)";
    eval_cold(src1, (uint32_t)strlen(src1), engine);

    const char* src2 = "(define amp 0.8)";
    eval_cold(src2, (uint32_t)strlen(src2), engine);

    // Save
    FlashStorage fs = make_flash(storage);
    REQUIRE(fs.save(engine) == true);

    // Load into fresh engine
    SignalEngine loaded;
    FlashStorage fs2 = make_flash(storage);
    REQUIRE(fs2.load(loaded) == true);

    // Check BPM was restored
    auto& si = SymbolIntern::getInstance();
    SymbolID bpm_sym = si.intern("bpm");
    REQUIRE(loaded.cells.cells[bpm_sym].kind == CellKind::Number);
    REQUIRE(loaded.cells.cells[bpm_sym].value == Approx(140.0));

    // Check user-defined cells
    SymbolID freq_sym = si.intern("freq");
    REQUIRE(loaded.cells.cells[freq_sym].kind == CellKind::Number);
    REQUIRE(loaded.cells.cells[freq_sym].value == Approx(440.0));

    SymbolID amp_sym = si.intern("amp");
    REQUIRE(loaded.cells.cells[amp_sym].kind == CellKind::Number);
    REQUIRE(loaded.cells.cells[amp_sym].value == Approx(0.8));
}

// ── Round-Trip: Callable Cells (define with expression) ────────────────────

TEST_CASE("FlashStorage: round-trip callable cells with source text", "[flash]") {
    MockStorage storage;
    SignalEngine engine;
    engine.init_defaults();

    const char* src = "(define lfo (sin (* beat 6.28)))";
    eval_cold(src, (uint32_t)strlen(src), engine);

    FlashStorage fs = make_flash(storage);
    REQUIRE(fs.save(engine) == true);

    SignalEngine loaded;
    FlashStorage fs2 = make_flash(storage);
    REQUIRE(fs2.load(loaded) == true);

    auto& si = SymbolIntern::getInstance();
    SymbolID lfo_sym = si.intern("lfo");
    REQUIRE(loaded.cells.cells[lfo_sym].kind == CellKind::Callable);
    REQUIRE(loaded.cells.callables[lfo_sym].source_length > 0);

    // Verify source text was preserved
    const char* restored_src = loaded.arena.read(
        loaded.cells.callables[lfo_sym].source_offset);
    REQUIRE(restored_src != nullptr);
    // Source should contain the expression body
    REQUIRE(std::string(restored_src, loaded.cells.callables[lfo_sym].source_length)
                .find("sin") != std::string::npos);
}

// ── Round-Trip: Data Cells ─────────────────────────────────────────────────

TEST_CASE("FlashStorage: round-trip data cells (vectors)", "[flash]") {
    MockStorage storage;
    SignalEngine engine;
    engine.init_defaults();

    const char* src = "(define scale [0 2 4 5 7 9 11])";
    eval_cold(src, (uint32_t)strlen(src), engine);

    FlashStorage fs = make_flash(storage);
    REQUIRE(fs.save(engine) == true);

    SignalEngine loaded;
    FlashStorage fs2 = make_flash(storage);
    REQUIRE(fs2.load(loaded) == true);

    auto& si = SymbolIntern::getInstance();
    SymbolID scale_sym = si.intern("scale");
    REQUIRE(loaded.cells.cells[scale_sym].kind == CellKind::Data);
    REQUIRE(loaded.cells.cells[scale_sym].value == Approx(7.0)); // length

    uint16_t length = 0;
    const double* data = loaded.cells.get_data_table(
        loaded.cells.cells[scale_sym].data_table_id, length);
    REQUIRE(data != nullptr);
    REQUIRE(length == 7);
    REQUIRE(data[0] == Approx(0.0));
    REQUIRE(data[2] == Approx(4.0));
    REQUIRE(data[6] == Approx(11.0));
}

// ── Round-Trip: Output Expressions ─────────────────────────────────────────

TEST_CASE("FlashStorage: round-trip output expressions", "[flash]") {
    MockStorage storage;
    SignalEngine engine;
    engine.init_defaults();

    // Set output a1
    const char* src = "(a1 (sin beat))";
    eval_cold(src, (uint32_t)strlen(src), engine);

    // Verify output source was stored
    REQUIRE(engine.output_sources[0].has_source == true);
    REQUIRE(engine.output_sources[0].arena_length > 0);

    FlashStorage fs = make_flash(storage);
    REQUIRE(fs.save(engine) == true);

    SignalEngine loaded;
    FlashStorage fs2 = make_flash(storage);
    REQUIRE(fs2.load(loaded) == true);

    // Output source should be restored
    REQUIRE(loaded.output_sources[0].has_source == true);
    REQUIRE(loaded.output_sources[0].arena_length > 0);

    const char* restored = loaded.arena.read(loaded.output_sources[0].arena_offset);
    REQUIRE(restored != nullptr);
}

// ── Corruption Detection ───────────────────────────────────────────────────

TEST_CASE("FlashStorage: detects corrupted data (bad CRC)", "[flash]") {
    MockStorage storage;
    SignalEngine engine;
    engine.init_defaults();

    const char* src = "(define x 42)";
    eval_cold(src, (uint32_t)strlen(src), engine);

    FlashStorage fs = make_flash(storage);
    REQUIRE(fs.save(engine) == true);
    REQUIRE(fs.has_saved_state() == true);

    // Corrupt a byte in the data region (after header)
    uint8_t byte;
    storage.read(FLASH_HEADER_SIZE + 2, &byte, 1);
    byte ^= 0xFF;
    storage.write(FLASH_HEADER_SIZE + 2, &byte, 1);

    // Load should fail
    SignalEngine loaded;
    FlashStorage fs2 = make_flash(storage);
    REQUIRE(fs2.load(loaded) == false);
}

TEST_CASE("FlashStorage: detects bad magic header", "[flash]") {
    MockStorage storage;
    SignalEngine engine;
    engine.init_defaults();

    FlashStorage fs = make_flash(storage);
    fs.save(engine);

    // Corrupt magic
    uint8_t bad = 'X';
    storage.write(0, &bad, 1);

    REQUIRE(fs.has_saved_state() == false);

    SignalEngine loaded;
    REQUIRE(fs.load(loaded) == false);
}

// ── Multiple Outputs ───────────────────────────────────────────────────────

TEST_CASE("FlashStorage: multiple outputs round-trip", "[flash]") {
    MockStorage storage;
    SignalEngine engine;
    engine.init_defaults();

    const char* src1 = "(a1 (sin beat))";
    eval_cold(src1, (uint32_t)strlen(src1), engine);

    const char* src2 = "(d1 (sqr beat))";
    eval_cold(src2, (uint32_t)strlen(src2), engine);

    FlashStorage fs = make_flash(storage);
    REQUIRE(fs.save(engine) == true);

    SignalEngine loaded;
    FlashStorage fs2 = make_flash(storage);
    REQUIRE(fs2.load(loaded) == true);

    REQUIRE(loaded.output_sources[0].has_source == true);  // a1
    REQUIRE(loaded.output_sources[8].has_source == true);  // d1
    REQUIRE(loaded.output_sources[1].has_source == false); // a2 not set
}

// ── Load Without Storage ───────────────────────────────────────────────────

TEST_CASE("FlashStorage: operations without storage return false", "[flash]") {
    FlashStorage fs;
    fs.init();
    // No storage set

    REQUIRE(fs.has_saved_state() == false);

    SignalEngine engine;
    engine.init_defaults();
    REQUIRE(fs.save(engine) == false);
    REQUIRE(fs.load(engine) == false);

    // erase should not crash
    fs.erase();
}
