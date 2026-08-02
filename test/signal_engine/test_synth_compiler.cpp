// Synth compiler domain tests (synth-nodes.md / VAL-COMP-001..012,018,019).
//
// These tests verify the transactional top-level synth declaration domain:
//   - Minimum and amplitude synth forms compile with registry defaults and stable identity
//   - Unknown defs, invalid parameters, malformed forms, nesting, and over-capacity
//     declarations fail with precise diagnostics
//   - Each top-level form is transactional and one shared revision covers its
//     graph and control table publication
//   - Control expression roots remain executable after GC
//   - Public artefacts avoid internal remapped node indices
//   - Native and generated execution support literal, time-dependent, and
//     input-dependent controls
//
// The harness mirrors test_state_identity.cpp's GoldenHarness shape so the
// synth compilation path is exercised through the real eval_cold entry point.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "src/signal_engine/signal_engine.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace sig;

namespace {

struct SynthHarness {
    SignalEngine engine;
    double cell_values[MAX_CELLS] = {};
    double hw_inputs[32] = {};
    double outputs[MAX_OUTPUTS] = {};
    double workspace[MAX_TOTAL_NODES] = {};

    SynthHarness()
    {
        engine.init_defaults();
    }

    EvalResult eval(const std::string& code)
    {
        return eval_cold(code.c_str(), static_cast<uint32_t>(code.size()), engine);
    }

    // Run a GC pass that preserves synth control roots. Direct
    // pool.gc_unreachable_nodes() would orphan synth control expressions
    // (VAL-COMP-011); this helper mirrors the engine's own GC integration.
    void gc()
    {
        register_synth_external_roots(engine);
        engine.pool.gc_unreachable_nodes();
        commit_synth_external_roots(engine);
        engine.pool.rebuild_execution_order();
    }

    bool eval_ok(const std::string& code)
    {
        EvalResult r = eval(code);
        INFO("code: " << code);
        if (r.kind == EvalResult::Error && r.diagnostic_count > 0) {
            for (uint8_t i = 0; i < r.diagnostic_count; i++) {
                INFO("diagnostic[" << i << "]: "
                     << (r.diagnostics[i].message ? r.diagnostics[i].message : ""));
                INFO("suggestion[" << i << "]: "
                     << (r.diagnostics[i].suggestion ? r.diagnostics[i].suggestion : ""));
            }
        }
        return r.kind != EvalResult::Error;
    }

    bool eval_fails(const std::string& code)
    {
        EvalResult r = eval(code);
        return r.kind == EvalResult::Error;
    }

    double sample_control(uint16_t index, double t = 0.0)
    {
        engine.cells.snapshot_values(cell_values, MAX_CELLS);
        ExecutionContext ctx{};
        ctx.t = t;
        ctx.dt = 0.0;
        ctx.cell_values = cell_values;
        ctx.hw_inputs = hw_inputs;
        ctx.data_pool = engine.cells.data_pool;
        ctx.data_offsets = engine.cells.data_offsets;
        ctx.data_lengths = engine.cells.data_lengths;
        ctx.prev_outputs = engine.pool.prev_output_values;
        ctx.output_values = outputs;
        ctx.workspace = workspace;
        execute_all_outputs(engine.pool, ctx);
        REQUIRE(index < engine.synth_graph.control_count());
        uint16_t root = engine.synth_graph.controls[index].root_node;
        REQUIRE(root != NODE_NONE);
        REQUIRE(root < engine.pool.node_count);
        return workspace[root];
    }

    // Returns the first diagnostic message produced by an eval (or "" if none).
    std::string first_message(const std::string& code)
    {
        EvalResult r = eval(code);
        if (r.diagnostic_count == 0) return "";
        return r.diagnostics[0].message ? r.diagnostics[0].message : "";
    }

    // Returns the first diagnostic suggestion produced by an eval.
    std::string first_suggestion(const std::string& code)
    {
        EvalResult r = eval(code);
        if (r.diagnostic_count == 0) return "";
        return r.diagnostics[0].suggestion ? r.diagnostics[0].suggestion : "";
    }
};

// Read the engine's published synth artefact JSON snapshot. The returned
// pointer is stable until the next eval. Returns "" if no synth artefact
// has been published.
std::string snapshot_synth_artifacts(const SynthHarness& h)
{
    const char* json = synth_artifacts_json(h.engine);
    return json ? std::string(json) : std::string();
}

} // namespace

// ============================================================================
// VAL-COMP-001: Minimum sine form compiles
// ============================================================================

TEST_CASE("synth: minimum sine form compiles as identity-keyed declaration",
          "[synth][val-comp-001]") {
    SynthHarness h;

    REQUIRE(h.eval_ok("(synth \"osc/sine\" :freq 440)"));

    // The patch graph must contain exactly one synth declaration.
    const auto& graph = h.engine.synth_graph;
    REQUIRE(graph.declaration_count() == 1);

    // The single declaration must be osc/sine.
    const auto& decl = graph.declarations[0];
    REQUIRE(decl.def_name == std::string("osc/sine"));
    REQUIRE(decl.def_version == 2);
    REQUIRE(decl.audio_inputs == 1);

    // The identity must be non-empty (hidden or explicit). Hidden identity
    // is supplied by the payload builder, so an anonymous form must still
    // receive one.
    REQUIRE(decl.identity != nullptr);
    REQUIRE(decl.identity[0] != '\0');
    REQUIRE(std::string(decl.identity).size() > 0);
}

