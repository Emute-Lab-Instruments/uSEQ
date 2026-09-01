#include "cold_eval.h"

#include <cmath>
#include "token.h"
#include "graph_builder.h"
#include "compiler_pipeline.h"
#include "executor.h"
#if USEQ_HAS_TEMPORAL_QUERY
#include "temporal_query.h"
#endif
#if USEQ_HAS_SYNTH_ENGINE
#include "synth_registry.h"
#endif
#include "../devtools/devtools.h"
#include "../modulisp/lisp/symbol_intern.h"
#include <cstdio>
#include <cstring>
#include <new>

namespace sig {

namespace {
struct RuntimeMemorySampleScope {
    RuntimeMemorySampleScope() { dt::sample_runtime_memory(); }
    ~RuntimeMemorySampleScope() { dt::sample_runtime_memory(); }
};
} // namespace

// ── SignalEngine::init_defaults ────────────────────────────────────────────

void SignalEngine::init_defaults(Sample bpm, int beats_per_bar,
                                 int bars_per_phrase, int phrases_per_section) {
    GraphBuilder::init_symbols();
    state = EngineState{};
    session_generation = 0;
    reset_session_storage(bpm, beats_per_bar, bars_per_phrase,
                          phrases_per_section, false);
}

void SignalEngine::reset_session_storage(Sample bpm, int beats_per_bar,
                                         int bars_per_phrase,
                                         int phrases_per_section,
                                         bool publish_session_clear) {
    cells.reset(bpm, beats_per_bar, bars_per_phrase, phrases_per_section);
    arena.reset();
    pool.reset();
    scratch_pool.reset();
    for (uint16_t i = 0; i < MAX_OUTPUTS; i++)
        output_sources[i] = OutputSource{};
    for (uint16_t i = 0; i < MAX_OUTPUTS; i++)
        output_compile_diagnostics[i] = ActiveCompileDiagnostic{};
    for (uint16_t i = 0; i < MAX_STATE_SLOTS; i++) {
        state_sources[i] = StateUpdateSource{};
        state_compile_diagnostics[i] = ActiveCompileDiagnostic{};
    }
    registry.clear();
#if USEQ_HAS_SYNTH_ENGINE
    if (publish_session_clear) {
        SynthRevision next_revision = synth_graph.revision + 1;
        synth_graph = SynthGraph{};
        synth_graph.revision = next_revision;
    } else {
        synth_graph = SynthGraph{};
    }
#endif
    if (publish_session_clear) session_generation++;
    memset(eval_text_buf, 0, sizeof(eval_text_buf));
#if USEQ_HAS_SYNTH_ENGINE
    memset(pending_state_identity, 0, sizeof(pending_state_identity));
    has_pending_state_identity = false;
    eval_anon_synth_ordinal = 0;
#endif
}

// ── Helper constructors ─────────────────────────────────────────────────────

static EvalResult make_ok() {
    EvalResult r;
    r.kind = EvalResult::Ok;
    return r;
}

static EvalResult make_number(Sample v) {
    EvalResult r;
    r.kind = EvalResult::Number;
    r.number = v;
    return r;
}

static EvalResult make_error(const char* message, const char* suggestion) {
    EvalResult r;
    r.kind = EvalResult::Error;
    if (r.diagnostic_count < 8) {
        r.diagnostics[r.diagnostic_count++] = {
            DiagnosticSeverity::Error, DiagnosticCategory::Runtime,
            0, 0, message, suggestion
        };
    }
    return r;
}

// Cell/callable arrays are sized MAX_CELLS but symbol IDs are unbounded
// (the interner keeps handing out fresh IDs). Every cell WRITE path must
// bounds-check the symbol ID or it writes out of bounds (A1). Read paths
// already guard.
static bool cell_id_out_of_range(SymbolID sym) {
    return sym >= MAX_CELLS;
}

static EvalResult make_too_many_definitions_error() {
    EvalResult r;
    r.kind = EvalResult::Error;
    r.diagnostics[r.diagnostic_count++] = {
        DiagnosticSeverity::Error, DiagnosticCategory::Overflow,
        0, 0, "Too many definitions — no cell space left for this name",
        "Remove unused definitions or reuse existing names"
    };
    return r;
}

static void publish_reactive_diagnostic(ActiveCompileDiagnostic& active,
                                        SymbolID triggered_by,
                                        const GraphBuildResult& result) {
    active = ActiveCompileDiagnostic{};
    active.active = true;
    active.triggered_by = triggered_by;
    if (result.diagnostic_count > 0) {
        active.diagnostic = result.diagnostics[0];
    } else {
        active.diagnostic = {
            DiagnosticSeverity::Error, DiagnosticCategory::Runtime,
            0, 0,
            "A dependency change could not be applied; the previous program is still running",
            "Repair the changed definition"
        };
    }
}

#if USEQ_HAS_SYNTH_ENGINE
static void publish_synth_reactive_diagnostic(
        SynthControlChannel& control, SymbolID triggered_by,
        const Diagnostic& diagnostic) {
    control.compile_diagnostic.publish(triggered_by, diagnostic);
}

static void clear_synth_reactive_diagnostic(SynthControlChannel& control) {
    control.compile_diagnostic.clear();
}
#endif

// Publication is deliberately separate from graph construction: the bounded
// GraphBuildResult is a candidate IR until one of these functions installs its
// root, dependencies, ownership, and health as one coherent state change.
static void publish_output_graph_plan(SignalEngine& engine,
                                      uint16_t output_index,
                                      const GraphBuildResult& result) {
    engine.pool.outputs[output_index].root_node = result.root_node;
    engine.pool.outputs[output_index].valid = true;
    engine.pool.runtime_fallback_mask &= ~((uint64_t)1 << output_index);
    engine.output_compile_diagnostics[output_index] =
        ActiveCompileDiagnostic{};
    engine.pool.output_deps[output_index].clear();
    for (uint8_t d = 0; d < result.dep_count; d++)
        engine.pool.output_deps[output_index].add(result.dep_cells[d]);
    engine.registry.commit_context(output_index,
                                   engine.pool.state_update_roots,
                                   engine.pool.state_owner_context);
}

static void publish_state_update_plan(SignalEngine& engine,
                                      uint16_t state_slot,
                                      const GraphBuildResult& result) {
    engine.pool.state_update_roots[state_slot] = result.root_node;
    engine.state_sources[state_slot].dep_count = result.dep_count;
    for (uint8_t d = 0; d < result.dep_count; d++)
        engine.state_sources[state_slot].dep_cells[d] = result.dep_cells[d];
    const uint16_t owner_context =
        static_cast<uint16_t>(MAX_OUTPUTS + state_slot);
    engine.registry.commit_context(owner_context,
                                   engine.pool.state_update_roots,
                                   engine.pool.state_owner_context);
    engine.state_compile_diagnostics[state_slot] =
        ActiveCompileDiagnostic{};
}

// ── Cold-path form evaluation ───────────────────────────────────────────────

static EvalResult eval_form(TokenStream& ts, SignalEngine& engine,
                            const char* source, uint32_t source_length,
                            SharedLiveEditIDs* shared_ids = nullptr);

// ── Live-edit argument rejection helper ────────────────────────────────────
// Returns true if the next form in the token stream is (live-edit ...).
// Does not consume any tokens.

static bool next_is_live_edit(const TokenStream& ts) {
    if (ts.pos + 1 >= ts.count) return false;
    if (ts.tokens[ts.pos].kind != TokenKind::LParen) return false;
    if (ts.tokens[ts.pos + 1].kind != TokenKind::Symbol) return false;
    return ts.tokens[ts.pos + 1].symbol == GraphBuilder::sym.live_edit;
}

// ── Source span helpers ─────────────────────────────────────────────────────

static uint32_t span_begin(const TokenStream& ts, uint16_t token_pos) {
    return ts.tokens[token_pos].span_start;
}

static uint32_t span_end_of(const TokenStream& ts, uint16_t token_pos) {
    if (token_pos == 0) return 0;
    const Token& last = ts.tokens[token_pos - 1];
    return last.span_start + last.span_len;
}

// Reclaim fixed-pool resources from the graph that was actually published.
// State update roots are retained only when a published root (or a live named
// defstate cell) reaches their slot. This preserves a retained LKG graph after
// failed reactive compilation while successful replacement releases ghosts.
static void compact_reachable_state_slots(SignalEngine& engine) {
    NodePool& pool = engine.pool;
    if (pool.state_slot_count == 0) return;

    bool queued[MAX_TOTAL_NODES] = {};
    bool slot_live[MAX_STATE_SLOTS] = {};
    uint16_t stack[MAX_TOTAL_NODES] = {};
    uint16_t stack_top = 0;
    auto push = [&](uint16_t node) {
        if (node == NODE_NONE || node >= pool.node_count || queued[node]) return;
        queued[node] = true;
        if (stack_top < MAX_TOTAL_NODES) stack[stack_top++] = node;
    };
    for (uint16_t o = 0; o < MAX_OUTPUTS; o++)
        push(pool.outputs[o].root_node);
#if USEQ_HAS_SYNTH_ENGINE
    for (uint16_t e = 0; e < pool.external_root_count; e++)
        push(pool.external_roots[e]);
#endif
    for (uint32_t c = 0; c < MAX_CELLS; c++) {
        const Cell& cell = engine.cells.cells[c];
        if (cell.kind != CellKind::Number || cell.flags != 0x02) continue;
        uint16_t slot = cell.data_table_id;
        if (slot >= pool.state_slot_count) continue;
        slot_live[slot] = true;
        push(pool.state_update_roots[slot]);
    }
    while (stack_top > 0) {
        uint16_t idx = stack[--stack_top];
        const Node& node = pool.nodes[idx];
        if (node.op == NodeOp::LoadState) {
            uint16_t slot = (uint16_t)node.imm;
            if (slot < pool.state_slot_count && !slot_live[slot]) {
                slot_live[slot] = true;
                push(pool.state_update_roots[slot]);
            }
        }
        push(node.input_a);
        push(node.input_b);
        push(node.input_c);
    }

    uint16_t remap[MAX_STATE_SLOTS];
    for (uint16_t s = 0; s < MAX_STATE_SLOTS; s++) remap[s] = NODE_NONE;
    uint16_t new_count = 0;
    for (uint16_t old = 0; old < pool.state_slot_count; old++) {
        if (!slot_live[old]) continue;
        uint16_t fresh = new_count++;
        remap[old] = fresh;
        if (fresh != old) {
            pool.state_values[fresh] = pool.state_values[old];
            pool.state_observed[fresh] = pool.state_observed[old];
            pool.state_update_roots[fresh] = pool.state_update_roots[old];
            pool.state_owner_context[fresh] = pool.state_owner_context[old];
            engine.state_sources[fresh] = engine.state_sources[old];
            engine.state_compile_diagnostics[fresh] =
                engine.state_compile_diagnostics[old];
        }
    }
    if (new_count == pool.state_slot_count) return;

    for (uint16_t n = 0; n < pool.node_count; n++) {
        if (pool.nodes[n].op != NodeOp::LoadState) continue;
        uint16_t old = (uint16_t)pool.nodes[n].imm;
        if (old < MAX_STATE_SLOTS && remap[old] != NODE_NONE)
            pool.nodes[n].imm = (Sample)remap[old];
    }
    for (uint32_t c = 0; c < MAX_CELLS; c++) {
        Cell& cell = engine.cells.cells[c];
        if (cell.kind != CellKind::Number || cell.flags != 0x02) continue;
        uint16_t old = cell.data_table_id;
        if (old < MAX_STATE_SLOTS && remap[old] != NODE_NONE)
            cell.data_table_id = remap[old];
    }

    uint16_t new_entry_count = 0;
    for (uint16_t i = 0; i < engine.registry.entry_count; i++) {
        StateResourceEntry entry = engine.registry.entries[i];
        if (entry.slot_index >= MAX_STATE_SLOTS ||
            remap[entry.slot_index] == NODE_NONE) continue;
        entry.slot_index = remap[entry.slot_index];
        if (entry.owner_context >= MAX_OUTPUTS &&
            entry.owner_context < MAX_OUTPUTS + MAX_STATE_SLOTS) {
            uint16_t owner_slot = entry.owner_context - MAX_OUTPUTS;
            if (remap[owner_slot] != NODE_NONE)
                entry.owner_context = (uint16_t)(MAX_OUTPUTS + remap[owner_slot]);
        }
        if ((entry.key.state_id & ANON_STATE_ID_BASE) != 0) {
            uint16_t context = (uint16_t)
                ((entry.key.state_id & ~ANON_STATE_ID_BASE) >> 12);
            if (context >= MAX_OUTPUTS &&
                context < MAX_OUTPUTS + MAX_STATE_SLOTS) {
                uint16_t owner_slot = context - MAX_OUTPUTS;
                if (remap[owner_slot] != NODE_NONE) {
                    StateID ordinal = entry.key.state_id & 0xFFFu;
                    entry.key.state_id = make_anon_state_id(
                        (uint16_t)(MAX_OUTPUTS + remap[owner_slot]),
                        (uint16_t)ordinal);
                }
            }
        }
        engine.registry.entries[new_entry_count++] = entry;
    }
    for (uint16_t i = new_entry_count; i < engine.registry.entry_count; i++)
        engine.registry.entries[i] = StateResourceEntry{};
    engine.registry.entry_count = new_entry_count;
    engine.registry.free_slot_count = 0;
    for (uint16_t i = 0; i < MAX_STATE_SLOTS; i++)
        engine.registry.free_slots[i] = NODE_NONE;

    for (uint16_t i = 0; i < pool.live_slot_count; i++) {
        uint16_t context = pool.live_slots[i].owner_context;
        if (context < MAX_OUTPUTS || context >= MAX_OUTPUTS + MAX_STATE_SLOTS)
            continue;
        uint16_t owner_slot = context - MAX_OUTPUTS;
        if (remap[owner_slot] != NODE_NONE)
            pool.live_slots[i].owner_context =
                (uint16_t)(MAX_OUTPUTS + remap[owner_slot]);
    }
    for (uint16_t s = 0; s < new_count; s++) {
        uint16_t context = pool.state_owner_context[s];
        if (context < MAX_OUTPUTS || context >= MAX_OUTPUTS + MAX_STATE_SLOTS)
            continue;
        uint16_t owner_slot = context - MAX_OUTPUTS;
        if (remap[owner_slot] != NODE_NONE)
            pool.state_owner_context[s] =
                (uint16_t)(MAX_OUTPUTS + remap[owner_slot]);
    }
    for (uint16_t s = new_count; s < pool.state_slot_count; s++) {
        pool.state_values[s] = 0.0;
        pool.state_observed[s] = false;
        pool.state_update_roots[s] = NODE_NONE;
        pool.state_owner_context[s] = ANON_STATE_CONTEXT_NONE;
        engine.state_sources[s] = StateUpdateSource{};
        engine.state_compile_diagnostics[s] = ActiveCompileDiagnostic{};
    }
    uint64_t remapped_state_failures = 0;
    for (uint16_t old = 0; old < MAX_STATE_SLOTS; old++) {
        if (remap[old] == NODE_NONE) continue;
        if ((pool.state_update_failure_mask & ((uint64_t)1 << old)) != 0)
            remapped_state_failures |= (uint64_t)1 << remap[old];
    }
    pool.state_update_failure_mask = remapped_state_failures;
    pool.state_slot_count = new_count;
}

static void compact_reachable_live_slots(SignalEngine& engine) {
    NodePool& pool = engine.pool;
    if (pool.live_slot_count == 0) return;
    bool live[MAX_LIVE_SLOTS] = {};
    for (uint16_t n = 0; n < pool.node_count; n++) {
        if (pool.nodes[n].op != NodeOp::SlotLoad) continue;
        uint16_t slot = (uint16_t)pool.nodes[n].imm;
        if (slot < pool.live_slot_count) live[slot] = true;
    }
    uint16_t remap[MAX_LIVE_SLOTS];
    for (uint16_t i = 0; i < MAX_LIVE_SLOTS; i++) remap[i] = NODE_NONE;
    uint16_t new_count = 0;
    for (uint16_t old = 0; old < pool.live_slot_count; old++) {
        if (!live[old]) continue;
        uint16_t fresh = new_count++;
        remap[old] = fresh;
        if (fresh != old) pool.live_slots[fresh] = pool.live_slots[old];
    }
    if (new_count == pool.live_slot_count) return;
    for (uint16_t n = 0; n < pool.node_count; n++) {
        if (pool.nodes[n].op != NodeOp::SlotLoad) continue;
        uint16_t old = (uint16_t)pool.nodes[n].imm;
        if (old < MAX_LIVE_SLOTS && remap[old] != NODE_NONE)
            pool.nodes[n].imm = (Sample)remap[old];
    }
    for (uint16_t o = 0; o < MAX_OUTPUTS; o++) {
        OutputDeps& deps = pool.output_deps[o];
        uint16_t kept = 0;
        for (uint16_t i = 0; i < deps.slot_count; i++) {
            uint16_t old = deps.slots[i];
            if (old < MAX_LIVE_SLOTS && remap[old] != NODE_NONE)
                deps.slots[kept++] = remap[old];
        }
        deps.slot_count = kept;
    }
    for (uint16_t s = new_count; s < pool.live_slot_count; s++)
        pool.live_slots[s] = NodePool::LiveSlot{};
    pool.live_slot_count = new_count;
}

static void reclaim_unowned_resources(SignalEngine& engine) {
#if USEQ_HAS_SYNTH_ENGINE
    register_synth_external_roots(engine);
#endif
    compact_reachable_state_slots(engine);
    engine.pool.gc_unreachable_nodes();
    compact_reachable_live_slots(engine);
    engine.pool.gc_unreachable_nodes();
#if USEQ_HAS_SYNTH_ENGINE
    commit_synth_external_roots(engine);
#endif
}

// Compact every source region still owned by published compiler state.
//
// SourceArena::store_reuse() prevents growth when a replacement fits its old
// slot, but a deliberately omitted synth control has no live slot to reuse
// when it is later reintroduced. Alternating an optional control present /
// absent therefore used to consume the append-only arena until a one-node
// patch could no longer be edited. Successful synth publication is a safe
// compaction boundary: parsing and graph construction are complete, all live
// owners have their final offsets, and no later step in the transaction can
// fail.
//
// The high bit is a temporary in-place visited marker. SOURCE_ARENA_SIZE is
// far below 2^31 in every build profile, so no valid arena offset can carry
// it. Repeated minimum selection avoids a large firmware-stack descriptor
// array; the bounded O(owner_count^2) work occurs only on the cold edit path.
static void compact_live_source_arena(SignalEngine& engine) {
    constexpr uint32_t moved_bit = UINT32_C(0x80000000);

    auto visit_live_sources = [&](auto&& visit) {
        for (uint32_t c = 0; c < MAX_CELLS; c++) {
            if (engine.cells.cells[c].kind != CellKind::Callable) continue;
            CallableInfo& info = engine.cells.callables[c];
            if (info.source_length > 0)
                visit(info.source_offset, info.source_length);
        }
        for (uint16_t o = 0; o < MAX_OUTPUTS; o++) {
            OutputSource& output = engine.output_sources[o];
            if (output.has_source && output.arena_length > 0)
                visit(output.arena_offset, output.arena_length);
        }
        for (uint16_t s = 0; s < MAX_STATE_SLOTS; s++) {
            StateUpdateSource& state = engine.state_sources[s];
            if (state.has_source && state.arena_length > 0)
                visit(state.arena_offset, state.arena_length);
        }
#if USEQ_HAS_SYNTH_ENGINE
        for (uint16_t i = 0; i < engine.synth_graph.control_count(); i++) {
            SynthControlChannel& control = engine.synth_graph.controls[i];
            if (control.source_length > 0)
                visit(control.source_offset, control.source_length);
        }
#endif
    };

    // Validate every reference before changing any offset. Published state
    // should make this impossible; returning preserves the debuggable corrupt
    // state rather than turning a bad reference into an out-of-bounds move.
    bool valid = true;
    visit_live_sources([&](uint32_t& offset, uint32_t length) {
        if (offset >= SOURCE_ARENA_SIZE ||
            length > SOURCE_ARENA_SIZE - offset ||
            offset + length > engine.arena.write_head) {
            valid = false;
        }
    });
    if (!valid) return;

    uint32_t compacted_head = 0;
    while (true) {
        uint32_t next_offset = UINT32_MAX;
        uint32_t next_length = 0;
        visit_live_sources([&](uint32_t& offset, uint32_t length) {
            if ((offset & moved_bit) != 0) return;
            if (offset < next_offset) {
                next_offset = offset;
                next_length = length;
            } else if (offset == next_offset && length > next_length) {
                // Shared regions are not currently emitted, but treating an
                // exact-offset alias as one region keeps compaction coherent.
                next_length = length;
            }
        });
        if (next_offset == UINT32_MAX) break;

        if (next_length > 0 && compacted_head != next_offset) {
            memmove(engine.arena.data + compacted_head,
                    engine.arena.data + next_offset, next_length);
        }
        visit_live_sources([&](uint32_t& offset, uint32_t) {
            if (offset == next_offset) offset = moved_bit | compacted_head;
        });
        compacted_head += next_length;
    }

    visit_live_sources([&](uint32_t& offset, uint32_t) {
        offset &= ~moved_bit;
    });
    engine.arena.write_head = compacted_head;
}

static bool source_aliases_live_arena(const char* source,
                                      const SignalEngine& engine) {
    if (!source) return false;
    uintptr_t address = reinterpret_cast<uintptr_t>(source);
    uintptr_t begin = reinterpret_cast<uintptr_t>(engine.arena.data);
    uintptr_t end = begin + SOURCE_ARENA_SIZE;
    return address >= begin && address < end;
}

static bool parse_numeric_vector(TokenStream& ts, Sample* values,
                                 uint16_t& count, EvalResult& error) {
    if (!ts.expect(TokenKind::LBracket)) {
        error = make_error("Expected a vector", "Try: [1 2 3]");
        return false;
    }
    count = 0;
    while (ts.peek().kind != TokenKind::RBracket && !ts.at_end()) {
        if (count >= 64) {
            error = make_error("Vector definition is too long (max 64 values)",
                               "Split the data into smaller vectors");
            return false;
        }
        Token element = ts.consume();
        if (element.kind != TokenKind::Number) {
            error = make_error(
                "Vector definitions require numeric literal elements",
                "Replace the nonnumeric element or use a signal expression outside the data vector");
            return false;
        }
        values[count++] = element.number;
    }
    if (!ts.expect(TokenKind::RBracket)) {
        error = make_error("Vector definition is missing ']'",
                           "Close the vector with ]");
        return false;
    }
    return true;
}

// ── define ──────────────────────────────────────────────────────────────────

static EvalResult do_define(TokenStream& ts, SignalEngine& engine,
                            const char* source, uint32_t source_length) {
    Token name_tok = ts.consume();
    if (name_tok.kind != TokenKind::Symbol) {
        return make_error("define needs a name", "Try: (define freq 440)");
    }
    SymbolID sym = name_tok.symbol;
    if (cell_id_out_of_range(sym)) return make_too_many_definitions_error();

    Token val_tok = ts.peek();

    if (val_tok.kind == TokenKind::Number) {
        // Simple numeric constant
        ts.consume();
        if (ts.peek().kind != TokenKind::RParen) {
            return make_error("define accepts exactly one value",
                              "Try: (define name value)");
        }
        engine.cells.cells[sym].kind = CellKind::Number;
        engine.cells.cells[sym].revision++;
        engine.cells.cells[sym].value = val_tok.number;
        // A plain define establishes a fresh, non-state binding. Clear any stale
        // defstate marker (flags 0x02) — otherwise graph_builder keeps emitting a
        // state_load from the old slot and this define is silently ignored.
        engine.cells.cells[sym].flags = 0;
        engine.cells.callables[sym] = CallableInfo{};
    }
    else if (val_tok.kind == TokenKind::LBracket) {
        // Vector data: [1 2 3 4]
        Sample values[64];
        uint16_t count = 0;
        EvalResult parse_error;
        if (!parse_numeric_vector(ts, values, count, parse_error))
            return parse_error;
        if (ts.peek().kind != TokenKind::RParen)
            return make_error("define accepts exactly one value",
                              "Try: (define name [1 2 3])");

        uint16_t table_id = engine.cells.store_data_table(values, count);
        if (table_id == UINT8_MAX)
            return make_error("Data table storage is full — definition not applied",
                              "Free space with (useq-clear) or reuse an existing vector");
        engine.cells.cells[sym].kind = CellKind::Data;
        engine.cells.cells[sym].data_table_id = table_id;
        engine.cells.cells[sym].revision++;
        engine.cells.cells[sym].value = (Sample)count;
        engine.cells.cells[sym].flags = 0; // clear stale defstate marker (see above)
        engine.cells.callables[sym] = CallableInfo{};
    }
    else {
        // Expression — store source text as callable with 0 params
        uint16_t expr_start = ts.pos;
        uint32_t byte_start = span_begin(ts, expr_start);

        // Skip past the expression to find its extent
        GraphBuilder::skip_form(ts);
        uint32_t byte_end = span_end_of(ts, ts.pos);

        if (ts.peek().kind != TokenKind::RParen) {
            return make_error("define accepts exactly one value",
                              "Try: (define name value)");
        }

        // Copy the expression source text into the arena FIRST. If the arena
        // is full, fail the define without touching the cell — otherwise the
        // cell would keep its OLD source text and dependents would silently
        // recompile a stale definition (F5).
        uint32_t offset = UINT32_MAX;
        uint32_t len = 0;
        if (source && byte_end > byte_start && byte_end <= source_length) {
            len = byte_end - byte_start;
            const CallableInfo previous =
                engine.cells.cells[sym].kind == CellKind::Callable
                    ? engine.cells.callables[sym] : CallableInfo{};
            offset = engine.arena.store_reuse(
                previous.source_offset, previous.source_length,
                source + byte_start, len);
            if (offset == UINT32_MAX) {
                return make_error(
                    "Program storage is full — definition not applied",
                    "Free space with (useq-clear) or shorten your program");
            }
        }

        engine.cells.cells[sym].kind = CellKind::Callable;
        engine.cells.cells[sym].revision++;
        engine.cells.cells[sym].flags = 0; // clear stale defstate marker (see above)
        engine.cells.callables[sym].param_count = 0;
        if (offset != UINT32_MAX) {
            engine.cells.callables[sym].source_offset = offset;
            engine.cells.callables[sym].source_length = len;
        }
    }

    // Notify dependents
    on_cell_changed(sym, engine);

    return make_ok();
}

// ── defn ────────────────────────────────────────────────────────────────────

static EvalResult do_defn(TokenStream& ts, SignalEngine& engine,
                          const char* source, uint32_t source_length) {
    Token name_tok = ts.consume();
    if (name_tok.kind != TokenKind::Symbol) {
        return make_error("defn needs a name", "Try: (defn osc [f ph] (sin (* ph f)))");
    }
    SymbolID sym = name_tok.symbol;
    if (cell_id_out_of_range(sym)) return make_too_many_definitions_error();

    // Parse parameter list
    if (!ts.expect(TokenKind::LBracket)) {
        return make_error("defn needs a parameter list in brackets",
                          "Try: (defn osc [f ph] (sin (* ph f)))");
    }

    // Parse into a local so a failed arena store leaves the old definition
    // fully intact (params AND source must update atomically).
    CallableInfo info{};
    info.param_count = 0;
    while (ts.peek().kind != TokenKind::RBracket && !ts.at_end()) {
        Token param = ts.consume();
        if (param.kind != TokenKind::Symbol) {
            return make_error("defn parameter names must be symbols",
                              "Try: (defn name [arg] body)");
        }
        if (info.param_count >= MAX_CALLABLE_PARAMS) {
            return make_error("defn has too many parameters",
                              "Split the function or use fewer parameters");
        }
        info.params[info.param_count++] = param.symbol;
    }
    if (!ts.expect(TokenKind::RBracket)) {
        return make_error("defn has an invalid parameter list",
                          "Try: (defn name [arg] body)");
    }

    // Store body source — skip body and record extent
    uint16_t body_start = ts.pos;
    uint32_t byte_start = span_begin(ts, body_start);

    GraphBuilder::skip_form(ts);
    uint32_t byte_end = span_end_of(ts, ts.pos);

    if (ts.peek().kind != TokenKind::RParen) {
        return make_error("defn accepts exactly one body expression",
                          "Try: (defn name [arg] body)");
    }

    // Copy the body source text into the arena FIRST. If the arena is full,
    // fail the defn without touching the cell so dependents never recompile
    // a half-updated definition (F5).
    if (source && byte_end > byte_start && byte_end <= source_length) {
        uint32_t len = byte_end - byte_start;
        const CallableInfo previous =
            engine.cells.cells[sym].kind == CellKind::Callable
                ? engine.cells.callables[sym] : CallableInfo{};
        uint32_t offset = engine.arena.store_reuse(
            previous.source_offset, previous.source_length,
            source + byte_start, len);
        if (offset == UINT32_MAX) {
            return make_error(
                "Program storage is full — definition not applied",
                "Free space with (useq-clear) or shorten your program");
        }
        info.source_offset = offset;
        info.source_length = len;
    }

    engine.cells.callables[sym] = info;
    engine.cells.cells[sym].kind = CellKind::Callable;
    engine.cells.cells[sym].flags = 0;
    engine.cells.cells[sym].revision++;

    on_cell_changed(sym, engine);

    return make_ok();
}

// ── set ─────────────────────────────────────────────────────────────────────

static EvalResult do_set(TokenStream& ts, SignalEngine& engine,
                         const char* source = nullptr) {
    Token name_tok = ts.consume();
    if (name_tok.kind != TokenKind::Symbol) {
        return make_error("set needs a name", "Try: (set x 42)");
    }
    SymbolID sym = name_tok.symbol;
    if (cell_id_out_of_range(sym)) return make_too_many_definitions_error();

    // If the target is a defstate cell (flags 0x02), the live value lives in
    // pool.state_values[slot], not the cell — a plain cell write was silently
    // ignored by every LoadState reader (A7). Write the state slot and keep
    // the marker so the update program keeps running from the new value.
    // (Mirrors define's handling of stale markers, which clears them instead
    // because define establishes a fresh non-state binding.)
    bool is_state_cell = engine.cells.cells[sym].kind == CellKind::Number
                      && engine.cells.cells[sym].flags == 0x02;

    auto store_number = [&](Sample v) {
        if (is_state_cell) {
            uint16_t slot = (uint16_t)engine.cells.cells[sym].data_table_id;
            engine.pool.state_values[slot] = v;
            engine.cells.cells[sym].value = v;
            engine.cells.cells[sym].revision++;
        } else {
            engine.cells.cells[sym].kind = CellKind::Number;
            engine.cells.cells[sym].revision++;
            engine.cells.cells[sym].value = v;
            engine.cells.callables[sym] = CallableInfo{};
        }
    };

    Token val_tok = ts.peek();
    if (val_tok.kind == TokenKind::Number) {
        ts.consume();
        if (ts.peek().kind != TokenKind::RParen) {
            return make_error("set accepts exactly one value",
                              "Try: (set name value)");
        }
        store_number(val_tok.number);
    } else {
        // Non-numeric: compile in scratch pool, evaluate once, store result.
        // This avoids leaking nodes/CSE/data into the live pool.
        uint8_t saved_tables = engine.cells.data_table_count;
        engine.scratch_pool.reset();

        // Mirror live state BEFORE compiling (A5, state-identity.md §6.6):
        // compilation writes init values into freshly-allocated scratch
        // slots; copying live values afterwards clobbered them, so stateful
        // expressions in a set/eval saw a stale live value instead of their
        // own :init.
        memcpy(engine.scratch_pool.state_values, engine.pool.state_values,
               sizeof(engine.pool.state_values));

        GraphBuildResult gr = build_output_graph(engine.scratch_pool, ts,
                                                  engine.cells, engine.arena, source);
        if (gr.has_error) {
            engine.cells.data_table_count = saved_tables;
            return make_error("set: expression could not be evaluated",
                              "Try: (set x 42)");
        }

        if (ts.peek().kind != TokenKind::RParen) {
            engine.cells.data_table_count = saved_tables;
            return make_error("set accepts exactly one value",
                              "Try: (set name value)");
        }

        if (engine.scratch_pool.nodes[gr.root_node].op == NodeOp::Const) {
            store_number(engine.scratch_pool.nodes[gr.root_node].imm);
        } else {
            engine.scratch_pool.outputs[0].root_node = gr.root_node;
            engine.scratch_pool.outputs[0].valid = true;
            engine.scratch_pool.rebuild_execution_order();

            Sample cell_vals[MAX_CELLS];
            engine.cells.snapshot_values(cell_vals, MAX_CELLS);
            Sample hw_inputs[32] = {};
            Sample workspace[MAX_TOTAL_NODES] = {};
            Sample outputs[MAX_OUTPUTS] = {};

            ExecutionContext ctx;
            ctx.t = engine.state.current_time;
            ctx.dt = engine.state.current_dt;
            ctx.cell_values = cell_vals;
            ctx.hw_inputs = hw_inputs;
            ctx.data_pool = engine.cells.data_pool;
            ctx.data_offsets = engine.cells.data_offsets;
            ctx.data_lengths = engine.cells.data_lengths;
            ctx.prev_outputs = engine.pool.prev_output_values;
            ctx.output_values = outputs;
            ctx.workspace = workspace;
            ctx.ground_t = engine.state.current_wall_time;
            execute_all_outputs(engine.scratch_pool, ctx);

            store_number(outputs[0]);
        }

        engine.cells.data_table_count = saved_tables;
    }

    on_cell_changed(sym, engine);

    return make_ok();
}

// ── defstate ────────────────────────────────────────────────────────────────

static EvalResult do_defstate(TokenStream& ts, SignalEngine& engine,
                              const char* source, uint32_t source_length) {
    // (defstate name init_expr update_expr)
    Token name_tok = ts.consume();
    if (name_tok.kind != TokenKind::Symbol) {
        return make_error("defstate needs a name",
                          "Try: (defstate counter 0 (+ counter 1))");
    }
    SymbolID sym = name_tok.symbol;
    if (cell_id_out_of_range(sym)) return make_too_many_definitions_error();

    // Parse init value — must be a number literal for simplicity.
    // Specifically reject live-edit in this position (§4.1.9).
    if (next_is_live_edit(ts)) {
        // Skip to closing paren
        while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) ts.consume();
        ts.expect(TokenKind::RParen);
        return make_error(
            "live-edit is not valid here — defstate initial value must be a literal number",
            "Try: (defstate counter 0 (+ counter 1))");
    }
    Token init_tok = ts.consume();
    if (init_tok.kind != TokenKind::Number) {
        return make_error("defstate initial value must be a number",
                          "Try: (defstate counter 0 (+ counter 1))");
    }
    Sample init_value = init_tok.number;

