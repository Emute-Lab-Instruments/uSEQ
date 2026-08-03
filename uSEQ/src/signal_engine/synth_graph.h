#ifndef SIGNAL_ENGINE_SYNTH_GRAPH_H
#define SIGNAL_ENGINE_SYNTH_GRAPH_H

#include "types.h"
#include "synth_registry.h"
#include "diagnostics.h"
#include <cstdint>
#include <cstring>

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
// the serialised public artefact (VAL-COMP-012). Ownership is represented by
// the declaration's dense first_control_index/control_count range. Parameter
// names are resolved from the immutable NodeDef descriptor by param_index.
// These two indices avoid repeated identity and parameter strings per row.
struct SynthControlChannel {
    uint8_t param_index                  = 0;
    SynthRateClass rate_class           = SynthRateClass::Block;
    SynthSmoothingClass smoothing_class = SynthSmoothingClass::Step;

    // Compiled control root node index. Internal: not serialised. The host
    // samples this root via the NodePool at each control block.
    uint16_t root_node                  = NODE_NONE;

    // Stable compiler ownership and reactive-recompile metadata. Internal;
    // never exposed through the host artefact.
    uint16_t owner_context              = 0;
    uint32_t source_offset              = 0;
    uint32_t source_length              = 0;
    CellIndex dep_cells[MAX_OUTPUT_DEPS] = {};
    uint8_t dep_count                   = 0;

    // Root-level non-finite containment for the audio control producer.
    double lkg_value                    = 0.0;
    bool has_lkg                        = false;

    // One fixed reactive-compile diagnostic slot per published control on the
    // WASM/desktop synth host. Keeping it on the dense row makes replacement,
    // removal, rollback, and artifact-order remapping atomic. Firmware does
    // not implement the synth host (synth-nodes.md §6.1), so it must not spend
    // scarce RP2040 SRAM on an ABI that cannot be observed there.
#if !defined(ARDUINO) && !defined(USEQ_FIRMWARE_PROFILE)
    struct CompileDiagnostic {
        const char* message = nullptr;
        const char* suggestion = nullptr;
        // Cell-backed symbols are bounded by MAX_CELLS before mutation, so a
        // 16-bit cause preserves every possible trigger without paying for
        // the interner's unbounded 32-bit ID in all 512 control rows.
        uint16_t triggered_by = 0;
        uint16_t span_start = 0;
        uint16_t span_len = 0;
        DiagnosticSeverity severity = DiagnosticSeverity::Error;
        DiagnosticCategory category = DiagnosticCategory::Runtime;

        void clear() { *this = CompileDiagnostic{}; }
        bool active() const { return message != nullptr; }
        void publish(SymbolID cause, const Diagnostic& diagnostic) {
            // A non-null message is the active marker. Every compiler
            // diagnostic is expected to have text; preserve the slot even if
            // a future producer violates that expectation.
            message = diagnostic.message ? diagnostic.message : "";
            suggestion = diagnostic.suggestion;
            triggered_by = static_cast<uint16_t>(cause);
            span_start = diagnostic.span_start;
            span_len = diagnostic.span_len;
            severity = diagnostic.severity;
            category = diagnostic.category;
        }
    } compile_diagnostic;
#endif
};

static_assert(MAX_NODEDEF_PARAMS <= UINT8_MAX,
              "synth parameter indices must cover every NodeDef parameter");

#if !defined(ARDUINO) && !defined(USEQ_FIRMWARE_PROFILE)
static_assert(MAX_CELLS <= UINT16_MAX,
              "synth diagnostic trigger IDs must cover every mutable cell");
#endif

struct SynthConnection {
    char from[MAX_SYNTH_IDENTITY] = {};
    char to[MAX_SYNTH_IDENTITY]   = {};
    char port[MAX_NODEDEF_NAME]   = {};
    uint16_t port_index           = 0;
};

// ── Capacity ───────────────────────────────────────────────────────────────
// The compiler and app share a 64-node ceiling. Current osc/sine has one
// audio input, so one edge per declaration covers the shipped registry.
constexpr uint16_t MAX_SYNTH_DECLARATIONS = SYNTH_MAX_NODES;
constexpr uint16_t MAX_SYNTH_CONTROLS = MAX_SYNTH_CONTROL_ROOTS;
constexpr uint16_t MAX_SYNTH_CONNECTIONS = MAX_SYNTH_DECLARATIONS;
#if !defined(ARDUINO) && !defined(USEQ_FIRMWARE_PROFILE)
static_assert(MAX_SYNTH_CONTROLS ==
                  MAX_SYNTH_DECLARATIONS * MAX_NODEDEF_PARAMS,
              "host synth storage must cover the descriptor ceiling");
