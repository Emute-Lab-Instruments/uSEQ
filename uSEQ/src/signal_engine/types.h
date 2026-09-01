#ifndef SIGNAL_ENGINE_TYPES_H
#define SIGNAL_ENGINE_TYPES_H

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include "build_profile.h"
#include "../modulisp/lisp/symbol_intern.h"

namespace sig {

using SymbolID = SymbolIntern::SymbolID;

// ── Numeric Width ───────────────────────────────────────────────────────────
// Sample is the engine's numeric scalar: signal values, cell values, time
// (t/dt), node results, and output samples all use it. One control point for
// the width so a measured firmware decision (software double vs hardware
// float on RP2040, workspace SRAM halves) can be made without touching call
// sites. Default stays double: phasor time accumulates over long sessions and
// float's 24-bit mantissa degrades beat phase measurably.
//
// Known costs of enabling USEQ_SAMPLE_F32 (deliberately not hidden):
//   - double literals (0.5) will promote under -Wdouble-promotion; the build
//     keeps a zero-warning policy, so suffixes/casts must be fixed then.
//   - Unqualified math calls (floor, sin) resolve to double overloads unless
//     std::-qualified; qualify before flipping.
//   - Flash persistence layout and the WASM C ABI are double today; they
//     convert at their boundary or version their formats.
#if defined(USEQ_SAMPLE_F32)
using Sample = float;
#else
using Sample = double;
#endif
static_assert(std::is_floating_point_v<Sample>,
              "Sample must be a floating-point type");

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

#if USEQ_HAS_SYNTH_ENGINE
constexpr size_t MAX_SYNTH_CONTROL_ROOTS = 512;
#endif

// Persistent dependency records refer only to entries in CellStore, not to
// the unbounded symbol-interner namespace.  A compact index therefore covers
// every valid dependency in every build profile without narrowing the public
// definition capacity.
using CellIndex = uint16_t;
static_assert(MAX_CELLS <= static_cast<size_t>(UINT16_MAX) + 1,
              "CellIndex must represent every retained cell");

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
