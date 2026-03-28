#include "cold_eval.h"
#include "token.h"
#include "graph_builder.h"
#include "../modulisp/lisp/symbol_intern.h"
#include <cstring>

namespace sig {

// ── Output source storage ───────────────────────────────────────────────────

OutputSource output_sources[MAX_OUTPUTS] = {};

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

static EvalResult eval_form(TokenStream& ts, CellStore& cells,
                            SourceArena& arena, NodePool& pool);

// ── define ──────────────────────────────────────────────────────────────────

static EvalResult do_define(TokenStream& ts, CellStore& cells,
                            SourceArena& arena, NodePool& pool) {
    Token name_tok = ts.consume();
    if (name_tok.kind != TokenKind::Symbol) {
        return make_error("define needs a name", "Try: (define freq 440)");
    }
    SymbolID sym = name_tok.symbol;

    Token val_tok = ts.peek();

    if (val_tok.kind == TokenKind::Number) {
        // Simple numeric constant
        ts.consume();
        cells.cells[sym].kind = CellKind::Number;
        cells.cells[sym].revision++;
        cells.cells[sym].value = val_tok.number;
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

        uint16_t table_id = cells.store_data_table(values, count);
        cells.cells[sym].kind = CellKind::Data;
        cells.cells[sym].data_table_id = table_id;
        cells.cells[sym].revision++;
        cells.cells[sym].value = (double)count;
    }
    else {
        // Expression — store source text as callable with 0 params
        // Record the start position in the source to find the raw text
        uint16_t expr_start = ts.pos;

        // Skip past the expression to find its extent
        GraphBuilder::skip_form(ts);
        uint16_t expr_end = ts.pos;

        // We need the raw source text. Reconstruct from token spans.
        // For now, use a simple approach: store the token positions
        // and the original source isn't directly available here.
        // TODO: pass original source through to store expression text properly
        cells.cells[sym].kind = CellKind::Callable;
        cells.cells[sym].revision++;
        cells.callables[sym].param_count = 0;
        // Source storage deferred — needs original source text
    }

    // Notify dependents
    on_cell_changed(sym, cells, arena, pool);

    return make_ok();
}

// ── defn ────────────────────────────────────────────────────────────────────

static EvalResult do_defn(TokenStream& ts, CellStore& cells,
                          SourceArena& arena, NodePool& pool) {
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

    CallableInfo& info = cells.callables[sym];
    info.param_count = 0;
    while (ts.peek().kind != TokenKind::RBracket && !ts.at_end()) {
        Token param = ts.consume();
        if (param.kind == TokenKind::Symbol && info.param_count < MAX_CALLABLE_PARAMS) {
            info.params[info.param_count++] = param.symbol;
        }
    }
    ts.expect(TokenKind::RBracket);

    // Store body source — skip body and record extent
    // TODO: store actual source text in arena
    uint16_t body_start = ts.pos;
    GraphBuilder::skip_form(ts);
    uint16_t body_end = ts.pos;
    (void)body_start;
    (void)body_end;

    cells.cells[sym].kind = CellKind::Callable;
    cells.cells[sym].revision++;

    on_cell_changed(sym, cells, arena, pool);

    return make_ok();
}

// ── set ─────────────────────────────────────────────────────────────────────

static EvalResult do_set(TokenStream& ts, CellStore& cells,
                         SourceArena& arena, NodePool& pool) {
    Token name_tok = ts.consume();
    if (name_tok.kind != TokenKind::Symbol) {
        return make_error("set needs a name", "Try: (set x 42)");
    }
    SymbolID sym = name_tok.symbol;

    Token val_tok = ts.peek();
    if (val_tok.kind == TokenKind::Number) {
        ts.consume();
        cells.cells[sym].kind = CellKind::Number;
        cells.cells[sym].revision++;
        cells.cells[sym].value = val_tok.number;
        // set does NOT store source expression
    } else {
        // For non-numeric set, evaluate the expression and store result
        // TODO: evaluate expression on cold path
        return make_error("set currently only supports numbers",
                          "Try: (set x 42)");
    }

    on_cell_changed(sym, cells, arena, pool);

    return make_ok();
}

// ── Output assignment ───────────────────────────────────────────────────────

static EvalResult do_output_assign(SymbolID output_sym, TokenStream& ts,
                                    CellStore& cells, SourceArena& arena,
                                    NodePool& pool) {
    uint16_t output_index = GraphBuilder::resolve_output_index(output_sym);
    if (output_index == NODE_NONE) {
        return make_error("Unknown output", "Try: (a1 expression)");
    }

    // Build the signal graph
    GraphBuildResult result = build_output_graph(pool, ts, cells, arena);

    if (result.has_error) {
        pool.outputs[output_index].valid = false;
        EvalResult r;
        r.kind = EvalResult::Error;
        memcpy(r.diagnostics, result.diagnostics,
               result.diagnostic_count * sizeof(Diagnostic));
        r.diagnostic_count = result.diagnostic_count;
        return r;
    }

    // Install the new graph root
    pool.outputs[output_index].root_node = result.root_node;
    pool.outputs[output_index].valid = true;

    // Re-sort execution order
    pool.rebuild_execution_order();

    return make_ok();
}

// ── Top-level eval ──────────────────────────────────────────────────────────

static EvalResult eval_form(TokenStream& ts, CellStore& cells,
                            SourceArena& arena, NodePool& pool) {
    Token tok = ts.peek();

    if (tok.kind == TokenKind::Number) {
        ts.consume();
        return make_number(tok.number);
    }

    if (tok.kind == TokenKind::Symbol) {
        ts.consume();
        SymbolID sym = tok.symbol;
        // Look up in cell table
        if (sym < MAX_CELLS && cells.cells[sym].kind == CellKind::Number) {
            return make_number(cells.cells[sym].value);
        }
        return make_ok();
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
            EvalResult r = do_define(ts, cells, arena, pool);
            ts.expect(TokenKind::RParen);
            return r;
        }
        if (op == sym.defn || op == sym.defun) {
            EvalResult r = do_defn(ts, cells, arena, pool);
            ts.expect(TokenKind::RParen);
            return r;
        }
        if (op == sym.set) {
            EvalResult r = do_set(ts, cells, arena, pool);
            ts.expect(TokenKind::RParen);
            return r;
        }

        // Output assignment
        if (GraphBuilder::is_output_symbol(op)) {
            EvalResult r = do_output_assign(op, ts, cells, arena, pool);
            ts.expect(TokenKind::RParen);
            return r;
        }

        // do / scope — evaluate children sequentially
        if (op == sym.do_ || op == sym.scope) {
            EvalResult last = make_ok();
            while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) {
                last = eval_form(ts, cells, arena, pool);
            }
            ts.expect(TokenKind::RParen);
            return last;
        }

