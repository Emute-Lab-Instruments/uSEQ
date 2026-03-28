#include "graph_builder.h"
#include "../modulisp/lisp/symbol_intern.h"
#include <cstring>
#include <cmath>

namespace sig {

// ── Static symbol cache ─────────────────────────────────────────────────────

GraphBuilder::Symbols GraphBuilder::sym = {};
bool GraphBuilder::symbols_initialized = false;

void GraphBuilder::init_symbols() {
    if (symbols_initialized) return;
    auto& si = SymbolIntern::getInstance();

    sym.t = si.intern("t");
    sym.beat = si.intern("beat");
    sym.bar = si.intern("bar");
    sym.phrase = si.intern("phrase");
    sym.section = si.intern("section");
    sym.beat_num = si.intern("beat-num");
    sym.bar_num = si.intern("bar-num");

    sym.bpm = si.intern("bpm");
    sym.beats_per_bar = si.intern("beats-per-bar");
    sym.bars_per_phrase = si.intern("bars-per-phrase");
    sym.phrases_per_section = si.intern("phrases-per-section");

    sym.fast = si.intern("fast");
    sym.slow = si.intern("slow");
    sym.offset = si.intern("offset");
    sym.shift = si.intern("shift");

    sym.if_ = si.intern("if");
    sym.let_ = si.intern("let");
    sym.do_ = si.intern("do");
    sym.for_ = si.intern("for");
    sym.while_ = si.intern("while");
    sym.fn = si.intern("fn");
    sym.lambda = si.intern("lambda");

    sym.define = si.intern("define");
    sym.def = si.intern("def");
    sym.defn = si.intern("defn");
    sym.defun = si.intern("defun");
    sym.defs = si.intern("defs");
    sym.set = si.intern("set");

    sym.step = si.intern("step");
    sym.gates = si.intern("gates");
    sym.trigs = si.intern("trigs");
    sym.euclid = si.intern("euclid");
    sym.eu = si.intern("eu");
    sym.seq = si.intern("seq");
    sym.from_list = si.intern("from-list");
    sym.interp = si.intern("interp");
    sym.flatseq = si.intern("flatseq");
    sym.dm = si.intern("dm");
    sym.range = si.intern("range");

    sym.sin_ = si.intern("sin");
    sym.cos_ = si.intern("cos");
    sym.tan_ = si.intern("tan");
    sym.abs_ = si.intern("abs");
    sym.floor_ = si.intern("floor");
    sym.ceil_ = si.intern("ceil");
    sym.sqrt_ = si.intern("sqrt");
    sym.neg = si.intern("neg");
    sym.min_ = si.intern("min");
    sym.max_ = si.intern("max");
    sym.pow_ = si.intern("pow");
    sym.mod_ = si.intern("mod");
    sym.clamp = si.intern("clamp");
    sym.frac_ = si.intern("frac");
    sym.mod_pct = si.intern("%");
    sym.input = si.intern("input");

    sym.not_ = si.intern("not");
    sym.and_ = si.intern("and");
    sym.or_ = si.intern("or");

    sym.tri = si.intern("tri");
    sym.sqr = si.intern("sqr");
    sym.pulse = si.intern("pulse");
    sym.usin = si.intern("usin");
    sym.ucos = si.intern("ucos");

    sym.bi_to_uni = si.intern("bi-to-uni");
    sym.b_to_u = si.intern("b>u");
    sym.uni_to_bi = si.intern("uni-to-bi");
    sym.u_to_b = si.intern("u>b");

    sym.scale = si.intern("scale");
    sym.lerp = si.intern("lerp");

    sym.random_ = si.intern("random");
    sym.index_rand = si.intern("index-rand");

    sym.quote = si.intern("quote");
    sym.scope = si.intern("scope");

    sym.loop_at = si.intern("loop-at");
    sym.eval_at_time = si.intern("eval-at-time");
    sym.gatesw = si.intern("gatesw");
    sym.zeros_ = si.intern("zeros");
    sym.get_expr = si.intern("get-expr");

    sym.rpulse = si.intern("rpulse");
    sym.rstep = si.intern("rstep");
    sym.ridx = si.intern("ridx");
    sym.rwarp = si.intern("rwarp");

    // Transport / time management (cold-path only)
    sym.set_bpm = si.intern("set-bpm");
    sym.set_time_sig = si.intern("set-time-sig");
    sym.useq_clear = si.intern("useq-clear");
    sym.set_time_offset = si.intern("useq-set-time-offset");
    sym.nudge_time = si.intern("useq-nudge-time");
    sym.useq_play = si.intern("useq-play");
    sym.useq_pause = si.intern("useq-pause");
    sym.useq_stop = si.intern("useq-stop");
    sym.useq_rewind = si.intern("useq-rewind");

    symbols_initialized = true;
}

// ── Scope ───────────────────────────────────────────────────────────────────

void Scope::bind(SymbolID name, uint16_t node_index) {
    if (local_count < MAX_LOCAL_BINDINGS) {
        locals[local_count++] = { name, node_index };
    }
}

const Scope::Binding* Scope::find(SymbolID name) const {
    // Search local scope first, then parent
    for (int i = (int)local_count - 1; i >= 0; i--) {
        if (locals[i].name == name) return &locals[i];
    }
    if (parent) return parent->find(name);
    return nullptr;
}

// ── GraphBuilder Construction ───────────────────────────────────────────────

GraphBuilder::GraphBuilder(NodePool& p, const CellStore& c, const SourceArena& s)
    : pool(p), cells(c), source(s)
{
    init_symbols();
}

// ── Helpers ─────────────────────────────────────────────────────────────────

bool GraphBuilder::is_const(uint16_t node_idx) const {
    return node_idx != NODE_NONE && pool.nodes[node_idx].op == NodeOp::Const;
}

double GraphBuilder::const_value(uint16_t node_idx) const {
    return pool.nodes[node_idx].imm;
}

void GraphBuilder::add_dependency(SymbolID s) {
    for (uint8_t i = 0; i < dep_count; i++) {
        if (dep_cells[i] == s) return;
    }
    if (dep_count < MAX_OUTPUT_DEPS) {
        dep_cells[dep_count++] = s;
    }
}

bool GraphBuilder::is_in_inline_stack(SymbolID s) const {
    for (uint8_t i = 0; i < inline_depth; i++) {
        if (inline_stack[i] == s) return true;
    }
    return false;
}

void GraphBuilder::push_inline_stack(SymbolID s) {
    if (inline_depth < MAX_INLINE_DEPTH) {
        inline_stack[inline_depth++] = s;
    }
}

void GraphBuilder::pop_inline_stack() {
    if (inline_depth > 0) inline_depth--;
}

// ── Error reporting ─────────────────────────────────────────────────────────

uint16_t GraphBuilder::report_error(const Token& tok, const char* message,
                                     const char* suggestion) {
    return report_error_at(tok.span_start, tok.span_len, message, suggestion);
}

uint16_t GraphBuilder::report_error_at(uint16_t span_start, uint16_t span_len,
                                        const char* message, const char* suggestion) {
    if (diagnostic_count < MAX_DIAGNOSTICS) {
        diagnostics[diagnostic_count++] = {
            DiagnosticSeverity::Error, DiagnosticCategory::Runtime,
            span_start, span_len, message, suggestion
        };
    }
    has_error = true;
    return NODE_NONE;
}

uint16_t GraphBuilder::report_error_with_fuzzy_match(SymbolID sym_id,
                                                      uint16_t span_start,
                                                      uint16_t span_len) {
    // TODO: implement fuzzy matching
    return report_error_at(span_start, span_len,
                           "Unknown name",
                           "Check your spelling");
}

uint16_t GraphBuilder::report_warning(uint16_t span_start, uint16_t span_len,
                                       const char* message, const char* suggestion) {
    if (diagnostic_count < MAX_DIAGNOSTICS) {
        diagnostics[diagnostic_count++] = {
            DiagnosticSeverity::Warning, DiagnosticCategory::Runtime,
            span_start, span_len, message, suggestion
        };
    }
    return NODE_NONE; // warnings don't abort compilation
}

// ── Side-effect detection ───────────────────────────────────────────────────

bool GraphBuilder::is_side_effect_form(SymbolID op) const {
    return op == sym.define || op == sym.def || op == sym.defn ||
           op == sym.defun || op == sym.defs || op == sym.set ||
           op == sym.zeros_ || op == sym.get_expr ||
           op == sym.set_bpm || op == sym.set_time_sig ||
           op == sym.useq_clear || op == sym.set_time_offset ||
           op == sym.nudge_time || op == sym.useq_play ||
           op == sym.useq_pause || op == sym.useq_stop ||
           op == sym.useq_rewind;
}

// ── Operator classification ─────────────────────────────────────────────────

bool GraphBuilder::is_arithmetic_op(SymbolID op) const {
    return op == SymbolIntern::getInstance().intern("+") ||
           op == SymbolIntern::getInstance().intern("-") ||
           op == SymbolIntern::getInstance().intern("*") ||
           op == SymbolIntern::getInstance().intern("/") ||
           op == sym.mod_pct;
}

bool GraphBuilder::is_comparison_op(SymbolID op) const {
    auto& si = SymbolIntern::getInstance();
    return op == si.intern(">") || op == si.intern("<") ||
           op == si.intern(">=") || op == si.intern("<=") ||
           op == si.intern("=");
}

bool GraphBuilder::is_logic_op(SymbolID op) const {
    return op == sym.not_ || op == sym.and_ || op == sym.or_;
}

bool GraphBuilder::is_unary_math(SymbolID op) const {
    return op == sym.sin_ || op == sym.cos_ || op == sym.tan_ ||
           op == sym.abs_ || op == sym.floor_ || op == sym.ceil_ ||
           op == sym.sqrt_ || op == sym.neg || op == sym.frac_ ||
           op == sym.usin || op == sym.ucos ||
           op == sym.bi_to_uni || op == sym.b_to_u ||
           op == sym.uni_to_bi || op == sym.u_to_b;
}

bool GraphBuilder::is_binary_math(SymbolID op) const {
    return op == sym.min_ || op == sym.max_ || op == sym.pow_ ||
           op == sym.mod_ || op == sym.pulse;
}

bool GraphBuilder::is_ternary_math(SymbolID op) const {
    return op == sym.clamp || op == sym.lerp || op == sym.scale;
}

NodeOp GraphBuilder::arithmetic_sym_to_op(SymbolID op) const {
    auto& si = SymbolIntern::getInstance();
    if (op == si.intern("+")) return NodeOp::Add;
    if (op == si.intern("-")) return NodeOp::Sub;
    if (op == si.intern("*")) return NodeOp::Mul;
    if (op == si.intern("/")) return NodeOp::Div;
    if (op == sym.mod_pct) return NodeOp::Mod;
    return NodeOp::Add;
}

NodeOp GraphBuilder::comparison_sym_to_op(SymbolID op) const {
    auto& si = SymbolIntern::getInstance();
    if (op == si.intern(">"))  return NodeOp::CmpGt;
    if (op == si.intern("<"))  return NodeOp::CmpLt;
    if (op == si.intern(">=")) return NodeOp::CmpGe;
    if (op == si.intern("<=")) return NodeOp::CmpLe;
    if (op == si.intern("="))  return NodeOp::CmpEq;
    return NodeOp::CmpEq;
}

NodeOp GraphBuilder::unary_sym_to_op(SymbolID op) const {
    if (op == sym.sin_)     return NodeOp::Sin;
    if (op == sym.cos_)     return NodeOp::Cos;
    if (op == sym.tan_)     return NodeOp::Tan;
    if (op == sym.abs_)     return NodeOp::Abs;
    if (op == sym.floor_)   return NodeOp::Floor;
    if (op == sym.ceil_)    return NodeOp::Ceil;
    if (op == sym.sqrt_)    return NodeOp::Sqrt;
    if (op == sym.neg)      return NodeOp::Neg;
    if (op == sym.frac_)    return NodeOp::Frac;
    if (op == sym.usin)     return NodeOp::USin;
    if (op == sym.ucos)     return NodeOp::UCos;
    if (op == sym.bi_to_uni || op == sym.b_to_u) return NodeOp::BiToUni;
    if (op == sym.uni_to_bi || op == sym.u_to_b) return NodeOp::UniToBi;
    return NodeOp::Abs;
}

bool GraphBuilder::is_output_symbol(SymbolID op) {
    // Check if symbol matches a1-a8, d1-d8, s1-s8
    const String& name = getSymbolString(op);
    if (name.length() < 2 || name.length() > 2) return false;
    char prefix = name[0];
    char digit = name[1];
    if (digit < '1' || digit > '8') return false;
    return prefix == 'a' || prefix == 'd' || prefix == 's';
}

uint16_t GraphBuilder::resolve_output_index(SymbolID op) {
    const String& name = getSymbolString(op);
    char prefix = name[0];
    uint16_t num = (uint16_t)(name[1] - '1');
    switch (prefix) {
        case 'a': return num;       // 0-7
        case 'd': return 8 + num;   // 8-15
        case 's': return 16 + num;  // 16-23
        default:  return NODE_NONE;
    }
}

uint16_t GraphBuilder::resolve_hardware_input(SymbolID sym_id) {
    const String& name = getSymbolString(sym_id);
    if (name == "in1")  return 0;
    if (name == "in2")  return 1;
    if (name == "ain1") return 2;
    if (name == "ain2") return 3;
    // knobs, etc. can be added later
    return NODE_NONE;
}

void GraphBuilder::skip_form(TokenStream& ts) {
    int depth = 0;
    while (!ts.at_end()) {
        Token tok = ts.consume();
        if (tok.kind == TokenKind::LParen || tok.kind == TokenKind::LBracket) depth++;
        if (tok.kind == TokenKind::RParen || tok.kind == TokenKind::RBracket) {
            if (depth <= 0) return;
            depth--;
        }
        if (depth == 0 && tok.kind != TokenKind::LParen && tok.kind != TokenKind::LBracket) return;
    }
}

// ── Temporal Templates ──────────────────────────────────────────────────────

uint16_t GraphBuilder::expand_beat(TimeContext& ctx) {
    uint16_t bpm = pool.make_cell_load(sym.bpm);
    uint16_t rate = pool.make_binop(NodeOp::Div, bpm, pool.make_const(60.0));
    uint16_t phase = pool.make_binop(NodeOp::Mul, ctx.t_node, rate);
    return pool.make_binop(NodeOp::Fmod, phase, pool.make_const(1.0));
}

uint16_t GraphBuilder::expand_bar(TimeContext& ctx) {
    uint16_t bpm = pool.make_cell_load(sym.bpm);
    uint16_t bpb = pool.make_cell_load(sym.beats_per_bar);
    uint16_t rate = pool.make_binop(NodeOp::Div,
                        pool.make_binop(NodeOp::Div, bpm, pool.make_const(60.0)), bpb);
    uint16_t phase = pool.make_binop(NodeOp::Mul, ctx.t_node, rate);
    return pool.make_binop(NodeOp::Fmod, phase, pool.make_const(1.0));
}

uint16_t GraphBuilder::expand_phrase(TimeContext& ctx) {
    uint16_t bpm = pool.make_cell_load(sym.bpm);
    uint16_t bpb = pool.make_cell_load(sym.beats_per_bar);
    uint16_t bpp = pool.make_cell_load(sym.bars_per_phrase);
    uint16_t rate = pool.make_binop(NodeOp::Div,
                        pool.make_binop(NodeOp::Div,
                            pool.make_binop(NodeOp::Div, bpm, pool.make_const(60.0)), bpb), bpp);
    uint16_t phase = pool.make_binop(NodeOp::Mul, ctx.t_node, rate);
    return pool.make_binop(NodeOp::Fmod, phase, pool.make_const(1.0));
}

uint16_t GraphBuilder::expand_section(TimeContext& ctx) {
    uint16_t bpm = pool.make_cell_load(sym.bpm);
    uint16_t bpb = pool.make_cell_load(sym.beats_per_bar);
    uint16_t bpp = pool.make_cell_load(sym.bars_per_phrase);
    uint16_t pps = pool.make_cell_load(sym.phrases_per_section);
    uint16_t rate = pool.make_binop(NodeOp::Div,
                        pool.make_binop(NodeOp::Div,
                            pool.make_binop(NodeOp::Div,
                                pool.make_binop(NodeOp::Div, bpm, pool.make_const(60.0)), bpb), bpp), pps);
    uint16_t phase = pool.make_binop(NodeOp::Mul, ctx.t_node, rate);
    return pool.make_binop(NodeOp::Fmod, phase, pool.make_const(1.0));
}

uint16_t GraphBuilder::expand_beat_num(TimeContext& ctx) {
    uint16_t bpm = pool.make_cell_load(sym.bpm);
    uint16_t rate = pool.make_binop(NodeOp::Div, bpm, pool.make_const(60.0));
    uint16_t count = pool.make_binop(NodeOp::Mul, ctx.t_node, rate);
    return pool.make_unary(NodeOp::Floor, count);
}

uint16_t GraphBuilder::expand_bar_num(TimeContext& ctx) {
    uint16_t bpm = pool.make_cell_load(sym.bpm);
    uint16_t bpb = pool.make_cell_load(sym.beats_per_bar);
    uint16_t rate = pool.make_binop(NodeOp::Div,
                        pool.make_binop(NodeOp::Div, bpm, pool.make_const(60.0)), bpb);
    uint16_t count = pool.make_binop(NodeOp::Mul, ctx.t_node, rate);
    return pool.make_unary(NodeOp::Floor, count);
}

// ── Expression Compilation ──────────────────────────────────────────────────

uint16_t GraphBuilder::compile_expr(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    if (has_error) return NODE_NONE;

    Token tok = ts.peek();

    // Number literal
    if (tok.kind == TokenKind::Number) {
        ts.consume();
        return pool.make_const(tok.number);
    }

    // Symbol
    if (tok.kind == TokenKind::Symbol) {
        ts.consume();
        return compile_symbol(tok.symbol, scope, ctx, tok.span_start, tok.span_len);
    }

    // Vector literal [1 2 3]
    if (tok.kind == TokenKind::LBracket) {
        return compile_vector_literal(ts, scope, ctx);
    }

    // List form (op args...)
    if (tok.kind == TokenKind::LParen) {
        ts.consume(); // eat '('
        Token op_tok = ts.peek();
        if (op_tok.kind == TokenKind::RParen) {
            ts.consume();
            return pool.make_const(0.0); // empty list
        }
        ts.consume();
        if (op_tok.kind != TokenKind::Symbol) {
            return report_error(op_tok, "Expected a function name after '('",
                                "Try: (sin (* t 440))");
        }
        uint16_t result = compile_form(op_tok.symbol, ts, scope, ctx, op_tok);
        // If compile_form errored, skip any unconsumed args to avoid hangs
        if (has_error) {
            while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) {
                if (ts.peek().kind == TokenKind::LParen) {
                    skip_form(ts);
                } else {
                    ts.consume();
                }
            }
        }
        ts.expect(TokenKind::RParen);
        return result;
    }

