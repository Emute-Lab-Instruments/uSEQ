#include "cold_eval.h"
#include "token.h"
#include "graph_builder.h"
#include "executor.h"
#include "synth_registry.h"
#include "../modulisp/lisp/symbol_intern.h"
#include <cstdio>
#include <cstring>

namespace sig {

// ── SignalEngine::init_defaults ────────────────────────────────────────────

void SignalEngine::init_defaults(double bpm, int beats_per_bar,
                                 int bars_per_phrase, int phrases_per_section) {
    GraphBuilder::init_symbols();
    cells.init_timing_defaults(bpm, beats_per_bar, bars_per_phrase,
                               phrases_per_section);
    state = EngineState{};
}

// ── Helper constructors ─────────────────────────────────────────────────────

static EvalResult make_ok() {
    EvalResult r;
    r.kind = EvalResult::Ok;
    return r;
}

static EvalResult make_number(double v) {
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
        engine.cells.cells[sym].kind = CellKind::Number;
        engine.cells.cells[sym].revision++;
        engine.cells.cells[sym].value = val_tok.number;
        // A plain define establishes a fresh, non-state binding. Clear any stale
        // defstate marker (flags 0x02) — otherwise graph_builder keeps emitting a
        // state_load from the old slot and this define is silently ignored.
        engine.cells.cells[sym].flags = 0;
    }
    else if (val_tok.kind == TokenKind::LBracket) {
        // Vector data: [1 2 3 4]
        ts.consume(); // eat '['
        double values[64];
        uint16_t count = 0;
        while (ts.peek().kind != TokenKind::RBracket && !ts.at_end() && count < 64) {
            Token elem = ts.consume();
            if (elem.kind == TokenKind::Number) {
                values[count++] = elem.number;
            }
        }
        ts.expect(TokenKind::RBracket);

        uint16_t table_id = engine.cells.store_data_table(values, count);
        engine.cells.cells[sym].kind = CellKind::Data;
        engine.cells.cells[sym].data_table_id = table_id;
        engine.cells.cells[sym].revision++;
        engine.cells.cells[sym].value = (double)count;
        engine.cells.cells[sym].flags = 0; // clear stale defstate marker (see above)
    }
    else {
        // Expression — store source text as callable with 0 params
        uint16_t expr_start = ts.pos;
        uint32_t byte_start = span_begin(ts, expr_start);

        // Skip past the expression to find its extent
        GraphBuilder::skip_form(ts);
        uint32_t byte_end = span_end_of(ts, ts.pos);

        // Copy the expression source text into the arena FIRST. If the arena
        // is full, fail the define without touching the cell — otherwise the
        // cell would keep its OLD source text and dependents would silently
        // recompile a stale definition (F5).
        uint32_t offset = UINT32_MAX;
        uint32_t len = 0;
        if (source && byte_end > byte_start && byte_end <= source_length) {
            len = byte_end - byte_start;
            const CallableInfo& previous = engine.cells.callables[sym];
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
        if (param.kind == TokenKind::Symbol && info.param_count < MAX_CALLABLE_PARAMS) {
            info.params[info.param_count++] = param.symbol;
        }
    }
    ts.expect(TokenKind::RBracket);

    // Store body source — skip body and record extent
    uint16_t body_start = ts.pos;
    uint32_t byte_start = span_begin(ts, body_start);

    GraphBuilder::skip_form(ts);
    uint32_t byte_end = span_end_of(ts, ts.pos);

    // Copy the body source text into the arena FIRST. If the arena is full,
    // fail the defn without touching the cell so dependents never recompile
    // a half-updated definition (F5).
    if (source && byte_end > byte_start && byte_end <= source_length) {
        uint32_t len = byte_end - byte_start;
        const CallableInfo& previous = engine.cells.callables[sym];
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

    auto store_number = [&](double v) {
        if (is_state_cell) {
            uint16_t slot = (uint16_t)engine.cells.cells[sym].data_table_id;
            engine.pool.state_values[slot] = v;
            engine.cells.cells[sym].value = v;
            engine.cells.cells[sym].revision++;
        } else {
            engine.cells.cells[sym].kind = CellKind::Number;
            engine.cells.cells[sym].revision++;
            engine.cells.cells[sym].value = v;
        }
    };

    Token val_tok = ts.peek();
    if (val_tok.kind == TokenKind::Number) {
        ts.consume();
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

        if (engine.scratch_pool.nodes[gr.root_node].op == NodeOp::Const) {
            store_number(engine.scratch_pool.nodes[gr.root_node].imm);
        } else {
            engine.scratch_pool.outputs[0].root_node = gr.root_node;
            engine.scratch_pool.outputs[0].valid = true;
            engine.scratch_pool.rebuild_execution_order();

            double cell_vals[MAX_CELLS];
            engine.cells.snapshot_values(cell_vals, MAX_CELLS);
            double hw_inputs[32] = {};
            double workspace[MAX_TOTAL_NODES] = {};
            double outputs[MAX_OUTPUTS] = {};

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
    double init_value = init_tok.number;

    // Resolve an existing state slot before storing its update source so a
    // re-definition can reuse that slot's arena region.
    uint16_t state_slot = NODE_NONE;
    if (engine.cells.cells[sym].kind == CellKind::Number
        && engine.cells.cells[sym].flags == 0x02) {
        // Existing state cell — reuse its slot, do NOT reset the value.
        state_slot = (uint16_t)engine.cells.cells[sym].data_table_id;
    }

    // Record the update expression source text FIRST. If the arena is full,
    // fail the defstate before mutating anything — otherwise the state cell
    // would keep its OLD update source and dependency changes would silently
    // recompile a stale update program (F5).
    uint16_t expr_start = ts.pos;
    uint32_t byte_start = span_begin(ts, expr_start);
    uint32_t src_offset = UINT32_MAX;
    uint32_t src_len = 0;
    {
        uint16_t saved = ts.pos;
        GraphBuilder::skip_form(ts);
        uint32_t byte_end = span_end_of(ts, ts.pos);
        ts.rewind(saved);

        if (source && byte_end > byte_start && byte_end <= source_length) {
            src_len = byte_end - byte_start;
            uint32_t previous_offset = UINT32_MAX;
            uint32_t previous_length = 0;
            if (state_slot != NODE_NONE &&
                engine.state_sources[state_slot].has_source) {
                previous_offset = engine.state_sources[state_slot].arena_offset;
                previous_length = engine.state_sources[state_slot].arena_length;
            }
            src_offset = engine.arena.store_reuse(
                previous_offset, previous_length,
                source + byte_start, src_len);
            if (src_offset == UINT32_MAX) {
                return make_error(
                    "Program storage is full — defstate not applied",
                    "Free space with (useq-clear) or shorten your program");
            }
        }
    }

    // Snapshot everything we mutate before the update expression compiles so
    // a compile failure can roll back cleanly (A6) — otherwise a failed
    // defstate leaves the name rebound to a half-initialised state cell,
    // corrupting whatever the previous binding was.
    Cell saved_cell = engine.cells.cells[sym];
    uint16_t saved_slot_count = engine.pool.state_slot_count;

    // Allocate a state slot (if this name already has a state slot, reuse it)
    bool allocated_new_slot = false;
    if (state_slot == NODE_NONE) {
        // New state cell — allocate slot and set initial value
        if (engine.pool.state_slot_count >= MAX_STATE_SLOTS) {
            return make_error(state_slots_exhausted_msg(),
                              "Remove unused defstate declarations");
        }
        state_slot = engine.pool.state_slot_count++;
        engine.pool.state_values[state_slot] = init_value;
        allocated_new_slot = true;
    }
    StateUpdateSource saved_source = engine.state_sources[state_slot];
    uint16_t saved_update_root = engine.pool.state_update_roots[state_slot];

    // Mark this cell as a state cell: kind=Number (readable), flags=0x02 (state marker),
    // data_table_id stores the state slot index
    engine.cells.cells[sym].kind = CellKind::Number;
    engine.cells.cells[sym].flags = 0x02;  // state cell marker
    engine.cells.cells[sym].data_table_id = state_slot;
    engine.cells.cells[sym].revision++;
    engine.cells.cells[sym].value = init_value;

    if (src_offset != UINT32_MAX) {
        engine.state_sources[state_slot].arena_offset = src_offset;
        engine.state_sources[state_slot].arena_length = src_len;
        engine.state_sources[state_slot].has_source = true;
    }

    // Compile the update expression as a signal graph
    uint8_t saved_tables = engine.cells.data_table_count;
    GraphBuildResult result = build_output_graph(engine.pool, ts,
                                                 engine.cells, engine.arena, source,
                                                 &engine.registry, nullptr,
                                                 (uint16_t)(MAX_OUTPUTS + state_slot));

    if (result.has_error) {
        // Roll back everything mutated before the compile (A6): the cell
        // (kind/flags/value/slot ref), the update root/source, and — if we
        // allocated a fresh slot — the allocation itself.
        engine.cells.data_table_count = saved_tables;
        engine.cells.cells[sym] = saved_cell;
        engine.cells.cells[sym].revision++;
        engine.pool.state_update_roots[state_slot] = saved_update_root;
        engine.state_sources[state_slot] = saved_source;
        if (allocated_new_slot) {
            engine.pool.state_values[state_slot] = 0.0;
            engine.pool.state_update_roots[state_slot] = sig::NODE_NONE;
            engine.state_sources[state_slot] = StateUpdateSource{};
            engine.pool.state_slot_count = saved_slot_count;
        }
        return make_error("defstate update expression failed to compile",
                          "Check the update expression");
    }

    // Store the update root and dependencies
    engine.pool.state_update_roots[state_slot] = result.root_node;
    engine.state_sources[state_slot].dep_count = result.dep_count;
    for (uint8_t d = 0; d < result.dep_count; d++) {
        engine.state_sources[state_slot].dep_cells[d] = result.dep_cells[d];
    }

    // Reclaim nodes orphaned by the recompile (e.g. the previous update
    // graph of this state slot), then rebuild execution order to include
    // state update subgraphs (F4).
    register_synth_external_roots(engine);
    engine.pool.gc_unreachable_nodes();
    commit_synth_external_roots(engine);
    engine.pool.rebuild_execution_order();
    classify_outputs(engine.pool);

    // Notify dependents so outputs referencing this cell get recompiled
    on_cell_changed(sym, engine);

    return make_ok();
}

// ── Transport / time management ─────────────────────────────────────────────

static EvalResult do_set_bpm(TokenStream& ts, SignalEngine& engine) {
    Token val = ts.consume();
    if (val.kind != TokenKind::Number) {
        return make_error("set-bpm needs a number", "Try: (set-bpm 120)");
    }
    auto& si = SymbolIntern::getInstance();
    SymbolID bpm_sym = si.intern("bpm");
    engine.cells.cells[bpm_sym].kind = CellKind::Number;
    engine.cells.cells[bpm_sym].value = val.number;
    engine.cells.cells[bpm_sym].revision++;
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

    auto& si = SymbolIntern::getInstance();
    SymbolID bpb_sym = si.intern("beats-per-bar");
    engine.cells.cells[bpb_sym].kind = CellKind::Number;
    engine.cells.cells[bpb_sym].value = beats_tok.number;
    engine.cells.cells[bpb_sym].revision++;
    on_cell_changed(bpb_sym, engine);

    (void)subdivision_tok;

    return make_ok();
}

static EvalResult do_useq_clear(SignalEngine& engine) {
    for (uint16_t i = 0; i < MAX_OUTPUTS; i++) {
        engine.pool.outputs[i].root_node = NODE_NONE;
        engine.pool.outputs[i].valid = false;
        engine.pool.outputs[i].lkg_value = (i < 8) ? 0.5 : 0.0;
        engine.output_sources[i].has_source = false;
    }
    engine.pool.exec_count = 0;

    // Clear state resources fully (A4, state-identity.md §4.5). Resetting
    // only state_slot_count left defstate cell markers (flags 0x02 +
    // data_table_id), state values/sources/update roots behind — so a
    // re-defstate of the same name "reused" a slot that no longer existed
    // and read frozen values.
    for (uint16_t s = 0; s < MAX_STATE_SLOTS; s++) {
        engine.pool.state_values[s] = 0.0;
        engine.pool.state_update_roots[s] = NODE_NONE;
        engine.state_sources[s] = StateUpdateSource{};
    }
    engine.pool.state_slot_count = 0;
    for (uint32_t c = 0; c < MAX_CELLS; c++) {
        if (engine.cells.cells[c].flags == 0x02) {
            engine.cells.cells[c].flags = 0;
            engine.cells.cells[c].data_table_id = 0;
            engine.cells.cells[c].revision++;
        }
    }
    engine.registry.clear();
    // Clear synth patch graph and control table (synth-nodes.md §5.6c).
    // Clearing advances the shared revision so consumers see the new
    // empty state.
    engine.synth_graph.clear_and_advance();
    return make_ok();
}

static EvalResult do_set_time_offset(TokenStream& ts, EngineState& state) {
    Token val = ts.consume();
    if (val.kind != TokenKind::Number) {
        return make_error("useq-set-time-offset needs a number in seconds",
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
    state.time_offset += val.number;
    return make_ok();
}

// ── Synth declaration (synth-nodes.md §3) ──────────────────────────────────
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
    EvalResult& out_error)
{
    out_error = EvalResult{};

    if (name_tok.kind != TokenKind::String) {
        out_error = make_synth_error_at(
            name_tok, DiagnosticCategory::Type,
            "synth needs a NodeDef name in quotes",
            "Try: (synth \"osc/sine\" :freq 440)");
        return nullptr;
    }

    char name_buf[MAX_NODEDEF_NAME];
    uint16_t name_len = name_tok.string.length;
    if (name_len >= MAX_NODEDEF_NAME) name_len = MAX_NODEDEF_NAME - 1;
    std::memcpy(name_buf, source_base + name_tok.string.offset, name_len);
    name_buf[name_len] = '\0';

    uint16_t version = 0;
    if (version_tok && version_tok->kind == TokenKind::Number) {
        version = (uint16_t)version_tok->number;
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

static EvalResult do_synth(TokenStream& ts, SignalEngine& engine,
                           const char* source, uint32_t source_length) {
    GraphBuilder::init_symbols();
    auto& sym = GraphBuilder::sym;

    // ── Parse def name (required string) ────────────────────────────────
    Token def_name_tok = ts.consume();
    if (def_name_tok.kind != TokenKind::String) {
        return make_synth_error_at(
            def_name_tok, DiagnosticCategory::Type,
            "synth needs a NodeDef name in quotes",
            "Try: (synth \"osc/sine\" :freq 440)");
    }

    // ── Optional keywords before param pairs: :version / :name / :id ───
    uint16_t requested_version = 0;
    Token version_tok;
    version_tok.kind = TokenKind::Eof;
    bool have_explicit_version = false;

    Token identity_tok;
    identity_tok.kind = TokenKind::Eof;
    bool have_explicit_identity = false;

    // Track param bindings declared in this form. Required: :freq.
    struct ParamBinding {
        const NodeDefParam* desc;
        Token kw_tok;
        uint16_t expr_start_pos;
        uint16_t expr_end_pos;
        uint32_t expr_byte_start;
        uint32_t expr_byte_end;
        bool present;
    };
    ParamBinding bindings[MAX_NODEDEF_PARAMS] = {};
    uint16_t binding_count = 0;

    // Track keywords seen (for duplicate detection).
    SymbolID seen_kws[MAX_NODEDEF_PARAMS] = {};
    uint16_t seen_kw_count = 0;

    bool kw_phase = true;
    while (kw_phase && ts.peek().kind == TokenKind::Symbol) {
        Token kw_tok = ts.peek();
        if (kw_tok.symbol == sym.kw_version) {
            ts.consume();
            Token v = ts.consume();
            if (v.kind != TokenKind::Number) {
                return make_synth_error_at(
                    v, DiagnosticCategory::Type,
                    ":version needs a whole number",
                    "Try: (synth \"osc/sine\" :version 1 :freq 440)");
            }
            requested_version = (uint16_t)v.number;
            version_tok = v;
            have_explicit_version = true;
            continue;
        }
        if (kw_tok.symbol == sym.kw_name || kw_tok.symbol == sym.kw_id) {
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
        source, resolve_err);
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
        for (uint16_t i = 0; i < seen_kw_count; i++) {
            if (seen_kws[i] == kw_tok.symbol) {
                char msg[96];
                std::snprintf(msg, sizeof(msg),
                              "Parameter %s was already set in this synth form",
                              param_buf);
                return make_synth_error_at(
                    kw_tok, DiagnosticCategory::Arity,
                    strdup_safe(msg),
                    "Remove the duplicate binding");
            }
        }
        if (seen_kw_count < MAX_NODEDEF_PARAMS) {
            seen_kws[seen_kw_count++] = kw_tok.symbol;
        }

        // Validate the parameter is declared by this NodeDef.
        const NodeDefParam* pdesc = nodedef_find_param(def, param_name);
        if (!pdesc) {
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

        if (binding_count >= MAX_NODEDEF_PARAMS) {
            return make_synth_error_at(
                kw_tok, DiagnosticCategory::Overflow,
                "Too many parameters on this synth form",
                "Check the NodeDef documentation");
        }
        bindings[binding_count].desc = pdesc;
        bindings[binding_count].kw_tok = kw_tok;
        bindings[binding_count].expr_start_pos = expr_start_pos;
        bindings[binding_count].expr_end_pos = ts.pos;
        bindings[binding_count].expr_byte_start = byte_start;
        bindings[binding_count].expr_byte_end = byte_end;
        bindings[binding_count].present = true;
        binding_count++;
    }

    // ── Required :freq (VAL-COMP-006) ──────────────────────────────────
    bool have_freq = false;
    for (uint16_t i = 0; i < binding_count; i++) {
        if (std::strcmp(bindings[i].desc->name, "freq") == 0) {
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
    // Order of authority: explicit :name > hidden :id > anonymous fallback.
    // The anonymous fallback exists for direct eval_cold testing without
    // the editor payload builder; in production, the payload builder
    // always injects a hidden :id (state-identity.md / VAL-COMP-004).
    char identity_buf[MAX_SYNTH_IDENTITY];
    if (have_explicit_identity) {
        uint16_t n = identity_tok.string.length;
        if (n >= MAX_SYNTH_IDENTITY) n = MAX_SYNTH_IDENTITY - 1;
        std::memcpy(identity_buf, source + identity_tok.string.offset, n);
        identity_buf[n] = '\0';
    } else {
        // Anonymous fallback. The synthetic identity is "::anon-<rev>"
        // so it is unambiguous and distinct from explicit user names.
        std::snprintf(identity_buf, sizeof(identity_buf),
                      "::anon-%lu", (unsigned long)engine.synth_graph.revision);
    }

    // ── Capacity check (VAL-COMP-019) ──────────────────────────────────
    // M1 hosts one synth instance. An eval that would introduce a NEW
    // distinct identity while another declaration is already active must
    // fail transactionally.
    SynthDeclaration* existing = engine.synth_graph.find(identity_buf);
    if (existing == nullptr &&
        engine.synth_graph.declaration_count() >= SYNTH_M1_MAX_NODES) {
        char msg[160];
        std::snprintf(msg, sizeof(msg),
                      "Only one synth can play at a time in M1; "
                      "another identity (\"%s\") is already active",
                      engine.synth_graph.declarations[0].identity);
        return make_synth_error_at(
            def_name_tok, DiagnosticCategory::Overflow,
            strdup_safe(msg),
            "Use (useq-clear) to free the active synth, or re-declare "
            "the same identity to update in place");
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
        return make_synth_error_at(
            def_name_tok, DiagnosticCategory::Overflow,
            "Synth declaration table is full",
            "Use (useq-clear) to free earlier synths");
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

    // ── Compile each bound param expression into the live NodePool ─────
    // The compiled root nodes are real graph nodes that participate in GC
    // and execution (VAL-COMP-011, VAL-COMP-018). We compile into the
    // engine's live pool, not the scratch pool, because synth control
    // roots persist across evals.
    for (uint16_t i = 0; i < binding_count; i++) {
        ParamBinding& b = bindings[i];
        if (!b.present) continue;

        uint32_t expr_len = b.expr_byte_end - b.expr_byte_start;
        if (expr_len == 0 || expr_len > source_length) continue;

        // Tokenise the expression slice.
        Token expr_tokens[MAX_TOKENS];
        Diagnostic expr_parse_errors[8];
        uint8_t expr_parse_err_count = 0;
        uint16_t expr_count = TokenStream::tokenize(
            source + b.expr_byte_start, expr_len,
            expr_tokens, MAX_TOKENS,
            expr_parse_errors, &expr_parse_err_count);
        if (expr_parse_err_count > 0) {
            EvalResult r;
            r.kind = EvalResult::Error;
            for (uint8_t e = 0; e < expr_parse_err_count && r.diagnostic_count < 8; e++) {
                // Remap expression-relative spans back to eval-relative.
                Diagnostic d = expr_parse_errors[e];
                d.span_start = (uint16_t)(d.span_start + b.expr_byte_start);
                r.diagnostics[r.diagnostic_count++] = d;
            }
            return r;
        }

        TokenStream ets;
        std::memcpy(ets.tokens, expr_tokens, expr_count * sizeof(Token));
        ets.count = expr_count;
        ets.pos = 0;

        // Save data-table state so a compile failure does not pollute the
        // cell store (mirrors eval_expression's pattern).
        uint8_t saved_tables = engine.cells.data_table_count;

        GraphBuildResult gbr = build_output_graph(
            engine.pool, ets, engine.cells, engine.arena,
            source, &engine.registry, nullptr,
            (uint16_t)(MAX_OUTPUTS + MAX_STATE_SLOTS + i));

        if (gbr.has_error) {
            engine.cells.data_table_count = saved_tables;
            EvalResult r;
            r.kind = EvalResult::Error;
            for (uint8_t e = 0; e < gbr.diagnostic_count && r.diagnostic_count < 8; e++) {
                Diagnostic d = gbr.diagnostics[e];
                // Remap expression-relative spans back to eval-relative.
                d.span_start = (uint16_t)(d.span_start + b.expr_byte_start);
                r.diagnostics[r.diagnostic_count++] = d;
            }
            return r;
        }

        // Append a control channel row.
        SynthControlChannel* ctl = engine.synth_graph.append_control();
        if (!ctl) {
            engine.cells.data_table_count = saved_tables;
            return make_synth_error_at(
                b.kw_tok, DiagnosticCategory::Overflow,
                "Synth control table is full",
                "Use (useq-clear) to free earlier synths");
        }
        std::strncpy(ctl->identity, identity_buf, MAX_SYNTH_IDENTITY - 1);
        ctl->identity[MAX_SYNTH_IDENTITY - 1] = '\0';
        std::strncpy(ctl->param_name, b.desc->name, MAX_NODEDEF_NAME - 1);
        ctl->param_name[MAX_NODEDEF_NAME - 1] = '\0';
        ctl->rate_class     = b.desc->rate_class;
        ctl->smoothing_class = b.desc->smoothing_class;
        ctl->root_node      = gbr.root_node;

        decl->control_count++;
    }

    // Graph + control table share one revision (VAL-COMP-009). The revision
    // is advanced by eval_cold at successful commit, not here, so that a
    // later-failing form in the same eval unit rolls back to the previous
    // revision (VAL-COMP-008/010).
    return make_ok();
}

// ── Output assignment ───────────────────────────────────────────────────────

static EvalResult do_output_assign(SymbolID output_sym, TokenStream& ts,
                                    SignalEngine& engine,
                                    const char* source, uint32_t source_length,
                                    SharedLiveEditIDs* shared_ids = nullptr) {
    uint16_t output_index = GraphBuilder::resolve_output_index(output_sym);
    if (output_index == NODE_NONE) {
        return make_error("Unknown output", "Try: (a1 expression)");
    }

    // Record the expression source text for recompilation. If the arena is
    // full, FAIL the assignment outright: installing the new graph while
    // output_sources[i] still points at the OLD text would make the next
    // dependency change silently recompile — and revert to — the stale
    // program (F5). The previous program keeps playing.
    uint16_t expr_start_pos = ts.pos;
    uint32_t byte_start = span_begin(ts, expr_start_pos);
    {
        uint16_t saved = ts.pos;
        GraphBuilder::skip_form(ts);
        uint32_t byte_end = span_end_of(ts, ts.pos);
        ts.rewind(saved);

        if (source && byte_end > byte_start && byte_end <= source_length) {
            uint32_t len = byte_end - byte_start;
            const OutputSource& previous = engine.output_sources[output_index];
            uint32_t offset = engine.arena.store_reuse(
                previous.has_source ? previous.arena_offset : UINT32_MAX,
                previous.has_source ? previous.arena_length : 0,
                source + byte_start, len);
            if (offset == UINT32_MAX) {
                return make_error(
                    "Program storage is full — output not changed",
                    "Free space with (useq-clear) or shorten your program");
            }
            engine.output_sources[output_index].arena_offset = offset;
            engine.output_sources[output_index].arena_length = len;
            engine.output_sources[output_index].has_source = true;
        }
    }

    // Build the signal graph
    uint8_t saved_tables = engine.cells.data_table_count;
    GraphBuildResult result = build_output_graph(engine.pool, ts,
                                                 engine.cells, engine.arena, source,
                                                 &engine.registry, shared_ids,
                                                 output_index);

    if (result.has_error) {
        engine.cells.data_table_count = saved_tables;
        // Per failure-model.md §2.6, a compile-time error leaves the active
        // program unchanged: do NOT demote `valid`. Demoting here was also the
        // root cause of A2 — sig::commit_outputs resurrects valid=true for any
        // output with a root node, so the flag flapped and the WASM batch-vis
        // row packing drifted mid-batch.
        EvalResult r;
        r.kind = EvalResult::Error;
        memcpy(r.diagnostics, result.diagnostics,
               result.diagnostic_count * sizeof(Diagnostic));
        r.diagnostic_count = result.diagnostic_count;
        return r;
    }

    // Install the new graph root
    engine.pool.outputs[output_index].root_node = result.root_node;
    engine.pool.outputs[output_index].valid = true;

    // Store cell dependencies for this output
    engine.pool.output_deps[output_index].clear();
    for (uint8_t d = 0; d < result.dep_count; d++) {
        engine.pool.output_deps[output_index].add(result.dep_cells[d]);
    }

    // Reclaim nodes no longer reachable from any output root
    register_synth_external_roots(engine);
    engine.pool.gc_unreachable_nodes();
    commit_synth_external_roots(engine);

    // Re-sort execution order
    engine.pool.rebuild_execution_order();
    classify_outputs(engine.pool);

    return make_ok();
}

// ── Scratch-isolated expression evaluation ─────────────────────────────────

EvalResult eval_expression(const char* source, uint32_t length,
                           SignalEngine& engine) {
    Token tokens[MAX_TOKENS];
    Diagnostic parse_errors[8];
    uint8_t parse_error_count = 0;

    uint16_t count = TokenStream::tokenize(source, length, tokens, MAX_TOKENS,
                                            parse_errors, &parse_error_count);
    if (parse_error_count > 0) {
        EvalResult r;
        r.kind = EvalResult::Error;
        memcpy(r.diagnostics, parse_errors,
               parse_error_count * sizeof(Diagnostic));
        r.diagnostic_count = parse_error_count;
        return r;
    }
    if (count == 0) return make_ok();

    TokenStream ts;
    memcpy(ts.tokens, tokens, count * sizeof(Token));
    ts.count = count;
    ts.pos = 0;

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
    double cell_values[MAX_CELLS];
    engine.cells.snapshot_values(cell_values, MAX_CELLS);

    // Execute one sample
    double hw_inputs[32] = {};
    double outputs[MAX_OUTPUTS] = {};
    double workspace[MAX_TOTAL_NODES] = {};

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
    execute_all_outputs(engine.scratch_pool, ctx);

    // Restore CellStore data table state
    engine.cells.data_table_count = saved_table_count;

    return make_number(outputs[0]);
}

// ── Top-level eval ──────────────────────────────────────────────────────────

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
            return make_number(engine.cells.cells[sym].value);
        }
        if (source && tok.span_start + tok.span_len <= source_length) {
            return eval_expression(
                source + tok.span_start, tok.span_len, engine);
        }
        return make_ok();
    }

    if (tok.kind == TokenKind::LBracket) {
        ts.consume(); // eat '['

        engine.scratch_pool.reset();
        uint8_t saved_table_count = engine.cells.data_table_count;

        char* vec_buf = engine.eval_text_buf;
        uint16_t buf_pos = 0;
        vec_buf[buf_pos++] = '[';
        bool first = true;
        bool any_error = false;
        EvalResult last_error = {};

        while (ts.peek().kind != TokenKind::RBracket && !ts.at_end()) {
            Token elem_start = ts.peek();

            // Skip the element form (handle both paren and bracket nesting)
            if (elem_start.kind == TokenKind::LParen ||
                elem_start.kind == TokenKind::LBracket) {
                int depth = 0;
                do {
                    Token t = ts.consume();
                    if (t.kind == TokenKind::LParen || t.kind == TokenKind::LBracket) depth++;
                    else if (t.kind == TokenKind::RParen || t.kind == TokenKind::RBracket) depth--;
                } while (depth > 0 && !ts.at_end());
            } else {
                ts.consume();
            }

            uint32_t elem_span_start = elem_start.span_start;
            Token prev = ts.tokens[ts.pos > 0 ? ts.pos - 1 : 0];
            uint32_t elem_span_end = (uint32_t)prev.span_start + prev.span_len;

            if (source && elem_span_end > elem_span_start &&
                elem_span_end <= source_length) {
                EvalResult er = eval_expression(
                    source + elem_span_start,
                    elem_span_end - elem_span_start, engine);
                if (er.kind == EvalResult::Error) {
                    any_error = true;
                    last_error = er;
                    break;
                }
                constexpr uint16_t BUF_CAP = sizeof(engine.eval_text_buf);
                if (!first && buf_pos < BUF_CAP - 20) vec_buf[buf_pos++] = ' ';
                first = false;
                int written = snprintf(vec_buf + buf_pos,
                                       BUF_CAP - buf_pos,
                                       "%.15g", er.number);
                if (written > 0) buf_pos += (uint16_t)written;
            }
        }

        while (ts.peek().kind != TokenKind::RBracket && !ts.at_end())
            ts.consume();
        ts.expect(TokenKind::RBracket);

        engine.cells.data_table_count = saved_table_count;

        if (any_error) return last_error;

        constexpr uint16_t BUF_CAP2 = sizeof(engine.eval_text_buf);
        if (buf_pos < BUF_CAP2 - 1) vec_buf[buf_pos++] = ']';
        vec_buf[buf_pos] = '\0';

        EvalResult r;
        r.kind = EvalResult::Text;
        r.text = vec_buf;
        r.text_length = buf_pos;
        return r;
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

        // Cell mutations
        if (op == sym.define || op == sym.def) {
            EvalResult r = do_define(ts, engine, source, source_length);
            ts.expect(TokenKind::RParen);
            return r;
        }
        if (op == sym.defn || op == sym.defun) {
            EvalResult r = do_defn(ts, engine, source, source_length);
            ts.expect(TokenKind::RParen);
            return r;
        }
        if (op == sym.defs) {
            // (defs [name1 val1 name2 val2 ...])
            if (!ts.expect(TokenKind::LBracket)) {
                return make_error("defs needs a bracket list of name-value pairs",
                                  "Try: (defs [x 1 y 2 z 3])");
            }
            while (ts.peek().kind != TokenKind::RBracket && !ts.at_end()) {
                Token name_tok = ts.consume();
                if (name_tok.kind != TokenKind::Symbol) {
                    return make_error("defs: expected a name",
                                      "Try: (defs [x 1 y 2])");
                }
                SymbolID cell_sym = name_tok.symbol;
                if (cell_id_out_of_range(cell_sym))
                    return make_too_many_definitions_error();

                if (ts.at_end() || ts.peek().kind == TokenKind::RBracket) {
                    return make_error("defs: each name needs a value",
                                      "Try: (defs [x 1 y 2])");
                }
                Token val_tok = ts.peek();
                if (val_tok.kind == TokenKind::Number) {
                    ts.consume();
                    engine.cells.cells[cell_sym].kind = CellKind::Number;
                    engine.cells.cells[cell_sym].revision++;
                    engine.cells.cells[cell_sym].value = val_tok.number;
                } else if (val_tok.kind == TokenKind::LBracket) {
                    // Vector: [1 2 3]
                    ts.consume();
                    double values[64];
                    uint16_t count = 0;
                    while (ts.peek().kind != TokenKind::RBracket &&
                           !ts.at_end() && count < 64) {
                        Token elem = ts.consume();
                        if (elem.kind == TokenKind::Number)
                            values[count++] = elem.number;
                    }
                    ts.expect(TokenKind::RBracket);
                    uint16_t tid = engine.cells.store_data_table(values, count);
                    engine.cells.cells[cell_sym].kind = CellKind::Data;
                    engine.cells.cells[cell_sym].data_table_id = tid;
                    engine.cells.cells[cell_sym].revision++;
                    engine.cells.cells[cell_sym].value = (double)count;
                } else {
                    // Expression — store as callable with 0 params.
                    // Arena store happens FIRST: a full arena fails this
                    // binding without touching the cell (F5).
                    uint16_t expr_start = ts.pos;
                    uint32_t byte_start = span_begin(ts, expr_start);
                    GraphBuilder::skip_form(ts);
                    uint32_t byte_end = span_end_of(ts, ts.pos);

                    uint32_t off = UINT32_MAX;
                    uint32_t len = 0;
                    if (source && byte_end > byte_start &&
                        byte_end <= source_length) {
                        len = byte_end - byte_start;
                        const CallableInfo& previous =
                            engine.cells.callables[cell_sym];
                        off = engine.arena.store_reuse(
                            previous.source_offset, previous.source_length,
                            source + byte_start, len);
                        if (off == UINT32_MAX) {
                            return make_error(
                                "Program storage is full — definition not applied",
                                "Free space with (useq-clear) or shorten your program");
                        }
                    }

                    engine.cells.cells[cell_sym].kind = CellKind::Callable;
                    engine.cells.cells[cell_sym].revision++;
                    engine.cells.callables[cell_sym].param_count = 0;
                    if (off != UINT32_MAX) {
                        engine.cells.callables[cell_sym].source_offset = off;
                        engine.cells.callables[cell_sym].source_length = len;
                    }
                }
                on_cell_changed(cell_sym, engine);
            }
            ts.expect(TokenKind::RBracket);
            ts.expect(TokenKind::RParen);
            return make_ok();
        }
        if (op == sym.defstate) {
            EvalResult r = do_defstate(ts, engine, source, source_length);
            ts.expect(TokenKind::RParen);
            return r;
        }
        if (op == sym.set) {
            EvalResult r = do_set(ts, engine, source);
            ts.expect(TokenKind::RParen);
            return r;
        }

        // Transport / time management
        if (op == sym.set_bpm) {
            if (next_is_live_edit(ts)) {
                // Skip to closing paren
                while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) ts.consume();
                ts.expect(TokenKind::RParen);
                return make_error(
                    "live-edit is not allowed as a direct argument of set-bpm",
                    "live-edit must be used inside an output expression like (a1 ...)");
            }
            EvalResult r = do_set_bpm(ts, engine);
            ts.expect(TokenKind::RParen);
            return r;
        }
        if (op == sym.set_time_sig) {
            if (next_is_live_edit(ts)) {
                while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) ts.consume();
                ts.expect(TokenKind::RParen);
                return make_error(
                    "live-edit is not allowed as a direct argument of set-time-sig",
                    "live-edit must be used inside an output expression like (a1 ...)");
            }
            EvalResult r = do_set_time_sig(ts, engine);
            ts.expect(TokenKind::RParen);
            return r;
        }
        if (op == sym.useq_clear) {
            EvalResult r = do_useq_clear(engine);
            ts.expect(TokenKind::RParen);
            return r;
        }
        if (op == sym.set_time_offset) {
            if (next_is_live_edit(ts)) {
                while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) ts.consume();
                ts.expect(TokenKind::RParen);
                return make_error(
                    "live-edit is not allowed as a direct argument of set-time-offset",
                    "live-edit must be used inside an output expression like (a1 ...)");
            }
            EvalResult r = do_set_time_offset(ts, engine.state);
            ts.expect(TokenKind::RParen);
            return r;
        }
        if (op == sym.nudge_time) {
            if (next_is_live_edit(ts)) {
                while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) ts.consume();
                ts.expect(TokenKind::RParen);
                return make_error(
                    "live-edit is not allowed as a direct argument of nudge-time",
                    "live-edit must be used inside an output expression like (a1 ...)");
            }
            EvalResult r = do_nudge_time(ts, engine.state);
            ts.expect(TokenKind::RParen);
            return r;
        }
        if (op == sym.useq_play) {
            engine.state.is_playing = true;
            ts.expect(TokenKind::RParen);
            return make_ok();
        }
        if (op == sym.useq_pause) {
            engine.state.is_playing = false;
            ts.expect(TokenKind::RParen);
            return make_ok();
        }
        if (op == sym.useq_stop) {
            engine.state.is_playing = false;
            engine.state.time_offset = 0.0;
            ts.expect(TokenKind::RParen);
            return make_ok();
        }
        if (op == sym.useq_rewind) {
            engine.state.time_offset = 0.0;
            ts.expect(TokenKind::RParen);
            return make_ok();
        }

        // synth — top-level NodeDef instantiation (synth-nodes.md §3)
        if (op == sym.synth) {
            EvalResult r = do_synth(ts, engine, source, source_length);
            ts.expect(TokenKind::RParen);
            return r;
        }

        // with-state-id — transparent identity wrapper (state-identity.md §6.3).
        // The editor payload builder wraps anonymous stateful forms in
        // `(with-state-id "<id>" <form>)`. The runtime treats the wrapper as
        // a passthrough: it skips the string identity argument and evaluates
        // the wrapped form. The identity has already been used by the editor
        // to assign stable identity; the runtime never needs to read it
        // because synth declarations carry their own :id field when present.
        if (op == sym.with_state_id) {
            // Skip the identity string argument. The next token must be a
            // string literal; we consume it without inspecting the value.
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
            // Evaluate the wrapped form (the synth declaration or other
            // stateful form). This recurses into the normal eval path so
            // `(with-state-id "..." (synth ...))` is equivalent to
            // `(synth ...)` for runtime purposes.
            EvalResult r = eval_form(ts, engine, source, source_length, shared_ids);
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
            int n = (int)n_tok.number;
            if (n < 1) n = 1;
            if (n > 64) n = 64;
            double values[64] = {};
            uint16_t table_id = engine.cells.store_data_table(values, (uint16_t)n);
            ts.expect(TokenKind::RParen);
            EvalResult r;
            r.kind = EvalResult::DataRef;
            r.number = (double)table_id;
            return r;
        }

        // get-expr — return stored source expression for a symbol
        if (op == sym.get_expr) {
            Token name_tok = ts.consume();
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
            while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) {
                last = eval_form(ts, engine, source, source_length, shared_ids);
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

// ── Synth GC integration ────────────────────────────────────────────────────
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

// ── eval_cold entry point (SignalEngine version) ───────────────────────────

EvalResult eval_cold(const char* source, uint32_t length, SignalEngine& engine) {
    Token tokens[MAX_TOKENS];
    Diagnostic parse_errors[8];
    uint8_t parse_error_count = 0;

    uint16_t count = TokenStream::tokenize(source, length, tokens, MAX_TOKENS,
                                            parse_errors, &parse_error_count);

    if (parse_error_count > 0) {
        EvalResult r;
        r.kind = EvalResult::Error;
        memcpy(r.diagnostics, parse_errors,
               parse_error_count * sizeof(Diagnostic));
        r.diagnostic_count = parse_error_count;
        return r;
    }

    TokenStream ts;
    memcpy(ts.tokens, tokens, count * sizeof(Token));
    ts.count = count;
    ts.pos = 0;

    // Cross-output live-edit ID tracking — cleared per eval batch
    SharedLiveEditIDs shared_ids;

    // ── Transactional synth artefact snapshot ───────────────────────────
    // Synth declarations are staged into engine.synth_graph during
    // eval_form. If any form in this eval unit fails, we restore the
    // pre-eval snapshot so the published graph/control table/revision
    // reflect only the last fully-successful eval (VAL-COMP-008/010).
    // The snapshot is cheap (one struct copy of POD arrays).
    SynthGraph synth_snapshot = engine.synth_graph;

    // Any cold eval may mutate cell values — bump the store revision so
    // per-tick snapshot consumers know to refresh (A12). Coarse but sound.
    engine.cells.store_revision++;

    // Handle multiple forms (implicit do)
    EvalResult last = make_ok();
    while (!ts.at_end() && ts.peek().kind != TokenKind::Eof) {
        last = eval_form(ts, engine, source, length, &shared_ids);
        if (last.kind == EvalResult::Error) {
            // Roll back synth artefacts to the pre-eval snapshot. The
            // revision counter is restored, so consumers can detect that
            // the graph did not advance (VAL-COMP-008/010).
            engine.synth_graph = synth_snapshot;
            return last;
        }
    }

    // Successful eval: advance the shared graph/control revision exactly
    // once so consumers see a single coherent update (VAL-COMP-009). We
    // compare the post-eval graph to the snapshot to avoid spurious
    // revision bumps on no-op evals (e.g. a bare `bar` query).
    if (engine.synth_graph.declaration_count() != synth_snapshot.declaration_count()
        || engine.synth_graph.control_count() != synth_snapshot.control_count()) {
        engine.synth_graph.advance_revision();
    } else {
        // Same shape — compare contents to detect param-only updates.
        bool changed = false;
        for (uint16_t i = 0; !changed && i < engine.synth_graph.control_count(); i++) {
            const SynthControlChannel& a = engine.synth_graph.controls[i];
            const SynthControlChannel& b = synth_snapshot.controls[i];
            if (a.root_node != b.root_node) changed = true;
            if (std::strcmp(a.identity, b.identity) != 0) changed = true;
            if (std::strcmp(a.param_name, b.param_name) != 0) changed = true;
        }
        if (changed) engine.synth_graph.advance_revision();
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

        Token tokens[MAX_TOKENS];
        uint8_t parse_errors = 0;
        uint16_t count = TokenStream::tokenize(
            src, engine.output_sources[i].arena_length,
            tokens, MAX_TOKENS, nullptr, &parse_errors);

        if (parse_errors != 0) {
            engine.pool.outputs[i].valid = false;
            continue;
        }

        TokenStream ts;
        memcpy(ts.tokens, tokens, count * sizeof(Token));
        ts.count = count;
        ts.pos = 0;

        GraphBuildResult result = build_output_graph(
            engine.pool, ts, engine.cells, engine.arena, src,
            &engine.registry, nullptr, i);

        if (!result.has_error) {
            engine.pool.outputs[i].root_node = result.root_node;
            engine.pool.outputs[i].valid = true;

            // Populate dependency tracking so on_cell_changed() works later
            engine.pool.output_deps[i].clear();
            for (uint8_t d = 0; d < result.dep_count; d++) {
                engine.pool.output_deps[i].add(result.dep_cells[d]);
            }
        } else {
            engine.pool.outputs[i].valid = false;
        }
    }

    // Reclaim stale nodes from any previous compilation (idempotent due to CSE)
    register_synth_external_roots(engine);
    engine.pool.gc_unreachable_nodes();
    commit_synth_external_roots(engine);
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
                Token tokens[MAX_TOKENS];
                uint8_t parse_errors = 0;
                uint16_t count = TokenStream::tokenize(
                    src, engine.output_sources[i].arena_length,
                    tokens, MAX_TOKENS, nullptr, &parse_errors);

                if (parse_errors == 0) {
                    TokenStream ts;
                    memcpy(ts.tokens, tokens, count * sizeof(Token));
                    ts.count = count;
                    ts.pos = 0;

                    uint8_t saved_tables = engine.cells.data_table_count;
                    GraphBuildResult result = build_output_graph(
                        engine.pool, ts, engine.cells, engine.arena, src,
                        &engine.registry, nullptr, i);
                    if (!result.has_error) {
                        engine.pool.outputs[i].root_node = result.root_node;
                        engine.pool.outputs[i].valid = true;

                        // Refresh the dependency list (F8) — the recompiled
                        // graph may reference different cells (e.g. a cell
                        // redefined from a number to an expression pulls in
                        // the cells that expression reads). Every sibling
                        // recompile path does this; skipping it here left
                        // outputs permanently deaf to their new deps.
                        engine.pool.output_deps[i].clear();
                        for (uint8_t d = 0; d < result.dep_count; d++) {
                            engine.pool.output_deps[i].add(result.dep_cells[d]);
                        }
                    } else {
                        engine.cells.data_table_count = saved_tables;
                        engine.pool.outputs[i].valid = false;
                    }
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

        Token tokens[MAX_TOKENS];
        uint8_t parse_errors = 0;
        uint16_t count = TokenStream::tokenize(
            src, engine.state_sources[s].arena_length,
            tokens, MAX_TOKENS, nullptr, &parse_errors);

        if (parse_errors == 0) {
            TokenStream ts;
            memcpy(ts.tokens, tokens, count * sizeof(Token));
            ts.count = count;
            ts.pos = 0;

            uint8_t saved_tables = engine.cells.data_table_count;
            GraphBuildResult result = build_output_graph(
                engine.pool, ts, engine.cells, engine.arena, src,
                &engine.registry, nullptr, (uint16_t)(MAX_OUTPUTS + s));
            if (!result.has_error) {
                engine.pool.state_update_roots[s] = result.root_node;
                // Update dependencies
                engine.state_sources[s].dep_count = result.dep_count;
                for (uint8_t d = 0; d < result.dep_count; d++) {
                    engine.state_sources[s].dep_cells[d] = result.dep_cells[d];
                }
            } else {
                engine.cells.data_table_count = saved_tables;
            }
        }
    }

    // Reclaim nodes orphaned by the recompiles above (F4). Every sibling
    // recompile path (eval_output, recompile_all_outputs, do_output_assign)
    // gc's before rebuilding; without this, live cell edits leak the old
    // graphs until the fixed node pool (360 nodes on firmware) fills up and
    // compilation silently fails.
    register_synth_external_roots(engine);
    engine.pool.gc_unreachable_nodes();
    commit_synth_external_roots(engine);
    engine.pool.rebuild_execution_order();

    // Recompilation may have changed which load ops each output references
    // (e.g. an output that used to be Pure now reads an input, or a feedback /
    // state dependency appeared/disappeared). Refresh the classification so
    // output_class / output_input_mask don't go stale after a cell change.
    classify_outputs(engine.pool);
}

} // namespace sig
