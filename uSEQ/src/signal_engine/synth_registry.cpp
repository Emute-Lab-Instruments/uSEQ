#include "synth_registry.h"
#include <cstring>
#include <algorithm>

namespace sig {

// ── Static NodeDef table ───────────────────────────────────────────────────
//
// The current proof set contains osc/sine version 2, with:
//   - one audio input (`fm`, per-sample Hz offset), one mono output
//   - :freq: block rate, step smoothing, default 440 Hz
//   - :amp:  block rate, declared smoothing (linear, implemented by def),
//            default 0.2
//
// The compiler consults this table to validate def names, versions,
// parameters, and defaults. The host/worklet consult it for transport and
// zone layout (out of scope for this feature).

static const NodeDefDescriptor kRegistryTable[] = {
    {
        "osc/sine",
        2,          // version (v2 adds the fm audio-input contract)
        1,          // audio_inputs
        1,          // audio_outputs (mono)
        false,      // voice_fanout
        {
            // params[0]: freq
            {
                "freq",
                440.0,
                SynthRateClass::Block,
                SynthSmoothingClass::Step,
            },
            // params[1]: amp
            {
                "amp",
                0.2,
                SynthRateClass::Block,
                SynthSmoothingClass::Linear,
            },
        },
        2,          // param_count
        {
            "fm",
        },
        // Convenience defaults surfaced for the form grammar:
        440.0,      // freq_default
        0.2,        // amp_default
    },
};

static constexpr uint16_t kRegistryCount =
    sizeof(kRegistryTable) / sizeof(kRegistryTable[0]);

const NodeDefDescriptor* synth_registry_table(uint16_t& count_out) {
    count_out = kRegistryCount;
    return kRegistryTable;
}

const NodeDefDescriptor* synth_registry_find(const char* name, uint16_t version) {
    if (!name) return nullptr;
    const NodeDefDescriptor* best = nullptr;
    uint16_t best_version = 0;
    for (uint16_t i = 0; i < kRegistryCount; i++) {
        const NodeDefDescriptor& d = kRegistryTable[i];
        if (std::strcmp(d.name, name) != 0) continue;
        if (version != 0 && d.version != version) continue;
        // version=0 means "any / latest": pick the highest available.
        if (version == 0) {
            if (best == nullptr || d.version > best_version) {
                best = &d;
                best_version = d.version;
            }
        } else {
            return &d;
        }
    }
    return best;
}

const NodeDefParam* nodedef_find_param(const NodeDefDescriptor* def,
                                       const char* param_name) {
    if (!def || !param_name) return nullptr;
    for (uint16_t i = 0; i < def->param_count; i++) {
        if (std::strcmp(def->params[i].name, param_name) == 0) {
            return &def->params[i];
        }
    }
    return nullptr;
}

int16_t nodedef_find_audio_input(const NodeDefDescriptor* def,
                                 const char* input_name) {
    if (!def || !input_name) return -1;
    for (uint16_t i = 0; i < def->audio_inputs &&
                         i < MAX_NODEDEF_AUDIO_INPUTS; i++) {
        if (def->audio_input_names[i] &&
            std::strcmp(def->audio_input_names[i], input_name) == 0) {
            return (int16_t)i;
        }
    }
    return -1;
}

// ── Tiny Levenshtein for parameter suggestion ──────────────────────────────
// Mirrors the algorithm in diagnostics.cpp. We keep this local rather than
// reusing find_fuzzy_match() because the candidate pool here is the def's
// declared params (short, fixed), not the cell table.

static int param_levenshtein(const char* s1, const char* s2) {
    int len1 = (int)std::strlen(s1);
    int len2 = (int)std::strlen(s2);
    if (len1 == 0) return len2;
    if (len2 == 0) return len1;
    if (len2 > 32) len2 = 32;
    int row[33];
    for (int j = 0; j <= len2; j++) row[j] = j;
    for (int i = 1; i <= len1; i++) {
        int prev = i - 1;
        row[0] = i;
        for (int j = 1; j <= len2; j++) {
            int temp = row[j];
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            row[j] = (std::min({prev + cost, row[j] + 1, row[j - 1] + 1}));
            prev = temp;
        }
    }
    return row[len2];
}

const char* nodedef_suggest_param(const NodeDefDescriptor* def,
                                  const char* candidate) {
    if (!def || !candidate) return nullptr;
    int candidate_len = (int)std::strlen(candidate);
    if (candidate_len == 0) return nullptr;

    const char* best = nullptr;
    int best_dist = 3; // threshold: only suggest if distance < 3

    for (uint16_t i = 0; i < def->param_count; i++) {
        const char* name = def->params[i].name;
        int d = param_levenshtein(candidate, name);
        if (d > 0 && d < best_dist) {
            best_dist = d;
            best = name;
        }
        // Prefix / case-insensitive near-miss (e.g. "amplitude" → "amp"):
        // if the candidate starts with the param name or vice versa, count
        // it as a strong match.
        size_t name_len = std::strlen(name);
        size_t min_len = std::min(name_len, (size_t)candidate_len);
        if (min_len >= 2 && std::strncmp(candidate, name, min_len) == 0) {
            // Strong prefix match — prefer it.
            return name;
        }
    }
    return best;
}

} // namespace sig
