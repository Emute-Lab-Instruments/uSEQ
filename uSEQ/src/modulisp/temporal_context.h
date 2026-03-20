#pragma once

#include "lisp/symbol_intern.h"
#include "lisp/value.h"
#include <cstddef>
#include <cstdint>

// Maximum number of symbol IDs we track for temporal variables.
// This must be larger than any SymbolID assigned to a temporal variable name.
static constexpr size_t TEMPORAL_LOOKUP_SIZE = 128;

// Identifies which field of TemporalContext a symbol maps to.
enum class TemporalField : uint8_t
{
    NONE = 0,
    T,
    BEAT,
    BAR,
    PHRASE,
    SECTION,
    TIME_SINCE_BOOT,
    BEAT_NUM,
    BAR_NUM,
    BEAT_DUR,
    BAR_DUR,
    PHRASE_DUR,
    SECTION_DUR
};

struct TemporalContext
{
    double t              = 0;
    double beat           = 0;
    double bar            = 0;
    double phrase         = 0;
    double section        = 0;
    double time_since_boot = 0;
    int    beatNum        = 0;
    int    barNum         = 0;
    double beatDur        = 0.5;
    double barDur         = 2.0;
    double phraseDur      = 8.0;
    double sectionDur     = 32.0;

    // Fast lookup by symbol ID — returns true if id is a temporal variable.
    // The Value is written into `out`.
    bool tryGet(SymbolIntern::SymbolID id, Value& out) const;

    // Must be called once at startup (after SymbolIntern is constructed)
    // to intern all temporal variable names and populate the lookup table.
    static void initLookupTable();

private:
    // Sparse lookup: index by SymbolID → TemporalField.
    // IDs >= TEMPORAL_LOOKUP_SIZE are not temporal.
    static TemporalField s_lookup[TEMPORAL_LOOKUP_SIZE];
    static bool s_initialized;
};
