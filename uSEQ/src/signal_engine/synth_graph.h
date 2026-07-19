#ifndef SIGNAL_ENGINE_SYNTH_GRAPH_H
#define SIGNAL_ENGINE_SYNTH_GRAPH_H

#include "types.h"
#include "synth_registry.h"
#include <cstdint>

namespace sig {

// ── Synth Patch Graph and Control Table (synth-nodes.md §7.2) ──────────────
//
// Public artefacts exposed to the host after a successful synth eval. Both
// the patch graph (node instances, def names/versions, audio-port routing,
// fades) and the control channel table (one entry per bound param) are
// versioned together under one compiler revision so consumers can detect
// coherent updates (VAL-COMP-009). Failed evals retain the previous
// successful artefacts and do not advance the revision (VAL-COMP-010).
//
// Public identifiers are stable identity strings supplied by the editor
// (hidden :id or explicit :name). Internal GC-remapped node indices are
// never exposed in the serialised artefact (VAL-COMP-012).

// ── Identity limits ────────────────────────────────────────────────────────
// Identity strings are short editor-sidecar IDs (e.g. "::a3f1" or "lead").
constexpr uint16_t MAX_SYNTH_IDENTITY = 32;

// ── Patch-graph declaration entry ──────────────────────────────────────────
struct SynthDeclaration {
    char identity[MAX_SYNTH_IDENTITY] = {};
    char def_name[MAX_NODEDEF_NAME]   = {};
    uint16_t def_version              = 0;
    uint16_t audio_inputs             = 0;
    uint16_t audio_outputs            = 0;
    bool voice_fanout                 = false;
    // The index of this declaration's entry within the control table's
    // per-declaration slice. Public artefacts reference declarations by
    // identity only; this index is an internal helper, not serialised.
    uint16_t first_control_index      = 0;
    uint16_t control_count            = 0;
};

// ── Control channel table entry ────────────────────────────────────────────
// One row per bound (declaration, param [, voice]) triple. The host reads
// the param expression's compiled root_node out of the live NodePool each
// control block. The root_node value is internal and must NOT appear in
// the serialised public artefact (VAL-COMP-012).
struct SynthControlChannel {
    char    identity[MAX_SYNTH_IDENTITY] = {};   // owning declaration identity
    char    param_name[MAX_NODEDEF_NAME] = {};   // e.g. "freq", "amp"
    SynthRateClass rate_class           = SynthRateClass::Block;
    SynthSmoothingClass smoothing_class = SynthSmoothingClass::Step;

    // Compiled control root node index. Internal: not serialised. The host
    // samples this root via the NodePool at each control block.
    uint16_t root_node                  = NODE_NONE;
};

// ── Capacity ───────────────────────────────────────────────────────────────
// M1 hosts one synth instance (SYNTH_M1_MAX_NODES). Declarations beyond that
// fail transactionally. The control table is bounded by the per-declaration
// param count times SYNTH_M1_MAX_NODES; we keep a small power-of-two for
// safety.
constexpr uint16_t MAX_SYNTH_DECLARATIONS = SYNTH_M1_MAX_NODES;
constexpr uint16_t MAX_SYNTH_CONTROLS =
    MAX_SYNTH_DECLARATIONS * MAX_NODEDEF_PARAMS;

// ── Compiler revision ──────────────────────────────────────────────────────
// One shared counter covers graph and control table. It advances ONLY when
// a full eval unit commits successfully; failed evals retain the previous
// revision (VAL-COMP-008, VAL-COMP-009, VAL-COMP-010). Revision 0 is the
// "empty graph" sentinel.
using SynthRevision = uint32_t;

// ── Patch graph container ──────────────────────────────────────────────────
struct SynthGraph {
    SynthDeclaration declarations[MAX_SYNTH_DECLARATIONS];
    SynthControlChannel controls[MAX_SYNTH_CONTROLS];

    uint16_t declaration_count_value = 0;
    uint16_t control_count_value     = 0;
    SynthRevision revision           = 0;

    // ── Accessors ─────────────────────────────────────────────────────────
    uint16_t declaration_count() const { return declaration_count_value; }
    uint16_t control_count()     const { return control_count_value; }

    // Reset to empty. Used by (useq-clear) and at startup. Does NOT advance
    // the revision: an empty graph is a valid committed state, so callers
    // that want to bump the revision should call advance_revision() after.
    void clear_no_revision() {
        declaration_count_value = 0;
        control_count_value     = 0;
    }

    void clear_and_advance() {
        clear_no_revision();
        advance_revision();
    }

    void advance_revision() { revision++; }

    // Find a declaration by identity. Returns nullptr if absent.
    SynthDeclaration* find(const char* identity) {
        if (!identity) return nullptr;
        for (uint16_t i = 0; i < declaration_count_value; i++) {
            if (std::strcmp(declarations[i].identity, identity) == 0)
                return &declarations[i];
        }
        return nullptr;
    }
    const SynthDeclaration* find(const char* identity) const {
        if (!identity) return nullptr;
        for (uint16_t i = 0; i < declaration_count_value; i++) {
            if (std::strcmp(declarations[i].identity, identity) == 0)
                return &declarations[i];
        }
        return nullptr;
    }

    // Append a blank declaration slot. Returns nullptr if capacity is full.
    SynthDeclaration* append_declaration() {
        if (declaration_count_value >= MAX_SYNTH_DECLARATIONS) return nullptr;
        return &declarations[declaration_count_value++];
    }

    // Append a control channel. Returns nullptr if capacity is full.
    SynthControlChannel* append_control() {
        if (control_count_value >= MAX_SYNTH_CONTROLS) return nullptr;
        return &controls[control_count_value++];
    }
};

// ── Public artefact serialisation (VAL-COMP-012) ───────────────────────────
//
// Renders a JSON snapshot of the current synth graph suitable for WASM ABI
// consumption. The schema is intentionally narrow: only stable identity
// strings, def names/versions, parameter names, and rate/smoothing classes
// are emitted. Internal GC-remapped node indices are never serialised.
//
// The output is written into the supplied buffer and is null-terminated.
// Returns false if the buffer is too small (the caller should provide at
// least SYNTH_ARTIFACT_JSON_CAP bytes).
constexpr uint16_t SYNTH_ARTIFACT_JSON_CAP = 2048;

bool synth_graph_render_json(const SynthGraph& graph, char* out, uint32_t cap);

// Convenience wrapper that renders into the engine's own scratch buffer.
// The returned pointer is valid until the next call to this function or
// until the engine is destroyed.
const char* synth_graph_render_json_scratch(const SynthGraph& graph);

// ── Engine-level accessor ──────────────────────────────────────────────────
// Returns the engine's published synth artefact snapshot as JSON. Mirrors
// the useq_last_diagnostics() pattern: the pointer is stable until the
// next eval. Equivalent to synth_graph_render_json_scratch(engine.synth_graph).
const char* synth_artifacts_json(const struct SignalEngine& engine);

} // namespace sig

#endif // SIGNAL_ENGINE_SYNTH_GRAPH_H