// ============================================================================
// VAL-COMP-002: Amplitude form compiles with bound frequency and amplitude
// ============================================================================

TEST_CASE("synth: amplitude form compiles with both controls",
          "[synth][val-comp-002]") {
    SynthHarness h;

    REQUIRE(h.eval_ok("(synth \"osc/sine\" :freq 440 :amp 0.1)"));

    const auto& graph = h.engine.synth_graph;
    REQUIRE(graph.declaration_count() == 1);
    REQUIRE(graph.control_count() == 2);

    // Both :freq and :amp must be bound control channels.
    bool has_freq = false, has_amp = false;
    for (uint16_t i = 0; i < graph.control_count(); i++) {
        const auto& ch = graph.controls[i];
        if (ch.param_name == std::string("freq")) has_freq = true;
        if (ch.param_name == std::string("amp"))  has_amp = true;
    }
    REQUIRE(has_freq);
    REQUIRE(has_amp);
}

// ============================================================================
// VAL-COMP-003: Omitted amplitude uses registry default; freq default is 440
// ============================================================================

TEST_CASE("synth: omitted amplitude uses registry default 0.2",
          "[synth][val-comp-003]") {
    SynthHarness h;

    // Omit :amp entirely — no control channel should be allocated for amp.
    REQUIRE(h.eval_ok("(synth \"osc/sine\" :freq 440)"));

    const auto& graph = h.engine.synth_graph;
    REQUIRE(graph.declaration_count() == 1);
    REQUIRE(graph.control_count() == 1);

    // The single channel must be freq.
    REQUIRE(graph.controls[0].param_name == std::string("freq"));

    // The NodeDef registry must declare freq default 440 and amp default 0.2.
    const NodeDefDescriptor* sine = synth_registry_find("osc/sine", 2);
    REQUIRE(sine != nullptr);
    REQUIRE(sine->freq_default == Approx(440.0));
    REQUIRE(sine->amp_default == Approx(0.2));
}

TEST_CASE("synth: optional-control replacement keeps source use live-bounded",
          "[synth][reclaim][arena]") {
    SynthHarness h;
    const std::string without_amp =
        "(synth \"osc/sine\" :name \"bounded\" :freq (+ 400 beat))";
    const std::string with_amp =
        "(synth \"osc/sine\" :name \"bounded\" :freq (+ 400 beat) "
        ":amp (+ 0.1 (* 0.01 bar)))";

    REQUIRE(h.eval_ok(without_amp));
    const uint32_t one_control_bytes = h.engine.arena.write_head;
    REQUIRE(one_control_bytes > 0);

    // N successful absent/present replacements exceed the arena capacity
    // under the old append-only lifecycle. Every iteration has only one live
    // identity and at most two live control sources.
    const uint32_t replacements =
        static_cast<uint32_t>(SOURCE_ARENA_SIZE / 8 + 1);
    uint32_t two_control_bytes = 0;
    for (uint32_t i = 0; i < replacements; i++) {
        INFO("replacement " << i);
        REQUIRE(h.eval_ok(with_amp));
        REQUIRE(h.engine.synth_graph.declaration_count() == 1);
        REQUIRE(h.engine.synth_graph.control_count() == 2);
        if (i == 0) two_control_bytes = h.engine.arena.write_head;
        REQUIRE(h.engine.arena.write_head == two_control_bytes);

        REQUIRE(h.eval_ok(without_amp));
        REQUIRE(h.engine.synth_graph.control_count() == 1);
        REQUIRE(h.engine.arena.write_head == one_control_bytes);
    }

    // N+1 and reuse remain successful after cumulative replacement text far
    // exceeds the fixed arena. A longer same-slot edit may append while
    // staging, then compacts back to exactly the current live source bytes.
    REQUIRE(h.eval_ok(with_amp));
    REQUIRE(h.engine.arena.write_head == two_control_bytes);
    REQUIRE(h.eval_ok(
        "(synth \"osc/sine\" :name \"bounded\" "
        ":freq (+ 400 (* beat 2) (* bar 3)) "
        ":amp (+ 0.1 (* 0.01 bar)))"));
    const uint32_t longer_live_bytes = h.engine.arena.write_head;
    REQUIRE(longer_live_bytes > two_control_bytes);
    REQUIRE(longer_live_bytes < SOURCE_ARENA_SIZE);
    REQUIRE(h.eval_ok(without_amp));
    REQUIRE(h.engine.arena.write_head == one_control_bytes);
}

// ============================================================================
// VAL-COMP-004: Supplied stable identity is authoritative
// ============================================================================