    if (tok.kind == TokenKind::Eof) {
        return report_error(tok, "Unexpected end of expression",
                            "Expression seems incomplete");
    }

    return report_error(tok, "Unexpected token",
                        "Expected a number, name, or '('");
}

// ── Symbol Resolution ───────────────────────────────────────────────────────

uint16_t GraphBuilder::compile_symbol(SymbolID sym_id, Scope& scope,
                                       TimeContext& ctx,
                                       uint16_t span_start, uint16_t span_len) {
    // 1. Local scope
    if (auto local = scope.find(sym_id)) {
        return local->node_index;
    }

    // 2. Raw time input
    if (sym_id == sym.t) return ctx.t_node;

    // 3. Well-known temporal templates
    if (sym_id == sym.beat)     return expand_beat(ctx);
    if (sym_id == sym.bar)      return expand_bar(ctx);
    if (sym_id == sym.phrase)   return expand_phrase(ctx);
    if (sym_id == sym.section)  return expand_section(ctx);
    if (sym_id == sym.beat_num) return expand_beat_num(ctx);
    if (sym_id == sym.bar_num)  return expand_bar_num(ctx);

    // 4. Hardware inputs
    uint16_t input_idx = resolve_hardware_input(sym_id);
    if (input_idx != NODE_NONE) {
        return pool.make_input_load(input_idx);
    }

    // 5. Cell table
    if (sym_id < MAX_CELLS) {
        const Cell& cell = cells.cells[sym_id];
        switch (cell.kind) {
            case CellKind::Number:
                add_dependency(sym_id);
                return pool.make_const(cell.value);

            case CellKind::Data:
                add_dependency(sym_id);
                return pool.make_const(cell.value); // length

            case CellKind::Callable: {
                const CallableInfo& info = cells.callables[sym_id];
                if (info.param_count == 0) {
                    // Expression cell — inline the body
                    add_dependency(sym_id);
                    return inline_expression_cell(sym_id, info, scope, ctx);
                }
                // Function with params — not valid as bare symbol
                return report_error_at(span_start, span_len,
                    "This is a function — it needs arguments",
                    "Try calling it: (name arg1 arg2)");
            }

            case CellKind::Empty:
                return report_error_with_fuzzy_match(sym_id, span_start, span_len);

            default:
                return pool.make_const(0.0); // nil
        }
    }

    return report_error_with_fuzzy_match(sym_id, span_start, span_len);
}

