#pragma once

#include <cstddef>
#include "lisp/symbol_intern.h"
#include "lisp/value.h"

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
    const double* input_values = nullptr;
    size_t input_count = 0;

    // Fast lookup by symbol ID — returns true if id is a temporal variable.
    bool tryGet(SymbolIntern::SymbolID id, Value& out) const;

    double input_at(size_t index) const
    {
        if (!input_values || index >= input_count)
        {
            return 0.0;
        }
        return input_values[index];
    }

    // Must be called once at startup to intern temporal variable names.
    static void initLookupTable();

private:
    // Pre-cached symbol IDs (set once by initLookupTable)
    static SymbolIntern::SymbolID s_t, s_beat, s_bar, s_phrase, s_section;
    static SymbolIntern::SymbolID s_time;
    static SymbolIntern::SymbolID s_beatNum, s_barNum;
    static SymbolIntern::SymbolID s_beatDur, s_barDur, s_phraseDur, s_sectionDur;
    static SymbolIntern::SymbolID s_beat_dur, s_bar_dur, s_phrase_dur, s_section_dur;
    static bool s_initialized;
};