    // Resolve an existing state slot before storing its update source so a
    // re-definition can reuse that slot's arena region.
    uint16_t state_slot = NODE_NONE;
    if (engine.cells.cells[sym].kind == CellKind::Number
        && engine.cells.cells[sym].flags == 0x02) {
        // Existing state cell — reuse its slot, do NOT reset the value.
        state_slot = (uint16_t)engine.cells.cells[sym].data_table_id;
    }

    // Locate and preflight the update source, but do not write it yet.
    // store_reuse() may overwrite the old region in place; performing it
    // before compilation made a rejected redefinition poison the source used
    // by later dependency recompilation.
    uint16_t expr_start = ts.pos;
    uint32_t byte_start = span_begin(ts, expr_start);
    uint32_t previous_offset = UINT32_MAX;
    uint32_t previous_length = 0;
    if (state_slot != NODE_NONE &&
        engine.state_sources[state_slot].has_source) {
        previous_offset = engine.state_sources[state_slot].arena_offset;
        previous_length = engine.state_sources[state_slot].arena_length;
    }
    uint16_t saved = ts.pos;
    GraphBuilder::skip_form(ts);
    uint32_t byte_end = span_end_of(ts, ts.pos);
    ts.rewind(saved);
    SourceMutationPlan source_plan = SourceMutationPlan::prepare(
        engine.arena, source, source_length, byte_start, byte_end,
        previous_offset, previous_length);
    if (source_plan.status == SourcePlanStatus::CapacityExceeded) {
        return make_error(
            "Program storage is full — defstate not applied",
            "Free space with (useq-clear) or shorten your program");
    }
    if (source_plan.status == SourcePlanStatus::InvalidSpan) {
        return make_error("defstate update has an invalid source span",
                          "Resubmit the complete defstate form");
    }