TEST_CASE("synth: explicit identity is preserved across edits",
          "[synth][val-comp-004]") {
    SynthHarness h;

    // First eval: explicit identity via :name
    REQUIRE(h.eval_ok("(synth \"osc/sine\" :name \"lead\" :freq 440)"));
    std::string id_first = h.engine.synth_graph.declarations[0].identity;

    REQUIRE(id_first == std::string("lead"));

    // Second eval: change frequency. Identity must remain "lead" — not
    // replaced by source text, range, ordinal, or hash identity.
    REQUIRE(h.eval_ok("(synth \"osc/sine\" :name \"lead\" :freq 660)"));
    std::string id_second = h.engine.synth_graph.declarations[0].identity;

    REQUIRE(id_second == id_first);
    REQUIRE(id_second == std::string("lead"));
}

// ============================================================================
// VAL-COMP-005: Invalid def names fail clearly
// ============================================================================

TEST_CASE("synth: invalid def names fail with no commit",
          "[synth][val-comp-005]") {
    SynthHarness h;

    // Non-string def name: must fail without committing.
    SECTION("non-string def name") {
        REQUIRE(h.eval_fails("(synth 440 :freq 440)"));
    }

    SECTION("unknown def name") {
        REQUIRE(h.eval_fails("(synth \"osc/unknown\" :freq 440)"));
        // Diagnostic should mention the unknown def.
        std::string msg = h.first_message("(synth \"osc/unknown\" :freq 440)");
        REQUIRE(msg.find("osc/unknown") != std::string::npos);
    }

    SECTION("unavailable def version") {
        // Asking for an explicit version that does not exist must fail.
        REQUIRE(h.eval_fails("(synth \"osc/sine\" :version 99 :freq 440)"));
    }

    // After every failure path the synth graph must be empty (no commit).
    REQUIRE(h.engine.synth_graph.declaration_count() == 0);
}

// ============================================================================
// VAL-COMP-006: Invalid parameters fail clearly
// ============================================================================

TEST_CASE("synth: invalid parameters produce precise diagnostics",
          "[synth][val-comp-006]") {
    SynthHarness h;

    SECTION("unknown parameter with fuzzy suggestion") {
        REQUIRE(h.eval_fails("(synth \"osc/sine\" :freq 440 :amplitude 0.1)"));
        std::string sug = h.first_suggestion(
            "(synth \"osc/sine\" :freq 440 :amplitude 0.1)");
        // The suggestion must point toward the correct parameter name.
        REQUIRE(sug.find("amp") != std::string::npos);
    }

    SECTION("duplicate parameter") {
        REQUIRE(h.eval_fails("(synth \"osc/sine\" :freq 440 :freq 880)"));
        std::string msg = h.first_message(
            "(synth \"osc/sine\" :freq 440 :freq 880)");
        REQUIRE(msg.find("freq") != std::string::npos);
    }

    SECTION("missing value") {
        REQUIRE(h.eval_fails("(synth \"osc/sine\" :freq)"));
    }

    SECTION("malformed pair (keyword then keyword)") {
        REQUIRE(h.eval_fails("(synth \"osc/sine\" :freq :amp 0.1)"));
    }

    SECTION("missing required :freq") {
        REQUIRE(h.eval_fails("(synth \"osc/sine\" :amp 0.1)"));
        std::string msg = h.first_message("(synth \"osc/sine\" :amp 0.1)");
        REQUIRE(msg.find("freq") != std::string::npos);
    }

    SECTION("empty explicit identity") {
        REQUIRE(h.eval_fails(
            "(synth \"osc/sine\" :name \"\" :freq 440)"));
        std::string msg = h.first_message(
            "(synth \"osc/sine\" :name \"\" :freq 440)");
        REQUIRE(msg.find("identity") != std::string::npos);
    }
}

// ============================================================================
// VAL-COMP-007: Synth is top-level-only
// ============================================================================

TEST_CASE("synth: nested synth form is rejected",
          "[synth][val-comp-007]") {
    SynthHarness h;

    // A synth form appears inside an output assignment — must be rejected
    // as boundary violation. The synth graph must remain empty.
    REQUIRE(h.eval_fails("(a1 (synth \"osc/sine\" :freq 440))"));
    REQUIRE(h.engine.synth_graph.declaration_count() == 0);
}

// ============================================================================
// VAL-COMP-008: Multi-form eval is a sequence of form transactions
// ============================================================================

TEST_CASE("synth: multi-form eval retains earlier committed forms",
          "[synth][val-comp-008]") {
    SynthHarness h;

    // First: a successful eval establishes baseline artefacts at revision 1.
    REQUIRE(h.eval_ok("(synth \"osc/sine\" :name \"lead\" :freq 440)"));
    uint32_t rev_after_first = h.engine.synth_graph.revision;
    uint16_t decl_count = h.engine.synth_graph.declaration_count();

    // Second: a multi-form eval where a LATER form fails. The first child is
    // already committed; evaluation stops before any later sibling.
    REQUIRE(h.eval_fails(
        "(do (synth \"osc/sine\" :name \"lead\" :freq 880) "
        "    (synth \"osc/unknown\" :freq 110))"));

    REQUIRE(h.engine.synth_graph.revision > rev_after_first);
    REQUIRE(h.engine.synth_graph.declaration_count() == decl_count);
    REQUIRE(std::string(h.engine.synth_graph.controls[0].param_name) ==
            "freq");
    uint16_t freq_root = h.engine.synth_graph.controls[0].root_node;
    REQUIRE(freq_root != NODE_NONE);
    REQUIRE(h.engine.pool.nodes[freq_root].op == NodeOp::Const);
    REQUIRE(h.engine.pool.nodes[freq_root].imm == Approx(880.0));
}