// ── Inline Expression Cell ──────────────────────────────────────────────────

uint16_t GraphBuilder::inline_expression_cell(SymbolID sym_id, const CallableInfo& info,
                                               Scope& scope, TimeContext& ctx) {
    if (is_in_inline_stack(sym_id)) {
        return report_error_at(0, 0,
            "This definition references itself — recursive definitions can't be used in outputs",
            "Try using 'for' over a fixed collection instead");
    }
    push_inline_stack(sym_id);

    const char* body_src = source.read(info.source_offset);
    if (!body_src) {
        pop_inline_stack();
        return pool.make_const(0.0);
    }

    Token body_tokens[MAX_TOKENS];
    uint8_t parse_errors = 0;
    uint16_t body_count = TokenStream::tokenize(body_src, info.source_length,
                                                 body_tokens, MAX_TOKENS,
                                                 nullptr, &parse_errors);

    TokenStream body_ts;
    memcpy(body_ts.tokens, body_tokens, body_count * sizeof(Token));
    body_ts.count = body_count;
    body_ts.pos = 0;

    uint16_t result = compile_expr(body_ts, scope, ctx);
    pop_inline_stack();
    return result;
}

// ── Form Compilation ────────────────────────────────────────────────────────

uint16_t GraphBuilder::compile_form(SymbolID op, TokenStream& ts,
                                     Scope& scope, TimeContext& ctx, Token op_tok) {
    // Time transforms
    if (op == sym.fast)   return compile_fast(ts, scope, ctx);
    if (op == sym.slow)   return compile_slow(ts, scope, ctx);
    if (op == sym.offset || op == sym.shift) return compile_offset(ts, scope, ctx);
    if (op == sym.loop_at) return compile_loop_at(ts, scope, ctx);
    if (op == sym.eval_at_time) return compile_eval_at_time(ts, scope, ctx);

    // Control flow
    if (op == sym.if_)    return compile_if(ts, scope, ctx);
    if (op == sym.let_)   return compile_let(ts, scope, ctx);
    if (op == sym.do_ || op == sym.scope) return compile_do(ts, scope, ctx);
    if (op == sym.for_)   return compile_for(ts, scope, ctx);
    if (op == sym.while_) return compile_while_gate(ts, scope, ctx);
    if (op == sym.fn || op == sym.lambda) return compile_lambda(ts, scope, ctx);

    // Side effects → compile-time error in signal context
    if (is_side_effect_form(op)) {
        // Skip remaining args so the parser doesn't hang
        while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) {
            if (ts.peek().kind == TokenKind::LParen) {
                skip_form(ts);
            } else {
                ts.consume();
            }
        }
        return report_error(op_tok,
            "This can't be used inside an output expression",
            "Use it at the top level instead");
    }

    // Variadic arithmetic
    if (is_arithmetic_op(op)) return compile_variadic_arithmetic(op, ts, scope, ctx);

    // Comparison
    if (is_comparison_op(op)) return compile_comparison(op, ts, scope, ctx);

    // Logic
    if (is_logic_op(op)) return compile_logic(op, ts, scope, ctx);

    // tri and sqr: accept 1 or 2 arguments
    // With 2 args, use the second as the phase (ignore first).
    // With 1 arg, use it as the phase.
    if (op == sym.tri || op == sym.sqr) {
        NodeOp nop = (op == sym.tri) ? NodeOp::Tri : NodeOp::Sqr;
        uint16_t first = compile_expr(ts, scope, ctx);
        if (ts.peek().kind != TokenKind::RParen) {
            // Two arguments: ignore first, use second as phase
            uint16_t second = compile_expr(ts, scope, ctx);
            return pool.make_unary(nop, second);
        }
        return pool.make_unary(nop, first);
    }

    // (input N) — hardware input channel
    if (op == sym.input) {
        uint16_t arg = compile_expr(ts, scope, ctx);
        if (is_const(arg)) {
            uint16_t ch = (uint16_t)const_value(arg);
            return pool.make_input_load(ch);
        }
        return report_error(op_tok,
            "input channel must be a constant",
            "Try: (input 0) or (input 1)");
    }

    // Unary math
    if (is_unary_math(op)) return compile_unary_math(unary_sym_to_op(op), ts, scope, ctx);

    // Binary math
    if (is_binary_math(op)) {
        NodeOp nop = NodeOp::Min;
        if (op == sym.min_)  nop = NodeOp::Min;
        if (op == sym.max_)  nop = NodeOp::Max;
        if (op == sym.pow_)  nop = NodeOp::Pow;
        if (op == sym.mod_)  nop = NodeOp::Mod;
        if (op == sym.pulse) nop = NodeOp::Pulse;
        return compile_binary_math(nop, ts, scope, ctx);
    }

    // Ternary math
    if (is_ternary_math(op)) {
        NodeOp nop = NodeOp::Clamp;
        if (op == sym.clamp) nop = NodeOp::Clamp;
        if (op == sym.lerp)  nop = NodeOp::Lerp;
        if (op == sym.scale) nop = NodeOp::Scale;
        return compile_ternary_math(nop, ts, scope, ctx);
    }

    // Range
    if (op == sym.range) return compile_range(ts, scope, ctx);

    // Domain-specific signal functions
    if (op == sym.step) return compile_step(ts, scope, ctx);
    if (op == sym.gates || op == sym.trigs) return compile_gates(op, ts, scope, ctx);
    if (op == sym.euclid || op == sym.eu) return compile_euclid(ts, scope, ctx);
    if (op == sym.seq || op == sym.from_list) return compile_seq(ts, scope, ctx);
    if (op == sym.interp || op == sym.flatseq) return compile_interp(ts, scope, ctx);
    if (op == sym.dm) return compile_dm(ts, scope, ctx);
    if (op == sym.gatesw) return compile_gatesw(ts, scope, ctx);

    // Ratio-rhythm functions
    if (op == sym.rpulse) return compile_rpulse(ts, scope, ctx);
    if (op == sym.rstep) return compile_rstep(ts, scope, ctx);
    if (op == sym.ridx) return compile_ridx(ts, scope, ctx);
    if (op == sym.rwarp) return compile_rwarp(ts, scope, ctx);

    // Random / hash
    if (op == sym.random_) return compile_random(ts, scope, ctx);
    if (op == sym.index_rand) return compile_index_rand(ts, scope, ctx);

    // Quote in signal context
    if (op == sym.quote) {
        // Skip the quoted form
        while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) {
            if (ts.peek().kind == TokenKind::LParen) {
                skip_form(ts);
            } else {
                ts.consume();
            }
        }
        return report_error(op_tok,
            "'quote' can't be used inside an output expression",
            "Use a literal vector instead: [1 0 1 0]");
    }

    // User-defined function call
    return compile_call(op, ts, scope, ctx, op_tok);
}