    // Snapshot everything graph construction can mutate. The source bytes are
    // still untouched and are published only after a successful compile.
    Cell saved_cell = engine.cells.cells[sym];
    GraphMutationTransaction graph_transaction(engine);

    // Allocate a state slot (if this name already has a state slot, reuse it)
    if (state_slot == NODE_NONE) {
        // New state cell — allocate slot and set initial value
        state_slot = engine.registry.take_free_slot();
        if (state_slot == NODE_NONE) {
            if (engine.pool.state_slot_count >= MAX_STATE_SLOTS) {
                return make_error(state_slots_exhausted_msg(),
                                  "Remove unused defstate declarations");
            }
            state_slot = engine.pool.state_slot_count++;
        }
        engine.pool.state_values[state_slot] = init_value;
        engine.pool.state_observed[state_slot] = false;
        engine.pool.state_update_roots[state_slot] = NODE_NONE;
        engine.pool.state_owner_context[state_slot] =
            (uint16_t)(MAX_OUTPUTS + state_slot);
        engine.state_sources[state_slot] = StateUpdateSource{};
    }
    StateUpdateSource saved_source = engine.state_sources[state_slot];

    // Mark this cell as a state cell: kind=Number (readable), flags=0x02 (state marker),
    // data_table_id stores the state slot index
    engine.cells.cells[sym].kind = CellKind::Number;
    engine.cells.cells[sym].flags = 0x02;  // state cell marker
    engine.cells.cells[sym].data_table_id = state_slot;
    engine.cells.cells[sym].revision++;
    engine.cells.cells[sym].value = init_value;

    // Compile the update expression as a signal graph
    uint16_t owner_context = (uint16_t)(MAX_OUTPUTS + state_slot);
    engine.pool.state_owner_context[state_slot] = owner_context;
    engine.registry.begin_context(owner_context);
    GraphBuildResult result = build_output_graph(engine.pool, ts,
                                                 engine.cells, engine.arena, source,
                                                 &engine.registry, nullptr,
                                                 owner_context);

    if (result.has_error || ts.peek().kind != TokenKind::RParen) {
        // Restore the old cell plus every live graph-side mutation. GC drops
        // nodes interned by the rejected build after old roots are restored.
        engine.cells.cells[sym] = saved_cell;
        engine.cells.cells[sym].revision++;
        engine.state_sources[state_slot] = saved_source;
        return make_error(
            result.has_error
                ? "defstate update expression failed to compile"
                : "defstate accepts exactly one update expression",
            result.has_error
                ? "Check the update expression"
                : "Try: (defstate name init update)");
    }

    // Compilation succeeded, so publishing the source cannot corrupt a
    // last-known-good definition. The preflight above guarantees capacity.
    if (source_plan.has_source()) {
        uint32_t src_offset = source_plan.publish(engine.arena);
        if (src_offset == UINT32_MAX) {
            engine.cells.cells[sym] = saved_cell;
            engine.cells.cells[sym].revision++;
            engine.state_sources[state_slot] = saved_source;
            return make_error(
                "Program storage is full — defstate not applied",
                "Free space with (useq-clear) or shorten your program");
        }
        engine.state_sources[state_slot].arena_offset = src_offset;
        engine.state_sources[state_slot].arena_length = source_plan.length;
        engine.state_sources[state_slot].has_source = true;
    }
    engine.state_sources[state_slot].state_symbol = sym;

    publish_state_update_plan(engine, state_slot, result);
    graph_transaction.accept();

    // Reclaim nodes orphaned by the recompile (e.g. the previous update
    // graph of this state slot), then rebuild execution order to include
    // state update subgraphs (F4).
    reclaim_unowned_resources(engine);
    engine.pool.rebuild_execution_order();
    classify_outputs(engine.pool);

    // Notify dependents so outputs referencing this cell get recompiled
    on_cell_changed(sym, engine);
    engine.cells.callables[sym] = CallableInfo{};

    return make_ok();
}

// ── Transport / time management ─────────────────────────────────────────────

static EvalResult do_set_bpm(TokenStream& ts, SignalEngine& engine) {
    Token val = ts.consume();
    if (val.kind != TokenKind::Number) {
        return make_error("set-bpm needs a number", "Try: (set-bpm 120)");
    }
    if (ts.peek().kind != TokenKind::RParen) {
        return make_error("set-bpm accepts exactly one number",
                          "Try: (set-bpm 120)");
    }
    if (ts.peek().kind != TokenKind::RParen) {
        return make_error("set-bpm accepts exactly one number",
                          "Try: (set-bpm 120)");
    }
    auto& si = SymbolIntern::getInstance();
    SymbolID bpm_sym = si.intern("bpm");
    engine.cells.cells[bpm_sym].kind = CellKind::Number;
    engine.cells.cells[bpm_sym].value = val.number;
    engine.cells.cells[bpm_sym].revision++;
    engine.cells.callables[bpm_sym] = CallableInfo{};
    on_cell_changed(bpm_sym, engine);
    return make_ok();
}

static EvalResult do_set_time_sig(TokenStream& ts, SignalEngine& engine) {
    Token beats_tok = ts.consume();
    if (beats_tok.kind != TokenKind::Number) {
        return make_error("set-time-sig needs two numbers",
                          "Try: (set-time-sig 4 4)");
    }
    Token subdivision_tok = ts.consume();
    if (subdivision_tok.kind != TokenKind::Number) {
        return make_error("set-time-sig needs two numbers",
                          "Try: (set-time-sig 4 4)");
    }
    if (ts.peek().kind != TokenKind::RParen) {
        return make_error("set-time-sig accepts exactly two numbers",
                          "Try: (set-time-sig 4 4)");
    }
    if (!std::isfinite(beats_tok.number) || beats_tok.number <= 0.0 ||
        std::floor(beats_tok.number) != beats_tok.number) {
        return make_error("set-time-sig needs a positive whole beat count",
                          "Try: (set-time-sig 3 4)");
    }
    // The current clock model defines bpm and `beat` in quarter-note units;
    // it has no denominator/subdivision cell.  Silently accepting 6/8 while
    // calculating a six-quarter-note bar is wrong, so the supported surface
    // is deliberately limited to */4 until that model is extended.
    if (!std::isfinite(subdivision_tok.number) ||
        subdivision_tok.number != 4.0) {
        return make_error(
            "set-time-sig currently supports quarter-note subdivision only",
            "Try: (set-time-sig 3 4)");
    }
    if (ts.peek().kind != TokenKind::RParen) {
        return make_error("set-time-sig accepts exactly two numbers",
                          "Try: (set-time-sig 4 4)");
    }

    auto& si = SymbolIntern::getInstance();
    SymbolID bpb_sym = si.intern("beats-per-bar");
    engine.cells.cells[bpb_sym].kind = CellKind::Number;
    engine.cells.cells[bpb_sym].value = beats_tok.number;
    engine.cells.cells[bpb_sym].revision++;
    engine.cells.callables[bpb_sym] = CallableInfo{};
    on_cell_changed(bpb_sym, engine);

    return make_ok();
}

static EvalResult do_useq_clear(SignalEngine& engine) {
    // A session clear is a semantic reset, not only a compiler-storage reset.
    // Monotonic session/synth notification tokens remain owned by
    // reset_session_storage().
    engine.state = EngineState{};
    engine.reset_session_storage();
    return make_ok();
}

static EvalResult do_set_time_offset(TokenStream& ts, EngineState& state) {
    Token val = ts.consume();
    if (val.kind != TokenKind::Number) {
        return make_error("useq-set-time-offset needs a number in seconds",
                          "Try: (useq-set-time-offset 1.0)");
    }
    if (ts.peek().kind != TokenKind::RParen) {
        return make_error("useq-set-time-offset accepts exactly one number",
                          "Try: (useq-set-time-offset 1.0)");
    }
    if (ts.peek().kind != TokenKind::RParen) {
        return make_error("useq-set-time-offset accepts exactly one number",
                          "Try: (useq-set-time-offset 1.0)");
    }
    state.time_offset = val.number;
    return make_ok();
}

static EvalResult do_nudge_time(TokenStream& ts, EngineState& state) {
    Token val = ts.consume();
    if (val.kind != TokenKind::Number) {
        return make_error("useq-nudge-time needs a number in seconds",
                          "Try: (useq-nudge-time 0.1)");
    }
    if (ts.peek().kind != TokenKind::RParen) {
        return make_error("useq-nudge-time accepts exactly one number",
                          "Try: (useq-nudge-time 0.1)");
    }
    if (ts.peek().kind != TokenKind::RParen) {
        return make_error("useq-nudge-time accepts exactly one number",
                          "Try: (useq-nudge-time 0.1)");
    }
    state.time_offset += val.number;
    return make_ok();
}

#if USEQ_HAS_SYNTH_ENGINE
// ── Host synth declaration (synth-nodes.md §3) ─────────────────────────────
//
// Top-level form that instantiates one NodeDef instance with bound ModuLisp
// control expressions. The declaration is staged into the live synth_graph
// immediately; eval_cold snapshots synth_graph at the start of every eval
// and restores the snapshot on any downstream error, so a later failing
// form in the same eval unit rolls back the staged declaration
// (VAL-COMP-008, VAL-COMP-010).
//
// Grammar (synth-nodes.md §3.1):
//   (synth <def-name-string>
//          [:version <int>]            ; optional, default 0 (= latest)
//          [:name <string>]            ; optional explicit identity
//          [:id <string>]              ; hidden identity (payload builder)
//          :<param> <expr> ...)        ; one or more param bindings
//
// M1 enforces SYNTH_M1_MAX_NODES (1) active declaration at a time; over-
// capacity evals fail transactionally with a precise diagnostic
// (VAL-COMP-019). Same-identity re-declaration is an update-in-place.

// Tiny static-string wrapper for messages built from a small buffer. The
// diagnostic message pointer is required to remain valid for the lifetime
// of the Diagnostic struct; since the engine consumes diagnostics
// synchronously inside eval_cold and never persists the pointer past the
// next eval, a small static rotating buffer pool is sufficient and avoids
// heap allocation on the firmware hot path.
//
// We keep 8 slots (matching MAX_DIAGNOSTICS) so a single eval can build up
// to 8 distinct messages without clobbering each other.
static const char* strdup_safe(const char* s) {
    static char pool[8][160];
    static uint8_t rotating = 0;
    char* slot = pool[rotating];
    rotating = (uint8_t)((rotating + 1) & 7);
    std::strncpy(slot, s, sizeof(pool[0]) - 1);
    slot[sizeof(pool[0]) - 1] = '\0';
    return slot;
}

static EvalResult make_synth_error_at(uint16_t span_start, uint16_t span_len,
                                      DiagnosticCategory cat,
                                      const char* message,
                                      const char* suggestion = nullptr) {
    EvalResult r;
    r.kind = EvalResult::Error;
    if (r.diagnostic_count < 8) {
        r.diagnostics[r.diagnostic_count++] = {
            DiagnosticSeverity::Error, cat,
            span_start, span_len, message, suggestion
        };
    }
    return r;
}

static EvalResult make_synth_error_at(const Token& tok,
                                      DiagnosticCategory cat,
                                      const char* message,
                                      const char* suggestion = nullptr) {
    return make_synth_error_at(tok.span_start, tok.span_len, cat,
                               message, suggestion);
}

// Resolve a NodeDef by name+version, with precise diagnostics for each
// failure mode (VAL-COMP-005).
static const NodeDefDescriptor* resolve_nodedef(
    const Token& name_tok, const Token* version_tok,
    const char* source_base,
    EvalResult& out_error,
    bool allow_symbol_name = false)
{
    out_error = EvalResult{};

    if (name_tok.kind != TokenKind::String &&
        !(allow_symbol_name && name_tok.kind == TokenKind::Symbol)) {
        out_error = make_synth_error_at(
            name_tok, DiagnosticCategory::Type,
            "synth needs a NodeDef name in quotes",
            "Try: (synth \"osc/sine\" :freq 440)");
        return nullptr;
    }

    char name_buf[MAX_NODEDEF_NAME];
    const bool symbol_name = name_tok.kind == TokenKind::Symbol;
    uint16_t name_len = symbol_name ? name_tok.span_len : name_tok.string.length;
    if (name_len >= MAX_NODEDEF_NAME) name_len = MAX_NODEDEF_NAME - 1;
    const uint32_t name_offset = symbol_name
        ? name_tok.span_start
        : name_tok.string.offset;
    std::memcpy(name_buf, source_base + name_offset, name_len);
    name_buf[name_len] = '\0';

    uint16_t version = 0;
    if (version_tok && version_tok->kind == TokenKind::Number) {
        Sample requested = version_tok->number;
        if (!std::isfinite(requested) || requested < 1.0 ||
            requested > 65535.0 || std::floor(requested) != requested) {
            out_error = make_synth_error_at(
                *version_tok, DiagnosticCategory::Type,
                ":version needs a whole number from 1 to 65535",
                "Try: (synth \"osc/sine\" :version 2 :freq 440)");
            return nullptr;
        }
        version = (uint16_t)requested;
    }

    const NodeDefDescriptor* def = synth_registry_find(name_buf, version);
    if (!def) {
        if (version != 0) {
            char msg_buf[128];
            std::snprintf(msg_buf, sizeof(msg_buf),
                          "NodeDef \"%s\" version %u is not available",
                          name_buf, (unsigned)version);
            out_error = make_synth_error_at(
                name_tok, DiagnosticCategory::UndefinedName,
                strdup_safe(msg_buf),
                "Try: (synth \"osc/sine\" :freq 440)");
        } else {
            char msg_buf[128];
            std::snprintf(msg_buf, sizeof(msg_buf),
                          "Unknown NodeDef \"%s\"", name_buf);
            out_error = make_synth_error_at(
                name_tok, DiagnosticCategory::UndefinedName,
                strdup_safe(msg_buf),
                "Try: (synth \"osc/sine\" :freq 440)");
        }
        return nullptr;
    }
    return def;
}

static int16_t synth_declaration_index(const SynthGraph& graph,
                                       const char* identity) {
    if (!identity) return -1;
    for (uint16_t i = 0; i < graph.declaration_count(); i++) {
        if (std::strcmp(graph.declarations[i].identity, identity) == 0)
            return (int16_t)i;
    }
    return -1;
}

static EvalResult validate_synth_patch_graph(const SynthGraph& graph,
                                              const Token& anchor) {
    uint16_t indegree[MAX_SYNTH_DECLARATIONS] = {};

    for (uint16_t i = 0; i < graph.connection_count(); i++) {
        const SynthConnection& edge = graph.connections[i];
        int16_t from = synth_declaration_index(graph, edge.from);
        int16_t to = synth_declaration_index(graph, edge.to);
        if (from < 0 || to < 0) {
            return make_synth_error_at(
                anchor, DiagnosticCategory::UndefinedName,
                "Synth connection references an unknown node identity",
                "Declare the source node before connecting it");
        }
        if (from == to) {
            return make_synth_error_at(
                anchor, DiagnosticCategory::Boundary,
                "A synth node cannot connect an audio input to itself",
                "Route the input from a different synth node");
        }

        const SynthDeclaration& source_decl = graph.declarations[from];
        const SynthDeclaration& dest_decl = graph.declarations[to];
        if (source_decl.audio_outputs == 0) {
            return make_synth_error_at(
                anchor, DiagnosticCategory::Boundary,
                "Synth connection source has no audio output",
                "Choose a NodeDef that produces audio");
        }
        const NodeDefDescriptor* dest_def =
            synth_registry_find(dest_decl.def_name, dest_decl.def_version);
        if (!dest_def || edge.port_index >= dest_def->audio_inputs ||
            edge.port_index >= MAX_NODEDEF_AUDIO_INPUTS ||
            !dest_def->audio_input_names[edge.port_index] ||
            std::strcmp(dest_def->audio_input_names[edge.port_index],
                        edge.port) != 0) {
            return make_synth_error_at(
                anchor, DiagnosticCategory::Boundary,
                "Synth connection does not match the destination audio port",
                "Recompile against the installed NodeDef descriptor");
        }
        for (uint16_t j = 0; j < i; j++) {
            const SynthConnection& prior = graph.connections[j];
            if (prior.port_index == edge.port_index &&
                std::strcmp(prior.to, edge.to) == 0) {
                return make_synth_error_at(
                    anchor, DiagnosticCategory::Boundary,
                    "A synth audio input can have only one source",
                    "Remove the duplicate input connection");
            }
        }
        indegree[to]++;
    }

    uint16_t queue[MAX_SYNTH_DECLARATIONS] = {};
    uint16_t read = 0;
    uint16_t write = 0;
    for (uint16_t i = 0; i < graph.declaration_count(); i++) {
        if (indegree[i] == 0) queue[write++] = i;
    }
    uint16_t visited = 0;
    while (read < write) {
        uint16_t from = queue[read++];
        visited++;
        for (uint16_t i = 0; i < graph.connection_count(); i++) {
            const SynthConnection& edge = graph.connections[i];
            if (std::strcmp(graph.declarations[from].identity, edge.from) != 0)
                continue;
            int16_t to = synth_declaration_index(graph, edge.to);
            if (to >= 0 && indegree[to] > 0 && --indegree[to] == 0)
                queue[write++] = (uint16_t)to;
        }
    }
    if (visited != graph.declaration_count()) {
        return make_synth_error_at(
            anchor, DiagnosticCategory::Boundary,
            "Synth audio connections must not contain a cycle",
            "Remove one connection from the feedback loop");
    }
    return make_ok();
}