// ============================================================================
// VAL-COMP-009: Successful artefacts share one revision
// ============================================================================

TEST_CASE("synth: graph and control table share one revision",
          "[synth][val-comp-009]") {
    SynthHarness h;

    REQUIRE(h.eval_ok("(synth \"osc/sine\" :freq 440 :amp 0.1)"));

    uint32_t graph_rev = h.engine.synth_graph.revision;
    // The control table is published in the same graph structure; the
    // public JSON snapshot reports the revision for both.
    std::string snap = snapshot_synth_artifacts(h);
    REQUIRE(snap.find("\"revision\"") != std::string::npos);
    REQUIRE(snap.find(std::to_string(graph_rev)) != std::string::npos);
}

// ============================================================================
// VAL-COMP-010: Failed eval preserves last successful artefacts
// ============================================================================

TEST_CASE("synth: failed eval preserves previous artefacts",
          "[synth][val-comp-010]") {
    SynthHarness h;

    REQUIRE(h.eval_ok("(synth \"osc/sine\" :name \"lead\" :freq 440)"));
    uint32_t rev_ok = h.engine.synth_graph.revision;
    std::string snap_ok = snapshot_synth_artifacts(h);

    // A subsequent failed eval must not advance revision or change the
    // published artefact snapshot.
    REQUIRE(h.eval_fails("(synth \"osc/unknown\" :freq 440)"));

    REQUIRE(h.engine.synth_graph.revision == rev_ok);
    REQUIRE(snapshot_synth_artifacts(h) == snap_ok);
}

TEST_CASE("synth: rejected control compilation restores graph resources",
          "[synth][transaction][state_identity]") {
    SynthHarness h;

    REQUIRE(h.eval_ok(
        "(synth \"osc/sine\" :name \"lead\" "
        ":freq (phasor 1 :id \"synth-phase\") :amp 0.2)"));
    uint32_t rev_ok = h.engine.synth_graph.revision;
    std::string snap_ok = snapshot_synth_artifacts(h);
    uint16_t slots_ok = h.engine.pool.state_slot_count;
    uint16_t entries_ok = h.engine.registry.entry_count;
    uint16_t update_ok = h.engine.pool.state_update_roots[0];

    // The first binding encounters and attempts to rewrite the existing
    // state resource before the later undefined binding rejects the form.
    REQUIRE(h.eval_fails(
        "(synth \"osc/sine\" :name \"lead\" "
        ":freq (phasor 2 :id \"synth-phase\") :amp missing-control)"));

    REQUIRE(h.engine.synth_graph.revision == rev_ok);
    REQUIRE(snapshot_synth_artifacts(h) == snap_ok);
    REQUIRE(h.engine.pool.state_slot_count == slots_ok);
    REQUIRE(h.engine.registry.entry_count == entries_ok);
    REQUIRE(h.engine.pool.state_update_roots[0] == update_ok);
}

TEST_CASE("synth: nested success cannot overwrite outer rollback image",
          "[synth][transaction][nested][rollback]") {
    SynthHarness h;

    REQUIRE(h.eval_ok(
        "(synth \"osc/sine\" :name \"lead\" "
        ":freq (phasor 1 :id \"lead-phase\") :amp 0.2)"));
    const uint32_t revision = h.engine.synth_graph.revision;
    const std::string artifact = snapshot_synth_artifacts(h);
    const uint16_t slots = h.engine.pool.state_slot_count;
    const uint16_t entries = h.engine.registry.entry_count;
    const uint16_t update_root = h.engine.pool.state_update_roots[0];
    const double state_value = h.engine.pool.state_values[0];

    REQUIRE(h.eval_fails(
        "(synth \"osc/sine\" :name \"lead\" "
        ":freq (phasor 2 :id \"lead-phase\") "
        ":fm (synth \"osc/sine\" :name \"child\" :freq 3) "
        ":amp missing-control)"));

    REQUIRE(h.engine.synth_graph.revision == revision);
    REQUIRE(snapshot_synth_artifacts(h) == artifact);
    REQUIRE(h.engine.synth_graph.find("child") == nullptr);
    REQUIRE(h.engine.pool.state_slot_count == slots);
    REQUIRE(h.engine.registry.entry_count == entries);
    REQUIRE(h.engine.pool.state_update_roots[0] == update_root);
    REQUIRE(h.engine.pool.state_values[0] == Approx(state_value));
}

// ============================================================================
// VAL-COMP-011: Synth control roots survive GC
// ============================================================================