        // Unknown form at top level — try to evaluate as expression
        // Skip and return ok for now
        while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) {
            ts.consume();
        }
        ts.expect(TokenKind::RParen);
        return make_ok();
    }

    return make_error("Unexpected input", "Try: (define name value) or (a1 expression)");
}

// ── eval_cold entry point ───────────────────────────────────────────────────

EvalResult eval_cold(const char* source, uint32_t length,
                     CellStore& cells, SourceArena& arena, NodePool& pool) {
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
        last = eval_form(ts, cells, arena, pool);
        if (last.kind == EvalResult::Error) return last;
    }

    return last;
}

// ── Dependency tracking ─────────────────────────────────────────────────────

void on_cell_changed(SymbolID cell_id, CellStore& cells,
                     SourceArena& arena, NodePool& pool) {
    // Check which outputs depend on this cell
    for (uint16_t i = 0; i < MAX_OUTPUTS; i++) {
        if (pool.outputs[i].root_node == NODE_NONE) continue;
        if (!pool.output_deps[i].contains(cell_id)) continue;

        // This output needs recompilation
        // Save LKG value
        if (pool.outputs[i].valid) {
            // LKG is already stored from last execution
        }

        // Recompile from stored output source
        if (output_sources[i].has_source) {
            const char* src = arena.read(output_sources[i].arena_offset);
            if (src) {
                Token tokens[MAX_TOKENS];
                uint8_t parse_errors = 0;
                uint16_t count = TokenStream::tokenize(
                    src, output_sources[i].arena_length,
                    tokens, MAX_TOKENS, nullptr, &parse_errors);

                if (parse_errors == 0) {
                    TokenStream ts;
                    memcpy(ts.tokens, tokens, count * sizeof(Token));
                    ts.count = count;
                    ts.pos = 0;

                    GraphBuildResult result = build_output_graph(pool, ts, cells, arena);
                    if (!result.has_error) {
                        pool.outputs[i].root_node = result.root_node;
                        pool.outputs[i].valid = true;
                    } else {
                        pool.outputs[i].valid = false;
                        // Keep LKG
                    }
                }
            }
        }
    }

    pool.rebuild_execution_order();
}

} // namespace sig
