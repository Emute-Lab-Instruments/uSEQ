#include "synth_graph.h"
#include "cold_eval.h"
#include <cstring>
#include <cstdio>

namespace sig {

// ── Internal scratch buffer for serialised artefacts ───────────────────────
// The WASM ABI returns const char* snapshots that are stable until the next
// eval, mirroring useq_last_diagnostics(). We back them with a static
// thread-local-ish buffer (single-threaded engine on both firmware and WASM).
static char g_artifact_scratch[SYNTH_ARTIFACT_JSON_CAP];

bool synth_graph_render_json(const SynthGraph& graph, char* out, uint32_t cap) {
    if (!out || cap == 0) return false;
    char* p = out;
    const char* end = out + cap;
    auto emit = [&](const char* s) -> bool {
        size_t n = std::strlen(s);
        if (p + n + 1 >= end) return false;
        std::memcpy(p, s, n);
        p += n;
        return true;
    };
    auto emit_quoted = [&](const char* s) -> bool {
        if (!emit("\"")) return false;
        // Escape nothing fancy for now: identities and def names are
        // restricted to alphanumerics, '/', '-' and the editor sidecar
        // "::" prefix. A defensive scan still strips any stray control
        // characters or embedded quotes.
        for (const char* c = s; *c; ++c) {
            char ch = *c;
            if (ch == '"' || ch == '\\') {
                if (p + 3 >= end) return false;
                *p++ = '\\';
                *p++ = ch;
            } else if ((unsigned char)ch < 0x20) {
                if (p + 7 >= end) return false;
                *p++ = ' ';
            } else {
                if (p + 2 >= end) return false;
                *p++ = ch;
            }
        }
        if (!emit("\"")) return false;
        return true;
    };

    if (!emit("{\"revision\":")) return false;
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%lu", (unsigned long)graph.revision);
        if (!emit(buf)) return false;
    }
    if (!emit(",\"graph\":[") || !emit("]}")) return false;

    // NOTE: the public schema is intentionally narrow (VAL-COMP-012). We
    // expose: revision, and (if non-empty) declarations and controls
    // arrays keyed by stable identity / param name. Internal GC-remapped
    // node indices are never serialised.
    if (graph.declaration_count() == 0) {
        // Minimal empty-graph payload.
        if (!emit(" ")) return false;
        // Replace the closing brace with an explicit empty arrays form.
        // Easier: rewrite the tail.
    }

    // Re-render with declarations/controls when present. We rebuild from
    // scratch to keep the code path simple.
    p = out;
    if (!emit("{\"revision\":")) return false;
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%lu", (unsigned long)graph.revision);
        if (!emit(buf)) return false;
    }
    if (!emit(",\"declarations\":[")) return false;
    for (uint16_t i = 0; i < graph.declaration_count(); i++) {
        const SynthDeclaration& d = graph.declarations[i];
        if (i > 0 && !emit(",")) return false;
        if (!emit("{\"identity\":")) return false;
        if (!emit_quoted(d.identity)) return false;
        if (!emit(",\"def\":")) return false;
        if (!emit_quoted(d.def_name)) return false;
        if (!emit(",\"version\":")) return false;
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%u", (unsigned)d.def_version);
            if (!emit(buf)) return false;
        }
        if (!emit(",\"audio_inputs\":")) return false;
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%u", (unsigned)d.audio_inputs);
            if (!emit(buf)) return false;
        }
        if (!emit(",\"audio_outputs\":")) return false;
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%u", (unsigned)d.audio_outputs);
            if (!emit(buf)) return false;
        }
        if (!emit("}")) return false;
    }
    if (!emit("],\"controls\":[")) return false;
    for (uint16_t i = 0; i < graph.control_count(); i++) {
        const SynthControlChannel& c = graph.controls[i];
        if (i > 0 && !emit(",")) return false;
        if (!emit("{\"identity\":")) return false;
        if (!emit_quoted(c.identity)) return false;
        if (!emit(",\"param\":")) return false;
        if (!emit_quoted(c.param_name)) return false;
        if (!emit(",\"rate\":")) return false;
        if (!emit(c.rate_class == SynthRateClass::Block ? "\"block\"" : "\"fast\"")) return false;
        if (!emit(",\"smoothing\":")) return false;
        const char* sm = "step";
        switch (c.smoothing_class) {
            case SynthSmoothingClass::Step:   sm = "step";   break;
            case SynthSmoothingClass::Linear: sm = "linear"; break;
            case SynthSmoothingClass::Slew:   sm = "slew";   break;
            case SynthSmoothingClass::Latch:  sm = "latch";  break;
        }
        if (!emit_quoted(sm)) return false;
        if (!emit("}")) return false;
    }
    if (!emit("]}")) return false;

    if ((uint32_t)(p - out) >= cap) return false;
    *p = '\0';
    return true;
}

const char* synth_graph_render_json_scratch(const SynthGraph& graph) {
    if (!synth_graph_render_json(graph, g_artifact_scratch,
                                 SYNTH_ARTIFACT_JSON_CAP)) {
        // Truncation / failure: emit a minimal valid JSON sentinel so the
        // consumer never sees invalid JSON over the wire.
        std::snprintf(g_artifact_scratch, sizeof(g_artifact_scratch),
                      "{\"revision\":%lu,\"error\":true}",
                      (unsigned long)graph.revision);
    }
    return g_artifact_scratch;
}

const char* synth_artifacts_json(const SignalEngine& engine) {
    return synth_graph_render_json_scratch(engine.synth_graph);
}

} // namespace sig