static const SynthControlChannel* find_synth_control(
    const SynthGraph& graph, const char* identity, const char* param_name) {
    const SynthDeclaration* declaration = graph.find(identity);
    if (!declaration) return nullptr;
    const uint16_t end = static_cast<uint16_t>(
        declaration->first_control_index + declaration->control_count);
    for (uint16_t i = declaration->first_control_index; i < end; i++) {
        const SynthControlChannel& control = graph.controls[i];
        const NodeDefParam* parameter = graph.parameter_for_control(i);
        if (parameter && std::strcmp(parameter->name, param_name) == 0)
            return &control;
    }
    return nullptr;
}

static uint16_t allocate_synth_owner_context(
    const SynthGraph& old_graph, const SynthGraph& candidate,
    const char* identity, const char* param_name) {
    constexpr uint16_t base = (uint16_t)(MAX_OUTPUTS + MAX_STATE_SLOTS);
    const SynthControlChannel* old =
        find_synth_control(old_graph, identity, param_name);
    if (old) return old->owner_context;

    const SynthDeclaration* old_declaration = old_graph.find(identity);

    // Mark the bounded owner-context namespace once. The previous search
    // rescanned both graphs for every candidate context, making allocation
    // quadratic in the number of controls for a fresh dense synth graph.
    bool used[MAX_SYNTH_CONTROLS] = {};
    for (uint16_t i = 0; i < old_graph.control_count(); i++) {
        const SynthControlChannel& c = old_graph.controls[i];
        const bool same_declaration = old_declaration &&
            i >= old_declaration->first_control_index &&
            static_cast<uint32_t>(i) <
                static_cast<uint32_t>(old_declaration->first_control_index) +
                    old_declaration->control_count;
        if (!same_declaration && c.owner_context >= base) {
            const uint16_t offset = static_cast<uint16_t>(c.owner_context - base);
            if (offset < MAX_SYNTH_CONTROLS) used[offset] = true;
        }
    }
    for (uint16_t i = 0; i < candidate.control_count(); i++) {
        const uint16_t context = candidate.controls[i].owner_context;
        if (context >= base) {
            const uint16_t offset = static_cast<uint16_t>(context - base);
            if (offset < MAX_SYNTH_CONTROLS) used[offset] = true;
        }
    }

    for (uint16_t offset = 0; offset < MAX_SYNTH_CONTROLS; offset++) {
        if (!used[offset]) return static_cast<uint16_t>(base + offset);
    }
    return ANON_STATE_CONTEXT_NONE;
}

static EvalResult do_synth(TokenStream& ts, SignalEngine& engine,
                           const char* source, uint32_t source_length,
                           char* out_identity = nullptr,
                           bool nested = false,
                           const SynthGraph* transaction_snapshot = nullptr,
                           uint16_t nested_depth = 0,
                           const Token* explicit_def_name = nullptr) {
    GraphBuilder::init_symbols();
    auto& sym = GraphBuilder::sym;

    if (nested_depth >= MAX_SYNTH_NESTING) {
        return make_synth_error_at(
            ts.peek(), DiagnosticCategory::Overflow,
            "Synth routing is nested too deeply",
            "Use at most 16 nested synth nodes in one routing chain");
    }

    // ── Parse def name (required string) ────────────────────────────────
    Token def_name_tok = explicit_def_name ? *explicit_def_name : ts.consume();
    if (def_name_tok.kind != TokenKind::String && !explicit_def_name) {
        return make_synth_error_at(
            def_name_tok, DiagnosticCategory::Type,
            "synth needs a NodeDef name in quotes",
            "Try: (synth \"osc/sine\" :freq 440)");
    }

    // ── Optional keywords before param pairs: :version / :name / :id ───
    Token version_tok;
    version_tok.kind = TokenKind::Eof;
    bool have_explicit_version = false;

    Token identity_tok;
    identity_tok.kind = TokenKind::Eof;
    bool have_explicit_identity = false;

    // Track param bindings declared in this form. Required: :freq.
    struct ParamBinding {
        const NodeDefParam* desc;
        int16_t audio_input_port;
        Token kw_tok;
        uint16_t expr_start_pos;
        uint16_t expr_end_pos;
        uint32_t expr_byte_start;
        uint32_t expr_byte_end;
        bool present;
    };
    constexpr uint16_t max_synth_bindings =
        MAX_NODEDEF_PARAMS + MAX_NODEDEF_AUDIO_INPUTS;
    constexpr uint16_t max_synth_keywords = max_synth_bindings + 3;
    ParamBinding bindings[max_synth_bindings] = {};
    uint16_t binding_count = 0;

    // Track keywords seen (for duplicate detection).
    SymbolID seen_kws[max_synth_keywords] = {};
    uint16_t seen_kw_count = 0;

    auto remember_keyword = [&](const Token& kw_tok) -> EvalResult {
        for (uint16_t i = 0; i < seen_kw_count; ++i) {
            if (seen_kws[i] == kw_tok.symbol) {
                const String& spelling =
                    SymbolIntern::getInstance().getString(kw_tok.symbol);
                char message[96];
                std::snprintf(message, sizeof(message),
                              "Synth keyword %s was already set in this form",
                              spelling.c_str());
                return make_synth_error_at(
                    kw_tok, DiagnosticCategory::Arity,
                    strdup_safe(message),
                    "Remove the duplicate binding");
            }
        }
        seen_kws[seen_kw_count++] = kw_tok.symbol;
        return make_ok();
    };

    bool kw_phase = true;
    while (kw_phase && ts.peek().kind == TokenKind::Symbol) {
        Token kw_tok = ts.peek();
        if (kw_tok.symbol == sym.kw_version) {
            EvalResult unique = remember_keyword(kw_tok);
            if (unique.kind == EvalResult::Error) return unique;
            ts.consume();
            Token v = ts.consume();
            if (v.kind != TokenKind::Number) {
                return make_synth_error_at(
                    v, DiagnosticCategory::Type,
                    ":version needs a whole number",
                    "Try: (synth \"osc/sine\" :version 1 :freq 440)");
            }
            version_tok = v;
            have_explicit_version = true;
            continue;
        }
        if (kw_tok.symbol == sym.kw_name || kw_tok.symbol == sym.kw_id) {
            EvalResult unique = remember_keyword(kw_tok);
            if (unique.kind == EvalResult::Error) return unique;
            ts.consume();
            Token s = ts.consume();
            if (s.kind != TokenKind::String) {
                const char* kw_label =
                    (kw_tok.symbol == sym.kw_name) ? ":name" : ":id";
                char msg[96];
                std::snprintf(msg, sizeof(msg),
                              "%s needs a string in quotes", kw_label);
                return make_synth_error_at(
                    s, DiagnosticCategory::Type,
                    strdup_safe(msg),
                    "Try: (synth \"osc/sine\" :name \"lead\" :freq 440)");
            }
            // Last identity keyword wins; :id is treated as the hidden
            // payload-builder injection and is preserved verbatim.
            identity_tok = s;
            have_explicit_identity = true;
            continue;
        }
        // Any other symbol is either a param keyword or malformed.
        kw_phase = false;
    }

    // ── Resolve NodeDef (VAL-COMP-005) ─────────────────────────────────
    version_tok.kind = have_explicit_version ? TokenKind::Number : TokenKind::Eof;
    EvalResult resolve_err;
    const NodeDefDescriptor* def = resolve_nodedef(
        def_name_tok,
        have_explicit_version ? &version_tok : nullptr,
        source, resolve_err, explicit_def_name != nullptr);
    if (!def) {
        return resolve_err;
    }

    // ── Parse param pairs (VAL-COMP-006) ───────────────────────────────
    // Each pair is `:<param> <expr>`. We collect all pairs first, then
    // compile each expression into the live pool.
    while (ts.peek().kind == TokenKind::Symbol && ts.peek().symbol != sym.kw_name
           && ts.peek().symbol != sym.kw_id && ts.peek().symbol != sym.kw_version) {
        Token kw_tok = ts.consume();

        // The keyword's string spelling lives in the symbol interner.
        auto& si = SymbolIntern::getInstance();
        const String& kw_str = si.getString(kw_tok.symbol);
        if (kw_str.length() == 0 || kw_str[0] != ':') {
            // Not a keyword — malformed.
            return make_synth_error_at(
                kw_tok, DiagnosticCategory::Syntax,
                "Expected a parameter keyword like :freq or :amp",
                "Try: (synth \"osc/sine\" :freq 440 :amp 0.2)");
        }

        // Convert interned symbol back to a C-string for registry lookup.
        char param_buf[MAX_NODEDEF_NAME];
        uint16_t plen = (uint16_t)kw_str.length();
        if (plen >= MAX_NODEDEF_NAME) plen = MAX_NODEDEF_NAME - 1;
        std::memcpy(param_buf, kw_str.c_str(), plen);
        param_buf[plen] = '\0';
        const char* param_name = param_buf + 1; // strip leading ':'

        // Duplicate detection.
        EvalResult unique = remember_keyword(kw_tok);
        if (unique.kind == EvalResult::Error) return unique;

        // Validate the parameter is declared by this NodeDef.
        const NodeDefParam* pdesc = nodedef_find_param(def, param_name);
        int16_t audio_input_port =
            nodedef_find_audio_input(def, param_name);
        if (!pdesc && audio_input_port < 0) {
            const char* suggestion = nodedef_suggest_param(def, param_name);
            char msg[128];
            std::snprintf(msg, sizeof(msg),
                          "NodeDef \"%s\" has no parameter \"%s\"",
                          def->name, param_name);
            char sug_buf[128];
            if (suggestion) {
                std::snprintf(sug_buf, sizeof(sug_buf),
                              "Did you mean :%s? Try: (synth \"%s\" :%s ...)",
                              suggestion, def->name, suggestion);
            } else {
                std::snprintf(sug_buf, sizeof(sug_buf),
                              "Check the NodeDef documentation for \"%s\"",
                              def->name);
            }
            return make_synth_error_at(
                kw_tok, DiagnosticCategory::UndefinedName,
                strdup_safe(msg), strdup_safe(sug_buf));
        }

        // Read the value expression. It must be present and non-keyword.
        if (ts.at_end() || ts.peek().kind == TokenKind::RParen) {
            char msg[96];
            std::snprintf(msg, sizeof(msg),
                          "Parameter %s needs a value expression", param_buf);
            return make_synth_error_at(
                kw_tok, DiagnosticCategory::Arity,
                strdup_safe(msg),
                "Try: (synth \"osc/sine\" :freq 440)");
        }
        // Reject a bare keyword following a keyword (malformed pair).
        if (ts.peek().kind == TokenKind::Symbol) {
            const String& next_str = si.getString(ts.peek().symbol);
            if (next_str.length() > 0 && next_str[0] == ':') {
                return make_synth_error_at(
                    ts.peek(), DiagnosticCategory::Syntax,
                    "Expected a value between parameters",
                    "Try: (synth \"osc/sine\" :freq 440 :amp 0.2)");
            }
        }

        uint16_t expr_start_pos = ts.pos;
        uint32_t byte_start = span_begin(ts, expr_start_pos);
        GraphBuilder::skip_form(ts);
        uint32_t byte_end = span_end_of(ts, ts.pos);

        if (binding_count >= max_synth_bindings) {
            return make_synth_error_at(
                kw_tok, DiagnosticCategory::Overflow,
                "Too many parameters on this synth form",
                "Check the NodeDef documentation");
        }
        bindings[binding_count].desc = pdesc;
        bindings[binding_count].audio_input_port = audio_input_port;
        bindings[binding_count].kw_tok = kw_tok;
        bindings[binding_count].expr_start_pos = expr_start_pos;
        bindings[binding_count].expr_end_pos = ts.pos;
        bindings[binding_count].expr_byte_start = byte_start;
        bindings[binding_count].expr_byte_end = byte_end;
        bindings[binding_count].present = true;
        binding_count++;
    }

    // Reject trailing atoms or malformed material before identity resolution
    // or any graph publication. `ts.expect()` in the caller is too late:
    // do_synth has already committed by then.
    if (ts.peek().kind != TokenKind::RParen) {
        return make_synth_error_at(
            ts.peek(), DiagnosticCategory::Syntax,
            "Unexpected value after synth parameter bindings",
            "Use :parameter value pairs only");
    }

    // ── Required :freq (VAL-COMP-006) ──────────────────────────────────
    bool have_freq = false;
    for (uint16_t i = 0; i < binding_count; i++) {
        if (bindings[i].desc &&
            std::strcmp(bindings[i].desc->name, "freq") == 0) {
            have_freq = true;
            break;
        }
    }
    if (!have_freq) {
        return make_synth_error_at(
            def_name_tok, DiagnosticCategory::Arity,
            "synth \"osc/sine\" needs a :freq parameter",
            "Try: (synth \"osc/sine\" :freq 440)");
    }

    // ── Identity resolution ────────────────────────────────────────────
    // Order of authority: explicit :name/:id > pending with-state-id
    // wrapper id > anonymous fallback (state-identity.md §2.2, ergo
    // e58f128f). The editor payload builder wraps anonymous synth forms in
    // `(with-state-id "<id>" ...)`; the wrapper handler stashes that id as
    // the engine's pending state identity and the first synth declaration
    // under the wrapper consumes it, so the same document form keeps the
    // same identity across re-evals (update-in-place, synth-nodes.md §5.5).
    char identity_buf[MAX_SYNTH_IDENTITY];
    if (have_explicit_identity) {
        uint16_t n = identity_tok.string.length;
        if (n == 0 || n >= MAX_SYNTH_IDENTITY) {
            return make_synth_error_at(
                identity_tok, DiagnosticCategory::Overflow,
                "Synth identity is empty or too long",
                "Use from 1 to 31 bytes for :name or :id");
        }
        std::memcpy(identity_buf, source + identity_tok.string.offset, n);
        identity_buf[n] = '\0';
        // The explicit identity supersedes and consumes any pending
        // wrapper id so it cannot fall through to a later anonymous
        // sibling inside the same wrapped form.
        engine.has_pending_state_identity = false;
    } else if (engine.has_pending_state_identity) {
        std::strncpy(identity_buf, engine.pending_state_identity,
                     MAX_SYNTH_IDENTITY - 1);
        identity_buf[MAX_SYNTH_IDENTITY - 1] = '\0';
        engine.has_pending_state_identity = false; // first synth wins
    } else {
        // Anonymous fallback for direct eval without editor sidecar (raw
        // REPL). Keyed by ordinal position within the current eval, so
        // re-evaluating the same program reuses its identity instead of
        // minting a fresh one per eval (state-identity.md §2.5).
        std::snprintf(identity_buf, sizeof(identity_buf), "::anon-%u",
                      (unsigned)engine.eval_anon_synth_ordinal++);
    }

    // ── Capacity check (VAL-COMP-019) ──────────────────────────────────
    SynthDeclaration* existing = engine.synth_graph.find(identity_buf);
    if (existing == nullptr &&
        engine.synth_graph.declaration_count() >= SYNTH_MAX_NODES) {
        char msg[160];
        std::snprintf(msg, sizeof(msg),
                      "Synth graph exceeds the %u-node capacity",
                      (unsigned)SYNTH_MAX_NODES);
        return make_synth_error_at(
            def_name_tok, DiagnosticCategory::Overflow,
            strdup_safe(msg),
            "Use (useq-clear) to free synths, or update an existing identity");
    }

    // A synth form is one publication transaction. Snapshot both the
    // descriptor/control table and every live graph-side structure before
    // removing the old declaration or compiling replacement controls.
    // Restoring the SynthGraph first ensures graph rollback preserves its
    // old external roots during reachability GC.
    SynthGraph* owned_snapshot = nullptr;
    if (!transaction_snapshot) {
        owned_snapshot = new (std::nothrow) SynthGraph(engine.synth_graph);
        dt::sample_runtime_memory();
        if (!owned_snapshot) {
            return make_synth_error_at(
                def_name_tok, DiagnosticCategory::Overflow,
                "Not enough memory to stage the synth transaction",
                "Reduce the synth graph and retry");
        }
        transaction_snapshot = owned_snapshot;
    }
    const SynthGraph& synth_snapshot = *transaction_snapshot;
    // The outer synth transaction owns the graph rollback image. A nested
    // synth is part of that same transaction; taking another image in the
    // engine's single scratch pool would overwrite the outer pre-state.
    GraphMutationTransaction graph_transaction;
    if (!nested) graph_transaction.begin(engine);
    auto rollback_synth = [&](EvalResult error) {
        engine.synth_graph = synth_snapshot;
        if (!nested) graph_transaction.rollback();
        delete owned_snapshot;
        return error;
    };

    // Existing control contexts are stable per (identity,param). Mark only
    // this declaration's old contexts unseen; successful replacement retires
    // removed parameters, while rollback restores the registry snapshot.
    const SynthDeclaration* old_declaration = synth_snapshot.find(identity_buf);
    if (old_declaration) {
        const uint16_t old_end = static_cast<uint16_t>(
            old_declaration->first_control_index +
            old_declaration->control_count);
        for (uint16_t i = old_declaration->first_control_index;
             i < old_end; i++) {
            const SynthControlChannel& old = synth_snapshot.controls[i];
            engine.registry.begin_context(old.owner_context);
        }
    }

    // ── Commit declaration + controls to synth_graph ───────────────────
    // Same-identity re-declaration is update-in-place: drop the existing
    // declaration's control rows first, then re-append fresh ones.
    if (existing) {
        // Remove the existing declaration and its control rows. The control
        // table is dense (rows are appended in declaration order); we
        // rebuild it in-place by shifting.
        uint16_t kill_first = existing->first_control_index;
        uint16_t kill_count = existing->control_count;

        // Compact the control table.
        for (uint16_t i = kill_first; i + kill_count < engine.synth_graph.control_count_value; i++) {
            engine.synth_graph.controls[i] =
                engine.synth_graph.controls[i + kill_count];
        }
        engine.synth_graph.control_count_value -= kill_count;

        for (uint16_t i = 0; i < engine.synth_graph.declaration_count(); i++) {
            SynthDeclaration& other = engine.synth_graph.declarations[i];
            if (&other != existing && other.first_control_index > kill_first)
                other.first_control_index -= kill_count;
        }

        // Updating a destination replaces its incoming routing, while its
        // outgoing edges remain attached to its stable identity.
        uint16_t edge_write = 0;
        for (uint16_t edge_read = 0;
             edge_read < engine.synth_graph.connection_count_value;
             edge_read++) {
            const SynthConnection& edge =
                engine.synth_graph.connections[edge_read];
            if (std::strcmp(edge.to, identity_buf) == 0) continue;
            if (edge_write != edge_read)
                engine.synth_graph.connections[edge_write] = edge;
            edge_write++;
        }
        engine.synth_graph.connection_count_value = edge_write;

        // Compact the declaration table.
        uint16_t decl_idx = (uint16_t)(existing - engine.synth_graph.declarations);
        for (uint16_t i = decl_idx; i + 1 < engine.synth_graph.declaration_count_value; i++) {
            engine.synth_graph.declarations[i] =
                engine.synth_graph.declarations[i + 1];
        }
        engine.synth_graph.declaration_count_value--;
    }

    SynthDeclaration* decl = engine.synth_graph.append_declaration();
    if (!decl) {
        return rollback_synth(make_synth_error_at(
            def_name_tok, DiagnosticCategory::Overflow,
            "Synth declaration table is full",
            "Use (useq-clear) to free earlier synths"));
    }
    std::strncpy(decl->identity, identity_buf, MAX_SYNTH_IDENTITY - 1);
    decl->identity[MAX_SYNTH_IDENTITY - 1] = '\0';
    std::strncpy(decl->def_name, def->name, MAX_NODEDEF_NAME - 1);
    decl->def_name[MAX_NODEDEF_NAME - 1] = '\0';
    decl->def_version   = def->version;
    decl->audio_inputs  = def->audio_inputs;
    decl->audio_outputs = def->audio_outputs;
    decl->voice_fanout  = def->voice_fanout;
    decl->first_control_index = engine.synth_graph.control_count_value;
    decl->control_count = 0;

    // Preflight persistent source storage for every control. Nested synths
    // append rather than overwrite old source so an outer rollback can restore
    // only the arena write head without having to copy the whole arena.
    uint32_t required_source_bytes = 0;
    for (uint16_t i = 0; i < binding_count; i++) {
        ParamBinding& b = bindings[i];
        if (!b.present || !b.desc) continue;
        uint32_t expr_len = b.expr_byte_end - b.expr_byte_start;
        const SynthControlChannel* old = find_synth_control(
            synth_snapshot, identity_buf, b.desc->name);
        bool can_reuse = !nested && old &&
            source_region_can_store(engine.arena, old->source_offset,
                                    old->source_length, expr_len) &&
            expr_len <= old->source_length;
        if (!can_reuse) {
            if (UINT32_MAX - required_source_bytes < expr_len) {
                return rollback_synth(make_synth_error_at(
                    b.kw_tok, DiagnosticCategory::Overflow,
                    "Synth control source is too large", nullptr));
            }
            required_source_bytes += expr_len;
        }
    }
    if (engine.arena.write_head > SOURCE_ARENA_SIZE ||
        required_source_bytes > SOURCE_ARENA_SIZE - engine.arena.write_head) {
        return rollback_synth(make_synth_error_at(
            def_name_tok, DiagnosticCategory::Overflow,
            "Source storage full — synth control was not published",
            "Use (useq-clear) to reclaim session source storage"));
    }

    uint32_t staged_source_offsets[max_synth_bindings];
    for (uint16_t i = 0; i < max_synth_bindings; i++)
        staged_source_offsets[i] = UINT32_MAX;

    // Compile controls and resolve audio routing. All mutations remain behind
    // the synth + graph snapshots until the complete post-diff graph validates.
    for (uint16_t i = 0; i < binding_count; i++) {
        ParamBinding& b = bindings[i];
        if (!b.present) continue;
        uint32_t expr_len = b.expr_byte_end - b.expr_byte_start;
        if (expr_len == 0 || b.expr_byte_start > source_length ||
            expr_len > source_length - b.expr_byte_start) {
            return rollback_synth(make_synth_error_at(
                b.kw_tok, DiagnosticCategory::Syntax,
                "Synth parameter expression has an invalid source span",
                nullptr));
        }

        if (!b.desc) {
            // Audio inputs accept only `(node "identity")` or a nested
            // `(synth ...)`. Arbitrary signal expressions are controls, not
            // audio-routing endpoints.
            uint16_t token_count = b.expr_end_pos - b.expr_start_pos;
            if (token_count < 4 ||
                ts.tokens[b.expr_start_pos].kind != TokenKind::LParen ||
                ts.tokens[b.expr_end_pos - 1].kind != TokenKind::RParen ||
                ts.tokens[b.expr_start_pos + 1].kind != TokenKind::Symbol) {
                return rollback_synth(make_synth_error_at(
                    b.kw_tok, DiagnosticCategory::Boundary,
                    "Audio inputs need (node \"identity\") or a nested synth",
                    "Try: :fm (node \"lfo\")"));
            }
            const String& head = SymbolIntern::getInstance().getString(
                ts.tokens[b.expr_start_pos + 1].symbol);
            char from_identity[MAX_SYNTH_IDENTITY] = {};
            if (head == "node") {
                if (token_count != 4 ||
                    ts.tokens[b.expr_start_pos + 2].kind != TokenKind::String) {
                    return rollback_synth(make_synth_error_at(
                        b.kw_tok, DiagnosticCategory::Syntax,
                        "node reference needs exactly one quoted identity",
                        "Try: (node \"lfo\")"));
                }
                const Token& id_tok = ts.tokens[b.expr_start_pos + 2];
                if (id_tok.string.length == 0 ||
                    id_tok.string.length >= MAX_SYNTH_IDENTITY) {
                    return rollback_synth(make_synth_error_at(
                        id_tok, DiagnosticCategory::Overflow,
                        "Referenced synth identity is empty or too long",
                        "Use an identity from 1 to 31 bytes"));
                }
                std::memcpy(from_identity, source + id_tok.string.offset,
                            id_tok.string.length);
                from_identity[id_tok.string.length] = '\0';
            } else if (head == "synth" ||
                       GraphBuilder::resolve_namespaced_operator(
                           ts.tokens[b.expr_start_pos + 1].symbol).name_space ==
                           OperatorNamespace::Osc) {
                TokenStream nested_ts;
                std::memcpy(nested_ts.tokens,
                            ts.tokens + b.expr_start_pos,
                            token_count * sizeof(Token));
                nested_ts.count = token_count;
                nested_ts.pos = 2; // after `(` and declaration head
                const Token* nested_explicit_def = head == "synth"
                    ? nullptr
                    : &nested_ts.tokens[1];
                EvalResult child = do_synth(
                    nested_ts, engine, source, source_length,
                    from_identity, true, transaction_snapshot,
                    (uint16_t)(nested_depth + 1), nested_explicit_def);
                if (child.kind == EvalResult::Error)
                    return rollback_synth(child);
                // Updating an existing child compacts the declaration table;
                // reacquire the parent by stable identity before touching it
                // again instead of retaining an invalidated array pointer.
                decl = engine.synth_graph.find(identity_buf);
                if (!decl) {
                    return rollback_synth(make_synth_error_at(
                        b.kw_tok, DiagnosticCategory::Runtime,
                        "Nested synth invalidated its parent declaration",
                        nullptr));
                }
                if (!nested_ts.expect(TokenKind::RParen) ||
                    !nested_ts.at_end()) {
                    return rollback_synth(make_synth_error_at(
                        b.kw_tok, DiagnosticCategory::Syntax,
                        "Nested synth has trailing input", nullptr));
                }
            } else {
                return rollback_synth(make_synth_error_at(
                    b.kw_tok, DiagnosticCategory::Boundary,
                    "Audio inputs need (node \"identity\") or a nested synth",
                    "Try: :fm (node \"lfo\")"));
            }

            SynthConnection* edge = engine.synth_graph.append_connection();
            if (!edge) {
                return rollback_synth(make_synth_error_at(
                    b.kw_tok, DiagnosticCategory::Overflow,
                    "Synth connection table is full",
                    "Remove a routed synth node"));
            }
            std::strncpy(edge->from, from_identity, MAX_SYNTH_IDENTITY - 1);
            std::strncpy(edge->to, identity_buf, MAX_SYNTH_IDENTITY - 1);
            std::strncpy(edge->port,
                         def->audio_input_names[b.audio_input_port],
                         MAX_NODEDEF_NAME - 1);
            edge->port_index = (uint16_t)b.audio_input_port;
            continue;
        }

        uint16_t owner_context = allocate_synth_owner_context(
            synth_snapshot, engine.synth_graph,
            identity_buf, b.desc->name);
        if (owner_context == ANON_STATE_CONTEXT_NONE) {
            return rollback_synth(make_synth_error_at(
                b.kw_tok, DiagnosticCategory::Overflow,
                "Synth control ownership table is full",
                "Use (useq-clear) to free earlier synths"));
        }
        engine.registry.begin_context(owner_context);

        const char* expr_source = source + b.expr_byte_start;
        ParsedProgram control_program;
        control_program.parse(expr_source, expr_len);
        if (!control_program.ok()) {
            EvalResult r;
            r.kind = EvalResult::Error;
            for (uint8_t e = 0;
                 e < control_program.diagnostic_count &&
                 r.diagnostic_count < 8; e++) {
                Diagnostic d = control_program.diagnostics[e];
                d.span_start = (uint16_t)(d.span_start + b.expr_byte_start);
                r.diagnostics[r.diagnostic_count++] = d;
            }
            return rollback_synth(r);
        }

        GraphBuildResult gbr = build_output_graph(
            engine.pool, control_program.stream, engine.cells, engine.arena,
            expr_source, &engine.registry, nullptr, owner_context);
        if (gbr.has_error) {
            EvalResult r;
            r.kind = EvalResult::Error;
            for (uint8_t e = 0;
                 e < gbr.diagnostic_count && r.diagnostic_count < 8; e++) {
                Diagnostic d = gbr.diagnostics[e];
                d.span_start = (uint16_t)(d.span_start + b.expr_byte_start);
                r.diagnostics[r.diagnostic_count++] = d;
            }
            return rollback_synth(r);
        }

        SynthControlChannel* ctl = engine.synth_graph.append_control();
        if (!ctl) {
            return rollback_synth(make_synth_error_at(
                b.kw_tok, DiagnosticCategory::Overflow,
                "Synth control table is full",
                "Use (useq-clear) to free earlier synths"));
        }
        const ptrdiff_t parameter_index = b.desc - def->params;
        if (parameter_index < 0 ||
            parameter_index >= static_cast<ptrdiff_t>(def->param_count) ||
            parameter_index >= static_cast<ptrdiff_t>(MAX_NODEDEF_PARAMS)) {
            return rollback_synth(make_synth_error_at(
                b.kw_tok, DiagnosticCategory::Runtime,
                "NodeDef parameter metadata is inconsistent", nullptr));
        }
        ctl->param_index = static_cast<uint8_t>(parameter_index);
        ctl->rate_class = b.desc->rate_class;
        ctl->smoothing_class = b.desc->smoothing_class;
        ctl->root_node = gbr.root_node;
        ctl->owner_context = owner_context;
        ctl->source_length = expr_len;
        ctl->dep_count = gbr.dep_count;
        for (uint8_t d = 0; d < gbr.dep_count; d++)
            ctl->dep_cells[d] = gbr.dep_cells[d];
        const SynthControlChannel* old = find_synth_control(
            synth_snapshot, identity_buf, b.desc->name);
        if (old) {
            ctl->lkg_value = old->lkg_value;
            ctl->has_lkg = old->has_lkg;
        }
        decl->control_count++;
    }

    if (!nested) {
        EvalResult graph_validation =
            validate_synth_patch_graph(engine.synth_graph, def_name_tok);
        if (graph_validation.kind == EvalResult::Error)
            return rollback_synth(graph_validation);
    }

    // Stage append-only writes first. They are reversible by restoring the
    // arena head. Reused regions are overwritten only after no fallible step
    // remains, so a rejected candidate can never poison prior source text.
    for (uint16_t i = 0; i < binding_count; i++) {
        ParamBinding& b = bindings[i];
        if (!b.present || !b.desc) continue;
        uint32_t expr_len = b.expr_byte_end - b.expr_byte_start;
        const SynthControlChannel* old = find_synth_control(
            synth_snapshot, identity_buf, b.desc->name);
        bool can_reuse = !nested && old && expr_len <= old->source_length;
        if (!can_reuse) {
            staged_source_offsets[i] = engine.arena.store(
                source + b.expr_byte_start, expr_len);
            if (staged_source_offsets[i] == UINT32_MAX) {
                return rollback_synth(make_synth_error_at(
                    b.kw_tok, DiagnosticCategory::Overflow,
                    "Source storage full — synth control was not published",
                    "Use (useq-clear) to reclaim session source storage"));
            }
        }
    }
    uint16_t control_index = decl->first_control_index;
    for (uint16_t i = 0; i < binding_count; i++) {
        ParamBinding& b = bindings[i];
        if (!b.present || !b.desc) continue;
        SynthControlChannel& ctl = engine.synth_graph.controls[control_index++];
        uint32_t expr_len = b.expr_byte_end - b.expr_byte_start;
        const SynthControlChannel* old = find_synth_control(
            synth_snapshot, identity_buf, b.desc->name);
        if (staged_source_offsets[i] != UINT32_MAX) {
            ctl.source_offset = staged_source_offsets[i];
        } else {
            ctl.source_offset = engine.arena.store_reuse(
                old->source_offset, old->source_length,
                source + b.expr_byte_start, expr_len);
        }
    }

    // Retire state resources for removed controls, and publish the state
    // writers for controls that survived or were added.
    if (old_declaration) {
        const uint16_t old_end = static_cast<uint16_t>(
            old_declaration->first_control_index +
            old_declaration->control_count);
        for (uint16_t i = old_declaration->first_control_index;
             i < old_end; i++) {
            const SynthControlChannel& old = synth_snapshot.controls[i];
            engine.registry.commit_context(
                old.owner_context, engine.pool.state_update_roots,
                engine.pool.state_owner_context);
        }
    }
    for (uint16_t i = decl->first_control_index;
         i < decl->first_control_index + decl->control_count; i++) {
        engine.registry.commit_context(
            engine.synth_graph.controls[i].owner_context,
            engine.pool.state_update_roots,
            engine.pool.state_owner_context);
    }

    if (out_identity) {
        std::strncpy(out_identity, identity_buf, MAX_SYNTH_IDENTITY - 1);
        out_identity[MAX_SYNTH_IDENTITY - 1] = '\0';
    }
    if (nested) return make_ok();

    // Graph + controls + routing share one revision and one publication.
    engine.synth_graph.advance_revision();
    reclaim_unowned_resources(engine);
    engine.pool.rebuild_execution_order();
    classify_outputs(engine.pool);
    graph_transaction.accept();
    delete owned_snapshot;
    return make_ok();
}
#endif