// ── Time Transforms ─────────────────────────────────────────────────────────

uint16_t GraphBuilder::compile_fast(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    uint16_t factor = compile_expr(ts, scope, ctx);
    TimeContext inner = { pool.make_binop(NodeOp::Mul, ctx.t_node, factor) };
    return compile_expr(ts, scope, inner);
}

uint16_t GraphBuilder::compile_slow(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    uint16_t factor = compile_expr(ts, scope, ctx);
    TimeContext inner = { pool.make_binop(NodeOp::Div, ctx.t_node, factor) };
    return compile_expr(ts, scope, inner);
}

uint16_t GraphBuilder::compile_offset(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    uint16_t amount = compile_expr(ts, scope, ctx);
    TimeContext inner = { pool.make_binop(NodeOp::Add, ctx.t_node, amount) };
    return compile_expr(ts, scope, inner);
}

uint16_t GraphBuilder::compile_loop_at(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    uint16_t duration = compile_expr(ts, scope, ctx);
    TimeContext inner = { pool.make_binop(NodeOp::Fmod, ctx.t_node, duration) };
    return compile_expr(ts, scope, inner);
}

uint16_t GraphBuilder::compile_eval_at_time(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    uint16_t time_node = compile_expr(ts, scope, ctx);
    TimeContext inner = { time_node };
    return compile_expr(ts, scope, inner);
}

// ── Control Flow ────────────────────────────────────────────────────────────

uint16_t GraphBuilder::compile_if(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    uint16_t cond = compile_expr(ts, scope, ctx);
    uint16_t then_val = compile_expr(ts, scope, ctx);
    uint16_t else_val = pool.make_const(0.0);
    if (ts.peek().kind != TokenKind::RParen) {
        else_val = compile_expr(ts, scope, ctx);
    }
    return pool.make_select(cond, then_val, else_val);
}

uint16_t GraphBuilder::compile_let(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (let [name1 val1 name2 val2 ...] body)
    // or (let ((name1 val1) (name2 val2) ...) body)
    Scope inner = {};
    inner.parent = &scope;

    Token tok = ts.peek();
    if (tok.kind == TokenKind::LBracket) {
        ts.consume(); // eat '['
        while (ts.peek().kind != TokenKind::RBracket && !ts.at_end()) {
            Token name_tok = ts.consume();
            if (name_tok.kind != TokenKind::Symbol) {
                return report_error(name_tok, "Expected a name in let binding",
                                    "Try: (let [x 1 y 2] (+ x y))");
            }
            uint16_t val = compile_expr(ts, scope, ctx);
            inner.bind(name_tok.symbol, val);
        }
        ts.expect(TokenKind::RBracket);
    } else if (tok.kind == TokenKind::LParen) {
        ts.consume(); // eat '('
        // Detect flat vs nested binding format:
        // Flat:   (let (x 1 y 2) body)     — first element is a symbol, second is NOT '('
        // Nested: (let ((x 1) (y 2)) body) — first element is '('
        Token first = ts.peek();
        if (first.kind == TokenKind::Symbol) {
            // Flat binding list: (name1 val1 name2 val2 ...)
            while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) {
                Token name_tok = ts.consume();
                if (name_tok.kind != TokenKind::Symbol) {
                    return report_error(name_tok, "Expected a name in let binding",
                                        "Try: (let (x 1 y 2) (+ x y))");
                }
                uint16_t val = compile_expr(ts, scope, ctx);
                inner.bind(name_tok.symbol, val);
            }
        } else {
            // Nested binding list: ((name1 val1) (name2 val2) ...)
            while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) {
                ts.expect(TokenKind::LParen);
                Token name_tok = ts.consume();
                if (name_tok.kind != TokenKind::Symbol) {
                    return report_error(name_tok, "Expected a name in let binding",
                                        "Try: (let ((x 1) (y 2)) (+ x y))");
                }
                uint16_t val = compile_expr(ts, scope, ctx);
                inner.bind(name_tok.symbol, val);
                ts.expect(TokenKind::RParen);
            }
        }
        ts.expect(TokenKind::RParen);
    }

    // Compile body expressions, return last
    uint16_t result = pool.make_const(0.0);
    while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) {
        result = compile_expr(ts, inner, ctx);
    }
    return result;
}

