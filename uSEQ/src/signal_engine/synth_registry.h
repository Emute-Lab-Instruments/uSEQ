#ifndef SIGNAL_ENGINE_SYNTH_REGISTRY_H
#define SIGNAL_ENGINE_SYNTH_REGISTRY_H

#include "types.h"
#include <cstdint>

namespace sig {

// ── Synth NodeDef Registry (synth-nodes.md §2) ─────────────────────────────
//
// Source-agnostic metadata describing each audio-rate NodeDef the host knows
// how to instantiate. In M1 the registry is a small static table (just
// osc/sine); the language/compiler only ever consumes the contract metadata,
// never the DSP implementation.
//
// The registry owns:
//   - def name (namespaced string like "osc/sine")
//   - def version (positive integer; 0 means "latest")
//   - audio input/output port counts (M1 osc/sine: 0 inputs, 1 mono output)
//   - parameter contract for :freq and :amp
//
// The v1 library is intentionally minimal: the compiler validates against
// this table and produces precise diagnostics for unknown defs, unknown
// parameters, or unsupported versions.

// ── Rate class (synth-nodes.md §2.3) ───────────────────────────────────────
enum class SynthRateClass : uint8_t {
    Block, // sampled once per audio block
    Fast,  // declared higher control rate (points-per-block)
};

// ── Smoothing class (synth-nodes.md §2.4) ──────────────────────────────────
enum class SynthSmoothingClass : uint8_t {
    Step,  // hold until next control point (default for pitch/freq)
    Linear,// ramp to next value (implemented inside the def)
    Slew,  // exponential approach (implemented inside the def)
    Latch, // event-like edge records (gates, triggers)
};

// ── Parameter contract per NodeDef ─────────────────────────────────────────
struct NodeDefParam {
    const char* name;             // e.g. "freq", "amp"
    double default_value;         // static default; omitted bindings take this
    SynthRateClass rate_class;
    SynthSmoothingClass smoothing_class;
};

// Maximum parameters per NodeDef. osc/sine has 2 (freq, amp). We allow a few
// more so the registry can describe slightly richer defs without a refactor.
constexpr uint16_t MAX_NODEDEF_PARAMS = 8;

// Maximum def entries in the static registry. M1 ships exactly one (osc/sine).
constexpr uint16_t MAX_NODEDEF_ENTRIES = 8;

// Maximum length of a NodeDef name string (namespaced identifiers are short).
constexpr uint16_t MAX_NODEDEF_NAME = 32;

struct NodeDefDescriptor {
    const char* name;             // e.g. "osc/sine"
    uint16_t version;             // e.g. 1
    uint16_t audio_inputs;        // osc/sine has 0
    uint16_t audio_outputs;       // osc/sine has 1 (mono)
    bool voice_fanout;            // osc/sine does not vector-fan-out in M1

    NodeDefParam params[MAX_NODEDEF_PARAMS];
    uint16_t param_count;

    // Convenience accessors used by the compiler. freq_default and amp_default
    // are surfaced directly because the synth form grammar is hard-wired to
    // the freq/amp pair for M1.
    double freq_default;          // registry-declared default for :freq
    double amp_default;           // registry-declared default for :amp
};

// ── Static registry accessor ───────────────────────────────────────────────
//
// Returns the canonical table of known NodeDefs. The table is a static
// constant (no dynamic registration in M1). The pointer is valid for the
// lifetime of the program.

const NodeDefDescriptor* synth_registry_table(uint16_t& count_out);

// Look up a NodeDef by (name, version). version=0 means "any / latest".
// Returns nullptr if no def matches both name and version. When version=0
// the highest available version for the given name is returned.

const NodeDefDescriptor* synth_registry_find(const char* name, uint16_t version);

// Look up a parameter descriptor by name on a specific NodeDef.
const NodeDefParam* nodedef_find_param(const NodeDefDescriptor* def,
                                       const char* param_name);

// Fuzzy-match a parameter name against the declared params. Returns the
// best candidate name (or nullptr if none is close enough). Used for the
// "did you mean" suggestion in unknown-parameter diagnostics.
const char* nodedef_suggest_param(const NodeDefDescriptor* def,
                                  const char* candidate);

// ── M1 single-node capacity ────────────────────────────────────────────────
// M1 hosts one synth instance at a time. This constant is the compile-time
// capacity enforced by the synth compiler domain; over-capacity evals fail
// transactionally with an actionable diagnostic.
constexpr uint16_t SYNTH_M1_MAX_NODES = 1;

} // namespace sig

#endif // SIGNAL_ENGINE_SYNTH_REGISTRY_H