// ── Clock source management ─────────────────────────────────────────────────
// Sets EngineState clock fields; the firmware tick loop reads these and
// reconfigures its ClockSync module. On non-hardware builds (WASM, desktop)
// these still set the state — they just have no observable effect since no
// gate pulses arrive.

static EvalResult do_set_clock_ext(TokenStream& ts, EngineState& state) {
    Token input_tok = ts.consume();
    if (input_tok.kind != TokenKind::Number ||
        !std::isfinite(input_tok.number) ||
        std::floor(input_tok.number) != input_tok.number) {
        return make_error("set-clock-ext needs input 1 or 2 and an optional divisor",
                          "Try: (set-clock-ext 1 1)");
    }
    int input = (int)input_tok.number;
    if (input != 1 && input != 2) {
        return make_error("set-clock-ext input must be 1 or 2",
                          "Try: (set-clock-ext 1 1) or (set-clock-ext 2 4)");
    }

    int divisor = 1;
    if (ts.peek().kind == TokenKind::Number) {
        Token div_tok = ts.consume();
        if (!std::isfinite(div_tok.number) || div_tok.number < 1.0 ||
            div_tok.number > 255.0 ||
            std::floor(div_tok.number) != div_tok.number) {
            return make_error(
                "set-clock-ext divisor must be a whole number from 1 to 255",
                "Try: (set-clock-ext 1 4)");
        }
        divisor = (int)div_tok.number;
    } else if (ts.peek().kind != TokenKind::RParen) {
        return make_error("set-clock-ext divisor must be a number",
                          "Try: (set-clock-ext 1 4)");
    }
    if (ts.peek().kind != TokenKind::RParen) {
        return make_error("set-clock-ext accepts an input and optional divisor",
                          "Try: (set-clock-ext 1 4)");
    }

    state.clock_source = (uint8_t)input;
    state.ext_clock_divisor = (uint8_t)divisor;
    state.ext_clock_reset = true;
    return make_ok();
}

static EvalResult clock_command_no_args(TokenStream& ts, const char* name) {
    if (ts.peek().kind == TokenKind::RParen) return make_ok();
    static char message[96];
    std::snprintf(message, sizeof(message), "%s accepts no arguments", name);
    return make_error(message, nullptr);
}

static EvalResult do_set_clock_int(TokenStream& ts, EngineState& state) {
    EvalResult arity = clock_command_no_args(ts, "set-clock-int");
    if (arity.kind == EvalResult::Error) return arity;
    state.clock_source = 0;
    return make_ok();
}

static EvalResult do_get_clock_source(TokenStream& ts, EngineState& state) {
    EvalResult arity = clock_command_no_args(ts, "get-clock-source");
    if (arity.kind == EvalResult::Error) return arity;
    EvalResult r;
    r.kind = EvalResult::Number;
    r.number = (Sample)state.clock_source;
    return r;
}

static EvalResult do_reset_clock_ext(TokenStream& ts, EngineState& state) {
    EvalResult arity = clock_command_no_args(ts, "reset-clock-ext");
    if (arity.kind == EvalResult::Error) return arity;
    state.ext_clock_reset = true;
    return make_ok();
}

