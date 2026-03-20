#include "temporal_context.h"

// Static member definitions
TemporalField TemporalContext::s_lookup[TEMPORAL_LOOKUP_SIZE] = {};
bool TemporalContext::s_initialized = false;

void TemporalContext::initLookupTable()
{
    if (s_initialized) return;

    // Zero-fill the table
    for (size_t i = 0; i < TEMPORAL_LOOKUP_SIZE; ++i)
        s_lookup[i] = TemporalField::NONE;

    auto& intern = SymbolIntern::getInstance();

    // Helper: register a name → field mapping
    auto reg = [&](const char* name, TemporalField field) {
        auto id = intern.intern(String(name));
        if (id < TEMPORAL_LOOKUP_SIZE)
            s_lookup[id] = field;
    };

    reg("t",            TemporalField::T);
    reg("beat",         TemporalField::BEAT);
    reg("bar",          TemporalField::BAR);
    reg("phrase",       TemporalField::PHRASE);
    reg("section",      TemporalField::SECTION);
    reg("time",         TemporalField::TIME_SINCE_BOOT);
    reg("beatNum",      TemporalField::BEAT_NUM);
    reg("barNum",       TemporalField::BAR_NUM);

    // Duration variables — canonical and alias forms
    reg("beatDur",      TemporalField::BEAT_DUR);
    reg("beat-dur",     TemporalField::BEAT_DUR);
    reg("barDur",       TemporalField::BAR_DUR);
    reg("bar-dur",      TemporalField::BAR_DUR);
    reg("phraseDur",    TemporalField::PHRASE_DUR);
    reg("phrase-dur",   TemporalField::PHRASE_DUR);
    reg("sectionDur",   TemporalField::SECTION_DUR);
    reg("section-dur",  TemporalField::SECTION_DUR);

    s_initialized = true;
}

bool TemporalContext::tryGet(SymbolIntern::SymbolID id, Value& out) const
{
    if (id == SymbolIntern::INVALID_ID || id >= TEMPORAL_LOOKUP_SIZE)
        return false;

    switch (s_lookup[id])
    {
    case TemporalField::T:              out = Value(t);              return true;
    case TemporalField::BEAT:           out = Value(beat);           return true;
    case TemporalField::BAR:            out = Value(bar);            return true;
    case TemporalField::PHRASE:         out = Value(phrase);         return true;
    case TemporalField::SECTION:        out = Value(section);        return true;
    case TemporalField::TIME_SINCE_BOOT:out = Value(time_since_boot);return true;
    case TemporalField::BEAT_NUM:       out = Value(beatNum);        return true;
    case TemporalField::BAR_NUM:        out = Value(barNum);         return true;
    case TemporalField::BEAT_DUR:       out = Value(beatDur);        return true;
    case TemporalField::BAR_DUR:        out = Value(barDur);         return true;
    case TemporalField::PHRASE_DUR:     out = Value(phraseDur);      return true;
    case TemporalField::SECTION_DUR:    out = Value(sectionDur);     return true;
    case TemporalField::NONE:           return false;
    }
    return false;
}