uint16_t GraphBuilder::compile_do(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    uint16_t result = pool.make_const(0.0);
    while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) {
        result = compile_expr(ts, scope, ctx);
    }
    return result;
}

uint16_t GraphBuilder::compile_for(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (for var collection body)
    Token var_tok = ts.consume();
    if (var_tok.kind != TokenKind::Symbol) {
        return report_error(var_tok, "'for' needs a variable name",
                            "Try: (for x [1 2 3] (* x 2))");
    }
    SymbolID var = var_tok.symbol;

    Collection col = resolve_collection(ts, scope, ctx);

    uint16_t body_start = ts.pos;

    if (!col.ok) {
        // Skip past body
        skip_form(ts);
        return report_error(var_tok,
            "for's collection couldn't be resolved at compile time",
            "Try using a literal vector: (for x [1 2 3 4] body)");
    }

    if (col.count == 0) {
        skip_form(ts);
        return pool.make_const(0.0);
    }

    uint16_t result = pool.make_const(0.0);
    for (uint16_t i = 0; i < col.count; i++) {
        Scope iter_scope = {};
        iter_scope.parent = &scope;
        iter_scope.bind(var, col.element_nodes[i]);

        ts.rewind(body_start);
        result = compile_expr(ts, iter_scope, ctx);
    }

    return result;
}

uint16_t GraphBuilder::compile_while_gate(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (while cond body) — returns body when cond is true, 0.0 when false
    uint16_t cond = compile_expr(ts, scope, ctx);
    uint16_t value = compile_expr(ts, scope, ctx);
    return pool.make_select(cond, value, pool.make_const(0.0));
}

uint16_t GraphBuilder::compile_lambda(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // Lambda in signal context is not supported as a value
    // but if immediately applied, could work. For now, error.
    return report_error_at(0, 0,
        "Lambda expressions can't be used directly in outputs",
        "Define the function with 'defn' and call it by name");
}

// ── User-defined function call ──────────────────────────────────────────────

uint16_t GraphBuilder::compile_call(SymbolID fn_sym, TokenStream& ts,
                                     Scope& scope, TimeContext& ctx, Token op_tok) {
    if (fn_sym >= MAX_CELLS) {
        return report_error_with_fuzzy_match(fn_sym, op_tok.span_start, op_tok.span_len);
    }

    const Cell& cell = cells.cells[fn_sym];
    if (cell.kind != CellKind::Callable) {
        return report_error_with_fuzzy_match(fn_sym, op_tok.span_start, op_tok.span_len);
    }

    const CallableInfo& info = cells.callables[fn_sym];
    add_dependency(fn_sym);

    // Recursion guard
    if (is_in_inline_stack(fn_sym)) {
        return report_error(op_tok,
            "This function calls itself — recursive functions can't be used in outputs",
            "Try using 'for' over a fixed collection instead");
    }
    push_inline_stack(fn_sym);

    // Compile arguments
    uint16_t arg_nodes[MAX_CALLABLE_PARAMS];
    uint8_t arg_count = 0;
    while (ts.peek().kind != TokenKind::RParen && arg_count < info.param_count && !ts.at_end()) {
        arg_nodes[arg_count++] = compile_expr(ts, scope, ctx);
    }

    if (arg_count != info.param_count) {
        pop_inline_stack();
        return report_error(op_tok,
            "Wrong number of arguments",
            "Check the function definition");
    }

    // Create local scope with param bindings
    Scope inner_scope = {};
    inner_scope.parent = &scope;
    for (uint8_t i = 0; i < arg_count; i++) {
        inner_scope.bind(info.params[i], arg_nodes[i]);
    }

    // Tokenize callable body from source arena
    const char* body_src = source.read(info.source_offset);
    if (!body_src) {
        pop_inline_stack();
        return pool.make_const(0.0);
    }

    Token body_tokens[MAX_TOKENS];
    uint8_t parse_errors = 0;
    uint16_t body_count = TokenStream::tokenize(body_src, info.source_length,
                                                 body_tokens, MAX_TOKENS,
                                                 nullptr, &parse_errors);

    TokenStream body_ts;
    memcpy(body_ts.tokens, body_tokens, body_count * sizeof(Token));
    body_ts.count = body_count;
    body_ts.pos = 0;

    uint16_t result = compile_expr(body_ts, inner_scope, ctx);
    pop_inline_stack();
    return result;
}

// ── Arithmetic ──────────────────────────────────────────────────────────────

uint16_t GraphBuilder::compile_variadic_arithmetic(SymbolID op, TokenStream& ts,
                                                    Scope& scope, TimeContext& ctx) {
    NodeOp nop = arithmetic_sym_to_op(op);

    // First argument
    uint16_t result = compile_expr(ts, scope, ctx);

    // Handle unary minus: (- x) → negate
    if (ts.peek().kind == TokenKind::RParen && nop == NodeOp::Sub) {
        return pool.make_unary(NodeOp::Neg, result);
    }

    // Left-fold remaining arguments
    while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) {
        uint16_t rhs = compile_expr(ts, scope, ctx);
        result = pool.make_binop(nop, result, rhs);
    }

    return result;
}

uint16_t GraphBuilder::compile_comparison(SymbolID op, TokenStream& ts,
                                           Scope& scope, TimeContext& ctx) {
    NodeOp nop = comparison_sym_to_op(op);
    uint16_t a = compile_expr(ts, scope, ctx);
    uint16_t b = compile_expr(ts, scope, ctx);
    return pool.make_binop(nop, a, b);
}

uint16_t GraphBuilder::compile_logic(SymbolID op, TokenStream& ts,
                                      Scope& scope, TimeContext& ctx) {
    if (op == sym.not_) {
        uint16_t a = compile_expr(ts, scope, ctx);
        return pool.make_unary(NodeOp::Not, a);
    }
    NodeOp nop = (op == sym.and_) ? NodeOp::And : NodeOp::Or;
    uint16_t a = compile_expr(ts, scope, ctx);
    uint16_t b = compile_expr(ts, scope, ctx);
    return pool.make_binop(nop, a, b);
}

uint16_t GraphBuilder::compile_unary_math(NodeOp op, TokenStream& ts,
                                           Scope& scope, TimeContext& ctx) {
    uint16_t a = compile_expr(ts, scope, ctx);
    return pool.make_unary(op, a);
}

uint16_t GraphBuilder::compile_binary_math(NodeOp op, TokenStream& ts,
                                            Scope& scope, TimeContext& ctx) {
    uint16_t a = compile_expr(ts, scope, ctx);
    uint16_t b = compile_expr(ts, scope, ctx);
    return pool.make_binop(op, a, b);
}

uint16_t GraphBuilder::compile_ternary_math(NodeOp op, TokenStream& ts,
                                             Scope& scope, TimeContext& ctx) {
    uint16_t a = compile_expr(ts, scope, ctx);
    uint16_t b = compile_expr(ts, scope, ctx);
    uint16_t c = compile_expr(ts, scope, ctx);
    return pool.make_ternary(op, a, b, c);
}

// ── Domain-Specific Signal Functions ────────────────────────────────────────

uint16_t GraphBuilder::compile_step(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    DataRef data = resolve_data_table(ts, scope, ctx);
    if (!data.ok) return NODE_NONE;

    uint16_t phase;
    if (ts.peek().kind != TokenKind::RParen) {
        phase = compile_expr(ts, scope, ctx);
    } else {
        phase = expand_beat(ctx);
    }

    uint16_t len = pool.make_const((double)data.length);
    uint16_t scaled = pool.make_binop(NodeOp::Mul, phase, len);
    uint16_t idx = pool.make_unary(NodeOp::Floor, scaled);
    Node n;
    n.op = NodeOp::VecIndex;
    n.input_a = idx;
    n.imm = (double)data.table_id;
    n.flags = 0;
    return pool.intern_node(n);
}