TEST_CASE("synth: control roots remain executable after GC",
          "[synth][val-comp-011]") {
    SynthHarness h;

    REQUIRE(h.eval_ok("(synth \"osc/sine\" :name \"lead\" :freq 440)"));

    // Force a garbage collection pass on the engine pool. The synth-aware GC
    // path preserves control roots registered via external_roots[].
    h.gc();

    // The synth control roots must still be present in the control table
    // and still compile to a valid node index.
    const auto& graph = h.engine.synth_graph;
    REQUIRE(graph.control_count() == 1);
    for (uint16_t i = 0; i < graph.control_count(); i++) {
        REQUIRE(graph.controls[i].root_node != NODE_NONE);
        REQUIRE(graph.controls[i].root_node < h.engine.pool.node_count);
    }
}

// ============================================================================
// VAL-COMP-012: Public artefacts use stable identifiers (no internal indices)
// ============================================================================

TEST_CASE("synth: public artefacts use stable identifiers",
          "[synth][val-comp-012]") {
    SynthHarness h;

    REQUIRE(h.eval_ok("(synth \"osc/sine\" :name \"lead\" :freq 440 :amp 0.1)"));

    std::string snap = snapshot_synth_artifacts(h);

    // The serialised artefact must expose the user-visible identity ("lead")
    // and must NOT expose internal GC-remapped node indices. We assert that
    // every declaration entry has an "identity" field and no entry exposes
    // internal "node_index" / "remapped_index" keys.
    REQUIRE(snap.find("\"identity\"") != std::string::npos);
    REQUIRE(snap.find("\"lead\"") != std::string::npos);
    REQUIRE(snap.find("node_index") == std::string::npos);
    REQUIRE(snap.find("remapped") == std::string::npos);
}

// ============================================================================
// VAL-COMP-018: Dynamic control expressions remain executable after commit/GC
// ============================================================================

TEST_CASE("synth: time-dependent and input-dependent controls compile",
          "[synth][val-comp-018]") {
    SynthHarness h;

    SECTION("time-dependent freq expr") {
        REQUIRE(h.eval_ok("(synth \"osc/sine\" :freq (* 220 (sin bar)))"));
        const auto& graph = h.engine.synth_graph;
        REQUIRE(graph.control_count() == 1);

        // Force GC; the control root must remain executable.
        h.gc();

        REQUIRE(graph.controls[0].root_node != NODE_NONE);
        REQUIRE(graph.controls[0].root_node < h.engine.pool.node_count);
    }

    SECTION("input-dependent freq expr") {
        REQUIRE(h.eval_ok("(synth \"osc/sine\" :freq (* 220 (+ 1 ain1)))"));
        const auto& graph = h.engine.synth_graph;
        REQUIRE(graph.control_count() == 1);

        h.gc();

        REQUIRE(graph.controls[0].root_node != NODE_NONE);
    }

    SECTION("amp dependent on cell") {
        REQUIRE(h.eval_ok("(define env 0.5)"));
        REQUIRE(h.eval_ok("(synth \"osc/sine\" :freq 440 :amp env)"));

        const auto& graph = h.engine.synth_graph;
        REQUIRE(graph.control_count() == 2);

        // Changing the cell must keep the synth control table intact.
        REQUIRE(h.eval_ok("(define env 0.9)"));
        REQUIRE(h.engine.synth_graph.control_count() == 2);
    }
}

// ============================================================================
// VAL-COMP-019: bounded M2 declaration capacity fails transactionally
// ============================================================================

TEST_CASE("synth: M2 declaration capacity fails transactionally",
          "[synth][val-comp-019]") {
    SynthHarness h;

    for (uint16_t i = 0; i < MAX_SYNTH_DECLARATIONS; i++) {
        REQUIRE(h.eval_ok("(synth \"osc/sine\" :name \"n" +
                          std::to_string(i) + "\" :freq 440)"));
    }
    REQUIRE(h.engine.synth_graph.declaration_count() ==
            MAX_SYNTH_DECLARATIONS);
    SynthRevision rev_baseline = h.engine.synth_graph.revision;
    std::string snapshot = snapshot_synth_artifacts(h);

    REQUIRE(h.eval_fails(
        "(synth \"osc/sine\" :name \"overflow\" :freq 110)"));
    REQUIRE(h.engine.synth_graph.revision == rev_baseline);
    REQUIRE(snapshot_synth_artifacts(h) == snapshot);

    REQUIRE(h.eval_ok(
        "(synth \"osc/sine\" :name \"n0\" :freq 660)"));
    REQUIRE(h.engine.synth_graph.declaration_count() ==
            MAX_SYNTH_DECLARATIONS);
}

TEST_CASE("synth: firmware control storage covers the shipped registry",
          "[synth][capacity][firmware]") {
    uint16_t registry_count = 0;
    const NodeDefDescriptor* registry =
        synth_registry_table(registry_count);
    uint16_t max_param_count = 0;
    for (uint16_t i = 0; i < registry_count; ++i) {
        if (registry[i].param_count > max_param_count)
            max_param_count = registry[i].param_count;
    }

    REQUIRE(FIRMWARE_MAX_SYNTH_CONTROLS >=
            static_cast<size_t>(SYNTH_MAX_NODES) * max_param_count);
}

