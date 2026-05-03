#ifndef SIGNAL_ENGINE_TYPES_H
#define SIGNAL_ENGINE_TYPES_H

#include <cstdint>
#include <cstddef>
#include "../modulisp/lisp/symbol_intern.h"

namespace sig {

using SymbolID = SymbolIntern::SymbolID;

// ── Limits ──────────────────────────────────────────────────────────────────
constexpr size_t MAX_CELLS          = 512;
constexpr size_t MAX_CALLABLE_PARAMS = 8;
constexpr size_t MAX_DATA_ENTRIES   = 2048;
constexpr size_t MAX_DATA_TABLES    = 64;
constexpr size_t MAX_TOTAL_NODES    = 1024;
constexpr size_t MAX_OUTPUTS        = 42;
constexpr size_t MAX_SCOPE_DEPTH    = 32;
constexpr size_t MAX_LOCAL_BINDINGS = 32;
constexpr size_t MAX_DIAGNOSTICS    = 16;
constexpr size_t MAX_INLINE_DEPTH   = 16;
constexpr size_t MAX_OUTPUT_DEPS    = 64;
constexpr size_t MAX_STATE_SLOTS    = 32;
constexpr size_t MAX_LIVE_SLOTS     = 32;
constexpr size_t MAX_LIVE_SLOT_ID   = 32;
constexpr size_t MAX_TOKENS         = 256;
constexpr size_t SOURCE_ARENA_SIZE  = 16384;
constexpr size_t CSE_TABLE_SIZE     = MAX_TOTAL_NODES * 2;
constexpr size_t BATCH_CHUNK_SIZE   = 256;

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