uint16_t GraphBuilder::compile_gates(SymbolID op, TokenStream& ts,
                                      Scope& scope, TimeContext& ctx) {
    // gates/trigs use step internally, then threshold
    DataRef data = resolve_data_table(ts, scope, ctx);
    if (!data.ok) return NODE_NONE;

    uint16_t phase;
    if (ts.peek().kind != TokenKind::RParen) {
        phase = compile_expr(ts, scope, ctx);
    } else {
        phase = expand_beat(ctx);
    }

    uint16_t len = pool.make_const((double)data.length);
    uint16_t scaled = pool.make_binop(NodeOp::Mul, phase, len);
    uint16_t idx = pool.make_unary(NodeOp::Floor, scaled);
    Node vec_node;
    vec_node.op = NodeOp::VecIndex;
    vec_node.input_a = idx;
    vec_node.imm = (double)data.table_id;
    vec_node.flags = 0;
    uint16_t raw = pool.intern_node(vec_node);

    // gates: value > 0
    return pool.make_binop(NodeOp::CmpGt, raw, pool.make_const(0.0));
}

uint16_t GraphBuilder::compile_euclid(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (euclid total active phase)
    // Matches old engine: idx = (step * active) % total; hit if idx < active AND rem < pw
    uint16_t total = compile_expr(ts, scope, ctx);
    uint16_t active = compile_expr(ts, scope, ctx);

    // Optional pulse-width argument (default 0.5)
    uint16_t pulse_width;
    uint16_t phase;
    if (ts.peek().kind != TokenKind::RParen) {
        uint16_t arg3 = compile_expr(ts, scope, ctx);
        if (ts.peek().kind != TokenKind::RParen) {
            // 4 args: (euclid total active pulseWidth phase)
            pulse_width = arg3;
            phase = compile_expr(ts, scope, ctx);
        } else {
            // 3 args: (euclid total active phase)
            pulse_width = pool.make_const(0.5);
            phase = arg3;
        }
    } else {
        pulse_width = pool.make_const(0.5);
        phase = expand_beat(ctx);
    }

    // scaled = phase * total
    uint16_t scaled = pool.make_binop(NodeOp::Mul, phase, total);

    // step_idx = min(floor(scaled), total - 1)  — clamp for phase=1.0 edge case
    uint16_t floored = pool.make_unary(NodeOp::Floor, scaled);
    uint16_t max_idx = pool.make_binop(NodeOp::Sub, total, pool.make_const(1.0));
    uint16_t step_idx = pool.make_binop(NodeOp::Min, floored, max_idx);

    // rem = scaled - step_idx  (fractional position within current step)
    uint16_t rem = pool.make_binop(NodeOp::Sub, scaled, step_idx);

    // idx = (step_idx * active) % total
    uint16_t product = pool.make_binop(NodeOp::Mul, step_idx, active);
    uint16_t idx = pool.make_binop(NodeOp::Fmod, product, total);

    // hit = idx < active
    uint16_t hit = pool.make_binop(NodeOp::CmpLt, idx, active);

    // gate = rem < pulse_width
    uint16_t gate = pool.make_binop(NodeOp::CmpLt, rem, pulse_width);

    // result = hit ? gate : 0
    return pool.make_select(hit, gate, pool.make_const(0.0));
}

uint16_t GraphBuilder::compile_seq(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (seq data phase) — same as step
    return compile_step(ts, scope, ctx);
}

uint16_t GraphBuilder::compile_interp(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    DataRef data = resolve_data_table(ts, scope, ctx);
    if (!data.ok) return NODE_NONE;

    uint16_t phase;
    if (ts.peek().kind != TokenKind::RParen) {
        phase = compile_expr(ts, scope, ctx);
    } else {
        phase = expand_beat(ctx);
    }

    Node n;
    n.op = NodeOp::VecLerp;
    n.input_a = phase;
    n.imm = (double)data.table_id;
    n.flags = 0;
    return pool.intern_node(n);
}

uint16_t GraphBuilder::compile_dm(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (dm condition default value)
    uint16_t cond = compile_expr(ts, scope, ctx);
    uint16_t default_val = compile_expr(ts, scope, ctx);
    uint16_t value = compile_expr(ts, scope, ctx);
    uint16_t is_true = pool.make_binop(NodeOp::CmpGt, cond, pool.make_const(0.0));
    return pool.make_select(is_true, value, default_val);
}

uint16_t GraphBuilder::compile_gatesw(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (gatesw data phase) — gate with width encoding
    // Pattern values 1-9 control pulse width (value/9.0). Output is binary.
    DataRef data = resolve_data_table(ts, scope, ctx);
    if (!data.ok) return NODE_NONE;

    uint16_t phase;
    if (ts.peek().kind != TokenKind::RParen) {
        phase = compile_expr(ts, scope, ctx);
    } else {
        phase = expand_beat(ctx);
    }

    uint16_t len = pool.make_const((double)data.length);
    uint16_t scaled = pool.make_binop(NodeOp::Mul, phase, len);
    uint16_t idx = pool.make_unary(NodeOp::Floor, scaled);

    // Read pattern value via VecIndex
    Node vec_node;
    vec_node.op = NodeOp::VecIndex;
    vec_node.input_a = idx;
    vec_node.input_b = NODE_NONE;
    vec_node.input_c = NODE_NONE;
    vec_node.imm = (double)data.table_id;
    vec_node.flags = 0;
    uint16_t raw = pool.intern_node(vec_node);

    // Width = value / 9.0
    uint16_t width = pool.make_binop(NodeOp::Div, raw, pool.make_const(9.0));

    // Fractional phase within step
    uint16_t frac_phase = pool.make_binop(NodeOp::Sub, scaled, idx);

    // Output: 1 if frac_phase < width, 0 otherwise
    return pool.make_binop(NodeOp::CmpLt, frac_phase, width);
}

// ── Ratio-Rhythm Functions ──────────────────────────────────────────────────

// Helper: resolve a data table and read its raw constant values.
// Computes cumulative normalized boundaries into out_cum.
static bool resolve_ratio_table(GraphBuilder& gb, TokenStream& ts, Scope& scope,
                                 TimeContext& ctx, double* out_values, double* out_cum,
                                 uint16_t& out_count, double& out_total) {
    auto data = gb.resolve_data_table(ts, scope, ctx);
    if (!data.ok || data.length == 0) return false;

    uint16_t len = 0;
    const double* raw = gb.cells.get_data_table(data.table_id, len);
    if (!raw || len == 0) return false;

    out_count = len;
    out_total = 0.0;
    for (uint16_t i = 0; i < len; i++) {
        out_values[i] = raw[i];
        out_total += raw[i];
    }
    if (out_total == 0.0) return false;

    double acc = 0.0;
    for (uint16_t i = 0; i < len; i++) {
        acc += out_values[i];
        out_cum[i] = acc / out_total;
    }
    return true;
}

uint16_t GraphBuilder::compile_ridx(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (ridx ratios phase)
    // Returns index / ratios.size() (normalized index of current subdivision)
    double values[64], cum[64];
    uint16_t count = 0;
    double total = 0.0;

    if (!resolve_ratio_table(*this, ts, scope, ctx, values, cum, count, total)) {
        return report_error_at(0, 0,
            "ridx needs a non-empty ratio vector",
            "Try: (ridx [1 2 1] beat)");
    }

    uint16_t phase;
    if (ts.peek().kind != TokenKind::RParen) {
        phase = compile_expr(ts, scope, ctx);
    } else {
        phase = expand_beat(ctx);
    }

    // Build nested Select chain:
    //   if (phase <= cum[0]) => 0/N
    //   elif (phase <= cum[1]) => 1/N
    //   else => (N-1)/N
    double n = (double)count;
    uint16_t result = pool.make_const((double)(count - 1) / n);

    for (int i = (int)count - 2; i >= 0; i--) {
        uint16_t boundary = pool.make_const(cum[i]);
        uint16_t cmp = pool.make_binop(NodeOp::CmpLe, phase, boundary);
        uint16_t this_val = pool.make_const((double)i / n);
        result = pool.make_select(cmp, this_val, result);
    }

    return result;
}