TEST_CASE("synth: named FM routing publishes an ABI-2 connection",
          "[synth][routing][m2]") {
    SynthHarness h;
    REQUIRE(h.eval_ok(
        "(synth \"osc/sine\" :name \"lfo\" :freq 2)"));
    REQUIRE(h.eval_ok(
        "(synth \"osc/sine\" :name \"carrier\" :freq 440 "
        ":fm (node \"lfo\"))"));

    const SynthGraph& graph = h.engine.synth_graph;
    REQUIRE(graph.declaration_count() == 2);
    REQUIRE(graph.connection_count() == 1);
    REQUIRE(std::string(graph.connections[0].from) == "lfo");
    REQUIRE(std::string(graph.connections[0].to) == "carrier");
    REQUIRE(std::string(graph.connections[0].port) == "fm");
    REQUIRE(graph.connections[0].port_index == 0);
    std::string json = snapshot_synth_artifacts(h);
    REQUIRE(json.find("\"connections\"") != std::string::npos);
    REQUIRE(json.find("\"port_index\":0") != std::string::npos);
}

TEST_CASE("synth: nested FM source is declared before its connection commits",
          "[synth][routing][nested][m2]") {
    SynthHarness h;
    REQUIRE(h.eval_ok(
        "(synth \"osc/sine\" :name \"carrier\" :freq 440 "
        ":fm (synth \"osc/sine\" :name \"lfo\" :freq 2))"));
    REQUIRE(h.engine.synth_graph.declaration_count() == 2);
    REQUIRE(h.engine.synth_graph.connection_count() == 1);
    REQUIRE(std::string(h.engine.synth_graph.connections[0].from) == "lfo");
    REQUIRE(std::string(h.engine.synth_graph.connections[0].to) == "carrier");
    REQUIRE(h.engine.synth_graph.revision == 1);
}

TEST_CASE("synth: routing failures preserve the complete prior artefact",
          "[synth][routing][transaction][m2]") {
    SynthHarness h;
    REQUIRE(h.eval_ok(
        "(synth \"osc/sine\" :name \"a\" :freq 2)"));
    REQUIRE(h.eval_ok(
        "(synth \"osc/sine\" :name \"b\" :freq 440 :fm (node \"a\"))"));
    SynthRevision baseline_revision = h.engine.synth_graph.revision;
    std::string baseline = snapshot_synth_artifacts(h);

    SECTION("unknown endpoint") {
        REQUIRE(h.eval_fails(
            "(synth \"osc/sine\" :name \"b\" :freq 440 "
            ":fm (node \"missing\"))"));
    }
    SECTION("cross-eval cycle") {
        REQUIRE(h.eval_fails(
            "(synth \"osc/sine\" :name \"a\" :freq 2 "
            ":fm (node \"b\"))"));
    }
    SECTION("arbitrary expression is not an audio endpoint") {
        REQUIRE(h.eval_fails(
            "(synth \"osc/sine\" :name \"b\" :freq 440 :fm (+ 1 2))"));
    }
    REQUIRE(h.engine.synth_graph.revision == baseline_revision);
    REQUIRE(snapshot_synth_artifacts(h) == baseline);
}

TEST_CASE("synth: destination update replaces only its incoming edge",
          "[synth][routing][update][m2]") {
    SynthHarness h;
    REQUIRE(h.eval_ok("(synth \"osc/sine\" :name \"a\" :freq 2)"));
    REQUIRE(h.eval_ok("(synth \"osc/sine\" :name \"c\" :freq 3)"));
    REQUIRE(h.eval_ok(
        "(synth \"osc/sine\" :name \"b\" :freq 440 :fm (node \"a\"))"));
    REQUIRE(h.engine.synth_graph.connection_count() == 1);
    REQUIRE(h.eval_ok(
        "(synth \"osc/sine\" :name \"b\" :freq 440 :fm (node \"c\"))"));
    REQUIRE(h.engine.synth_graph.connection_count() == 1);
    REQUIRE(std::string(h.engine.synth_graph.connections[0].from) == "c");
    REQUIRE(std::string(h.engine.synth_graph.connections[0].to) == "b");
}

TEST_CASE("synth: control ownership is stable across parameter reordering",
          "[synth][state_identity][m2]") {
    SynthHarness h;
    REQUIRE(h.eval_ok(
        "(synth \"osc/sine\" :name \"lead\" "
        ":freq (phasor 1) :amp (phasor 2))"));
    uint16_t slots = h.engine.pool.state_slot_count;
    uint16_t freq_context = h.engine.synth_graph.controls[0].owner_context;
    uint16_t amp_context = h.engine.synth_graph.controls[1].owner_context;
    REQUIRE(freq_context != amp_context);

    REQUIRE(h.eval_ok(
        "(synth \"osc/sine\" :name \"lead\" "
        ":amp (phasor 2) :freq (phasor 1))"));
    REQUIRE(h.engine.pool.state_slot_count == slots);
    const SynthControlChannel* freq = nullptr;
    const SynthControlChannel* amp = nullptr;
    for (uint16_t i = 0; i < h.engine.synth_graph.control_count(); i++) {
        const SynthControlChannel& control =
            h.engine.synth_graph.controls[i];
        if (std::string(control.param_name) == "freq") freq = &control;
        if (std::string(control.param_name) == "amp") amp = &control;
    }
    REQUIRE(freq != nullptr);
    REQUIRE(amp != nullptr);
    REQUIRE(freq->owner_context == freq_context);
    REQUIRE(amp->owner_context == amp_context);
}