static EvalResult do_reset_clock_int(TokenStream& ts, EngineState& state) {
    EvalResult arity = clock_command_no_args(ts, "reset-clock-int");
    if (arity.kind == EvalResult::Error) return arity;
    state.rewind();
    return make_ok();
}

// ── Output assignment ───────────────────────────────────────────────────────

static EvalResult do_unassign(TokenStream& ts, SignalEngine& engine) {
    Token output_tok = ts.consume();
    if (output_tok.kind != TokenKind::Symbol ||
        !GraphBuilder::is_output_symbol(output_tok.symbol)) {
        return make_error("unassign needs one output name",
                          "Try: (unassign a1)");
    }
    if (ts.peek().kind != TokenKind::RParen) {
        return make_error("unassign accepts exactly one output name",
                          "Try: (unassign a1)");
    }

    const uint16_t output_index =
        GraphBuilder::resolve_output_index(output_tok.symbol);

    // Validation above is complete before the publication point.  Removing
    // a program also removes its sample history, source/dependencies and
    // owner-scoped state/live resources; the next running tick emits neutral
    // zero for the now-inactive slot.
    engine.pool.outputs[output_index] = OutputSlot{};
    engine.pool.prev_output_values[output_index] = 0.0;
    engine.pool.output_deps[output_index].clear();
    engine.output_sources[output_index] = OutputSource{};
    engine.pool.runtime_fallback_mask &=
        ~((uint64_t)1 << output_index);
    engine.output_compile_diagnostics[output_index] =
        ActiveCompileDiagnostic{};

    engine.registry.begin_context(output_index);
    engine.registry.commit_context(output_index,
                                   engine.pool.state_update_roots,
                                   engine.pool.state_owner_context);
    reclaim_unowned_resources(engine);
    engine.pool.rebuild_execution_order();
    classify_outputs(engine.pool);
    return make_ok();
}

static EvalResult do_output_assign(SymbolID output_sym, TokenStream& ts,
                                    SignalEngine& engine,
                                    const char* source, uint32_t source_length,
                                    SharedLiveEditIDs* shared_ids = nullptr) {
    uint16_t output_index = GraphBuilder::resolve_output_index(output_sym);
    if (output_index == NODE_NONE) {
        return make_error("Unknown output", "Try: (a1 expression)");
    }

    // Locate and preflight expression storage, but publish the bytes only
    // after graph construction succeeds. store_reuse() may overwrite an old
    // region in place, so writing here would poison reactive recompilation
    // when the replacement later fails.
    uint16_t expr_start_pos = ts.pos;
    uint32_t byte_start = span_begin(ts, expr_start_pos);
    const OutputSource previous_source = engine.output_sources[output_index];
    uint16_t saved = ts.pos;
    GraphBuilder::skip_form(ts);
    uint32_t byte_end = span_end_of(ts, ts.pos);
    ts.rewind(saved);
    SourceMutationPlan source_plan = SourceMutationPlan::prepare(
        engine.arena, source, source_length, byte_start, byte_end,
        previous_source.has_source
            ? previous_source.arena_offset : UINT32_MAX,
        previous_source.has_source ? previous_source.arena_length : 0);
    if (source_plan.status == SourcePlanStatus::CapacityExceeded) {
        return make_error(
            "Program storage is full — output not changed",
            "Free space with (useq-clear) or shorten your program");
    }
    if (source_plan.status == SourcePlanStatus::InvalidSpan) {
        return make_error("Output expression has an invalid source span",
                          "Resubmit the complete output form");
    }

    GraphMutationTransaction graph_transaction(engine);
    engine.registry.begin_context(output_index);
    GraphBuildResult result = build_output_graph(engine.pool, ts,
                                                 engine.cells, engine.arena, source,
                                                 &engine.registry, shared_ids,
                                                 output_index);

    if (result.has_error || ts.peek().kind != TokenKind::RParen) {
        // Per failure-model.md §2.6, a compile-time error leaves the active
        // program unchanged: do NOT demote `valid`. Demoting here was also the
        // root cause of A2 — sig::commit_outputs resurrects valid=true for any
        // output with a root node, so the flag flapped and the WASM batch-vis
        // row packing drifted mid-batch.
        EvalResult r;
        r.kind = EvalResult::Error;
        if (result.has_error) {
            memcpy(r.diagnostics, result.diagnostics,
                   result.diagnostic_count * sizeof(Diagnostic));
            r.diagnostic_count = result.diagnostic_count;
        } else {
            r = make_error("Output assignment accepts exactly one expression",
                           "Try: (a1 expression)");
        }
        return r;
    }

    if (source_plan.has_source()) {
        uint32_t offset = source_plan.publish(engine.arena);
        if (offset == UINT32_MAX) {
            return make_error(
                "Program storage is full — output not changed",
                "Free space with (useq-clear) or shorten your program");
        }
        engine.output_sources[output_index].arena_offset = offset;
        engine.output_sources[output_index].arena_length = source_plan.length;
        engine.output_sources[output_index].has_source = true;
    }

    // Publish the candidate root, dependencies, ownership, and health only
    // after parsing, storage capacity, and graph construction all succeeded.
    publish_output_graph_plan(engine, output_index, result);
    graph_transaction.accept();

    // Reclaim nodes no longer reachable from any output root
    reclaim_unowned_resources(engine);

    // Re-sort execution order
    engine.pool.rebuild_execution_order();
    classify_outputs(engine.pool);

    return make_ok();
}

// ── Scratch-isolated expression evaluation ─────────────────────────────────

EvalResult eval_expression(const char* source, uint32_t length,
                           SignalEngine& engine) {
    ParsedProgram program;
    program.parse(source, length);
    if (!program.ok()) {
        EvalResult r;
        r.kind = EvalResult::Error;
        memcpy(r.diagnostics, program.diagnostics,
               program.diagnostic_count * sizeof(Diagnostic));
        r.diagnostic_count = program.diagnostic_count;
        return r;
    }
    if (program.empty()) return make_ok();
    TokenStream& ts = program.stream;

    // Save CellStore data table state (compilation may append vector literals)
    uint8_t saved_table_count = engine.cells.data_table_count;

    // Reset scratch pool
    engine.scratch_pool.reset();

    // Mirror live state values into the scratch pool BEFORE compiling (A5,
    // state-identity.md §6.6): compiling a stateful expression writes its
    // init value into a freshly-allocated scratch slot; copying live values
    // afterwards clobbered those inits.
    memcpy(engine.scratch_pool.state_values, engine.pool.state_values,
           sizeof(engine.pool.state_values));

    // Compile into scratch pool
    GraphBuildResult result = build_output_graph(
        engine.scratch_pool, ts, engine.cells, engine.arena, source);

    if (result.has_error) {
        engine.cells.data_table_count = saved_table_count;
        EvalResult r;
        r.kind = EvalResult::Error;
        memcpy(r.diagnostics, result.diagnostics,
               result.diagnostic_count * sizeof(Diagnostic));
        r.diagnostic_count = result.diagnostic_count;
        return r;
    }

    // Install as output 0 and build execution order
    engine.scratch_pool.outputs[0].root_node = result.root_node;
    engine.scratch_pool.outputs[0].valid = true;
    engine.scratch_pool.rebuild_execution_order();

    // Snapshot cell values
    Sample cell_values[MAX_CELLS];
    engine.cells.snapshot_values(cell_values, MAX_CELLS);

    // Execute one sample
    Sample hw_inputs[32] = {};
    Sample outputs[MAX_OUTPUTS] = {};
    Sample workspace[MAX_TOTAL_NODES] = {};

    ExecutionContext ctx;
    ctx.t = engine.state.current_time;
    ctx.dt = engine.state.current_dt;
    ctx.cell_values = cell_values;
    ctx.hw_inputs = hw_inputs;
    ctx.data_pool = engine.cells.data_pool;
    ctx.data_offsets = engine.cells.data_offsets;
    ctx.data_lengths = engine.cells.data_lengths;
    ctx.prev_outputs = engine.pool.prev_output_values;
    ctx.output_values = outputs;
    ctx.workspace = workspace;
    ctx.ground_t = engine.state.current_wall_time;
    execute_all_outputs(engine.scratch_pool, ctx);

    // Restore CellStore data table state
    engine.cells.data_table_count = saved_table_count;

    return make_number(outputs[0]);
}

// ── Atomic bulk definitions ───────────────────────────────────────────────

static EvalResult do_defs(TokenStream& ts, SignalEngine& engine,
                          const char* source, uint32_t source_length) {
    if (!ts.expect(TokenKind::LBracket)) {
        return make_error("defs needs a bracket list of name-value pairs",
                          "Try: (defs [x 1 y 2 z 3])");
    }

    constexpr uint16_t MAX_DEFS_BINDINGS = 64;
    SymbolID symbols[MAX_DEFS_BINDINGS] = {};
    uint16_t binding_count = 0;
    uint16_t list_start = ts.pos;
    uint16_t new_table_count = 0;
    uint32_t new_data_entries = 0;
    uint32_t new_source_bytes = 0;

    // Validation/preflight pass. No live store is touched until the complete
    // list, outer arity, and cumulative fixed-store capacity are proven.
    while (ts.peek().kind != TokenKind::RBracket && !ts.at_end()) {
        if (binding_count >= MAX_DEFS_BINDINGS) {
            return make_error("defs has too many bindings",
                              "Split the definitions into smaller forms");
        }
        Token name_tok = ts.consume();
        if (name_tok.kind != TokenKind::Symbol) {
            return make_error("defs: expected a name",
                              "Try: (defs [x 1 y 2])");
        }
        if (cell_id_out_of_range(name_tok.symbol))
            return make_too_many_definitions_error();
        for (uint16_t i = 0; i < binding_count; i++) {
            if (symbols[i] == name_tok.symbol) {
                return make_error("defs contains the same name twice",
                                  "Keep one value for each name");
            }
        }
        symbols[binding_count++] = name_tok.symbol;

        if (ts.at_end() || ts.peek().kind == TokenKind::RBracket) {
            return make_error("defs: each name needs a value",
                              "Try: (defs [x 1 y 2])");
        }

        Token val_tok = ts.peek();
        if (val_tok.kind == TokenKind::Number) {
            ts.consume();
        } else if (val_tok.kind == TokenKind::LBracket) {
            ts.consume();
            uint16_t count = 0;
            while (ts.peek().kind != TokenKind::RBracket && !ts.at_end()) {
                Token elem = ts.consume();
                if (elem.kind != TokenKind::Number) {
                    return make_error("defs vectors contain only numbers",
                                      "Try: (defs [steps [1 0 1 0]])");
                }
                if (++count > 64) {
                    return make_error("defs vector is too long",
                                      "Use at most 64 values");
                }
            }
            if (!ts.expect(TokenKind::RBracket)) {
                return make_error("defs has an unterminated vector",
                                  "Close the vector with ]");
            }
            new_table_count++;
            new_data_entries += count;
        } else {
            uint32_t byte_start = span_begin(ts, ts.pos);
            GraphBuilder::skip_form(ts);
            uint32_t byte_end = span_end_of(ts, ts.pos);
            if (!source || byte_end <= byte_start ||
                byte_end > source_length) {
                return make_error("defs has an invalid value expression",
                                  "Try: (defs [x (+ 1 2)])");
            }
            uint32_t len = byte_end - byte_start;
            const CallableInfo previous =
                engine.cells.cells[name_tok.symbol].kind == CellKind::Callable
                    ? engine.cells.callables[name_tok.symbol] : CallableInfo{};
            bool can_reuse =
                previous.source_offset <= SOURCE_ARENA_SIZE &&
                previous.source_length <=
                    SOURCE_ARENA_SIZE - previous.source_offset &&
                len <= previous.source_length;
            if (!can_reuse) {
                new_source_bytes += len;
            }
        }
    }

    if (!ts.expect(TokenKind::RBracket)) {
        return make_error("defs needs a closing bracket",
                          "Try: (defs [x 1 y 2])");
    }
    if (ts.peek().kind != TokenKind::RParen) {
        return make_error("defs accepts exactly one binding vector",
                          "Try: (defs [x 1 y 2])");
    }

    uint32_t data_used = 0;
    if (engine.cells.data_table_count > 0) {
        uint16_t last = engine.cells.data_table_count - 1;
        data_used = engine.cells.data_offsets[last] +
                    engine.cells.data_lengths[last];
    }
    if ((uint32_t)engine.cells.data_table_count + new_table_count >
            MAX_DATA_TABLES ||
        data_used + new_data_entries > MAX_DATA_ENTRIES) {
        return make_error("Data table storage is full — defs not applied",
                          "Free space with (useq-clear) or use fewer vectors");
    }
    if (engine.arena.write_head > SOURCE_ARENA_SIZE ||
        new_source_bytes > SOURCE_ARENA_SIZE - engine.arena.write_head) {
        return make_error("Program storage is full — defs not applied",
                          "Free space with (useq-clear) or shorten your program");
    }

    // Commit pass. Every operation below has been capacity-checked above.
    ts.rewind(list_start);
    for (uint16_t binding = 0; binding < binding_count; binding++) {
        Token name_tok = ts.consume();
        SymbolID cell_sym = name_tok.symbol;
        Token val_tok = ts.peek();

        if (val_tok.kind == TokenKind::Number) {
            ts.consume();
            engine.cells.cells[cell_sym].kind = CellKind::Number;
            engine.cells.cells[cell_sym].flags = 0;
            engine.cells.cells[cell_sym].revision++;
            engine.cells.cells[cell_sym].value = val_tok.number;
            engine.cells.callables[cell_sym] = CallableInfo{};
        } else if (val_tok.kind == TokenKind::LBracket) {
            ts.consume();
            Sample values[64];
            uint16_t count = 0;
            while (ts.peek().kind != TokenKind::RBracket) {
                values[count++] = ts.consume().number;
            }
            ts.expect(TokenKind::RBracket);
            uint16_t table_id =
                engine.cells.store_data_table(values, count);
            engine.cells.cells[cell_sym].kind = CellKind::Data;
            engine.cells.cells[cell_sym].flags = 0;
            engine.cells.cells[cell_sym].data_table_id = table_id;
            engine.cells.cells[cell_sym].revision++;
            engine.cells.cells[cell_sym].value = (Sample)count;
            engine.cells.callables[cell_sym] = CallableInfo{};
        } else {
            uint32_t byte_start = span_begin(ts, ts.pos);
            GraphBuilder::skip_form(ts);
            uint32_t byte_end = span_end_of(ts, ts.pos);
            uint32_t len = byte_end - byte_start;
            const CallableInfo previous =
                engine.cells.cells[cell_sym].kind == CellKind::Callable
                    ? engine.cells.callables[cell_sym] : CallableInfo{};
            uint32_t offset = engine.arena.store_reuse(
                previous.source_offset, previous.source_length,
                source + byte_start, len);
            engine.cells.cells[cell_sym].kind = CellKind::Callable;
            engine.cells.cells[cell_sym].flags = 0;
            engine.cells.cells[cell_sym].revision++;
            engine.cells.callables[cell_sym] = CallableInfo{};
            engine.cells.callables[cell_sym].source_offset = offset;
            engine.cells.callables[cell_sym].source_length = len;
        }
    }
    ts.expect(TokenKind::RBracket);

    for (uint16_t i = 0; i < binding_count; i++) {
        on_cell_changed(symbols[i], engine);
    }
    return make_ok();
}

// ── Top-level eval ──────────────────────────────────────────────────────────

struct EvalDispatchResult {
    bool handled = false;
    EvalResult result = {};
};

static EvalResult finish_parenthesized_form(TokenStream& ts,
                                            EvalResult result) {
    ts.expect(TokenKind::RParen);
    return result;
}

#if USEQ_HAS_TEMPORAL_QUERY

constexpr size_t MAX_PRINTED_TEMPORAL_EVENTS = 64;

class TemporalTextBuilder {
public:
    TemporalTextBuilder(char* storage, size_t storage_capacity)
        : buffer(storage), capacity(storage_capacity) {
        if (capacity > 0) buffer[0] = '\0';
    }

    void append(const char* text) {
        if (!valid || !text) return;
        const size_t length = std::strlen(text);
        if (position >= capacity || length >= capacity - position) {
            valid = false;
            return;
        }
        std::memcpy(buffer + position, text, length);
        position += length;
        buffer[position] = '\0';
    }

    void append_number(Sample value) {
        if (!valid) return;
        const int written = std::snprintf(
            buffer + position, capacity - position, "%.15g", value);
        accept_written(written);
    }

    void append_size(size_t value) {
        if (!valid) return;
        const int written = std::snprintf(
            buffer + position, capacity - position, "%zu", value);
        accept_written(written);
    }

    void append_node(uint16_t value) {
        if (!valid) return;
        const int written = std::snprintf(
            buffer + position, capacity - position, "%u",
            static_cast<unsigned>(value));
        accept_written(written);
    }

    bool ok() const { return valid; }
    uint16_t length() const { return static_cast<uint16_t>(position); }

private:
    char* buffer = nullptr;
    size_t capacity = 0;
    size_t position = 0;
    bool valid = true;

    void accept_written(int written) {
        if (position >= capacity || written < 0 ||
            static_cast<size_t>(written) >= capacity - position) {
            valid = false;
            return;
        }
        position += static_cast<size_t>(written);
    }
};

static const char* temporal_direction_keyword(TemporalDirection direction) {
    switch (direction) {
        case TemporalDirection::Any: return ":any";
        case TemporalDirection::Rising: return ":rising";
        case TemporalDirection::Falling: return ":falling";
        case TemporalDirection::Tangent: return ":tangent";
    }
    return ":any";
}

static EvalResult finish_temporal_text(SignalEngine& engine,
                                       const TemporalTextBuilder& text) {
    if (!text.ok()) {
        return make_error(
            "Temporal query result is too large to print",
            "Query a shorter time window");
    }
    EvalResult result;
    result.kind = EvalResult::Text;
    result.text = engine.eval_text_buf;
    result.text_length = text.length();
    return result;
}