#endif
// Bounds recursive audio-routing compilation and its cold-path stack use.
constexpr uint16_t MAX_SYNTH_NESTING = 16;

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
    SynthConnection connections[MAX_SYNTH_CONNECTIONS];

    uint16_t declaration_count_value = 0;
    uint16_t control_count_value     = 0;
    uint16_t connection_count_value  = 0;
    SynthRevision revision           = 0;

    // ── Accessors ─────────────────────────────────────────────────────────
    uint16_t declaration_count() const { return declaration_count_value; }
    uint16_t control_count()     const { return control_count_value; }
    uint16_t connection_count()  const { return connection_count_value; }

    const SynthDeclaration* declaration_for_control(
            uint16_t control_index) const {
        if (control_index >= control_count_value) return nullptr;
        // Declaration control slices are appended in declaration order and
        // remain dense after replacement/removal compaction. Search those
        // ordered ranges logarithmically; this is used by every synth-control
        // lookup during compilation and artifact inspection.
        uint16_t first_decl = 0;
        uint16_t past_last_decl = declaration_count_value;
        while (first_decl < past_last_decl) {
            const uint16_t middle = static_cast<uint16_t>(
                first_decl + (past_last_decl - first_decl) / 2);
            const SynthDeclaration& declaration = declarations[middle];
            const uint32_t first = declaration.first_control_index;
            const uint32_t past_last = first + declaration.control_count;
            if (control_index < first) {
                past_last_decl = middle;
            } else if (control_index >= past_last) {
                first_decl = static_cast<uint16_t>(middle + 1);
            } else {
                return &declaration;
            }
        }
        return nullptr;
    }

    const NodeDefParam* parameter_for_control(uint16_t control_index) const {
        const SynthDeclaration* declaration =
            declaration_for_control(control_index);
        if (!declaration) return nullptr;
        const NodeDefDescriptor* descriptor = synth_registry_find(
            declaration->def_name, declaration->def_version);
        if (!descriptor) return nullptr;
        const uint8_t parameter_index = controls[control_index].param_index;
        if (parameter_index >= descriptor->param_count ||
            parameter_index >= MAX_NODEDEF_PARAMS) {
            return nullptr;
        }
        return &descriptor->params[parameter_index];
    }

    // Reset to empty. Used by (useq-clear) and at startup. Does NOT advance
    // the revision: an empty graph is a valid committed state, so callers
    // that want to bump the revision should call advance_revision() after.
    void clear_no_revision() {
        declaration_count_value = 0;
        control_count_value     = 0;
        connection_count_value  = 0;
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
        // Dense-table compaction leaves retired bytes above the logical end.
        // A newly appended control is a new subject, so it must not inherit
        // the prior row's LKG, dependencies, or reactive diagnostic slot.
        SynthControlChannel& control = controls[control_count_value++];
        control = SynthControlChannel{};
        return &control;
    }

    SynthConnection* append_connection() {
        if (connection_count_value >= MAX_SYNTH_CONNECTIONS) return nullptr;
        return &connections[connection_count_value++];
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
constexpr uint32_t SYNTH_ARTIFACT_JSON_CAP = 32768;

bool synth_graph_render_json(const SynthGraph& graph, char* out, uint32_t cap);

// Convenience wrapper that renders into the engine's own scratch buffer.
// The returned pointer is valid until the next call to this function or
// until the engine is destroyed.
const char* synth_graph_render_json_scratch(const SynthGraph& graph);

// ── Versioned synth artefact ABI (VAL-COMP-015) ────────────────────────────
//
// The synth artefact payload carries an `abi` version marker so future
// consumers can reject incompatible bundles explicitly. The native engine
// advertises a single canonical ABI version; any consumer built against a
// different version must refuse to read the payload.
//
// Version history:
//   1 — single-node declarations + controls; no routable audio graph.
//   2 — required connections[] and multi-node patch-graph semantics.
constexpr uint16_t SYNTH_ARTIFACT_ABI_VERSION = 2;

/**
 * Return true iff the engine's synth-artefact ABI can serve a consumer
 * built against the supplied `consumer_abi_version`. The current engine
 * accepts only its own declared ABI; future versions may accept a range.
 *
 * Callers MUST consult this helper before interpreting the body bytes of
 * `synth_artifacts_json` / `useq_synth_artifacts`. Incompatible consumers
 * receive a minimal error object instead of the artefact body (see
 * `synth_artifacts_render_abi_wrapper`).
 */
bool synth_artifacts_supports_abi(uint16_t consumer_abi_version);

/**
 * Render the versioned synth artefact payload into `out` for a consumer
 * built against `consumer_abi_version`.
 *
 * On success the buffer contains a JSON object shaped:
 *   {"abi":<version>,"revision":N,"declarations":[...],"controls":[...]}
 * and the function returns true.
 *
 * If the consumer ABI version is unsupported, the buffer is filled with a
 * minimal JSON error object (still valid JSON) and the function returns
 * false. The caller MUST NOT interpret the body bytes when this function
 * returns false — the only safe interpretation is the `abi_error` field.
 *
 * This helper is the native counterpart of the WASM `useq_synth_artifacts`
 * wrapper. Mirrors its byte shape exactly so native and WASM consumers
 * observe identical payloads.
 */
bool synth_artifacts_render_abi_wrapper(const struct SignalEngine& engine,
                                        uint16_t consumer_abi_version,
                                        char* out, uint32_t cap);

// ── Engine-level accessor ──────────────────────────────────────────────────
// Returns the engine's published synth artefact snapshot as JSON. Mirrors
// the useq_last_diagnostics() pattern: the pointer is stable until the
// next eval. Equivalent to synth_graph_render_json_scratch(engine.synth_graph).
const char* synth_artifacts_json(const struct SignalEngine& engine);

} // namespace sig

#endif // SIGNAL_ENGINE_SYNTH_GRAPH_H