TEST_CASE("synth: dynamic control roots execute and react to cell changes",
          "[synth][reactive][execution][m2]") {
    SynthHarness h;
    REQUIRE(h.eval_ok("(define base 220)"));
    REQUIRE(h.eval_ok(
        "(synth \"osc/sine\" :name \"lead\" :freq (+ base (* 10 bar)))"));
    REQUIRE(h.sample_control(0, 0.0) == Approx(220.0));
    REQUIRE(h.sample_control(0, 1.0) == Approx(225.0));

    REQUIRE(h.eval_ok("(define base 330)"));
    REQUIRE(h.sample_control(0, 0.0) == Approx(330.0));
    REQUIRE(h.engine.synth_graph.controls[0].source_length > 0);
}

TEST_CASE("synth: malformed trailing input and invalid version are atomic",
          "[synth][adversarial][transaction]") {
    SynthHarness h;
    REQUIRE(h.eval_ok(
        "(synth \"osc/sine\" :name \"lead\" :freq 440)"));
    std::string baseline = snapshot_synth_artifacts(h);
    SynthRevision revision = h.engine.synth_graph.revision;

    REQUIRE(h.eval_fails(
        "(synth \"osc/sine\" :name \"lead\" :freq 880 999)"));
    REQUIRE(h.eval_fails(
        "(synth \"osc/sine\" :version 2.5 :name \"lead\" :freq 880)"));
    REQUIRE(snapshot_synth_artifacts(h) == baseline);
    REQUIRE(h.engine.synth_graph.revision == revision);
}

// ============================================================================
// Hidden identity: anonymous synth gets a stable hidden id from the payload
// builder. The compiler just needs to accept it and retain it.
// ============================================================================

TEST_CASE("synth: anonymous form retains supplied hidden identity",
          "[synth][synth-anon-identity]") {
    SynthHarness h;

    // A hidden :id keyword mirrors what the payload builder injects for an
    // anonymous synth. The compiler must accept it and treat it as the
    // authoritative identity.
    REQUIRE(h.eval_ok("(synth \"osc/sine\" :id \"::anon-1\" :freq 440)"));
    REQUIRE(std::string(h.engine.synth_graph.declarations[0].identity)
            == std::string("::anon-1"));

    // Re-evaluating with the same hidden id must be treated as an
    // update-in-place rather than capacity overflow.
    REQUIRE(h.eval_ok("(synth \"osc/sine\" :id \"::anon-1\" :freq 880)"));
    REQUIRE(h.engine.synth_graph.declaration_count() == 1);
}

// ============================================================================
// with-state-id wrapper identity (ergo e58f128f). The editor payload builder
// wraps anonymous top-level synth forms in `(with-state-id "<id>" ...)`.
// Per state-identity.md §2.2 the wrapper and `:id` normalise to the same
// internal identity annotation, so the wrapper id must become the synth's
// identity when the form carries no explicit :name/:id. Resolution order:
// explicit :name/:id > wrapper id > anonymous per-eval ordinal fallback.
// ============================================================================

TEST_CASE("synth: with-state-id wrapper id becomes anonymous synth identity",
          "[synth][with-state-id][synth-anon-identity]") {
    SynthHarness h;

    // First eval instantiates under the wrapper-supplied identity.
    REQUIRE(h.eval_ok(
        "(with-state-id \"sid-A\" (synth \"osc/sine\" :freq 440))"));
    REQUIRE(h.engine.synth_graph.declaration_count() == 1);
    REQUIRE(std::string(h.engine.synth_graph.declarations[0].identity)
            == std::string("sid-A"));

    // Re-eval with a changed param must be update-in-place — same identity,
    // no "another identity already active" capacity error — across at
    // least 3 re-evals (M1 acceptance, synth-nodes.md §5.1/§5.5).
    REQUIRE(h.eval_ok(
        "(with-state-id \"sid-A\" (synth \"osc/sine\" :freq 660))"));
    REQUIRE(h.engine.synth_graph.declaration_count() == 1);
    REQUIRE(std::string(h.engine.synth_graph.declarations[0].identity)
            == std::string("sid-A"));

    REQUIRE(h.eval_ok(
        "(with-state-id \"sid-A\" (synth \"osc/sine\" :freq 660))"));
    REQUIRE(h.eval_ok(
        "(with-state-id \"sid-A\" (synth \"osc/sine\" :freq 550))"));
    REQUIRE(h.engine.synth_graph.declaration_count() == 1);
    REQUIRE(std::string(h.engine.synth_graph.declarations[0].identity)
            == std::string("sid-A"));
}

