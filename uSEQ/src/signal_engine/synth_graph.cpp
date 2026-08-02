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
                if (p + 2 >= end) return false;
                *p++ = ' ';
            } else {
                if (p + 2 >= end) return false;
                *p++ = ch;
            }
        }
        if (!emit("\"")) return false;
        return true;
    };

    // Single canonical render pass. The public schema is intentionally
    // narrow (VAL-COMP-012): revision, declarations[], controls[] keyed by
    // stable identity / param name. Internal GC-remapped node indices are
    // never serialised.
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
        const SynthDeclaration* declaration =
            graph.declaration_for_control(i);
        if (!declaration) return false;
        if (i > 0 && !emit(",")) return false;
        if (!emit("{\"identity\":")) return false;
        if (!emit_quoted(declaration->identity)) return false;
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
    if (!emit("],\"connections\":[")) return false;
    for (uint16_t i = 0; i < graph.connection_count(); i++) {
        const SynthConnection& c = graph.connections[i];
        if (i > 0 && !emit(",")) return false;
        if (!emit("{\"from\":")) return false;
        if (!emit_quoted(c.from)) return false;
        if (!emit(",\"to\":")) return false;
        if (!emit_quoted(c.to)) return false;
        if (!emit(",\"port\":")) return false;
        if (!emit_quoted(c.port)) return false;
        if (!emit(",\"port_index\":")) return false;
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%u", (unsigned)c.port_index);
            if (!emit(buf)) return false;
        }
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

// ── Versioned ABI surface (VAL-COMP-015) ───────────────────────────────────

bool synth_artifacts_supports_abi(uint16_t consumer_abi_version) {
    // The current engine advertises exactly one ABI version. Consumers
    // built against any other version must be rejected explicitly so we
    // never let a newer or older consumer misread the payload layout.
    return consumer_abi_version == SYNTH_ARTIFACT_ABI_VERSION;
}

bool synth_artifacts_render_abi_wrapper(const SignalEngine& engine,
                                        uint16_t consumer_abi_version,
                                        char* out, uint32_t cap) {
    if (!out || cap == 0) return false;

    // Reject incompatible consumers up front with a minimal valid-JSON
    // error envelope. The caller MUST NOT interpret the body bytes when
    // this function returns false.
    if (!synth_artifacts_supports_abi(consumer_abi_version)) {
        // The error envelope includes the engine's advertised ABI version
        // and the rejected consumer version so the diagnostics surfaced
        // to the user are actionable.
        char err[96];
        std::snprintf(err, sizeof(err),
                      "{\"abi\":%u,\"abi_error\":true,"
                      "\"engine_abi\":%u,\"consumer_abi\":%u}",
                      (unsigned)SYNTH_ARTIFACT_ABI_VERSION,
                      (unsigned)SYNTH_ARTIFACT_ABI_VERSION,
                      (unsigned)consumer_abi_version);
        if (std::strlen(err) + 1 > cap) return false;
        std::memcpy(out, err, std::strlen(err) + 1);
        return false;
    }

    // Render the body into a scratch buffer first so we can prepend the
    // `abi` marker without a double-copy of the engine state.
    static char body_scratch[SYNTH_ARTIFACT_JSON_CAP];
    const char* body = synth_graph_render_json_scratch(engine.synth_graph);
    if (!body) return false;
    // synth_graph_render_json_scratch returns a pointer into a separate
    // scratch buffer; copy into body_scratch so the wrap below is stable
    // even if we ever recurse into the scratch again.
    std::strncpy(body_scratch, body, SYNTH_ARTIFACT_JSON_CAP - 1);
    body_scratch[SYNTH_ARTIFACT_JSON_CAP - 1] = '\0';

    // Wrap with the `abi` marker. We need to drop the leading '{' of the
    // body so we don't produce `{{...}}`.
    const char* body_open = body_scratch[0] == '{' ? body_scratch + 1
                                                    : body_scratch;
    int written = std::snprintf(out, cap, "{\"abi\":%u,%s",
                                (unsigned)SYNTH_ARTIFACT_ABI_VERSION,
                                body_open);
    if (written < 0 || (uint32_t)written >= cap) return false;
    return true;
}

const char* synth_artifacts_json(const SignalEngine& engine) {
    return synth_graph_render_json_scratch(engine.synth_graph);
}

} // namespace sig
