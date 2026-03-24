#include "temporal_context.h"

// Static member definitions
SymbolIntern::SymbolID TemporalContext::s_t = 0;
SymbolIntern::SymbolID TemporalContext::s_beat = 0;
SymbolIntern::SymbolID TemporalContext::s_bar = 0;
SymbolIntern::SymbolID TemporalContext::s_phrase = 0;
SymbolIntern::SymbolID TemporalContext::s_section = 0;
SymbolIntern::SymbolID TemporalContext::s_time = 0;
SymbolIntern::SymbolID TemporalContext::s_beatNum = 0;
SymbolIntern::SymbolID TemporalContext::s_barNum = 0;
SymbolIntern::SymbolID TemporalContext::s_beatDur = 0;
SymbolIntern::SymbolID TemporalContext::s_barDur = 0;
SymbolIntern::SymbolID TemporalContext::s_phraseDur = 0;
SymbolIntern::SymbolID TemporalContext::s_sectionDur = 0;
SymbolIntern::SymbolID TemporalContext::s_beat_dur = 0;
SymbolIntern::SymbolID TemporalContext::s_bar_dur = 0;
SymbolIntern::SymbolID TemporalContext::s_phrase_dur = 0;
SymbolIntern::SymbolID TemporalContext::s_section_dur = 0;
bool TemporalContext::s_initialized = false;

void TemporalContext::initLookupTable()
{
    if (s_initialized) return;

    auto& si = SymbolIntern::getInstance();
    s_t           = si.intern("t");
    s_beat        = si.intern("beat");
    s_bar         = si.intern("bar");
    s_phrase      = si.intern("phrase");
    s_section     = si.intern("section");
    s_time        = si.intern("time");
    s_beatNum     = si.intern("beatNum");
    s_barNum      = si.intern("barNum");
    s_beatDur     = si.intern("beatDur");
    s_barDur      = si.intern("barDur");
    s_phraseDur   = si.intern("phraseDur");
    s_sectionDur  = si.intern("sectionDur");
    s_beat_dur    = si.intern("beat-dur");
    s_bar_dur     = si.intern("bar-dur");
    s_phrase_dur  = si.intern("phrase-dur");
    s_section_dur = si.intern("section-dur");

    s_initialized = true;
}

bool TemporalContext::tryGet(SymbolIntern::SymbolID id, Value& out) const
{
    if (id == s_t)                                { out = Value(t);               return true; }
    if (id == s_beat)                              { out = Value(beat);            return true; }
    if (id == s_bar)                               { out = Value(bar);             return true; }
    if (id == s_phrase)                             { out = Value(phrase);          return true; }
    if (id == s_section)                            { out = Value(section);         return true; }
    if (id == s_time)                               { out = Value(time_since_boot); return true; }
    if (id == s_beatNum)                            { out = Value(beatNum);         return true; }
    if (id == s_barNum)                             { out = Value(barNum);          return true; }
    if (id == s_beatDur  || id == s_beat_dur)       { out = Value(beatDur);         return true; }
    if (id == s_barDur   || id == s_bar_dur)        { out = Value(barDur);          return true; }
    if (id == s_phraseDur || id == s_phrase_dur)    { out = Value(phraseDur);       return true; }
    if (id == s_sectionDur || id == s_section_dur)  { out = Value(sectionDur);      return true; }
    return false;
}
