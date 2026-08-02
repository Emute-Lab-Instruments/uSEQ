#ifndef SIGNAL_ENGINE_TYPES_H
#define SIGNAL_ENGINE_TYPES_H

#include <cstdint>
#include <cstddef>
#include "../modulisp/lisp/symbol_intern.h"

namespace sig {

using SymbolID = SymbolIntern::SymbolID;

// ── Limits ──────────────────────────────────────────────────────────────────
// Firmware builds use smaller limits to fit within the RP2040's 264 KB SRAM.
// USEQ_FIRMWARE_PROFILE selects the same retained capacities in a native
// process so capacity and endurance workloads can run without Arduino I/O.
// It is a resource-profile simulation, not target timing evidence.
// WASM/desktop builds keep generous limits since memory is plentiful.
// MAX_CELLS must exceed the number of built-in symbols (~177) since cells
// are indexed by SymbolID and user-defined names follow the built-ins.

#if defined(ARDUINO) || defined(USEQ_FIRMWARE_PROFILE)
constexpr size_t MAX_CELLS             = 256;
constexpr size_t MAX_TOTAL_NODES       = 360;
constexpr size_t MAX_DATA_ENTRIES      = 512;
constexpr size_t MAX_DATA_TABLES       = 32;
constexpr size_t MAX_LIVE_SLOTS        = 16;
constexpr size_t MAX_LIVE_SLOT_OPTIONS = 8;
constexpr size_t SOURCE_ARENA_SIZE     = 4096;
constexpr size_t CSE_TABLE_SIZE        = MAX_TOTAL_NODES;
constexpr size_t MAX_OUTPUT_DEPS       = 32;
constexpr size_t MAX_STATE_SLOTS       = 16;
#else
constexpr size_t MAX_CELLS             = 512;
constexpr size_t MAX_TOTAL_NODES       = 1024;
constexpr size_t MAX_DATA_ENTRIES      = 2048;
constexpr size_t MAX_DATA_TABLES       = 64;
constexpr size_t MAX_LIVE_SLOTS        = 256;
constexpr size_t MAX_LIVE_SLOT_OPTIONS = 16;
constexpr size_t SOURCE_ARENA_SIZE     = 16384;
constexpr size_t CSE_TABLE_SIZE        = MAX_TOTAL_NODES * 2;
constexpr size_t MAX_OUTPUT_DEPS       = 64;
constexpr size_t MAX_STATE_SLOTS       = 32;
#endif

constexpr size_t MAX_CALLABLE_PARAMS = 8;
constexpr size_t MAX_OUTPUTS        = 42;
constexpr size_t MAX_SCOPE_DEPTH    = 32;
constexpr size_t MAX_LOCAL_BINDINGS = 32;
constexpr size_t MAX_DIAGNOSTICS    = 16;
constexpr size_t MAX_INLINE_DEPTH   = 16;
// Max syntactic nesting depth for compile_expr recursion. compile_expr recurses
// on nested forms on the small RP2040 stack (inside tick() for live edits), so a
// pathologically deep program could overflow the hardware stack. This bound
// aborts compilation with a diagnostic well before that happens.
constexpr size_t MAX_COMPILE_DEPTH  = 64;
constexpr size_t MAX_LIVE_SLOT_ID   = 32;
constexpr size_t MAX_LIVE_SLOT_OPTION_LEN = 32;
constexpr size_t MAX_TOKENS         = 256;
constexpr size_t BATCH_CHUNK_SIZE   = 256;

// The shipped NodeDef registry currently exposes at most two control
// parameters per declaration.  A firmware profile can therefore preserve
// all 64 valid shipped-registry declarations with 128 persistent control
// roots.  Native/WASM retain the wider descriptor ceiling so richer host-only
// registries do not inherit the RP2040 storage constraint.
constexpr size_t FIRMWARE_MAX_SYNTH_CONTROLS = 128;
#if defined(ARDUINO) || defined(USEQ_FIRMWARE_PROFILE)
constexpr size_t MAX_SYNTH_CONTROL_ROOTS = FIRMWARE_MAX_SYNTH_CONTROLS;
#else
constexpr size_t MAX_SYNTH_CONTROL_ROOTS = 512;
#endif

constexpr uint16_t NODE_NONE        = 0xFFFF;
constexpr uint8_t  FLAG_TIME_INVARIANT = 0x01;

// ── Output Classification (visualisation.md §4) ────────────────────────────
enum class OutputClass : uint8_t {
    Inactive     = 0,  // no graph assigned
    Pure         = 1,  // closed-form function of t only (+ cells, data)
    InputDep     = 2,  // references hardware inputs but no cross-sample state
    Stateful     = 3,  // uses LoadState, LoadDt, or PrevOutputLoad
};

} // namespace sig

#endif // SIGNAL_ENGINE_TYPES_H