uint16_t GraphBuilder::compile_rstep(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (rstep ratios phase)
    // Returns lastAccumulatedSum / ratioSum (normalized start of current subdivision)
    double values[64], cum[64];
    uint16_t count = 0;
    double total = 0.0;

    if (!resolve_ratio_table(*this, ts, scope, ctx, values, cum, count, total)) {
        return report_error_at(0, 0,
            "rstep needs a non-empty ratio vector",
            "Try: (rstep [1 2 1] beat)");
    }

    uint16_t phase;
    if (ts.peek().kind != TokenKind::RParen) {
        phase = compile_expr(ts, scope, ctx);
    } else {
        phase = expand_beat(ctx);
    }

    // Build nested Select chain:
    //   if (phase <= cum[0]) => 0.0
    //   elif (phase <= cum[1]) => cum[0]
    //   else => cum[N-2]
    uint16_t result = pool.make_const(count >= 2 ? cum[count - 2] : 0.0);

    for (int i = (int)count - 2; i >= 0; i--) {
        uint16_t boundary = pool.make_const(cum[i]);
        uint16_t cmp = pool.make_binop(NodeOp::CmpLe, phase, boundary);
        double start_val = (i == 0) ? 0.0 : cum[i - 1];
        uint16_t this_val = pool.make_const(start_val);
        result = pool.make_select(cmp, this_val, result);
    }

    return result;
}

uint16_t GraphBuilder::compile_rpulse(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (rpulse ratios pulseWidth phase)
    // Within current subdivision, compute local beat phase and compare to pulseWidth.
    double values[64], cum[64];
    uint16_t count = 0;
    double total = 0.0;

    if (!resolve_ratio_table(*this, ts, scope, ctx, values, cum, count, total)) {
        return report_error_at(0, 0,
            "rpulse needs a non-empty ratio vector",
            "Try: (rpulse [1 2 1] 0.5 beat)");
    }

    uint16_t pulse_width = compile_expr(ts, scope, ctx);

    uint16_t phase;
    if (ts.peek().kind != TokenKind::RParen) {
        phase = compile_expr(ts, scope, ctx);
    } else {
        phase = expand_beat(ctx);
    }

    // For each subdivision [start, end):
    //   local_phase = (phase - start) / (end - start)
    //   hit = local_phase <= pulseWidth
    auto make_local_pulse = [&](int i) -> uint16_t {
        double start = (i == 0) ? 0.0 : cum[i - 1];
        double end = cum[i];
        double width = end - start;
        if (width <= 0.0) return pool.make_const(0.0);
        uint16_t offset_phase = pool.make_binop(NodeOp::Sub, phase, pool.make_const(start));
        uint16_t local_phase = pool.make_binop(NodeOp::Div, offset_phase, pool.make_const(width));
        return pool.make_binop(NodeOp::CmpLe, local_phase, pulse_width);
    };

    uint16_t result = make_local_pulse(count - 1);
    for (int i = (int)count - 2; i >= 0; i--) {
        uint16_t boundary = pool.make_const(cum[i]);
        uint16_t cmp = pool.make_binop(NodeOp::CmpLe, phase, boundary);
        uint16_t this_val = make_local_pulse(i);
        result = pool.make_select(cmp, this_val, result);
    }

    return result;
}

uint16_t GraphBuilder::compile_rwarp(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (rwarp ratios phase)
    // Remap phase through ratio boundaries to uniform spacing.
    // output = (index + beatPhase) / N
    double values[64], cum[64];
    uint16_t count = 0;
    double total = 0.0;

    if (!resolve_ratio_table(*this, ts, scope, ctx, values, cum, count, total)) {
        return report_error_at(0, 0,
            "rwarp needs a non-empty ratio vector",
            "Try: (rwarp [1 2 1] beat)");
    }

    uint16_t phase;
    if (ts.peek().kind != TokenKind::RParen) {
        phase = compile_expr(ts, scope, ctx);
    } else {
        phase = expand_beat(ctx);
    }

    double n = (double)count;
    double iw = 1.0 / n;

    auto make_warped = [&](int i) -> uint16_t {
        double start = (i == 0) ? 0.0 : cum[i - 1];
        double end = cum[i];
        double width = end - start;
        if (width <= 0.0) return pool.make_const((double)i * iw);
        uint16_t offset_phase = pool.make_binop(NodeOp::Sub, phase, pool.make_const(start));
        uint16_t local_phase = pool.make_binop(NodeOp::Div, offset_phase, pool.make_const(width));
        uint16_t idx_plus_local = pool.make_binop(NodeOp::Add, pool.make_const((double)i), local_phase);
        return pool.make_binop(NodeOp::Mul, idx_plus_local, pool.make_const(iw));
    };

    uint16_t result = make_warped(count - 1);
    for (int i = (int)count - 2; i >= 0; i--) {
        uint16_t boundary = pool.make_const(cum[i]);
        uint16_t cmp = pool.make_binop(NodeOp::CmpLe, phase, boundary);
        uint16_t this_val = make_warped(i);
        result = pool.make_select(cmp, this_val, result);
    }

    return result;
}

// -- Random / Hash -----------------------------------------------------------

uint16_t GraphBuilder::compile_random(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (random)       -> HashIndex(beat_num) -> [0,1]
    // (random lo hi) -> lo + HashIndex(beat_num) * (hi - lo)
    if (ts.peek().kind == TokenKind::RParen) {
        // No args: hash of beat_num
        uint16_t beat_num = expand_beat_num(ctx);
        return pool.make_unary(NodeOp::HashIndex, beat_num);
    }

    uint16_t lo = compile_expr(ts, scope, ctx);
    if (ts.peek().kind == TokenKind::RParen) {
        // One arg: treat as (random 0 lo)
        uint16_t beat_num = expand_beat_num(ctx);
        uint16_t raw = pool.make_unary(NodeOp::HashIndex, beat_num);
        return pool.make_binop(NodeOp::Mul, raw, lo);
    }

    uint16_t hi = compile_expr(ts, scope, ctx);
    // Two args: lo + hash * (hi - lo)
    uint16_t beat_num = expand_beat_num(ctx);
    uint16_t raw = pool.make_unary(NodeOp::HashIndex, beat_num);
    uint16_t range_node = pool.make_binop(NodeOp::Sub, hi, lo);
    uint16_t scaled = pool.make_binop(NodeOp::Mul, raw, range_node);
    return pool.make_binop(NodeOp::Add, lo, scaled);
}

uint16_t GraphBuilder::compile_index_rand(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (index-rand idx)       -> HashIndex(idx) -> [0,1]
    // (index-rand idx lo hi) -> lo + HashIndex(idx) * (hi - lo)
    uint16_t idx = compile_expr(ts, scope, ctx);

    if (ts.peek().kind == TokenKind::RParen) {
        // One arg: hash of idx
        return pool.make_unary(NodeOp::HashIndex, idx);
    }

    uint16_t lo = compile_expr(ts, scope, ctx);
    if (ts.peek().kind == TokenKind::RParen) {
        // Two args: treat as (index-rand idx 0 lo) -- hash * lo
        uint16_t raw = pool.make_unary(NodeOp::HashIndex, idx);
        return pool.make_binop(NodeOp::Mul, raw, lo);
    }

    uint16_t hi = compile_expr(ts, scope, ctx);
    // Three args: lo + hash * (hi - lo)
    uint16_t raw = pool.make_unary(NodeOp::HashIndex, idx);
    uint16_t range_node = pool.make_binop(NodeOp::Sub, hi, lo);
    uint16_t scaled = pool.make_binop(NodeOp::Mul, raw, range_node);
    return pool.make_binop(NodeOp::Add, lo, scaled);
}

uint16_t GraphBuilder::compile_range(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (range end) or (range start end) or (range start end step)
    // All arguments must fold to constants.
    uint16_t arg1 = compile_expr(ts, scope, ctx);

    double start = 0, end_val, step_val = 1;

    if (ts.peek().kind == TokenKind::RParen) {
        if (!is_const(arg1)) {
            return report_error_at(0, 0,
                "range needs values known at compile time",
                "Try: (range 1 8) or use a literal vector [1 2 3 4 5 6 7]");
        }
        end_val = const_value(arg1);
    } else {
        uint16_t arg2 = compile_expr(ts, scope, ctx);
        if (ts.peek().kind == TokenKind::RParen) {
            if (!is_const(arg1) || !is_const(arg2)) {
                return report_error_at(0, 0,
                    "range needs values known at compile time",
                    "Try: (range 1 8) or use a literal vector");
            }
            start = const_value(arg1);
            end_val = const_value(arg2);
        } else {
            uint16_t arg3 = compile_expr(ts, scope, ctx);
            if (!is_const(arg1) || !is_const(arg2) || !is_const(arg3)) {
                return report_error_at(0, 0,
                    "range needs values known at compile time",
                    "Try: (range 1 8) or use a literal vector");
            }
            start = const_value(arg1);
            end_val = const_value(arg2);
            step_val = const_value(arg3);
        }
    }

    // This is used as a collection producer (e.g., for `for` loops)
    // We return the last element as the scalar value
    // The collection is built in resolve_collection which calls compile_range
    double v = start;
    uint16_t last = pool.make_const(0.0);
    for (int i = 0; i < 64 && (step_val > 0 ? v < end_val : v > end_val); i++) {
        last = pool.make_const(v);
        v += step_val;
    }
    return last;
}

