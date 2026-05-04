#include "cold_eval.h"
#include "token.h"
#include "graph_builder.h"
#include "executor.h"
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

// ── Cold-path form evaluation ───────────────────────────────────────────────

static EvalResult eval_form(TokenStream& ts, SignalEngine& engine,
                            const char* source, uint32_t source_length);

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

    Token val_tok = ts.peek();

    if (val_tok.kind == TokenKind::Number) {
        // Simple numeric constant
        ts.consume();
        engine.cells.cells[sym].kind = CellKind::Number;
        engine.cells.cells[sym].revision++;
        engine.cells.cells[sym].value = val_tok.number;
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
    }
    else {
        // Expression — store source text as callable with 0 params
        uint16_t expr_start = ts.pos;
        uint32_t byte_start = span_begin(ts, expr_start);

        // Skip past the expression to find its extent
        GraphBuilder::skip_form(ts);
        uint32_t byte_end = span_end_of(ts, ts.pos);

        engine.cells.cells[sym].kind = CellKind::Callable;
        engine.cells.cells[sym].revision++;
        engine.cells.callables[sym].param_count = 0;

        // Copy the expression source text into the arena
        if (source && byte_end > byte_start && byte_end <= source_length) {
            uint32_t len = byte_end - byte_start;
            uint32_t offset = engine.arena.store(source + byte_start, len);
            if (offset != UINT32_MAX) {
                engine.cells.callables[sym].source_offset = offset;
                engine.cells.callables[sym].source_length = len;
            }
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

    // Parse parameter list
    if (!ts.expect(TokenKind::LBracket)) {
        return make_error("defn needs a parameter list in brackets",
                          "Try: (defn osc [f ph] (sin (* ph f)))");
    }

    CallableInfo& info = engine.cells.callables[sym];
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

    engine.cells.cells[sym].kind = CellKind::Callable;
    engine.cells.cells[sym].revision++;

    // Copy the body source text into the arena
    if (source && byte_end > byte_start && byte_end <= source_length) {
        uint32_t len = byte_end - byte_start;
        uint32_t offset = engine.arena.store(source + byte_start, len);
        if (offset != UINT32_MAX) {
            info.source_offset = offset;
            info.source_length = len;
        }
    }

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

    Token val_tok = ts.peek();
    if (val_tok.kind == TokenKind::Number) {
        ts.consume();
        engine.cells.cells[sym].kind = CellKind::Number;
        engine.cells.cells[sym].revision++;
        engine.cells.cells[sym].value = val_tok.number;
    } else {
        // Non-numeric: compile in scratch pool, evaluate once, store result.
        // This avoids leaking nodes/CSE/data into the live pool.
        uint8_t saved_tables = engine.cells.data_table_count;
        engine.scratch_pool.reset();

        GraphBuildResult gr = build_output_graph(engine.scratch_pool, ts,
                                                  engine.cells, engine.arena, source);
        if (gr.has_error) {
            engine.cells.data_table_count = saved_tables;
            return make_error("set: expression could not be evaluated",
                              "Try: (set x 42)");
        }

        if (engine.scratch_pool.nodes[gr.root_node].op == NodeOp::Const) {
            engine.cells.cells[sym].kind = CellKind::Number;
            engine.cells.cells[sym].value = engine.scratch_pool.nodes[gr.root_node].imm;
            engine.cells.cells[sym].revision++;
        } else {
            engine.scratch_pool.outputs[0].root_node = gr.root_node;
            engine.scratch_pool.outputs[0].valid = true;
            engine.scratch_pool.rebuild_execution_order();

            memcpy(engine.scratch_pool.state_values, engine.pool.state_values,
                   sizeof(engine.pool.state_values));

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

            engine.cells.cells[sym].kind = CellKind::Number;
            engine.cells.cells[sym].value = outputs[0];
            engine.cells.cells[sym].revision++;
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

    // Parse init value — must be a number literal for simplicity
    Token init_tok = ts.consume();
    if (init_tok.kind != TokenKind::Number) {
        return make_error("defstate initial value must be a number",
                          "Try: (defstate counter 0 (+ counter 1))");
    }
    double init_value = init_tok.number;

    // Allocate a state slot (if this name already has a state slot, reuse it)
    uint16_t state_slot = NODE_NONE;
    if (sym < MAX_CELLS && engine.cells.cells[sym].kind == CellKind::Number
        && engine.cells.cells[sym].flags == 0x02) {
        // Existing state cell — reuse its slot, do NOT reset the value
        state_slot = (uint16_t)engine.cells.cells[sym].data_table_id;
    }

    if (state_slot == NODE_NONE) {
        // New state cell — allocate slot and set initial value
        if (engine.pool.state_slot_count >= MAX_STATE_SLOTS) {
            return make_error("Too many state variables (max 32)",
                              "Remove unused defstate declarations");
        }
        state_slot = engine.pool.state_slot_count++;
        engine.pool.state_values[state_slot] = init_value;
    }

    // Mark this cell as a state cell: kind=Number (readable), flags=0x02 (state marker),
    // data_table_id stores the state slot index
    engine.cells.cells[sym].kind = CellKind::Number;
    engine.cells.cells[sym].flags = 0x02;  // state cell marker
    engine.cells.cells[sym].data_table_id = state_slot;
    engine.cells.cells[sym].revision++;
    engine.cells.cells[sym].value = init_value;

    // Record the update expression source text for recompilation
    uint16_t expr_start = ts.pos;
    uint32_t byte_start = span_begin(ts, expr_start);
    {
        uint16_t saved = ts.pos;
        GraphBuilder::skip_form(ts);
        uint32_t byte_end = span_end_of(ts, ts.pos);
        ts.rewind(saved);

        if (source && byte_end > byte_start && byte_end <= source_length) {
            uint32_t len = byte_end - byte_start;
            uint32_t offset = engine.arena.store(source + byte_start, len);
            if (offset != UINT32_MAX) {
                engine.state_sources[state_slot].arena_offset = offset;
                engine.state_sources[state_slot].arena_length = len;
                engine.state_sources[state_slot].has_source = true;
            }
        }
    }

    // Compile the update expression as a signal graph
    GraphBuildResult result = build_output_graph(engine.pool, ts,
                                                 engine.cells, engine.arena, source,
                                                 &engine.registry);

    if (result.has_error) {
        engine.pool.state_update_roots[state_slot] = sig::NODE_NONE;
        return make_error("defstate update expression failed to compile",
                          "Check the update expression");
    }

    // Store the update root and dependencies
    engine.pool.state_update_roots[state_slot] = result.root_node;
    engine.state_sources[state_slot].dep_count = result.dep_count;
    for (uint8_t d = 0; d < result.dep_count; d++) {
        engine.state_sources[state_slot].dep_cells[d] = result.dep_cells[d];
    }

    // Rebuild execution order to include state update subgraphs
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

// ── Output assignment ───────────────────────────────────────────────────────

static EvalResult do_output_assign(SymbolID output_sym, TokenStream& ts,
                                    SignalEngine& engine,
                                    const char* source, uint32_t source_length) {
    uint16_t output_index = GraphBuilder::resolve_output_index(output_sym);
    if (output_index == NODE_NONE) {
        return make_error("Unknown output", "Try: (a1 expression)");
    }

    // Record the expression source text for recompilation
    uint16_t expr_start_pos = ts.pos;
    uint32_t byte_start = span_begin(ts, expr_start_pos);
    {
        uint16_t saved = ts.pos;
        GraphBuilder::skip_form(ts);
        uint32_t byte_end = span_end_of(ts, ts.pos);
        ts.rewind(saved);

        if (source && byte_end > byte_start && byte_end <= source_length) {
            uint32_t len = byte_end - byte_start;
            uint32_t offset = engine.arena.store(source + byte_start, len);
            if (offset != UINT32_MAX) {
                engine.output_sources[output_index].arena_offset = offset;
                engine.output_sources[output_index].arena_length = len;
                engine.output_sources[output_index].has_source = true;
            }
        }
    }

    // Build the signal graph
    GraphBuildResult result = build_output_graph(engine.pool, ts,
                                                 engine.cells, engine.arena, source,
                                                 &engine.registry);

    if (result.has_error) {
        engine.pool.outputs[output_index].valid = false;
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
    engine.pool.gc_unreachable_nodes();

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

    // Mirror live state values into scratch pool for LoadState nodes
    memcpy(engine.scratch_pool.state_values, engine.pool.state_values,
           sizeof(engine.pool.state_values));

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
                            const char* source, uint32_t source_length) {
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
        // Try as a signal expression (e.g. bar, beat, phrase)
        return eval_expression(
            source + tok.span_start, tok.span_len, engine);
    }

    if (tok.kind == TokenKind::LBracket) {
        // Top-level vector: evaluate each element via scratch eval, return text
        ts.consume(); // eat '['

        // Reset scratch pool once for the whole vector
        engine.scratch_pool.reset();
        uint8_t saved_table_count = engine.cells.data_table_count;

        static char vec_buf[512];
        uint16_t buf_pos = 0;
        vec_buf[buf_pos++] = '[';
        bool first = true;
        bool any_error = false;
        EvalResult last_error = {};

        while (ts.peek().kind != TokenKind::RBracket && !ts.at_end()) {
            // Get source slice for this element
            Token elem_start = ts.peek();

            // Skip the element form
            if (elem_start.kind == TokenKind::LParen) {
                int depth = 0;
                do {
                    Token t = ts.consume();
                    if (t.kind == TokenKind::LParen) depth++;
                    else if (t.kind == TokenKind::RParen) depth--;
                } while (depth > 0 && !ts.at_end());
            } else {
                ts.consume();
            }

            uint32_t elem_span_start = elem_start.span_start;
            Token prev = ts.tokens[ts.pos > 0 ? ts.pos - 1 : 0];
            uint32_t elem_span_end = prev.span_start + prev.span_len;

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
                if (!first && buf_pos < sizeof(vec_buf) - 20) vec_buf[buf_pos++] = ' ';
                first = false;
                int written = snprintf(vec_buf + buf_pos,
                                       sizeof(vec_buf) - buf_pos,
                                       "%.15g", er.number);
                if (written > 0) buf_pos += (uint16_t)written;
            }
        }

        // Skip to closing bracket
        while (ts.peek().kind != TokenKind::RBracket && !ts.at_end())
            ts.consume();
        ts.expect(TokenKind::RBracket);

        engine.cells.data_table_count = saved_table_count;

        if (any_error) return last_error;

        if (buf_pos < sizeof(vec_buf) - 1) vec_buf[buf_pos++] = ']';
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
                    // Expression — store as callable with 0 params
                    uint16_t expr_start = ts.pos;
                    uint32_t byte_start = span_begin(ts, expr_start);
                    GraphBuilder::skip_form(ts);
                    uint32_t byte_end = span_end_of(ts, ts.pos);

                    engine.cells.cells[cell_sym].kind = CellKind::Callable;
                    engine.cells.cells[cell_sym].revision++;
                    engine.cells.callables[cell_sym].param_count = 0;
                    if (source && byte_end > byte_start &&
                        byte_end <= source_length) {
                        uint32_t len = byte_end - byte_start;
                        uint32_t off = engine.arena.store(
                            source + byte_start, len);
                        if (off != UINT32_MAX) {
                            engine.cells.callables[cell_sym].source_offset = off;
                            engine.cells.callables[cell_sym].source_length = len;
                        }
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
            EvalResult r = do_set_bpm(ts, engine);
            ts.expect(TokenKind::RParen);
            return r;
        }
        if (op == sym.set_time_sig) {
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
            EvalResult r = do_set_time_offset(ts, engine.state);
            ts.expect(TokenKind::RParen);
            return r;
        }
        if (op == sym.nudge_time) {
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
                                            source, source_length);
            ts.expect(TokenKind::RParen);
            return r;
        }

        // do / scope — evaluate children sequentially
        if (op == sym.do_ || op == sym.scope) {
            EvalResult last = make_ok();
            while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) {
                last = eval_form(ts, engine, source, source_length);
            }
            ts.expect(TokenKind::RParen);
            return last;
        }

        // Unknown form at top level — try as signal expression.
        // Extract the full form source from the opening '(' through ')'.
        {
            uint32_t form_start = tok.span_start; // position of '('
            // Skip remaining tokens to find the closing ')'
            while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) {
                ts.consume();
            }
            Token rparen = ts.consume(); // eat ')'
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

    // Handle multiple forms (implicit do)
    EvalResult last = make_ok();
    while (!ts.at_end() && ts.peek().kind != TokenKind::Eof) {
        last = eval_form(ts, engine, source, length);
        if (last.kind == EvalResult::Error) return last;
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
            &engine.registry);

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
    engine.pool.gc_unreachable_nodes();
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

                    GraphBuildResult result = build_output_graph(
                        engine.pool, ts, engine.cells, engine.arena, src,
                        &engine.registry);
                    if (!result.has_error) {
                        engine.pool.outputs[i].root_node = result.root_node;
                        engine.pool.outputs[i].valid = true;
                    } else {
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

            GraphBuildResult result = build_output_graph(
                engine.pool, ts, engine.cells, engine.arena, src,
                &engine.registry);
            if (!result.has_error) {
                engine.pool.state_update_roots[s] = result.root_node;
                // Update dependencies
                engine.state_sources[s].dep_count = result.dep_count;
                for (uint8_t d = 0; d < result.dep_count; d++) {
                    engine.state_sources[s].dep_cells[d] = result.dep_cells[d];
                }
            }
        }
    }

    engine.pool.rebuild_execution_order();
}

} // namespace sig