TEST_CASE("synth: explicit :name takes precedence over with-state-id wrapper",
          "[synth][with-state-id][val-comp-004]") {
    SynthHarness h;

    // The user-visible :name is authoritative; the wrapper id is sidecar
    // metadata (synth-nodes.md §5.1: :name is sugar for the state identity).
    REQUIRE(h.eval_ok(
        "(with-state-id \"sid-B\" "
        "(synth \"osc/sine\" :name \"lead\" :freq 440))"));
    REQUIRE(h.engine.synth_graph.declaration_count() == 1);
    REQUIRE(std::string(h.engine.synth_graph.declarations[0].identity)
            == std::string("lead"));

    // Re-eval under the same wrapper + name stays one declaration.
    REQUIRE(h.eval_ok(
        "(with-state-id \"sid-B\" "
        "(synth \"osc/sine\" :name \"lead\" :freq 660))"));
    REQUIRE(h.engine.synth_graph.declaration_count() == 1);
    REQUIRE(std::string(h.engine.synth_graph.declarations[0].identity)
            == std::string("lead"));
}

TEST_CASE("synth: wrapper id does not leak past its wrapped form",
          "[synth][with-state-id]") {
    SynthHarness h;

    // A named synth under the wrapper consumes the pending wrapper id
    // (first-synth-wins); the id must be cleared when the wrapper form
    // ends either way. A later anonymous synth (after clearing the graph)
    // must fall back to the anonymous scheme, not inherit "sid-C".
    REQUIRE(h.eval_ok(
        "(with-state-id \"sid-C\" "
        "(synth \"osc/sine\" :name \"lead\" :freq 440))"));
    REQUIRE(h.eval_ok("(useq-clear)"));
    REQUIRE(h.eval_ok("(synth \"osc/sine\" :freq 220)"));
    REQUIRE(h.engine.synth_graph.declaration_count() == 1);
    REQUIRE(std::string(h.engine.synth_graph.declarations[0].identity)
            != std::string("sid-C"));
}

TEST_CASE("synth: useq-clear publishes one empty-graph revision",
          "[synth][clear][revision]") {
    SynthHarness h;
    REQUIRE(h.eval_ok("(synth \"osc/sine\" :freq 440)"));
    SynthRevision before = h.engine.synth_graph.revision;

    REQUIRE(h.eval_ok("(useq-clear)"));
    REQUIRE(h.engine.synth_graph.declaration_count() == 0);
    REQUIRE(h.engine.synth_graph.control_count() == 0);
    REQUIRE(h.engine.synth_graph.revision == before + 1);

    // A later form's error cannot resurrect the pre-clear graph: its control
    // roots referred to the node pool that clear has reclaimed.
    REQUIRE_FALSE(h.eval_ok("(synth \"osc/sine\" :freq 220) (useq-clear) "
                            "(set-bpm nope)"));
    REQUIRE(h.engine.synth_graph.declaration_count() == 0);
    REQUIRE(h.engine.synth_graph.control_count() == 0);
    REQUIRE(h.engine.pool.external_root_count == 0);
}

TEST_CASE("synth: unwrapped anonymous synth re-eval updates in place",
          "[synth][synth-anon-identity]") {
    SynthHarness h;

    // state-identity.md §2.5: the anonymous fallback derives from the
    // ordinal position within the compile, so re-evaluating the same
    // program reuses the identity instead of leaking one per eval (and,
    // in M1, instead of failing the single-node capacity check).
    REQUIRE(h.eval_ok("(synth \"osc/sine\" :freq 440)"));
    std::string first_id = h.engine.synth_graph.declarations[0].identity;
    REQUIRE(h.eval_ok("(synth \"osc/sine\" :freq 660)"));
    REQUIRE(h.engine.synth_graph.declaration_count() == 1);
    REQUIRE(std::string(h.engine.synth_graph.declarations[0].identity)
            == first_id);
}

// ============================================================================
// GC pairing: param-only re-evals must not leak pool nodes. Before the
// commit-time GC in eval_cold, 40 re-evals grew node_count 12 -> 88 with
// zero reclamation (ergo 72ff4fa5); the pool (MAX_TOTAL_NODES) would fill
// after ~100 edits and unrelated compiles would start failing.
// ============================================================================

TEST_CASE("synth: param re-evals reclaim replaced control graphs",
          "[synth][synth-gc-pairing]") {
    SynthHarness h;

    // Establish the steady-state shape first, then capture the baseline.
    REQUIRE(h.eval_ok("(synth \"osc/sine\" :name \"lead\" "
                      ":freq (+ 0 (* 2 bar)))"));
    uint16_t baseline = h.engine.pool.node_count;

    for (int i = 1; i <= 40; i++) {
        std::string code = "(synth \"osc/sine\" :name \"lead\" :freq (+ " +
                           std::to_string(i) + " (* 2 bar)))";
        REQUIRE(h.eval_ok(code));
    }

    // Each re-eval compiles a fresh param graph; commit-time GC must
    // reclaim the replaced one so the pool stays bounded near baseline
    // instead of growing linearly.
    REQUIRE(h.engine.pool.node_count <= baseline + 8);

    // The surviving declaration and its control roots must stay valid.
    REQUIRE(h.engine.synth_graph.declaration_count() == 1);
    for (uint16_t i = 0; i < h.engine.synth_graph.control_count(); i++) {
        REQUIRE(h.engine.synth_graph.controls[i].root_node
                < h.engine.pool.node_count);
    }
}