// ── Vector Literal ──────────────────────────────────────────────────────────

uint16_t GraphBuilder::compile_vector_literal(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    ts.consume(); // eat '['

    // Collect elements as doubles (for data table)
    double values[64];
    uint16_t count = 0;

    while (ts.peek().kind != TokenKind::RBracket && !ts.at_end() && count < 64) {
        uint16_t elem = compile_expr(ts, scope, ctx);
        if (is_const(elem)) {
            values[count++] = const_value(elem);
        } else {
            // Non-constant element — can't create a data table
            // For now, just use the last value
            values[count++] = 0.0;
        }
    }
    ts.expect(TokenKind::RBracket);

    // Store as data table
    // HACK: we need mutable access to the cell store for data tables
    // For now, return the length as a constant (the data table should be
    // set up by the cold path before the graph builder runs)
    return pool.make_const((double)count);
}

// ── Collection Resolution ───────────────────────────────────────────────────

GraphBuilder::Collection GraphBuilder::resolve_collection(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    Collection col;

    Token tok = ts.peek();

    // Literal vector [1 2 3]
    if (tok.kind == TokenKind::LBracket) {
        ts.consume(); // eat '['
        while (ts.peek().kind != TokenKind::RBracket && !ts.at_end() && col.count < 64) {
            uint16_t elem = compile_expr(ts, scope, ctx);
            col.element_nodes[col.count++] = elem;
        }
        ts.expect(TokenKind::RBracket);
        col.ok = true;
        return col;
    }

    // Symbol reference — look up in cell table
    if (tok.kind == TokenKind::Symbol) {
        ts.consume();
        SymbolID sym_id = tok.symbol;

        if (sym_id < MAX_CELLS) {
            const Cell& cell = cells.cells[sym_id];
            if (cell.kind == CellKind::Data) {
                add_dependency(sym_id);
                uint16_t len;
                const double* data = cells.get_data_table(cell.data_table_id, len);
                if (data) {
                    for (uint16_t i = 0; i < len && col.count < 64; i++) {
                        col.element_nodes[col.count++] = pool.make_const(data[i]);
                    }
                    col.ok = true;
                    return col;
                }
            }
        }

        col.ok = false;
        return col;
    }

    // Expression — compile it and check if it produced a range
    if (tok.kind == TokenKind::LParen) {
        // Check if it's a (range ...) call
        uint16_t saved_pos = ts.pos;
        ts.consume(); // eat '('
        Token fn_tok = ts.peek();
        if (fn_tok.kind == TokenKind::Symbol && fn_tok.symbol == sym.range) {
            ts.consume(); // eat 'range'

            // Parse range arguments
            uint16_t arg1 = compile_expr(ts, scope, ctx);
            double start = 0, end_val, step_val = 1;

            if (ts.peek().kind == TokenKind::RParen) {
                if (is_const(arg1)) {
                    end_val = const_value(arg1);
                    ts.expect(TokenKind::RParen);
                    for (double v = start; step_val > 0 ? v < end_val : v > end_val; v += step_val) {
                        if (col.count >= 64) break;
                        col.element_nodes[col.count++] = pool.make_const(v);
                    }
                    col.ok = true;
                    return col;
                }
            } else {
                uint16_t arg2 = compile_expr(ts, scope, ctx);
                if (ts.peek().kind == TokenKind::RParen) {
                    if (is_const(arg1) && is_const(arg2)) {
                        start = const_value(arg1);
                        end_val = const_value(arg2);
                        ts.expect(TokenKind::RParen);
                        for (double v = start; step_val > 0 ? v < end_val : v > end_val; v += step_val) {
                            if (col.count >= 64) break;
                            col.element_nodes[col.count++] = pool.make_const(v);
                        }
                        col.ok = true;
                        return col;
                    }
                } else {
                    uint16_t arg3 = compile_expr(ts, scope, ctx);
                    ts.expect(TokenKind::RParen);
                    if (is_const(arg1) && is_const(arg2) && is_const(arg3)) {
                        start = const_value(arg1);
                        end_val = const_value(arg2);
                        step_val = const_value(arg3);
                        for (double v = start; step_val > 0 ? v < end_val : v > end_val; v += step_val) {
                            if (col.count >= 64) break;
                            col.element_nodes[col.count++] = pool.make_const(v);
                        }
                        col.ok = true;
                        return col;
                    }
                }
            }
        }

        // Not a range or couldn't resolve — rewind and fail
        ts.rewind(saved_pos);
        skip_form(ts);
        col.ok = false;
        return col;
    }

    col.ok = false;
    return col;
}

// ── Data Table Resolution ───────────────────────────────────────────────────

GraphBuilder::DataRef GraphBuilder::resolve_data_table(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    DataRef ref = { 0, 0, false };

    Token tok = ts.peek();

    // Literal vector [1 2 3]
    if (tok.kind == TokenKind::LBracket) {
        ts.consume(); // eat '['
        double values[64];
        uint16_t count = 0;
        while (ts.peek().kind != TokenKind::RBracket && !ts.at_end() && count < 64) {
            uint16_t elem = compile_expr(ts, scope, ctx);
            if (is_const(elem)) {
                values[count++] = const_value(elem);
            } else {
                values[count++] = 0.0; // non-const elements default to 0
            }
        }
        ts.expect(TokenKind::RBracket);

        // Store in data pool (needs mutable CellStore — cast away const for now)
        CellStore& mut_cells = const_cast<CellStore&>(cells);
        uint16_t table_id = mut_cells.store_data_table(values, count);
        if (table_id != UINT8_MAX) {
            ref.table_id = table_id;
            ref.length = count;
            ref.ok = true;
        }
        return ref;
    }

    // Symbol reference
    if (tok.kind == TokenKind::Symbol) {
        ts.consume();
        SymbolID sym_id = tok.symbol;
        if (sym_id < MAX_CELLS) {
            const Cell& cell = cells.cells[sym_id];
            if (cell.kind == CellKind::Data) {
                add_dependency(sym_id);
                ref.table_id = cell.data_table_id;
                ref.length = (uint16_t)cell.value;
                ref.ok = true;
                return ref;
            }
        }
        report_error_at(tok.span_start, tok.span_len,
            "Expected a vector or data reference",
            "Try: [1 0 1 0] or a defined vector name");
        return ref;
    }

    // Quoted list '(1 0 1 0) — treat as vector
    // TODO: handle quoted lists

    report_error_at(tok.span_start, tok.span_len,
        "Expected a vector or data reference",
        "Try: [1 0 1 0] or a defined vector name");
    return ref;
}

// ── Top-level build function ────────────────────────────────────────────────

GraphBuildResult build_output_graph(
    NodePool& pool,
    TokenStream& ts,
    const CellStore& cells,
    const SourceArena& source
) {
    GraphBuilder builder(pool, cells, source);
    Scope root_scope = {};
    TimeContext ctx = { pool.make_raw_time_load() };

    uint16_t root = builder.compile_expr(ts, root_scope, ctx);

    GraphBuildResult result;
    result.root_node = root;
    memcpy(result.diagnostics, builder.diagnostics,
           builder.diagnostic_count * sizeof(Diagnostic));
    result.diagnostic_count = builder.diagnostic_count;
    result.has_error = builder.has_error;

    // Copy dependency info
    memcpy(result.dep_cells, builder.dep_cells,
           builder.dep_count * sizeof(SymbolID));
    result.dep_count = builder.dep_count;

    return result;
}

} // namespace sig