static EvalResult format_temporal_status(SignalEngine& engine,
                                         const TemporalQueryResult& query) {
    TemporalTextBuilder text(engine.eval_text_buf,
                             sizeof(engine.eval_text_buf));
    text.append("{:status :");
    text.append(temporal_query_status_to_cstr(query.status));
    text.append(" :analysis :analytic-piecewise-affine-wave :segments ");
    text.append_size(query.segment_count);
    if (query.detail_node != NODE_NONE) {
        text.append(" :detail-node ");
        text.append_node(query.detail_node);
    }
    text.append("}");
    return finish_temporal_text(engine, text);
}

static void append_temporal_arc(TemporalTextBuilder& text, TemporalArc arc) {
    text.append("[");
    text.append_number(arc.start);
    text.append(" ");
    text.append_number(arc.end);
    text.append("]");
}

static EvalResult format_temporal_events(
        SignalEngine& engine, const TemporalQueryResult& query,
        const TemporalEvent* events) {
    if (query.status != TemporalQueryStatus::Complete)
        return format_temporal_status(engine, query);

    TemporalTextBuilder text(engine.eval_text_buf,
                             sizeof(engine.eval_text_buf));
    text.append("[");
    for (size_t index = 0; index < query.event_count; ++index) {
        const TemporalEvent& event = events[index];
        if (index > 0) text.append(" ");
        if (event.kind == TemporalEventKind::Instant) {
            text.append("[:instant ");
            text.append(temporal_direction_keyword(event.direction));
            text.append(" :whole ");
            append_temporal_arc(text, event.whole);
            text.append(" :part ");
            append_temporal_arc(text, event.part);
            text.append("]");
        } else {
            text.append("[:span :whole ");
            if (event.whole_known) append_temporal_arc(text, event.whole);
            else text.append("nil");
            text.append(" :part ");
            append_temporal_arc(text, event.part);
            text.append("]");
        }
    }
    text.append("]");
    return finish_temporal_text(engine, text);
}

static bool parse_temporal_pair(TokenStream& ts, Sample& first,
                                Sample& second, EvalResult& error,
                                const char* message,
                                const char* suggestion) {
    if (ts.peek().kind != TokenKind::LBracket) {
        error = make_error(message, suggestion);
        return false;
    }
    ts.consume();
    Token first_token = ts.consume();
    Token second_token = ts.consume();
    if (first_token.kind != TokenKind::Number ||
        second_token.kind != TokenKind::Number ||
        ts.peek().kind != TokenKind::RBracket ||
        !std::isfinite(first_token.number) ||
        !std::isfinite(second_token.number)) {
        error = make_error(message, suggestion);
        return false;
    }
    ts.consume();
    first = first_token.number;
    second = second_token.number;
    return true;
}

static EvalResult temporal_graph_error(const GraphBuildResult& graph) {
    EvalResult error;
    error.kind = EvalResult::Error;
    std::memcpy(error.diagnostics, graph.diagnostics,
                graph.diagnostic_count * sizeof(Diagnostic));
    error.diagnostic_count = graph.diagnostic_count;
    return error;
}

static EvalResult do_temporal_query(SymbolID op, TokenStream& ts,
                                    SignalEngine& engine,
                                    const char* source,
                                    const GraphBuilder::Symbols& symbols) {
    TemporalArc arc;
    TemporalCondition condition = TemporalCondition::equal_to(0.0);
    bool explain_only = false;
    EvalResult parse_error;

    if (op == symbols.q_zeros || op == symbols.q_explain) {
        if (!parse_temporal_pair(
                ts, arc.start, arc.end, parse_error,
                "Temporal query needs a [start end] time window",
                op == symbols.q_zeros
                    ? "Try: (q/zeros [0 1] (bsin t))"
                    : "Try: (q/explain [0 1] (bsin t))"))
            return parse_error;
        explain_only = op == symbols.q_explain;
    } else if (op == symbols.q_crossings) {
        Token level = ts.consume();
        if (level.kind != TokenKind::Number || !std::isfinite(level.number)) {
            return make_error(
                "q/crossings needs a finite level first",
                "Try: (q/crossings 0 [0 1] (bsin t))");
        }
        if (!parse_temporal_pair(
                ts, arc.start, arc.end, parse_error,
                "q/crossings needs a [start end] time window",
                "Try: (q/crossings 0 [0 1] (bsin t))"))
            return parse_error;
        TemporalDirection direction = TemporalDirection::Any;
        if (ts.peek().kind == TokenKind::Symbol &&
            (ts.peek().symbol == symbols.kw_rising ||
             ts.peek().symbol == symbols.kw_falling)) {
            direction = ts.consume().symbol == symbols.kw_rising
                ? TemporalDirection::Rising
                : TemporalDirection::Falling;
        }
        condition = TemporalCondition::crosses(level.number, direction);
    } else if (op == symbols.q_range) {
        Sample low = 0.0;
        Sample high = 0.0;
        if (!parse_temporal_pair(
                ts, low, high, parse_error,
                "q/range needs a [low high] value range first",
                "Try: (q/range [-0.5 0.5] [0 1] (bsin t))"))
            return parse_error;
        if (!parse_temporal_pair(
                ts, arc.start, arc.end, parse_error,
                "q/range needs a [start end] time window second",
                "Try: (q/range [-0.5 0.5] [0 1] (bsin t))"))
            return parse_error;
        if (low > high) {
            return make_error(
                "q/range needs low to be less than or equal to high",
                "Try: (q/range [-0.5 0.5] [0 1] (bsin t))");
        }
        condition = TemporalCondition::between(low, high);
    }

    if (!(arc.start < arc.end)) {
        return make_error(
            "Temporal query time window must increase",
            "Use a window like [0 1], where start is before end");
    }
    if (ts.peek().kind == TokenKind::RParen || ts.at_end()) {
        return make_error(
            "Temporal query needs a final target signal",
            "Put the signal last, for example (q/zeros [0 1] (bsin t))");
    }

    TemporalQueryOptions options;
    options.max_events = MAX_PRINTED_TEMPORAL_EVENTS;
    TemporalEvent events[MAX_PRINTED_TEMPORAL_EVENTS] = {};
    TemporalQueryResult query;

    // A bare active output is a reference to its published root for query
    // purposes, not the previous-sample value that it denotes in a signal.
    const bool final_output_target =
        ts.peek().kind == TokenKind::Symbol &&
        GraphBuilder::is_output_symbol(ts.peek().symbol) &&
        ts.pos + 1 < ts.count &&
        ts.tokens[ts.pos + 1].kind == TokenKind::RParen;

    if (final_output_target) {
        const uint16_t output_index =
            GraphBuilder::resolve_output_index(ts.consume().symbol);
        if (explain_only) {
            if (output_index >= MAX_OUTPUTS ||
                !engine.pool.outputs[output_index].valid ||
                engine.pool.outputs[output_index].root_node == NODE_NONE) {
                query.status = TemporalQueryStatus::InactiveOutput;
            } else {
                query = inspect_temporal_root(
                    engine.pool, engine.cells,
                    engine.pool.outputs[output_index].root_node, arc, options);
            }
        } else {
            query = query_temporal_output(
                engine.pool, engine.cells, output_index, arc, condition,
                events, MAX_PRINTED_TEMPORAL_EVENTS, options);
        }
    } else {
        const uint8_t saved_table_count = engine.cells.data_table_count;
        engine.scratch_pool.reset();
        std::memcpy(engine.scratch_pool.state_values,
                    engine.pool.state_values,
                    sizeof(engine.pool.state_values));

        GraphBuildResult graph = build_output_graph(
            engine.scratch_pool, ts, engine.cells, engine.arena, source);
        if (graph.has_error) {
            engine.cells.data_table_count = saved_table_count;
            return temporal_graph_error(graph);
        }
        if (ts.peek().kind != TokenKind::RParen) {
            engine.cells.data_table_count = saved_table_count;
            return make_error(
                "The temporal query target signal must be the last argument",
                "Move the signal to the final position");
        }

        query = explain_only
            ? inspect_temporal_root(engine.scratch_pool, engine.cells,
                                    graph.root_node, arc, options)
            : query_temporal_root(engine.scratch_pool, engine.cells,
                                  graph.root_node, arc, condition, events,
                                  MAX_PRINTED_TEMPORAL_EVENTS, options);
        engine.cells.data_table_count = saved_table_count;
    }

    return explain_only
        ? format_temporal_status(engine, query)
        : format_temporal_events(engine, query, events);
}

#endif // USEQ_HAS_TEMPORAL_QUERY

static EvalDispatchResult eval_cell_mutation_form(
        SymbolID op, TokenStream& ts, SignalEngine& engine,
        const char* source, uint32_t source_length,
        const GraphBuilder::Symbols& symbols) {
    EvalResult result;
    if (op == symbols.define || op == symbols.def) {
        result = do_define(ts, engine, source, source_length);
    } else if (op == symbols.defn || op == symbols.defun) {
        result = do_defn(ts, engine, source, source_length);
    } else if (op == symbols.defs) {
        result = do_defs(ts, engine, source, source_length);
    } else if (op == symbols.defstate) {
        result = do_defstate(ts, engine, source, source_length);
    } else if (op == symbols.set) {
        result = do_set(ts, engine, source);
    } else if (op == symbols.unassign) {
        result = do_unassign(ts, engine);
    } else {
        return {};
    }
    return {true, finish_parenthesized_form(ts, result)};
}

static EvalDispatchResult reject_direct_live_edit_argument(
        TokenStream& ts, const char* message) {
    if (!next_is_live_edit(ts)) return {};
    while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) ts.consume();
    ts.expect(TokenKind::RParen);
    return {true, make_error(
        message,
        "live-edit must be used inside an output expression like (a1 ...)")};
}

static EvalDispatchResult eval_transport_form(
        SymbolID op, TokenStream& ts, SignalEngine& engine,
        const GraphBuilder::Symbols& symbols) {
    EvalDispatchResult rejected;
    EvalResult result;
    if (op == symbols.set_bpm) {
        rejected = reject_direct_live_edit_argument(
            ts, "live-edit is not allowed as a direct argument of set-bpm");
        if (rejected.handled) return rejected;
        result = do_set_bpm(ts, engine);
    } else if (op == symbols.set_time_sig) {
        rejected = reject_direct_live_edit_argument(
            ts, "live-edit is not allowed as a direct argument of set-time-sig");
        if (rejected.handled) return rejected;
        result = do_set_time_sig(ts, engine);
    } else if (op == symbols.set_time_offset) {
        rejected = reject_direct_live_edit_argument(
            ts, "live-edit is not allowed as a direct argument of set-time-offset");
        if (rejected.handled) return rejected;
        result = do_set_time_offset(ts, engine.state);
    } else if (op == symbols.nudge_time) {
        rejected = reject_direct_live_edit_argument(
            ts, "live-edit is not allowed as a direct argument of nudge-time");
        if (rejected.handled) return rejected;
        result = do_nudge_time(ts, engine.state);
    } else if (op == symbols.useq_clear) {
        if (ts.peek().kind != TokenKind::RParen) {
            return {true, make_error("useq-clear accepts no arguments",
                                     "Try: (useq-clear)")};
        }
        result = do_useq_clear(engine);
    } else if (op == symbols.useq_play || op == symbols.useq_pause ||
               op == symbols.useq_stop || op == symbols.useq_rewind) {
        if (ts.peek().kind != TokenKind::RParen) {
            const char* suggestion = op == symbols.useq_play ? "Try: (useq-play)" :
                                     op == symbols.useq_pause ? "Try: (useq-pause)" :
                                     op == symbols.useq_stop ? "Try: (useq-stop)" :
                                                              "Try: (useq-rewind)";
            const char* message = op == symbols.useq_play
                ? "useq-play accepts no arguments"
                : op == symbols.useq_pause
                    ? "useq-pause accepts no arguments"
                    : op == symbols.useq_stop
                        ? "useq-stop accepts no arguments"
                        : "useq-rewind accepts no arguments";
            return {true, make_error(message, suggestion)};
        }
        if (op == symbols.useq_play) engine.state.play();
        else if (op == symbols.useq_pause) engine.state.pause();
        else if (op == symbols.useq_stop) engine.state.stop();
        else engine.state.rewind();
        result = make_ok();
    } else {
        return {};
    }
    return {true, finish_parenthesized_form(ts, result)};
}

static EvalResult eval_top_level_vector(TokenStream& ts, SignalEngine& engine,
                                        const char* source,
                                        uint32_t source_length) {
    ts.consume();
    engine.scratch_pool.reset();
    const uint8_t saved_table_count = engine.cells.data_table_count;

    char* buffer = engine.eval_text_buf;
    uint16_t buffer_position = 0;
    buffer[buffer_position++] = '[';
    bool first = true;
    EvalResult error = {};

    while (ts.peek().kind != TokenKind::RBracket && !ts.at_end()) {
        const Token element_start = ts.peek();
        if (element_start.kind == TokenKind::LParen ||
            element_start.kind == TokenKind::LBracket) {
            GraphBuilder::skip_form(ts);
        } else {
            ts.consume();
        }

        const uint32_t span_start = element_start.span_start;
        const Token previous = ts.tokens[ts.pos > 0 ? ts.pos - 1 : 0];
        const uint32_t span_end =
            static_cast<uint32_t>(previous.span_start) + previous.span_len;
        if (!source || span_end <= span_start || span_end > source_length) {
            continue;
        }

        const EvalResult element = eval_expression(
            source + span_start, span_end - span_start, engine);
        if (element.kind == EvalResult::Error) {
            error = element;
            break;
        }
        constexpr uint16_t capacity = sizeof(engine.eval_text_buf);
        if (!first && buffer_position < capacity - 20) {
            buffer[buffer_position++] = ' ';
        }
        first = false;
        const int written = snprintf(buffer + buffer_position,
                                     capacity - buffer_position,
                                     "%.15g", element.number);
        if (written > 0) buffer_position += static_cast<uint16_t>(written);
    }

    while (ts.peek().kind != TokenKind::RBracket && !ts.at_end()) ts.consume();
    ts.expect(TokenKind::RBracket);
    engine.cells.data_table_count = saved_table_count;
    if (error.kind == EvalResult::Error) return error;

    constexpr uint16_t capacity = sizeof(engine.eval_text_buf);
    if (buffer_position < capacity - 1) buffer[buffer_position++] = ']';
    buffer[buffer_position] = '\0';
    EvalResult result;
    result.kind = EvalResult::Text;
    result.text = buffer;
    result.text_length = buffer_position;
    return result;
}

static EvalResult eval_form(TokenStream& ts, SignalEngine& engine,
                            const char* source, uint32_t source_length,
                            SharedLiveEditIDs* shared_ids) {
    Token tok = ts.peek();

    if (tok.kind == TokenKind::Number) {
        ts.consume();
        return make_number(tok.number);
    }

    if (tok.kind == TokenKind::Symbol) {
        ts.consume();
        SymbolID sym = tok.symbol;
        if (sym < MAX_CELLS && engine.cells.cells[sym].kind == CellKind::Number) {
            const Cell& cell = engine.cells.cells[sym];
            if (cell.flags == 0x02 &&
                cell.data_table_id < engine.pool.state_slot_count) {
                return make_number(engine.pool.state_values[cell.data_table_id]);
            }
            return make_number(cell.value);
        }
        if (source && tok.span_start + tok.span_len <= source_length) {
            return eval_expression(
                source + tok.span_start, tok.span_len, engine);
        }
        return make_ok();
    }

    if (tok.kind == TokenKind::LBracket) {
        return eval_top_level_vector(ts, engine, source, source_length);
    }

    if (tok.kind == TokenKind::LParen) {
        ts.consume(); // eat '('
        Token op_tok = ts.consume();
        if (op_tok.kind != TokenKind::Symbol) {
            return make_error("Expected a function name after '('",
                              "Try: (define name value)");
        }
        SymbolID op = op_tok.symbol;

        GraphBuilder::init_symbols();
        auto& sym = GraphBuilder::sym;

        const EvalDispatchResult mutation = eval_cell_mutation_form(
            op, ts, engine, source, source_length, sym);
        if (mutation.handled) return mutation.result;

        const EvalDispatchResult transport =
            eval_transport_form(op, ts, engine, sym);
        if (transport.handled) return transport.result;

#if USEQ_HAS_TEMPORAL_QUERY
        if (op == sym.q_zeros || op == sym.q_crossings ||
            op == sym.q_range || op == sym.q_explain) {
            EvalResult result = do_temporal_query(
                op, ts, engine, source, sym);
            ts.expect(TokenKind::RParen);
            return result;
        }
#endif

#if USEQ_HAS_SYNTH_ENGINE
        const NamespacedOperator namespaced =
            GraphBuilder::resolve_namespaced_operator(op);

        // synth — top-level NodeDef instantiation (synth-nodes.md §3)
        if (op == sym.synth) {
            EvalResult r = do_synth(ts, engine, source, source_length);
            ts.expect(TokenKind::RParen);
            return r;
        }

        // osc/<name> — registry-backed sugar for (synth "osc/<name>" ...).
        if (namespaced.name_space == OperatorNamespace::Osc) {
            EvalResult r = do_synth(
                ts, engine, source, source_length,
                nullptr, false, nullptr, 0, &op_tok);
            ts.expect(TokenKind::RParen);
            return r;
        }

        // with-state-id — identity wrapper (state-identity.md §2.2).
        // The editor payload builder wraps anonymous stateful forms in
        // `(with-state-id "<id>" <form>)`. The wrapper and `:id` normalise
        // to the same internal identity annotation: the id is stashed as
        // the pending state identity, and the first synth declaration
        // evaluated under the wrapper consumes it (using it only when the
        // form has no explicit :name/:id — ergo e58f128f). Nested wrappers
        // scope via save/restore; the slot is always restored on return so
        // an id can never leak past its wrapped form.
        if (op == sym.with_state_id) {
            Token id_tok = ts.consume();
            if (id_tok.kind != TokenKind::String) {
                // Malformed wrapper — drain to RParen and report.
                while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) ts.consume();
                ts.expect(TokenKind::RParen);
                return make_error(
                    "with-state-id needs a string identity in the first position",
                    "This form is generated by the editor; if you are seeing "
                    "this error, the editor payload builder may be out of "
                    "sync with the runtime.");
            }

            // Prove the wrapper contains exactly one complete child before
            // evaluating that potentially effectful child.
            uint16_t child_start = ts.pos;
            GraphBuilder::skip_form(ts);
            if (ts.peek().kind != TokenKind::RParen) {
                return make_error(
                    "with-state-id accepts exactly one wrapped form",
                    "Try: (with-state-id \"id\" (a1 expression))");
            }
            ts.rewind(child_start);

            char saved_id[MAX_SYNTH_IDENTITY];
            std::memcpy(saved_id, engine.pending_state_identity,
                        MAX_SYNTH_IDENTITY);
            bool saved_flag = engine.has_pending_state_identity;

            uint16_t n = id_tok.string.length;
            if (n >= MAX_SYNTH_IDENTITY) n = MAX_SYNTH_IDENTITY - 1;
            std::memcpy(engine.pending_state_identity,
                        source + id_tok.string.offset, n);
            engine.pending_state_identity[n] = '\0';
            engine.has_pending_state_identity = true;

            EvalResult r = eval_form(ts, engine, source, source_length, shared_ids);

            std::memcpy(engine.pending_state_identity, saved_id,
                        MAX_SYNTH_IDENTITY);
            engine.has_pending_state_identity = saved_flag;
            ts.expect(TokenKind::RParen);
            return r;
        }
#endif

        // Clock source management
        if (op == sym.set_clock_ext) {
            EvalResult r = do_set_clock_ext(ts, engine.state);
            ts.expect(TokenKind::RParen);
            return r;
        }
        if (op == sym.set_clock_int) {
            EvalResult r = do_set_clock_int(ts, engine.state);
            ts.expect(TokenKind::RParen);
            return r;
        }
        if (op == sym.get_clock_source) {
            EvalResult r = do_get_clock_source(ts, engine.state);
            ts.expect(TokenKind::RParen);
            return r;
        }
        if (op == sym.reset_clock_ext) {
            EvalResult r = do_reset_clock_ext(ts, engine.state);
            ts.expect(TokenKind::RParen);
            return r;
        }
        if (op == sym.reset_clock_int) {
            EvalResult r = do_reset_clock_int(ts, engine.state);
            ts.expect(TokenKind::RParen);
            return r;
        }

        // zeros — create a vector of N zeros
        if (op == sym.zeros_) {
            Token n_tok = ts.consume();
            if (n_tok.kind != TokenKind::Number) {
                ts.expect(TokenKind::RParen);
                return make_error("zeros needs a number",
                                  "Try: (zeros 8)");
            }
            if (ts.peek().kind != TokenKind::RParen) {
                return make_error("zeros accepts exactly one number",
                                  "Try: (zeros 8)");
            }
            int n = (int)n_tok.number;
            if (n < 1) n = 1;
            if (n > 64) n = 64;
            Sample values[64] = {};
            uint16_t table_id = engine.cells.store_data_table(values, (uint16_t)n);
            ts.expect(TokenKind::RParen);
            EvalResult r;
            r.kind = EvalResult::DataRef;
            r.number = (Sample)table_id;
            return r;
        }

        // get-expr — return stored source expression for a symbol
        if (op == sym.get_expr) {
            Token name_tok = ts.consume();
            if (ts.peek().kind != TokenKind::RParen) {
                return make_error("get-expr accepts exactly one name",
                                  "Try: (get-expr my-fn)");
            }
            ts.expect(TokenKind::RParen);
            if (name_tok.kind != TokenKind::Symbol) {
                return make_error("get-expr needs a name",
                                  "Try: (get-expr my-fn)");
            }
            SymbolID name_sym = name_tok.symbol;
            if (name_sym < MAX_CELLS &&
                engine.cells.cells[name_sym].kind == CellKind::Callable &&
                engine.cells.callables[name_sym].source_length > 0) {
                const char* src = engine.arena.read(
                    engine.cells.callables[name_sym].source_offset);
                if (src) {
                    EvalResult r;
                    r.kind = EvalResult::Text;
                    r.text = src;
                    r.text_length = (uint16_t)engine.cells.callables[name_sym].source_length;
                    return r;
                }
            }
            return make_error("No expression stored for this name",
                              "Define it first with (define name expr) or (defn name [args] body)");
        }

        // Output assignment
        if (GraphBuilder::is_output_symbol(op)) {
            EvalResult r = do_output_assign(op, ts, engine,
                                            source, source_length, shared_ids);
            ts.expect(TokenKind::RParen);
            return r;
        }

        // do / scope — evaluate children sequentially
        if (op == sym.do_ || op == sym.scope) {
            EvalResult last = make_ok();
            bool compact_between_forms =
                !source_aliases_live_arena(source, engine);
            while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) {
                last = eval_form(ts, engine, source, source_length, shared_ids);
                if (last.kind == EvalResult::Error) {
                    // A submission is a sequence of per-form transactions.
                    // Earlier successful children remain committed, but the
                    // first failure stops the sequence and later children are
                    // not evaluated.
                    while (ts.peek().kind != TokenKind::RParen &&
                           !ts.at_end()) {
                        GraphBuilder::skip_form(ts);
                    }
                    ts.expect(TokenKind::RParen);
                    return last;
                }
                // Each child is its own publication transaction. Compact at
                // that boundary so one explicit do/scope cannot strand source
                // proportional to its edit history. A Text result can point
                // into the arena (get-expr), so defer until a later non-Text
                // child or the next external eval.
                if (compact_between_forms && last.kind != EvalResult::Text)
                    compact_live_source_arena(engine);
            }
            ts.expect(TokenKind::RParen);
            return last;
        }

        // Unknown form at top level — try as signal expression.
        // Extract the full form source from the opening '(' through ')'.
        // Track paren/bracket depth so nested forms (e.g. (eval-at-time T (* 0.5 bar)))
        // don't terminate the slice at the first inner ')' and leave the outer
        // ')' dangling for the next eval_form iteration.
        {
            uint32_t form_start = tok.span_start; // position of '('
            int depth = 0;
            while (!ts.at_end()) {
                Token nxt = ts.peek();
                if (depth == 0 && nxt.kind == TokenKind::RParen) break;
                ts.consume();
                if (nxt.kind == TokenKind::LParen ||
                    nxt.kind == TokenKind::LBracket) {
                    depth++;
                } else if (nxt.kind == TokenKind::RParen ||
                           nxt.kind == TokenKind::RBracket) {
                    if (depth > 0) depth--;
                }
            }
            Token rparen = ts.consume(); // eat outer ')'
            uint32_t form_end = rparen.span_start + rparen.span_len;
            if (source && form_end > form_start && form_end <= source_length) {
                return eval_expression(
                    source + form_start, form_end - form_start, engine);
            }
            return make_ok();
        }
    }

    return make_error("Unexpected input", "Try: (define name value) or (a1 expression)");
}

#if USEQ_HAS_SYNTH_ENGINE
// ── Host synth GC integration ───────────────────────────────────────────────
//
// Synth control channel expressions compile to real nodes in the live
// NodePool (synth-nodes.md §7.2). The pool's GC pass must keep those roots
// reachable and remap their indices, otherwise forced GC after a synth
// eval drops the control expressions and the host reads freed memory
// (VAL-COMP-011). We register the synth control roots with the pool's
// external-roots array; the GC walks and remaps them exactly like output
// roots. The synth_graph owns the authoritative indices — after GC, the
// pool updates them in place via external_roots[].

void register_synth_external_roots(SignalEngine& engine) {
    engine.pool.clear_external_roots();
    for (uint16_t i = 0; i < engine.synth_graph.control_count(); i++) {
        uint16_t root = engine.synth_graph.controls[i].root_node;
        if (root != NODE_NONE) {
            engine.pool.register_external_root(root);
        }
    }
}

void commit_synth_external_roots(SignalEngine& engine) {
    uint16_t e = 0;
    for (uint16_t i = 0; i < engine.synth_graph.control_count(); i++) {
        uint16_t root = engine.synth_graph.controls[i].root_node;
        if (root == NODE_NONE) continue;
        if (e < engine.pool.external_root_count) {
            engine.synth_graph.controls[i].root_node =
                engine.pool.external_roots[e++];
        }
    }
}
#endif

// ── eval_cold entry point (SignalEngine version) ───────────────────────────

EvalResult eval_cold(const char* source, uint32_t length, SignalEngine& engine) {
    RuntimeMemorySampleScope memory_sample_scope;
    ParsedProgram program;
    program.parse(source, length);

    if (!program.ok()) {
        EvalResult r;
        r.kind = EvalResult::Error;
        memcpy(r.diagnostics, program.diagnostics,
               program.diagnostic_count * sizeof(Diagnostic));
        r.diagnostic_count = program.diagnostic_count;
        return r;
    }
    TokenStream& ts = program.stream;

    // Cross-output live-edit ID tracking — cleared per eval batch
    SharedLiveEditIDs shared_ids;

#if USEQ_HAS_SYNTH_ENGINE
    // Reset the per-eval anonymous synth ordinal and defensively clear any
    // stale pending wrapper identity (state-identity.md §2.2/§2.5). Both
    // are eval-scoped: the ordinal keys the anonymous fallback identity,
    // and the pending id only lives inside a with-state-id wrapper.
    engine.eval_anon_synth_ordinal = 0;
    engine.has_pending_state_identity = false;
#endif

    // Any cold eval may mutate cell values — bump the store revision so
    // per-tick snapshot consumers know to refresh (A12). Coarse but sound.
    engine.cells.store_revision++;

    // Handle multiple forms (implicit do)
    EvalResult last = make_ok();
    bool compact_between_forms = !source_aliases_live_arena(source, engine);
    while (!ts.at_end() && ts.peek().kind != TokenKind::Eof) {
        last = eval_form(ts, engine, source, length, &shared_ids);
        if (last.kind == EvalResult::Error) {
            // A submission is a sequence of per-form transactions: retain
            // earlier committed forms, stop at the first rejected one.
            return last;
        }
        // All fallible work for this per-form transaction is complete. Keep
        // arena use proportional to published owners rather than replacement
        // history. Do not relocate an arena-backed input buffer or a Text
        // result whose pointer is itself the requested stored source.
        if (compact_between_forms && last.kind != EvalResult::Text)
            compact_live_source_arena(engine);
    }

    return last;
}

// ── Bulk recompilation ─────────────────────────────────────────────────────
// Rebuilds every output graph from stored source text.  Mirrors the
// per-output recompilation in on_cell_changed() but operates on ALL outputs
// unconditionally — used after flash load when no graphs exist yet.

void recompile_all_outputs(SignalEngine& engine) {
    for (uint16_t i = 0; i < MAX_OUTPUTS; i++) {
        if (!engine.output_sources[i].has_source) continue;

        const char* src = engine.arena.read(
            engine.output_sources[i].arena_offset);
        if (!src) continue;

        ParsedProgram program;
        program.parse(src, engine.output_sources[i].arena_length);

        if (!program.ok()) {
            // Stored source can be corrupt (for example after loading an old
            // flash image), but recompilation is still a publication
            // transaction.  Keep any already-live graph intact.
            continue;
        }

        GraphMutationTransaction graph_transaction(engine);
        engine.registry.begin_context(i);
        GraphBuildResult result = build_output_graph(
            engine.pool, program.stream, engine.cells, engine.arena, src,
            &engine.registry, nullptr, i);

        if (!result.has_error) {
            publish_output_graph_plan(engine, i, result);
            graph_transaction.accept();
        }
    }

    // Reclaim stale nodes from any previous compilation (idempotent due to CSE)
    reclaim_unowned_resources(engine);
    engine.pool.rebuild_execution_order();
    classify_outputs(engine.pool);
}

// ── Dependency tracking (SignalEngine version) ─────────────────────────────

void on_cell_changed(SymbolID cell_id, SignalEngine& engine) {
    for (uint16_t i = 0; i < MAX_OUTPUTS; i++) {
        if (engine.pool.outputs[i].root_node == NODE_NONE) continue;
        if (!engine.pool.output_deps[i].contains(cell_id)) continue;

        // This output needs recompilation
        if (engine.output_sources[i].has_source) {
            const char* src = engine.arena.read(
                engine.output_sources[i].arena_offset);
            if (src) {
                ParsedProgram program;
                program.parse(src, engine.output_sources[i].arena_length);

                if (program.ok()) {
                    GraphMutationTransaction graph_transaction(engine);
                    engine.registry.begin_context(i);
                    GraphBuildResult result = build_output_graph(
                        engine.pool, program.stream, engine.cells,
                        engine.arena, src,
                        &engine.registry, nullptr, i);
                    if (!result.has_error) {
                        publish_output_graph_plan(engine, i, result);
                        graph_transaction.accept();
                    } else {
                        // A reactive compile is a candidate publication just
                        // like a direct output assignment.  Retain the old
                        // root, validity, dependencies, state values, and
                        // capacity when the candidate is rejected.
                        publish_reactive_diagnostic(
                            engine.output_compile_diagnostics[i], cell_id,
                            result);
                    }
                } else {
                    ActiveCompileDiagnostic& active =
                        engine.output_compile_diagnostics[i];
                    active = ActiveCompileDiagnostic{};
                    active.active = true;
                    active.triggered_by = cell_id;
                    active.diagnostic = {
                        DiagnosticSeverity::Error,
                        DiagnosticCategory::Syntax, 0, 0,
                        "Stored output source could not be parsed; the previous program is still running",
                        "Reassign this output with valid source"
                    };
                }
            }
        }
    }

    // Recompile state update graphs that depend on the changed cell
    for (uint16_t s = 0; s < engine.pool.state_slot_count; s++) {
        if (!engine.state_sources[s].has_source) continue;

        bool depends = false;
        for (uint8_t d = 0; d < engine.state_sources[s].dep_count; d++) {
            if (engine.state_sources[s].dep_cells[d] == cell_id) {
                depends = true;
                break;
            }
        }
        if (!depends) continue;

        const char* src = engine.arena.read(engine.state_sources[s].arena_offset);
        if (!src) continue;

        ParsedProgram program;
        program.parse(src, engine.state_sources[s].arena_length);

        if (program.ok()) {
            GraphMutationTransaction graph_transaction(engine);
            uint16_t owner_context =
                (uint16_t)(MAX_OUTPUTS + s);
            engine.registry.begin_context(owner_context);
            GraphBuildResult result = build_output_graph(
                engine.pool, program.stream, engine.cells, engine.arena, src,
                &engine.registry, nullptr, owner_context);
            if (!result.has_error) {
                publish_state_update_plan(engine, s, result);
                graph_transaction.accept();
            } else {
                publish_reactive_diagnostic(
                    engine.state_compile_diagnostics[s], cell_id, result);
            }
        } else {
            ActiveCompileDiagnostic& active =
                engine.state_compile_diagnostics[s];
            active = ActiveCompileDiagnostic{};
            active.active = true;
            active.triggered_by = cell_id;
            active.diagnostic = {
                DiagnosticSeverity::Error, DiagnosticCategory::Syntax,
                0, 0,
                "Stored state update source could not be parsed; the previous update is still running",
                "Redefine this state with a valid update expression"
            };
        }
    }

#if USEQ_HAS_SYNTH_ENGINE
    // Synth controls are persistent programs too. Recompile only channels
    // whose recorded cell dependency changed, preserving the prior root and
    // its state/resources when the candidate cannot be built.
    for (uint16_t i = 0; i < engine.synth_graph.control_count(); i++) {
        SynthControlChannel& control = engine.synth_graph.controls[i];
        bool depends = false;
        for (uint8_t d = 0; d < control.dep_count; d++) {
            if (control.dep_cells[d] == cell_id) {
                depends = true;
                break;
            }
        }
        if (!depends || control.source_length == 0) continue;

        const char* src = engine.arena.read(control.source_offset);
        if (!src) {
            const Diagnostic diagnostic = {
                DiagnosticSeverity::Error, DiagnosticCategory::Runtime,
                0, 0,
                "Stored synth-control source is unavailable; the previous control is still running",
                "Replace the synth declaration with valid control source"
            };
            publish_synth_reactive_diagnostic(control, cell_id, diagnostic);
            continue;
        }
        ParsedProgram program;
        program.parse(src, control.source_length);
        if (!program.ok()) {
            publish_synth_reactive_diagnostic(
                control, cell_id, program.diagnostics[0]);
            continue;
        }

        GraphMutationTransaction graph_transaction(engine);
        engine.registry.begin_context(control.owner_context);
        GraphBuildResult result = build_output_graph(
            engine.pool, program.stream, engine.cells, engine.arena, src,
            &engine.registry, nullptr, control.owner_context);
        if (result.has_error) {
            if (result.diagnostic_count > 0) {
                publish_synth_reactive_diagnostic(
                    control, cell_id, result.diagnostics[0]);
            } else {
                const Diagnostic diagnostic = {
                    DiagnosticSeverity::Error,
                    DiagnosticCategory::Runtime, 0, 0,
                    "A synth-control dependency change could not be applied; the previous control is still running",
                    "Repair the changed definition"
                };
                publish_synth_reactive_diagnostic(
                    control, cell_id, diagnostic);
            }
            continue;
        }
        control.root_node = result.root_node;
        control.dep_count = result.dep_count;
        for (uint8_t d = 0; d < result.dep_count; d++)
            control.dep_cells[d] = result.dep_cells[d];
        engine.registry.commit_context(
            control.owner_context, engine.pool.state_update_roots,
            engine.pool.state_owner_context);
        clear_synth_reactive_diagnostic(control);
        graph_transaction.accept();
    }
#endif

    // Reclaim nodes orphaned by the recompiles above (F4). Every sibling
    // recompile path (eval_output, recompile_all_outputs, do_output_assign)
    // gc's before rebuilding; without this, live cell edits leak the old
    // graphs until the fixed node pool (360 nodes on firmware) fills up and
    // compilation silently fails.
    reclaim_unowned_resources(engine);
    engine.pool.rebuild_execution_order();

    // Recompilation may have changed which load ops each output references
    // (e.g. an output that used to be Pure now reads an input, or a feedback /
    // state dependency appeared/disappeared). Refresh the classification so
    // output_class / output_input_mask don't go stale after a cell change.
    classify_outputs(engine.pool);
}

} // namespace sig
