#include "graph_builder.h"
#include "../modulisp/lisp/symbol_intern.h"
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace sig {

// ── Static symbol cache ─────────────────────────────────────────────────────

GraphBuilder::Symbols GraphBuilder::sym = {};
bool GraphBuilder::symbols_initialized = false;

// ── Form dispatch table statics ─────────────────────────────────────────────

GraphBuilder::FormEntry GraphBuilder::form_table[FORM_TABLE_CAPACITY] = {};
uint16_t GraphBuilder::form_table_count = 0;
bool GraphBuilder::form_table_sorted = false;

void GraphBuilder::init_symbols() {
    if (symbols_initialized) return;
    auto& si = SymbolIntern::getInstance();
    #define SYM(f, s, c) sym.f = si.intern(s);
    #include "symbols.def"
    #undef SYM
    symbols_initialized = true;
    init_form_table();
}

void GraphBuilder::init_form_table() {
    if (form_table_sorted) return;
    form_table_count = 0;

    auto add = [](SymbolID s, uint16_t (GraphBuilder::*h)(TokenStream&, Scope&, TimeContext&)) {
        if (form_table_count < FORM_TABLE_CAPACITY) {
            form_table[form_table_count++] = {s, h};
        }
    };

    // Time transforms
    add(sym.fast,         &GraphBuilder::compile_fast);
    add(sym.slow,         &GraphBuilder::compile_slow);
    add(sym.offset,       &GraphBuilder::compile_offset);
    add(sym.shift,        &GraphBuilder::compile_offset);   // alias
    add(sym.loop_at,      &GraphBuilder::compile_loop_at);
    add(sym.eval_at_time, &GraphBuilder::compile_eval_at_time);

    // Control flow
    add(sym.if_,          &GraphBuilder::compile_if);
    add(sym.let_,         &GraphBuilder::compile_let);
    add(sym.do_,          &GraphBuilder::compile_do);
    add(sym.scope,        &GraphBuilder::compile_do);       // alias
    add(sym.for_,         &GraphBuilder::compile_for);
    add(sym.while_,       &GraphBuilder::compile_while_gate);

    // Domain signal functions
    add(sym.step,         &GraphBuilder::compile_step);
    add(sym.seq,          &GraphBuilder::compile_seq);
    add(sym.from_list,    &GraphBuilder::compile_seq);      // alias
    add(sym.euclid,       &GraphBuilder::compile_euclid);
    add(sym.eu,           &GraphBuilder::compile_euclid);   // alias
    add(sym.interp,       &GraphBuilder::compile_interp);
    add(sym.flatseq,      &GraphBuilder::compile_interp);   // alias
    add(sym.dm,           &GraphBuilder::compile_dm);
    add(sym.range,        &GraphBuilder::compile_range);
    add(sym.gatesw,       &GraphBuilder::compile_gatesw);
    add(sym.random_,      &GraphBuilder::compile_random);
    add(sym.index_rand,   &GraphBuilder::compile_index_rand);

    // Ratio-rhythm functions
    add(sym.rpulse,       &GraphBuilder::compile_rpulse);
    add(sym.rstep,        &GraphBuilder::compile_rstep);
    add(sym.ridx,         &GraphBuilder::compile_ridx);
    add(sym.rwarp,        &GraphBuilder::compile_rwarp);

    // State
    add(sym.integrate,    &GraphBuilder::compile_integrate);

    // UGens — primary names
    add(sym.phasor_,      &GraphBuilder::compile_phasor);
    add(sym.lfo,          &GraphBuilder::compile_lfo);
    add(sym.slew,         &GraphBuilder::compile_slew);
    add(sym.one_pole,     &GraphBuilder::compile_one_pole);
    add(sym.env_follow,   &GraphBuilder::compile_env_follow);
    add(sym.sah,          &GraphBuilder::compile_sah);
    add(sym.noise,        &GraphBuilder::compile_noise);
    add(sym.toggle,       &GraphBuilder::compile_toggle);
    add(sym.count,        &GraphBuilder::compile_count);

    // UGens — aliases with wave-type defaults
    add(sym.osc,          &GraphBuilder::compile_lfo_sin);
    add(sym.tri_osc,      &GraphBuilder::compile_lfo_tri);
    add(sym.saw,          &GraphBuilder::compile_lfo_saw);
    add(sym.sqr_osc,      &GraphBuilder::compile_lfo_sqr);
    add(sym.envelope_follower, &GraphBuilder::compile_env_follow);
    add(sym.latch,        &GraphBuilder::compile_sah);

    // Live-edit
    add(sym.live_edit,    &GraphBuilder::compile_live_edit);

    // Output feedback
    add(sym.prev,         &GraphBuilder::compile_prev);

    // Sort by SymbolID for binary search
    std::sort(form_table, form_table + form_table_count,
              [](const FormEntry& a, const FormEntry& b) { return a.sym < b.sym; });
    form_table_sorted = true;
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

GraphBuilder::GraphBuilder(NodePool& p, CellStore& c, const SourceArena& s)
    : pool(p), cells(c), source(s)
{
    init_symbols();
    live_slot_count_at_start = pool.live_slot_count;
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
    return report_error_at_cat(DiagnosticCategory::Runtime,
                               span_start, span_len, message, suggestion);
}

uint16_t GraphBuilder::report_error_cat(DiagnosticCategory cat, const Token& tok,
                                         const char* message, const char* suggestion) {
    return report_error_at_cat(cat, tok.span_start, tok.span_len, message, suggestion);
}

uint16_t GraphBuilder::report_error_at_cat(DiagnosticCategory cat,
                                            uint16_t span_start, uint16_t span_len,
                                            const char* message, const char* suggestion) {
    if (diagnostic_count < MAX_DIAGNOSTICS) {
        diagnostics[diagnostic_count++] = {
            DiagnosticSeverity::Error, cat,
            span_start, span_len, message, suggestion
        };
    }
    has_error = true;
    return NODE_NONE;
}

// Static buffers for fuzzy match error messages (avoids heap allocation).
// These persist for the lifetime of the diagnostic, which is fine since
// graph building is single-threaded and diagnostics are copied out.
static char fuzzy_msg_buf[128];
static char fuzzy_sug_buf[128];

uint16_t GraphBuilder::report_error_with_fuzzy_match(SymbolID sym_id,
                                                      uint16_t span_start,
                                                      uint16_t span_len) {
    SymbolID match = find_fuzzy_match(sym_id, cells);
    auto& si = SymbolIntern::getInstance();

    if (match != SymbolIntern::INVALID_ID) {
        const String& unknown_str = si.getString(sym_id);
        const String& match_str = si.getString(match);
        snprintf(fuzzy_msg_buf, sizeof(fuzzy_msg_buf),
                 "Unknown '%s'. Did you mean '%s'?",
                 unknown_str.c_str(), match_str.c_str());
        snprintf(fuzzy_sug_buf, sizeof(fuzzy_sug_buf),
                 "Try: %s", match_str.c_str());
        return report_error_at_cat(DiagnosticCategory::UndefinedName,
                                   span_start, span_len,
                                   fuzzy_msg_buf, fuzzy_sug_buf);
    }

    return report_error_at_cat(DiagnosticCategory::UndefinedName,
                               span_start, span_len,
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
// All symbols tagged "side_effect" in symbols.def.

bool GraphBuilder::is_side_effect_form(SymbolID op) const {
    return op == sym.define || op == sym.def || op == sym.defn ||
           op == sym.defun || op == sym.defs || op == sym.set ||
           op == sym.defstate ||
           op == sym.zeros_ || op == sym.get_expr ||
           op == sym.set_bpm || op == sym.set_time_sig ||
           op == sym.useq_clear || op == sym.set_time_offset ||
           op == sym.nudge_time || op == sym.useq_play ||
           op == sym.useq_pause || op == sym.useq_stop ||
           op == sym.useq_rewind;
}

// ── Operator classification ─────────────────────────────────────────────────
// Uses cached symbol IDs from init_symbols() — no intern() calls at runtime.

bool GraphBuilder::is_arithmetic_op(SymbolID op) const {
    return op == sym.plus || op == sym.minus || op == sym.star ||
           op == sym.slash || op == sym.mod_pct ||
           op == sym.min_ || op == sym.max_;
}

bool GraphBuilder::is_comparison_op(SymbolID op) const {
    return op == sym.gt || op == sym.lt || op == sym.ge ||
           op == sym.le || op == sym.eq;
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
    return op == sym.pow_ || op == sym.expt || op == sym.mod_ || op == sym.pulse;
}

bool GraphBuilder::is_ternary_math(SymbolID op) const {
    return op == sym.clamp || op == sym.lerp || op == sym.scale;
}

NodeOp GraphBuilder::arithmetic_sym_to_op(SymbolID op) const {
    if (op == sym.plus)    return NodeOp::Add;
    if (op == sym.minus)   return NodeOp::Sub;
    if (op == sym.star)    return NodeOp::Mul;
    if (op == sym.slash)   return NodeOp::Div;
    if (op == sym.mod_pct) return NodeOp::Mod;
    if (op == sym.min_)    return NodeOp::Min;
    if (op == sym.max_)    return NodeOp::Max;
    return NodeOp::Add;
}

NodeOp GraphBuilder::comparison_sym_to_op(SymbolID op) const {
    if (op == sym.gt) return NodeOp::CmpGt;
    if (op == sym.lt) return NodeOp::CmpLt;
    if (op == sym.ge) return NodeOp::CmpGe;
    if (op == sym.le) return NodeOp::CmpLe;
    if (op == sym.eq) return NodeOp::CmpEq;
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
    if (name == "in1")  return 0;   // INP_I1
    if (name == "in2")  return 1;   // INP_I2
    if (name == "ain1") return 8;   // INP_AI1 (was incorrectly 2, which is INP_M1)
    if (name == "ain2") return 9;   // INP_AI2 (was incorrectly 3, which is INP_M2)
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
    return pool.make_binop(NodeOp::Mod, phase, pool.make_const(1.0));
}

uint16_t GraphBuilder::expand_bar(TimeContext& ctx) {
    uint16_t bpm = pool.make_cell_load(sym.bpm);
    uint16_t bpb = pool.make_cell_load(sym.beats_per_bar);
    uint16_t rate = pool.make_binop(NodeOp::Div,
                        pool.make_binop(NodeOp::Div, bpm, pool.make_const(60.0)), bpb);
    uint16_t phase = pool.make_binop(NodeOp::Mul, ctx.t_node, rate);
    return pool.make_binop(NodeOp::Mod, phase, pool.make_const(1.0));
}

uint16_t GraphBuilder::expand_phrase(TimeContext& ctx) {
    uint16_t bpm = pool.make_cell_load(sym.bpm);
    uint16_t bpb = pool.make_cell_load(sym.beats_per_bar);
    uint16_t bpp = pool.make_cell_load(sym.bars_per_phrase);
    uint16_t rate = pool.make_binop(NodeOp::Div,
                        pool.make_binop(NodeOp::Div,
                            pool.make_binop(NodeOp::Div, bpm, pool.make_const(60.0)), bpb), bpp);
    uint16_t phase = pool.make_binop(NodeOp::Mul, ctx.t_node, rate);
    return pool.make_binop(NodeOp::Mod, phase, pool.make_const(1.0));
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
    return pool.make_binop(NodeOp::Mod, phase, pool.make_const(1.0));
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
    // Nesting-depth guard: compile_expr recurses on nested forms on the small
    // RP2040 stack (this runs inside tick() for live edits). Abort with a clear
    // diagnostic before a deeply-nested program can overflow the hardware stack.
    // RAII guard decrements on every return path.
    struct DepthGuard {
        uint16_t& d;
        DepthGuard(uint16_t& d_) : d(d_) { ++d; }
        ~DepthGuard() { --d; }
    } depth_guard(compile_depth);
    if (compile_depth > MAX_COMPILE_DEPTH) {
        Token tok = ts.peek();
        // Consume one token so error-recovery callers keep making progress.
        if (tok.kind == TokenKind::LParen) {
            skip_form(ts);
        } else if (tok.kind != TokenKind::RParen && tok.kind != TokenKind::Eof) {
            ts.consume();
        }
        return report_error_cat(DiagnosticCategory::Syntax, tok,
                                "Expression nested too deeply",
                                "Simplify or split this deeply-nested form");
    }

    // If already in error state, still consume one expression to keep the
    // token stream advancing (prevents infinite loops in variadic callers).
    if (has_error) {
        Token tok = ts.peek();
        if (tok.kind == TokenKind::LParen) {
            skip_form(ts);
        } else if (tok.kind != TokenKind::RParen && tok.kind != TokenKind::Eof) {
            ts.consume();
        }
        return NODE_NONE;
    }

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
            return report_error_cat(DiagnosticCategory::Syntax, op_tok,
                                    "Expected a function name after '('",
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
        // Catch unconsumed arguments: if the form compiled without error but
        // left extra tokens before the closing paren, report a clear arity
        // error naming the function.  Previously these extras silently
        // desynchronised the token stream, producing confusing errors later.
        if (!has_error && ts.peek().kind != TokenKind::RParen && !ts.at_end()) {
            auto& si = SymbolIntern::getInstance();
            const String& name = si.getString(op_tok.symbol);
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "'%s' got more arguments than expected",
                     name.c_str());
            result = report_error_at_cat(DiagnosticCategory::Arity,
                op_tok.span_start, op_tok.span_len,
                msg, "Remove the extra arguments");
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
        return report_error_cat(DiagnosticCategory::Syntax, tok,
                                "Unexpected end of expression",
                                "Expression seems incomplete");
    }

    return report_error_cat(DiagnosticCategory::Syntax, tok,
                            "Unexpected token",
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

    // 2b. dt (time delta)
    if (sym_id == sym.dt) return pool.make_dt_load();

    // 3. Well-known temporal templates
    if (sym_id == sym.beat)     return expand_beat(ctx);
    if (sym_id == sym.bar)      return expand_bar(ctx);
    if (sym_id == sym.phrase)   return expand_phrase(ctx);
    if (sym_id == sym.section)  return expand_section(ctx);
    if (sym_id == sym.beat_num) return expand_beat_num(ctx);
    if (sym_id == sym.bar_num)  return expand_bar_num(ctx);

    // 4. Derived timing
    if (sym_id == sym.beat_dur) {
        uint16_t bpm_load = pool.make_cell_load(sym.bpm);
        uint16_t sixty = pool.make_const(60.0);
        return pool.make_binop(NodeOp::Div, sixty, bpm_load);
    }
    if (sym_id == sym.bar_dur) {
        uint16_t bpm_load = pool.make_cell_load(sym.bpm);
        uint16_t sixty = pool.make_const(60.0);
        uint16_t beat_dur = pool.make_binop(NodeOp::Div, sixty, bpm_load);
        uint16_t bpb_load = pool.make_cell_load(sym.beats_per_bar);
        return pool.make_binop(NodeOp::Mul, beat_dur, bpb_load);
    }

    // 5. Hardware inputs
    uint16_t input_idx = resolve_hardware_input(sym_id);
    if (input_idx != NODE_NONE) {
        return pool.make_input_load(input_idx);
    }

    // 6. Output references — previous tick's value
    if (is_output_symbol(sym_id)) {
        uint16_t idx = resolve_output_index(sym_id);
        return pool.make_prev_output_load(idx);
    }

    // 7. Cell table
    if (sym_id < MAX_CELLS) {
        const Cell& cell = cells.cells[sym_id];
        switch (cell.kind) {
            case CellKind::Number:
                add_dependency(sym_id);
                // State cells (flags 0x02) load from state slot, not const
                if (cell.flags == 0x02) {
                    return pool.make_state_load(cell.data_table_id);
                }
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
                return report_error_at_cat(DiagnosticCategory::Arity,
                    span_start, span_len,
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

    const char* saved_base = source_base;
    source_base = body_src;
    uint16_t result = compile_expr(body_ts, scope, ctx);
    source_base = saved_base;
    pop_inline_stack();
    return result;
}

// ── Form Compilation ────────────────────────────────────────────────────────

uint16_t GraphBuilder::compile_form(SymbolID op, TokenStream& ts,
                                     Scope& scope, TimeContext& ctx, Token op_tok) {
    // 1. Side effects → compile-time error in signal context
    //    (compile_expr drains remaining tokens to RParen after we return)
    if (is_side_effect_form(op)) {
        return report_error_cat(DiagnosticCategory::Boundary, op_tok,
            "This can't be used inside an output expression",
            "Use it at the top level instead");
    }

    // 2. Table lookup — binary search in sorted form_table
    {
        uint16_t lo = 0, hi = form_table_count;
        while (lo < hi) {
            uint16_t mid = (lo + hi) / 2;
            if (form_table[mid].sym < op) lo = mid + 1;
            else hi = mid;
        }
        if (lo < form_table_count && form_table[lo].sym == op) {
            return (this->*form_table[lo].handler)(ts, scope, ctx);
        }
    }

    // 3. Variadic arithmetic (left-fold, not in table)
    if (is_arithmetic_op(op)) return compile_variadic_arithmetic(op, ts, scope, ctx);

    // 4. Comparison (binary, needs op-to-nodeop mapping)
    if (is_comparison_op(op)) return compile_comparison(op, ts, scope, ctx);

    // 5. Logic
    if (is_logic_op(op)) return compile_logic(op, ts, scope, ctx);

    // 6. Unary / binary / ternary math
    if (is_unary_math(op)) return compile_unary_math(unary_sym_to_op(op), ts, scope, ctx);

    if (is_binary_math(op)) {
        NodeOp nop = NodeOp::Expt;
        bool swap_args = false;
        if (op == sym.pow_)  { nop = NodeOp::Expt; swap_args = true; } // (pow a b) = b^a
        if (op == sym.expt)  nop = NodeOp::Expt;
        if (op == sym.mod_)  nop = NodeOp::Mod;
        if (op == sym.pulse) nop = NodeOp::Pulse;
        if (swap_args)
            return compile_binary_math_swapped(nop, ts, scope, ctx);
        return compile_binary_math(nop, ts, scope, ctx);
    }

    if (is_ternary_math(op)) {
        NodeOp nop = NodeOp::Clamp;
        if (op == sym.clamp) nop = NodeOp::Clamp;
        if (op == sym.lerp)  nop = NodeOp::Lerp;
        if (op == sym.scale) nop = NodeOp::Scale;
        return compile_ternary_math(nop, ts, scope, ctx);
    }

    // 7. Special forms with non-standard signatures
    if (op == sym.gates || op == sym.trigs) return compile_gates(op, ts, scope, ctx);

    if (op == sym.tri || op == sym.sqr) {
        NodeOp nop = (op == sym.tri) ? NodeOp::Tri : NodeOp::Sqr;
        uint16_t first = compile_expr(ts, scope, ctx);
        if (ts.peek().kind != TokenKind::RParen) {
            uint16_t second = compile_expr(ts, scope, ctx);
            return pool.make_unary(nop, second);
        }
        return pool.make_unary(nop, first);
    }

    if (op == sym.fn || op == sym.lambda) return compile_lambda(ts, scope, ctx);

    if (op == sym.quote) {
        return report_error_cat(DiagnosticCategory::Boundary, op_tok,
            "'quote' can't be used inside an output expression",
            "Use a literal vector instead: [1 0 1 0]");
    }

    if (op == sym.input) {
        uint16_t arg = compile_expr(ts, scope, ctx);
        if (arg == NODE_NONE) return NODE_NONE;
        if (is_const(arg)) {
            uint16_t ch = (uint16_t)const_value(arg);
            return pool.make_input_load(ch);
        }
        return report_error_cat(DiagnosticCategory::Type, op_tok,
            "input channel must be a constant",
            "Try: (input 0) or (input 1)");
    }

    // 8. User-defined function call (fallback)
    return compile_call(op, ts, scope, ctx, op_tok);
}

// ── Time Transforms ─────────────────────────────────────────────────────────

uint16_t GraphBuilder::compile_fast(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    uint16_t factor = compile_expr(ts, scope, ctx);
    if (factor == NODE_NONE) return NODE_NONE;
    TimeContext inner = { pool.make_binop(NodeOp::Mul, ctx.t_node, factor) };
    return compile_expr(ts, scope, inner);
}

uint16_t GraphBuilder::compile_slow(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    uint16_t factor = compile_expr(ts, scope, ctx);
    if (factor == NODE_NONE) return NODE_NONE;
    TimeContext inner = { pool.make_binop(NodeOp::Div, ctx.t_node, factor) };
    return compile_expr(ts, scope, inner);
}

uint16_t GraphBuilder::compile_offset(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    uint16_t amount = compile_expr(ts, scope, ctx);
    if (amount == NODE_NONE) return NODE_NONE;
    TimeContext inner = { pool.make_binop(NodeOp::Add, ctx.t_node, amount) };
    return compile_expr(ts, scope, inner);
}

uint16_t GraphBuilder::compile_loop_at(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    uint16_t duration = compile_expr(ts, scope, ctx);
    if (duration == NODE_NONE) return NODE_NONE;
    TimeContext inner = { pool.make_binop(NodeOp::Mod, ctx.t_node, duration) };
    return compile_expr(ts, scope, inner);
}

uint16_t GraphBuilder::compile_eval_at_time(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    uint16_t time_node = compile_expr(ts, scope, ctx);
    if (time_node == NODE_NONE) return NODE_NONE;
    TimeContext inner = { time_node };
    return compile_expr(ts, scope, inner);
}

// ── Output Feedback ────────────────────────────────────────────────────────

uint16_t GraphBuilder::compile_prev(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    (void)scope;
    (void)ctx;
    if (ts.pos >= ts.count || ts.tokens[ts.pos].kind != TokenKind::Symbol) {
        return report_error_at_cat(DiagnosticCategory::Arity,
            ts.pos > 0 ? ts.tokens[ts.pos - 1].span_start : 0,
            ts.pos > 0 ? ts.tokens[ts.pos - 1].span_len : 0,
            "(prev) needs an output name like a1, d1, etc.",
            "(prev a1)");
    }
    Token sym_tok = ts.consume();
    SymbolID sym_id = sym_tok.symbol;

    if (!is_output_symbol(sym_id)) {
        return report_error_cat(DiagnosticCategory::Type, sym_tok,
            "(prev) argument must be an output name (a1-a8, d1-d8, s1-s8)",
            "(prev a1)");
    }

    uint16_t idx = resolve_output_index(sym_id);
    return pool.make_prev_output_load(idx);
}

// ── Control Flow ────────────────────────────────────────────────────────────

uint16_t GraphBuilder::compile_if(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    uint16_t cond = compile_expr(ts, scope, ctx);
    if (cond == NODE_NONE) return NODE_NONE;
    uint16_t then_val = compile_expr(ts, scope, ctx);
    if (then_val == NODE_NONE) return NODE_NONE;
    uint16_t else_val = pool.make_const(0.0);
    if (ts.peek().kind != TokenKind::RParen) {
        else_val = compile_expr(ts, scope, ctx);
        if (else_val == NODE_NONE) return NODE_NONE;
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
                return report_error_cat(DiagnosticCategory::Syntax, name_tok,
                                        "Expected a name in let binding",
                                        "Try: (let [x 1 y 2] (+ x y))");
            }
            uint16_t val = compile_expr(ts, inner, ctx);
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
                    return report_error_cat(DiagnosticCategory::Syntax, name_tok,
                                            "Expected a name in let binding",
                                            "Try: (let (x 1 y 2) (+ x y))");
                }
                uint16_t val = compile_expr(ts, inner, ctx);
                inner.bind(name_tok.symbol, val);
            }
        } else {
            // Nested binding list: ((name1 val1) (name2 val2) ...)
            while (ts.peek().kind != TokenKind::RParen && !ts.at_end()) {
                ts.expect(TokenKind::LParen);
                Token name_tok = ts.consume();
                if (name_tok.kind != TokenKind::Symbol) {
                    return report_error_cat(DiagnosticCategory::Syntax, name_tok,
                                            "Expected a name in let binding",
                                            "Try: (let ((x 1) (y 2)) (+ x y))");
                }
                uint16_t val = compile_expr(ts, inner, ctx);
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
        return report_error_cat(DiagnosticCategory::Syntax, var_tok,
                                "'for' needs a variable name",
                                "Try: (for x [1 2 3] (* x 2))");
    }
    SymbolID var = var_tok.symbol;

    Collection col = resolve_collection(ts, scope, ctx);

    uint16_t body_start = ts.pos;

    if (!col.ok) {
        // Skip past body
        skip_form(ts);
        return report_error_cat(DiagnosticCategory::Type, var_tok,
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
    if (cond == NODE_NONE) return NODE_NONE;
    uint16_t value = compile_expr(ts, scope, ctx);
    if (value == NODE_NONE) return NODE_NONE;
    return pool.make_select(cond, value, pool.make_const(0.0));
}

uint16_t GraphBuilder::compile_lambda(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // Lambda in signal context is not supported as a value
    // but if immediately applied, could work. For now, error.
    return report_error_at_cat(DiagnosticCategory::Boundary, 0, 0,
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

    // Recursion guard — detects both direct and mutual recursion
    if (is_in_inline_stack(fn_sym)) {
        return report_error(op_tok,
            "This function calls itself — recursive functions can't be used in outputs",
            "Try using 'for' over a fixed collection instead");
    }
    // Depth limit safety net — catches unbounded inlining chains
    if (inline_depth >= MAX_INLINE_DEPTH) {
        return report_error(op_tok,
            "Function call chain is too deep",
            "Simplify by reducing the number of nested function calls");
    }
    push_inline_stack(fn_sym);

    // Compile arguments
    uint16_t arg_nodes[MAX_CALLABLE_PARAMS];
    uint8_t arg_count = 0;
    while (ts.peek().kind != TokenKind::RParen && arg_count < info.param_count && !ts.at_end()) {
        uint16_t arg = compile_expr(ts, scope, ctx);
        if (arg == NODE_NONE) {
            pop_inline_stack();
            return NODE_NONE;
        }
        arg_nodes[arg_count++] = arg;
    }

    if (arg_count != info.param_count) {
        pop_inline_stack();
        return report_error_cat(DiagnosticCategory::Arity, op_tok,
            "Wrong number of arguments",
            "Check the function definition");
    }

    // Check for too many arguments (extras not consumed by the loop)
    if (ts.peek().kind != TokenKind::RParen) {
        pop_inline_stack();
        return report_error_cat(DiagnosticCategory::Arity, op_tok,
            "Too many arguments",
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

    // Nullary forms: (+) → 0, (*) → 1
    if (ts.peek().kind == TokenKind::RParen) {
        if (nop == NodeOp::Add) return pool.make_const(0.0);
        if (nop == NodeOp::Mul) return pool.make_const(1.0);
        // (- ) and (/ ) with no args are errors
        return report_error_at(0, 0, "This operator needs at least one argument",
                               "Try: (+ 1 2) or (* 3 4)");
    }

    // First argument
    uint16_t result = compile_expr(ts, scope, ctx);
    if (result == NODE_NONE) return NODE_NONE;

    // Handle unary minus: (- x) → negate
    if (ts.peek().kind == TokenKind::RParen && nop == NodeOp::Sub) {
        return pool.make_unary(NodeOp::Neg, result);
    }

    // Handle unary division: (/ x) → (/ 1 x)
    if (ts.peek().kind == TokenKind::RParen && nop == NodeOp::Div) {
        return pool.make_binop(NodeOp::Div, pool.make_const(1.0), result);
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
    // Arity check: needs exactly 1 argument
    if (ts.peek().kind == TokenKind::RParen) {
        return report_error_at_cat(DiagnosticCategory::Arity,
            0, 0, "This function needs 1 value",
            "Try giving it an argument, like (sin beat)");
    }
    uint16_t a = compile_expr(ts, scope, ctx);
    if (a == NODE_NONE) return NODE_NONE;
    // Check for extra arguments
    if (ts.peek().kind != TokenKind::RParen) {
        return report_error_at_cat(DiagnosticCategory::Arity,
            ts.peek().span_start, ts.peek().span_len,
            "This function takes 1 value, but got more",
            "Remove the extra arguments");
    }
    return pool.make_unary(op, a);
}

uint16_t GraphBuilder::compile_binary_math(NodeOp op, TokenStream& ts,
                                            Scope& scope, TimeContext& ctx) {
    // Arity check: needs exactly 2 arguments
    if (ts.peek().kind == TokenKind::RParen) {
        return report_error_at_cat(DiagnosticCategory::Arity,
            0, 0, "This function needs 2 values",
            "Try: (pow 2 3) or (mod 10 3)");
    }
    uint16_t a = compile_expr(ts, scope, ctx);
    if (a == NODE_NONE) return NODE_NONE;
    if (ts.peek().kind == TokenKind::RParen) {
        return report_error_at_cat(DiagnosticCategory::Arity,
            0, 0, "This function needs 2 values, but only got 1",
            "Add another argument");
    }
    uint16_t b = compile_expr(ts, scope, ctx);
    return pool.make_binop(op, a, b);
}

uint16_t GraphBuilder::compile_binary_math_swapped(NodeOp op, TokenStream& ts,
                                            Scope& scope, TimeContext& ctx) {
    if (ts.peek().kind == TokenKind::RParen) {
        return report_error_at_cat(DiagnosticCategory::Arity,
            0, 0, "This function needs 2 values",
            "Try: (pow 2 10) — computes 10 raised to 2");
    }
    uint16_t a = compile_expr(ts, scope, ctx);
    if (a == NODE_NONE) return NODE_NONE;
    if (ts.peek().kind == TokenKind::RParen) {
        return report_error_at_cat(DiagnosticCategory::Arity,
            0, 0, "This function needs 2 values, but only got 1",
            "Add another argument");
    }
    uint16_t b = compile_expr(ts, scope, ctx);
    return pool.make_binop(op, b, a);
}

uint16_t GraphBuilder::compile_ternary_math(NodeOp op, TokenStream& ts,
                                             Scope& scope, TimeContext& ctx) {
    // Arity check: needs exactly 3 arguments
    if (ts.peek().kind == TokenKind::RParen) {
        return report_error_at_cat(DiagnosticCategory::Arity,
            0, 0, "This function needs 3 values",
            "Try: (clamp value low high) or (lerp a b t)");
    }
    uint16_t a = compile_expr(ts, scope, ctx);
    if (a == NODE_NONE) return NODE_NONE;
    if (ts.peek().kind == TokenKind::RParen) {
        return report_error_at_cat(DiagnosticCategory::Arity,
            0, 0, "This function needs 3 values, but got fewer",
            "Make sure you provide all 3 arguments");
    }
    uint16_t b = compile_expr(ts, scope, ctx);
    if (b == NODE_NONE) return NODE_NONE;
    if (ts.peek().kind == TokenKind::RParen) {
        return report_error_at_cat(DiagnosticCategory::Arity,
            0, 0, "This function needs 3 values, but only got 2",
            "Add the third argument");
    }
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
    // (gates data)              — width=0.5, phase=beat
    // (gates data phase)        — width=0.5
    // (gates data width phase)  — explicit width (duty cycle 0-1)
    DataRef data = resolve_data_table(ts, scope, ctx);
    if (!data.ok) return NODE_NONE;

    uint16_t width;
    uint16_t phase;
    if (ts.peek().kind != TokenKind::RParen) {
        uint16_t arg2 = compile_expr(ts, scope, ctx);
        if (ts.peek().kind != TokenKind::RParen) {
            // 3 args: (gates data width phase)
            width = arg2;
            phase = compile_expr(ts, scope, ctx);
        } else {
            // 2 args: (gates data phase)
            width = pool.make_const(0.5);
            phase = arg2;
        }
    } else {
        width = pool.make_const(0.5);
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

    if (op == sym.trigs) {
        // trigs: value > 0 (no width — instantaneous trigger)
        return pool.make_binop(NodeOp::CmpGt, raw, pool.make_const(0.0));
    }

    // gates: on when value > 0 AND fractional phase within step < width
    uint16_t is_on = pool.make_binop(NodeOp::CmpGt, raw, pool.make_const(0.0));
    uint16_t frac = pool.make_binop(NodeOp::Sub, scaled, idx);
    uint16_t in_width = pool.make_binop(NodeOp::CmpLt, frac, width);
    return pool.make_binop(NodeOp::Mul, is_on, in_width);
}

uint16_t GraphBuilder::compile_euclid(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (euclid total active phase)
    // Matches old engine: idx = (step * active) % total; hit if idx < active AND rem < pw

    // Arity check: needs at least 2 args (total, active)
    if (ts.peek().kind == TokenKind::RParen) {
        return report_error_at_cat(DiagnosticCategory::Arity,
            0, 0, "euclid needs at least 2 values: total hits and active hits",
            "Try: (euclid 8 3 beat)");
    }
    uint16_t total = compile_expr(ts, scope, ctx);
    if (total == NODE_NONE) return NODE_NONE;
    if (ts.peek().kind == TokenKind::RParen) {
        return report_error_at_cat(DiagnosticCategory::Arity,
            0, 0, "euclid needs at least 2 values: total hits and active hits",
            "Try: (euclid 8 3 beat)");
    }
    uint16_t active = compile_expr(ts, scope, ctx);
    if (active == NODE_NONE) return NODE_NONE;

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
    uint16_t idx = pool.make_binop(NodeOp::Mod, product, total);

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
        return report_error_at_cat(DiagnosticCategory::Arity, 0, 0,
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
        return report_error_at_cat(DiagnosticCategory::Arity, 0, 0,
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
        return report_error_at_cat(DiagnosticCategory::Arity, 0, 0,
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
        return report_error_at_cat(DiagnosticCategory::Arity, 0, 0,
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

// ── State Functions ─────────────────────────────────────────────────────────

uint16_t GraphBuilder::compile_integrate(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (integrate rate_expr [:id <id>])
    // Allocates a state slot, builds update graph: state + rate * dt
    // Returns LoadState node for reads

    uint16_t rate = compile_expr(ts, scope, ctx);
    if (rate == NODE_NONE) return NODE_NONE;

    StateID state_id = 0;
    while (ts.peek().kind == TokenKind::Symbol) {
        Token kw = ts.peek();
        const String& kw_str = getSymbolString(kw.symbol);
        if (kw_str.length() == 0 || kw_str.c_str()[0] != ':') break;
        ts.consume();
        if (kw.symbol == sym.kw_id) {
            Token id_tok = ts.consume();
            if (id_tok.kind == TokenKind::String && source_base) {
                state_id = internSymbol(source_base + id_tok.string.offset, id_tok.string.length);
            } else if (id_tok.kind == TokenKind::Symbol) {
                state_id = id_tok.symbol;
            } else {
                return report_error_cat(DiagnosticCategory::Type, id_tok,
                    ":id must be a string or keyword",
                    "Try: (phasor 1 :id \"my-phase\")");
            }
        } else {
            compile_expr(ts, scope, ctx);
        }
    }

    uint16_t slot = resolve_or_alloc(state_id, ResourceKind::Integrator, 0, 0.0);
    if (slot == NODE_NONE) return NODE_NONE;

    // Build update graph: state_load + rate * dt_load
    uint16_t state_load = pool.make_state_load(slot);
    uint16_t dt_load = pool.make_dt_load();
    uint16_t rate_dt = pool.make_binop(NodeOp::Mul, rate, dt_load);
    uint16_t updated = pool.make_binop(NodeOp::Add, state_load, rate_dt);

    // Store the update root
    pool.state_update_roots[slot] = updated;

    // Return state_load for reads (caller reads current state value)
    return state_load;
}

// ── UGen Helpers ───────────────────────────────────────────────────────────

uint16_t GraphBuilder::alloc_state_slot(double init_value) {
    if (pool.state_slot_count >= MAX_STATE_SLOTS) {
        report_error_at(0, 0,
            "Too many state variables (max 32)",
            "Remove unused integrate or defstate declarations");
        return NODE_NONE;
    }
    uint16_t slot = pool.state_slot_count++;
    pool.state_values[slot] = init_value;
    return slot;
}

uint16_t GraphBuilder::resolve_or_alloc(StateID state_id, ResourceKind kind,
                                        uint8_t role, double init_value) {
    if (state_id != 0 && registry) {
        StateResourceKey key{state_id, kind, role};
        uint16_t slot = registry->resolve(key, init_value,
                                          pool.state_values,
                                          pool.state_slot_count);
        if (slot == NODE_NONE) {
            report_error_at(0, 0,
                "Too many state variables (max 32)",
                "Remove unused stateful expressions");
        }
        return slot;
    }
    return alloc_state_slot(init_value);
}

// ── UGen: phasor ───────────────────────────────────────────────────────────

uint16_t GraphBuilder::compile_phasor(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    uint16_t freq = compile_expr(ts, scope, ctx);
    if (freq == NODE_NONE) return NODE_NONE;

    double init_phase = 0.0;
    StateID state_id = 0;

    while (ts.peek().kind == TokenKind::Symbol) {
        Token kw = ts.peek();
        const String& kw_str = getSymbolString(kw.symbol);
        if (kw_str.length() == 0 || kw_str.c_str()[0] != ':') break;
        ts.consume();
        if (kw.symbol == sym.kw_phase) {
            uint16_t val = compile_expr(ts, scope, ctx);
            if (is_const(val)) init_phase = const_value(val);
        } else if (kw.symbol == sym.kw_id) {
            Token id_tok = ts.consume();
            if (id_tok.kind == TokenKind::String && source_base) {
                state_id = internSymbol(source_base + id_tok.string.offset, id_tok.string.length);
            } else if (id_tok.kind == TokenKind::Symbol) {
                state_id = id_tok.symbol;
            } else {
                return report_error_cat(DiagnosticCategory::Type, id_tok,
                    ":id must be a string or keyword",
                    "Try: (phasor 1 :id \"my-phase\")");
            }
        } else {
            compile_expr(ts, scope, ctx);
        }
    }

    uint16_t slot = resolve_or_alloc(state_id, ResourceKind::OscillatorPhase, 0, init_phase);
    if (slot == NODE_NONE) return NODE_NONE;

    uint16_t state_load = pool.make_state_load(slot);
    uint16_t dt_load = pool.make_dt_load();
    uint16_t freq_dt = pool.make_binop(NodeOp::Mul, freq, dt_load);
    uint16_t updated = pool.make_binop(NodeOp::Add, state_load, freq_dt);
    uint16_t wrapped = pool.make_unary(NodeOp::Frac, updated);

    pool.state_update_roots[slot] = wrapped;
    return state_load;
}

// ── UGen: lfo ──────────────────────────────────────────────────────────────

uint16_t GraphBuilder::build_osc_output(uint16_t state_load, uint16_t wave_type, uint16_t pw_node) {
    switch (wave_type) {
        case 0:  return pool.make_unary(NodeOp::USin, state_load);
        case 1:  return pool.make_unary(NodeOp::Tri, state_load);
        case 2:  return state_load; // saw = phasor [0,1)
        case 3:  return pool.make_binop(NodeOp::CmpLt, state_load, pw_node);
        default: return pool.make_unary(NodeOp::USin, state_load);
    }
}

uint16_t GraphBuilder::build_lfo(TokenStream& ts, Scope& scope, TimeContext& ctx, uint16_t default_wave) {
    uint16_t freq = compile_expr(ts, scope, ctx);
    if (freq == NODE_NONE) return NODE_NONE;

    uint16_t wave_type = default_wave;
    double init_phase = 0.0;
    uint16_t pulse_width_node = pool.make_const(0.5);
    StateID state_id = 0;

    auto& si = SymbolIntern::getInstance();

    while (ts.peek().kind == TokenKind::Symbol) {
        Token kw = ts.peek();
        const String& kw_str = si.getString(kw.symbol);
        if (kw_str.length() == 0 || kw_str.c_str()[0] != ':') break;

        ts.consume();

        if (kw.symbol == sym.kw_wave) {
            Token val = ts.consume();
            if (val.kind != TokenKind::Symbol) {
                return report_error_cat(DiagnosticCategory::Type, val,
                    ":wave must be a keyword like :sin, :tri, :saw, or :sqr",
                    "Try: (lfo 440 :wave :saw)");
            }
            if (val.symbol == sym.kw_sin)       wave_type = 0;
            else if (val.symbol == sym.kw_tri)  wave_type = 1;
            else if (val.symbol == sym.kw_saw_kw) wave_type = 2;
            else if (val.symbol == sym.kw_sqr)  wave_type = 3;
            else {
                return report_error_cat(DiagnosticCategory::Type, val,
                    "Unknown waveform",
                    "Try :sin, :tri, :saw, or :sqr");
            }
        } else if (kw.symbol == sym.kw_phase) {
            uint16_t val = compile_expr(ts, scope, ctx);
            if (is_const(val)) init_phase = const_value(val);
        } else if (kw.symbol == sym.kw_pw) {
            pulse_width_node = compile_expr(ts, scope, ctx);
        } else if (kw.symbol == sym.kw_id) {
            Token id_tok = ts.consume();
            if (id_tok.kind == TokenKind::String && source_base) {
                state_id = internSymbol(source_base + id_tok.string.offset, id_tok.string.length);
            } else if (id_tok.kind == TokenKind::Symbol) {
                state_id = id_tok.symbol;
            } else {
                return report_error_cat(DiagnosticCategory::Type, id_tok,
                    ":id must be a string or keyword",
                    "Try: (phasor 1 :id \"my-phase\")");
            }
        } else {
            compile_expr(ts, scope, ctx);
        }
    }

    uint16_t slot = resolve_or_alloc(state_id, ResourceKind::OscillatorPhase, 0, init_phase);
    if (slot == NODE_NONE) return NODE_NONE;

    uint16_t state_load = pool.make_state_load(slot);
    uint16_t dt_load = pool.make_dt_load();
    uint16_t freq_dt = pool.make_binop(NodeOp::Mul, freq, dt_load);
    uint16_t updated = pool.make_binop(NodeOp::Add, state_load, freq_dt);
    uint16_t phase = pool.make_unary(NodeOp::Frac, updated);

    pool.state_update_roots[slot] = phase;

    return build_osc_output(state_load, wave_type, pulse_width_node);
}

uint16_t GraphBuilder::compile_lfo(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    return build_lfo(ts, scope, ctx, 0); // default: :sin
}

uint16_t GraphBuilder::compile_lfo_sin(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    return build_lfo(ts, scope, ctx, 0);
}

uint16_t GraphBuilder::compile_lfo_tri(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    return build_lfo(ts, scope, ctx, 1);
}

uint16_t GraphBuilder::compile_lfo_saw(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    return build_lfo(ts, scope, ctx, 2);
}

uint16_t GraphBuilder::compile_lfo_sqr(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    return build_lfo(ts, scope, ctx, 3);
}

// ── UGen: slew ─────────────────────────────────────────────────────────────

uint16_t GraphBuilder::compile_slew(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (slew target rate [:id <id>])
    // Slew-rate limiter: moves toward target at max `rate` units/sec.
    // update: state + clamp(target - state, -rate*dt, rate*dt)

    uint16_t target = compile_expr(ts, scope, ctx);
    if (target == NODE_NONE) return NODE_NONE;
    uint16_t rate = compile_expr(ts, scope, ctx);
    if (rate == NODE_NONE) return NODE_NONE;

    StateID state_id = 0;
    while (ts.peek().kind == TokenKind::Symbol) {
        Token kw = ts.peek();
        const String& kw_str = getSymbolString(kw.symbol);
        if (kw_str.length() == 0 || kw_str.c_str()[0] != ':') break;
        ts.consume();
        if (kw.symbol == sym.kw_id) {
            Token id_tok = ts.consume();
            if (id_tok.kind == TokenKind::String && source_base) {
                state_id = internSymbol(source_base + id_tok.string.offset, id_tok.string.length);
            } else if (id_tok.kind == TokenKind::Symbol) {
                state_id = id_tok.symbol;
            } else {
                return report_error_cat(DiagnosticCategory::Type, id_tok,
                    ":id must be a string or keyword",
                    "Try: (phasor 1 :id \"my-phase\")");
            }
        } else {
            compile_expr(ts, scope, ctx);
        }
    }

    uint16_t slot = resolve_or_alloc(state_id, ResourceKind::SlewAccumulator, 0, 0.0);
    if (slot == NODE_NONE) return NODE_NONE;

    uint16_t state_load = pool.make_state_load(slot);
    uint16_t dt_load = pool.make_dt_load();

    // delta = target - state
    uint16_t delta = pool.make_binop(NodeOp::Sub, target, state_load);

    // step = rate * dt
    uint16_t step = pool.make_binop(NodeOp::Mul, rate, dt_load);

    // neg_step = -step
    uint16_t neg_step = pool.make_unary(NodeOp::Neg, step);

    // clamped_delta = clamp(-step, step, delta) — value-last convention
    uint16_t clamped = pool.make_ternary(NodeOp::Clamp, neg_step, step, delta);

    // new_state = state + clamped_delta
    uint16_t updated = pool.make_binop(NodeOp::Add, state_load, clamped);

    pool.state_update_roots[slot] = updated;
    return state_load;
}

// ── UGen: one-pole ─────────────────────────────────────────────────────────

uint16_t GraphBuilder::compile_one_pole(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (one-pole input cutoff [:id <id>])
    // First-order low-pass: alpha = min(1, 2*pi*cutoff*dt)
    // update: state + alpha * (input - state)

    uint16_t input = compile_expr(ts, scope, ctx);
    if (input == NODE_NONE) return NODE_NONE;
    uint16_t cutoff = compile_expr(ts, scope, ctx);
    if (cutoff == NODE_NONE) return NODE_NONE;

    StateID state_id = 0;
    while (ts.peek().kind == TokenKind::Symbol) {
        Token kw = ts.peek();
        const String& kw_str = getSymbolString(kw.symbol);
        if (kw_str.length() == 0 || kw_str.c_str()[0] != ':') break;
        ts.consume();
        if (kw.symbol == sym.kw_id) {
            Token id_tok = ts.consume();
            if (id_tok.kind == TokenKind::String && source_base) {
                state_id = internSymbol(source_base + id_tok.string.offset, id_tok.string.length);
            } else if (id_tok.kind == TokenKind::Symbol) {
                state_id = id_tok.symbol;
            } else {
                return report_error_cat(DiagnosticCategory::Type, id_tok,
                    ":id must be a string or keyword",
                    "Try: (phasor 1 :id \"my-phase\")");
            }
        } else {
            compile_expr(ts, scope, ctx);
        }
    }

    uint16_t slot = resolve_or_alloc(state_id, ResourceKind::OnePole, 0, 0.0);
    if (slot == NODE_NONE) return NODE_NONE;

    uint16_t state_load = pool.make_state_load(slot);
    uint16_t dt_load = pool.make_dt_load();

    // 2*pi*cutoff*dt
    uint16_t two_pi = pool.make_const(6.283185307179586);
    uint16_t cutoff_dt = pool.make_binop(NodeOp::Mul, cutoff, dt_load);
    uint16_t alpha_raw = pool.make_binop(NodeOp::Mul, two_pi, cutoff_dt);

    // alpha = min(1.0, alpha_raw)
    uint16_t one = pool.make_const(1.0);
    uint16_t alpha = pool.make_binop(NodeOp::Min, one, alpha_raw);

    // diff = input - state
    uint16_t diff = pool.make_binop(NodeOp::Sub, input, state_load);

    // scaled = alpha * diff
    uint16_t scaled = pool.make_binop(NodeOp::Mul, alpha, diff);

    // updated = state + scaled
    uint16_t updated = pool.make_binop(NodeOp::Add, state_load, scaled);

    pool.state_update_roots[slot] = updated;
    return state_load;
}

// ── UGen: env-follow ───────────────────────────────────────────────────────

uint16_t GraphBuilder::compile_env_follow(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (env-follow input attack release [:id <id>])
    // Asymmetric envelope follower.
    // if |input| > state: state + attack * (|input| - state)
    // else:               state + release * (|input| - state)
    // attack/release are rate coefficients (higher = faster tracking).

    uint16_t input = compile_expr(ts, scope, ctx);
    if (input == NODE_NONE) return NODE_NONE;

    // Default attack/release if not provided
    uint16_t attack_node;
    uint16_t release_node;

    if (ts.peek().kind != TokenKind::RParen) {
        attack_node = compile_expr(ts, scope, ctx);
    } else {
        attack_node = pool.make_const(10.0);
    }
    if (ts.peek().kind != TokenKind::RParen) {
        release_node = compile_expr(ts, scope, ctx);
    } else {
        release_node = pool.make_const(5.0);
    }

    StateID state_id = 0;
    while (ts.peek().kind == TokenKind::Symbol) {
        Token kw = ts.peek();
        const String& kw_str = getSymbolString(kw.symbol);
        if (kw_str.length() == 0 || kw_str.c_str()[0] != ':') break;
        ts.consume();
        if (kw.symbol == sym.kw_id) {
            Token id_tok = ts.consume();
            if (id_tok.kind == TokenKind::String && source_base) {
                state_id = internSymbol(source_base + id_tok.string.offset, id_tok.string.length);
            } else if (id_tok.kind == TokenKind::Symbol) {
                state_id = id_tok.symbol;
            } else {
                return report_error_cat(DiagnosticCategory::Type, id_tok,
                    ":id must be a string or keyword",
                    "Try: (phasor 1 :id \"my-phase\")");
            }
        } else {
            compile_expr(ts, scope, ctx);
        }
    }

    uint16_t slot = resolve_or_alloc(state_id, ResourceKind::EnvelopeFollower, 0, 0.0);
    if (slot == NODE_NONE) return NODE_NONE;

    uint16_t state_load = pool.make_state_load(slot);
    uint16_t dt_load = pool.make_dt_load();

    // abs_input = abs(input)
    uint16_t abs_input = pool.make_unary(NodeOp::Abs, input);

    // diff = abs_input - state
    uint16_t diff = pool.make_binop(NodeOp::Sub, abs_input, state_load);

    // is_rising = diff > 0
    uint16_t zero = pool.make_const(0.0);
    uint16_t is_rising = pool.make_binop(NodeOp::CmpGt, diff, zero);

    // attack_coeff = min(1, attack * dt)
    uint16_t a_dt = pool.make_binop(NodeOp::Mul, attack_node, dt_load);
    uint16_t a_coeff = pool.make_binop(NodeOp::Min, pool.make_const(1.0), a_dt);

    // release_coeff = min(1, release * dt)
    uint16_t r_dt = pool.make_binop(NodeOp::Mul, release_node, dt_load);
    uint16_t r_coeff = pool.make_binop(NodeOp::Min, pool.make_const(1.0), r_dt);

    // coeff = is_rising ? attack_coeff : release_coeff
    uint16_t coeff = pool.make_select(is_rising, a_coeff, r_coeff);

    // updated = state + coeff * diff
    uint16_t scaled = pool.make_binop(NodeOp::Mul, coeff, diff);
    uint16_t updated = pool.make_binop(NodeOp::Add, state_load, scaled);

    pool.state_update_roots[slot] = updated;
    return state_load;
}

// ── UGen: sah (sample-and-hold) ────────────────────────────────────────────

uint16_t GraphBuilder::compile_sah(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (sah input trigger [:id <id>])
    // Aliases: latch
    // When trigger rises through 0.5, sample input. Otherwise hold previous.
    // update: (trigger > 0.5 && prev_trigger <= 0.5) ? input : state
    //
    // Rising-edge detection requires prev_trigger as a second state slot.

    uint16_t input = compile_expr(ts, scope, ctx);
    if (input == NODE_NONE) return NODE_NONE;
    uint16_t trigger = compile_expr(ts, scope, ctx);
    if (trigger == NODE_NONE) return NODE_NONE;

    StateID state_id = 0;
    while (ts.peek().kind == TokenKind::Symbol) {
        Token kw = ts.peek();
        const String& kw_str = getSymbolString(kw.symbol);
        if (kw_str.length() == 0 || kw_str.c_str()[0] != ':') break;
        ts.consume();
        if (kw.symbol == sym.kw_id) {
            Token id_tok = ts.consume();
            if (id_tok.kind == TokenKind::String && source_base) {
                state_id = internSymbol(source_base + id_tok.string.offset, id_tok.string.length);
            } else if (id_tok.kind == TokenKind::Symbol) {
                state_id = id_tok.symbol;
            } else {
                return report_error_cat(DiagnosticCategory::Type, id_tok,
                    ":id must be a string or keyword",
                    "Try: (phasor 1 :id \"my-phase\")");
            }
        } else {
            compile_expr(ts, scope, ctx);
        }
    }

    // Slot 0: held value
    uint16_t slot0 = resolve_or_alloc(state_id, ResourceKind::HeldValue, 0, 0.0);
    if (slot0 == NODE_NONE) return NODE_NONE;

    // Slot 1: previous trigger value
    uint16_t slot1 = resolve_or_alloc(state_id, ResourceKind::TriggerMemory, 0, 0.0);
    if (slot1 == NODE_NONE) return NODE_NONE;

    uint16_t held_load = pool.make_state_load(slot0);
    uint16_t prev_trig_load = pool.make_state_load(slot1);

    // rising = (trigger > 0.5) && (prev_trigger <= 0.5)
    uint16_t threshold = pool.make_const(0.5);
    uint16_t trig_hi = pool.make_binop(NodeOp::CmpGt, trigger, threshold);
    uint16_t prev_trig_lo = pool.make_binop(NodeOp::CmpLe, prev_trig_load, threshold);
    uint16_t rising = pool.make_binop(NodeOp::And, trig_hi, prev_trig_lo);

    // held = rising ? input : held
    uint16_t new_held = pool.make_select(rising, input, held_load);

    pool.state_update_roots[slot0] = new_held;
    pool.state_update_roots[slot1] = trigger; // store current trigger for next tick

    return held_load;
}

// ── UGen: noise ────────────────────────────────────────────────────────────

uint16_t GraphBuilder::compile_noise(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (noise [:id <id>])
    // White noise source using deterministic hash of running counter.
    // Uses one state slot as a counter that increments each tick.
    // Output: HashIndex(counter) → [0,1]

    StateID state_id = 0;
    while (ts.peek().kind == TokenKind::Symbol) {
        Token kw = ts.peek();
        const String& kw_str = getSymbolString(kw.symbol);
        if (kw_str.length() == 0 || kw_str.c_str()[0] != ':') break;
        ts.consume();
        if (kw.symbol == sym.kw_id) {
            Token id_tok = ts.consume();
            if (id_tok.kind == TokenKind::String && source_base) {
                state_id = internSymbol(source_base + id_tok.string.offset, id_tok.string.length);
            } else if (id_tok.kind == TokenKind::Symbol) {
                state_id = id_tok.symbol;
            } else {
                return report_error_cat(DiagnosticCategory::Type, id_tok,
                    ":id must be a string or keyword",
                    "Try: (phasor 1 :id \"my-phase\")");
            }
        } else {
            compile_expr(ts, scope, ctx);
        }
    }

    uint16_t slot = resolve_or_alloc(state_id, ResourceKind::NoiseCounter, 0, 0.0);
    if (slot == NODE_NONE) return NODE_NONE;

    uint16_t state_load = pool.make_state_load(slot);
    uint16_t one = pool.make_const(1.0);

    // counter increments each tick
    uint16_t updated = pool.make_binop(NodeOp::Add, state_load, one);

    pool.state_update_roots[slot] = updated;

    // output = HashIndex(state) → [0,1]
    // Map to [-1, 1] for bipolar noise
    uint16_t hash_raw = pool.make_unary(NodeOp::HashIndex, state_load);
    return pool.make_unary(NodeOp::UniToBi, hash_raw);
}

// ── UGen: toggle ───────────────────────────────────────────────────────────

uint16_t GraphBuilder::compile_toggle(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (toggle trigger [:id <id>])
    // T-flip-flop: toggles 0↔1 on each rising edge of trigger.
    // Needs 2 state slots: toggle state + prev trigger.

    uint16_t trigger = compile_expr(ts, scope, ctx);
    if (trigger == NODE_NONE) return NODE_NONE;

    StateID state_id = 0;
    while (ts.peek().kind == TokenKind::Symbol) {
        Token kw = ts.peek();
        const String& kw_str = getSymbolString(kw.symbol);
        if (kw_str.length() == 0 || kw_str.c_str()[0] != ':') break;
        ts.consume();
        if (kw.symbol == sym.kw_id) {
            Token id_tok = ts.consume();
            if (id_tok.kind == TokenKind::String && source_base) {
                state_id = internSymbol(source_base + id_tok.string.offset, id_tok.string.length);
            } else if (id_tok.kind == TokenKind::Symbol) {
                state_id = id_tok.symbol;
            } else {
                return report_error_cat(DiagnosticCategory::Type, id_tok,
                    ":id must be a string or keyword",
                    "Try: (phasor 1 :id \"my-phase\")");
            }
        } else {
            compile_expr(ts, scope, ctx);
        }
    }

    uint16_t slot0 = resolve_or_alloc(state_id, ResourceKind::ToggleState, 0, 0.0); // toggle state
    if (slot0 == NODE_NONE) return NODE_NONE;
    uint16_t slot1 = resolve_or_alloc(state_id, ResourceKind::TriggerMemory, 0, 0.0); // prev trigger
    if (slot1 == NODE_NONE) return NODE_NONE;

    uint16_t state_load = pool.make_state_load(slot0);
    uint16_t prev_trig_load = pool.make_state_load(slot1);

    uint16_t threshold = pool.make_const(0.5);
    uint16_t trig_hi = pool.make_binop(NodeOp::CmpGt, trigger, threshold);
    uint16_t prev_trig_lo = pool.make_binop(NodeOp::CmpLe, prev_trig_load, threshold);
    uint16_t rising = pool.make_binop(NodeOp::And, trig_hi, prev_trig_lo);

    // If rising: flip (1 - state). Otherwise: keep state.
    uint16_t one = pool.make_const(1.0);
    uint16_t flipped = pool.make_binop(NodeOp::Sub, one, state_load);
    uint16_t new_state = pool.make_select(rising, flipped, state_load);

    pool.state_update_roots[slot0] = new_state;
    pool.state_update_roots[slot1] = trigger;

    return state_load;
}

// ── UGen: count ────────────────────────────────────────────────────────────

uint16_t GraphBuilder::compile_count(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (count trigger [:reset reset-trigger] [:id <id>])
    // Counts rising edges of trigger. Resets to 0 on rising edge of reset.
    // Needs 3 state slots: counter, prev trigger, prev reset.

    uint16_t trigger = compile_expr(ts, scope, ctx);
    if (trigger == NODE_NONE) return NODE_NONE;

    // Optional reset argument
    uint16_t reset_trigger = pool.make_const(0.0);
    StateID state_id = 0;

    // Parse keywords for :reset and :id
    while (ts.peek().kind == TokenKind::Symbol) {
        Token kw = ts.peek();
        const String& kw_str = getSymbolString(kw.symbol);
        if (kw_str.length() == 0 || kw_str.c_str()[0] != ':') break;
        ts.consume();
        if (kw.symbol == sym.kw_reset) {
            reset_trigger = compile_expr(ts, scope, ctx);
        } else if (kw.symbol == sym.kw_id) {
            Token id_tok = ts.consume();
            if (id_tok.kind == TokenKind::String && source_base) {
                state_id = internSymbol(source_base + id_tok.string.offset, id_tok.string.length);
            } else if (id_tok.kind == TokenKind::Symbol) {
                state_id = id_tok.symbol;
            } else {
                return report_error_cat(DiagnosticCategory::Type, id_tok,
                    ":id must be a string or keyword",
                    "Try: (phasor 1 :id \"my-phase\")");
            }
        } else {
            compile_expr(ts, scope, ctx);
        }
    }

    uint16_t slot0 = resolve_or_alloc(state_id, ResourceKind::Counter, 0, 0.0); // counter
    if (slot0 == NODE_NONE) return NODE_NONE;
    uint16_t slot1 = resolve_or_alloc(state_id, ResourceKind::TriggerMemory, 0, 0.0); // prev trigger
    if (slot1 == NODE_NONE) return NODE_NONE;
    uint16_t slot2 = resolve_or_alloc(state_id, ResourceKind::ResetLatch, 0, 0.0); // prev reset
    if (slot2 == NODE_NONE) return NODE_NONE;

    uint16_t counter_load = pool.make_state_load(slot0);
    uint16_t prev_trig_load = pool.make_state_load(slot1);
    uint16_t prev_reset_load = pool.make_state_load(slot2);

    uint16_t threshold = pool.make_const(0.5);
    uint16_t one = pool.make_const(1.0);

    // Detect trigger rising edge
    uint16_t trig_hi = pool.make_binop(NodeOp::CmpGt, trigger, threshold);
    uint16_t prev_trig_lo = pool.make_binop(NodeOp::CmpLe, prev_trig_load, threshold);
    uint16_t trig_rising = pool.make_binop(NodeOp::And, trig_hi, prev_trig_lo);

    // Detect reset rising edge
    uint16_t reset_hi = pool.make_binop(NodeOp::CmpGt, reset_trigger, threshold);
    uint16_t prev_reset_lo = pool.make_binop(NodeOp::CmpLe, prev_reset_load, threshold);
    uint16_t reset_rising = pool.make_binop(NodeOp::And, reset_hi, prev_reset_lo);

    // new_counter = reset_rising ? 0 : (trig_rising ? counter + 1 : counter)
    uint16_t incremented = pool.make_binop(NodeOp::Add, counter_load, one);
    uint16_t after_trig = pool.make_select(trig_rising, incremented, counter_load);
    uint16_t new_counter = pool.make_select(reset_rising, pool.make_const(0.0), after_trig);

    pool.state_update_roots[slot0] = new_counter;
    pool.state_update_roots[slot1] = trigger;
    pool.state_update_roots[slot2] = reset_trigger;

    return counter_load;
}

// -- Live-edit ---------------------------------------------------------------

uint16_t GraphBuilder::compile_live_edit(TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (live-edit <seed> :id <string> :min <num> :max <num> [:name <str>] [:step <num>] [:precision <int>])
    uint16_t form_start = ts.peek().span_start > 0 ? ts.peek().span_start - 1 : 0;

    // 0. Reject if in a context that forbids live-edit (defstate :initial, quote)
    if (reject_live_edit) {
        return report_error_at_cat(DiagnosticCategory::Boundary, form_start, 1,
            "live-edit is not valid here",
            "live-edit cannot appear inside defstate initial values or quoted forms");
    }

    // 1. Parse seed — must be a numeric literal
    Token seed_tok = ts.peek();
    if (seed_tok.kind == TokenKind::LParen) {
        // Peek for nested live-edit
        uint16_t saved_pos = ts.pos;
        ts.consume(); // skip LParen
        Token inner_head = ts.peek();
        ts.rewind(saved_pos);
        if (inner_head.kind == TokenKind::Symbol && inner_head.symbol == sym.live_edit) {
            return report_error_at_cat(DiagnosticCategory::Type, seed_tok.span_start, seed_tok.span_len,
                "Can't use a nested live-edit as the seed of another live-edit",
                "Each live-edit wraps a single literal value");
        }
        return report_error_at_cat(DiagnosticCategory::Type, seed_tok.span_start, seed_tok.span_len,
            "live-edit seed must be a literal number, not an expression",
            "Try: (live-edit 0.5 :id \"x\" :min 0 :max 1)");
    }
    if (seed_tok.kind == TokenKind::Symbol) {
        // Check for nested live-edit
        const String& name = getSymbolString(seed_tok.symbol);
        if (name == "live-edit") {
            return report_error_at_cat(DiagnosticCategory::Type, seed_tok.span_start, seed_tok.span_len,
                "Can't nest live-edit inside another live-edit",
                "Each live-edit wraps a single literal value");
        }
        // Check for keywords appearing where seed should be (missing seed)
        if (name.length() > 0 && name.c_str()[0] == ':') {
            return report_error_at_cat(DiagnosticCategory::Type, seed_tok.span_start, seed_tok.span_len,
                "live-edit seed is missing — first arg must be a number",
                "Try: (live-edit 0.5 :id \"x\" :min 0 :max 1)");
        }
        return report_error_at_cat(DiagnosticCategory::Type, seed_tok.span_start, seed_tok.span_len,
            "live-edit seed must be a literal number",
            "Try: (live-edit 0.5 :id \"x\" :min 0 :max 1)");
    }
    if (seed_tok.kind != TokenKind::Number) {
        return report_error_at_cat(DiagnosticCategory::Type, seed_tok.span_start, seed_tok.span_len,
            "live-edit seed must be a literal number",
            "Try: (live-edit 0.5 :id \"x\" :min 0 :max 1)");
    }
    double seed = seed_tok.number;
    ts.consume();

    // 2. Parse keyword arguments
    char id_buf[MAX_LIVE_SLOT_ID] = {};
    bool has_id = false, has_min = false, has_max = false;
    double min_val = 0.0, max_val = 1.0;

    auto& si = SymbolIntern::getInstance();

    while (ts.peek().kind == TokenKind::Symbol) {
        Token kw_tok = ts.peek();
        const String& kw = si.getString(kw_tok.symbol);
        if (kw.length() == 0 || kw.c_str()[0] != ':') break;

        ts.consume(); // consume keyword

        if (kw == ":id") {
            Token val = ts.consume();
            if (val.kind != TokenKind::String) {
                return report_error_at_cat(DiagnosticCategory::Type, val.span_start, val.span_len,
                    ":id must be a string",
                    "Try: :id \"myknob\"");
            }
            if (!source_base) {
                return report_error_at(val.span_start, val.span_len,
                    "Internal: source text unavailable for string resolution", nullptr);
            }
            uint16_t len = val.string.length;
            if (len >= MAX_LIVE_SLOT_ID) len = MAX_LIVE_SLOT_ID - 1;
            memcpy(id_buf, source_base + val.string.offset, len);
            id_buf[len] = '\0';
            has_id = true;
        } else if (kw == ":min") {
            Token val = ts.consume();
            if (val.kind != TokenKind::Number) {
                return report_error_at_cat(DiagnosticCategory::Type, val.span_start, val.span_len,
                    ":min must be a number",
                    "Try: :min 0");
            }
            min_val = val.number;
            has_min = true;
        } else if (kw == ":max") {
            Token val = ts.consume();
            if (val.kind != TokenKind::Number) {
                return report_error_at_cat(DiagnosticCategory::Type, val.span_start, val.span_len,
                    ":max must be a number",
                    "Try: :max 1");
            }
            max_val = val.number;
            has_max = true;
        } else if (kw == ":name" || kw == ":step" || kw == ":precision") {
            // Accept and skip — compiler-irrelevant metadata
            ts.consume();
        } else {
            // Unknown keyword — skip its value
            ts.consume();
        }
    }

    // 3. Validate required args
    if (!has_id) {
        return report_error_at_cat(DiagnosticCategory::Arity, form_start, 1,
            "live-edit requires :id",
            "Try: (live-edit 0.5 :id \"x\" :min 0 :max 1)");
    }
    if (!has_min) {
        return report_error_at_cat(DiagnosticCategory::Arity, form_start, 1,
            "live-edit requires :min",
            "Try: (live-edit 0.5 :id \"x\" :min 0 :max 1)");
    }
    if (!has_max) {
        return report_error_at_cat(DiagnosticCategory::Arity, form_start, 1,
            "live-edit requires :max",
            "Try: (live-edit 0.5 :id \"x\" :min 0 :max 1)");
    }
    if (min_val >= max_val) {
        return report_error_at_cat(DiagnosticCategory::Overflow, form_start, 1,
            "live-edit :min must be less than :max",
            "Swap :min and :max values");
    }

    // 4. Check for duplicate id — within this build AND across outputs.
    // If the slot already exists (from a prior build or earlier in this build
    // via inline expansion), reuse it. Only error if two *literal* live-edit
    // forms in non-inlined source declare the same id — detected by checking
    // whether we're currently inside an inline expansion.

    // 4a. Cross-output duplicate check (shared table spans all outputs in one eval batch)
    if (shared_live_edit_ids && inline_depth == 0) {
        if (shared_live_edit_ids->contains(id_buf)) {
            // Already seen in a different output's compilation
            return report_error_at_cat(DiagnosticCategory::Boundary, form_start, 1,
                "duplicate live-edit :id in this document",
                "Each live-edit must have a unique :id across all outputs");
        }
    }

    int16_t pre_existing = pool.find_live_slot(id_buf);
    if (pre_existing >= 0) {
        // Slot already allocated. If we're inside inline expansion, reuse is fine.
        // If we're NOT in inline expansion and we already saw this id in this
        // build, it's a true source-level duplicate.
        bool seen_this_build = false;
        for (uint8_t i = 0; i < live_edit_ids_count; i++) {
            if (strncmp(live_edit_ids_seen[i], id_buf, MAX_LIVE_SLOT_ID) == 0) {
                seen_this_build = true;
                break;
            }
        }
        if (seen_this_build && inline_depth == 0) {
            return report_error_at_cat(DiagnosticCategory::Boundary, form_start, 1,
                "duplicate live-edit :id in this document",
                "Each live-edit must have a unique :id");
        }
        // Reuse existing slot (inline expansion or re-eval)
        if (!seen_this_build && live_edit_ids_count < MAX_IDS_PER_BUILD) {
            strncpy(live_edit_ids_seen[live_edit_ids_count], id_buf, MAX_LIVE_SLOT_ID - 1);
            live_edit_ids_seen[live_edit_ids_count][MAX_LIVE_SLOT_ID - 1] = '\0';
            live_edit_ids_count++;
        }
        // Register in shared cross-output table
        if (shared_live_edit_ids && inline_depth == 0) {
            shared_live_edit_ids->add(id_buf);
        }
        // Update bounds (may have changed on re-eval)
        pool.live_slots[pre_existing].min_val = min_val;
        pool.live_slots[pre_existing].max_val = max_val;
        pool.live_slots[pre_existing].seed = seed;
        double& v = pool.live_slots[pre_existing].value;
        if (v < min_val) v = min_val;
        if (v > max_val) v = max_val;
        return pool.make_slot_load((uint16_t)pre_existing);
    }

    // First time seeing this id — record it in both local and shared tables
    if (live_edit_ids_count < MAX_IDS_PER_BUILD) {
        strncpy(live_edit_ids_seen[live_edit_ids_count], id_buf, MAX_LIVE_SLOT_ID - 1);
        live_edit_ids_seen[live_edit_ids_count][MAX_LIVE_SLOT_ID - 1] = '\0';
        live_edit_ids_count++;
    }
    if (shared_live_edit_ids && inline_depth == 0) {
        shared_live_edit_ids->add(id_buf);
    }

    // 5. Allocate slot
    int16_t slot_idx = pool.alloc_live_slot(id_buf, seed, min_val, max_val);
    if (slot_idx < 0) {
        return report_error_at_cat(DiagnosticCategory::Overflow, form_start, 1,
            MAX_LIVE_SLOTS == 256
                ? "too many live-edit slots (max 256)"
                : "too many live-edit slots (max 32)",
            "Remove unused live-edit declarations");
    }

    // 6. Return SlotLoad node
    return pool.make_slot_load((uint16_t)slot_idx);
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
            return report_error_at_cat(DiagnosticCategory::Type, 0, 0,
                "range needs values known at compile time",
                "Try: (range 1 8) or use a literal vector [1 2 3 4 5 6 7]");
        }
        end_val = const_value(arg1);
    } else {
        uint16_t arg2 = compile_expr(ts, scope, ctx);
        if (ts.peek().kind == TokenKind::RParen) {
            if (!is_const(arg1) || !is_const(arg2)) {
                return report_error_at_cat(DiagnosticCategory::Type, 0, 0,
                    "range needs values known at compile time",
                    "Try: (range 1 8) or use a literal vector");
            }
            start = const_value(arg1);
            end_val = const_value(arg2);
        } else {
            uint16_t arg3 = compile_expr(ts, scope, ctx);
            if (!is_const(arg1) || !is_const(arg2) || !is_const(arg3)) {
                return report_error_at_cat(DiagnosticCategory::Type, 0, 0,
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

    // Store as data table and return the length as a constant
    cells.store_data_table(values, count);
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

        // Store in data pool
        uint16_t table_id = cells.store_data_table(values, count);
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
        report_error_at_cat(DiagnosticCategory::Type, tok.span_start, tok.span_len,
            "Expected a vector or data reference",
            "Try: [1 0 1 0] or a defined vector name");
        return ref;
    }

    // Quoted list '(1 0 1 0) — treat as vector
    // TODO: handle quoted lists

    report_error_at_cat(DiagnosticCategory::Type, tok.span_start, tok.span_len,
        "Expected a vector or data reference",
        "Try: [1 0 1 0] or a defined vector name");
    return ref;
}

// ── Top-level build function ────────────────────────────────────────────────

GraphBuildResult build_output_graph(
    NodePool& pool,
    TokenStream& ts,
    CellStore& cells,
    const SourceArena& source,
    const char* source_base,
    StateResourceRegistry* registry,
    SharedLiveEditIDs* shared_ids
) {
    GraphBuilder builder(pool, cells, source);
    builder.source_base = source_base;
    builder.registry = registry;
    builder.shared_live_edit_ids = shared_ids;
    Scope root_scope = {};
    TimeContext ctx = { pool.make_raw_time_load() };

    uint16_t root = builder.compile_expr(ts, root_scope, ctx);

    // Warning #3: check for live-edit slots allocated but never read in this graph.
    // Walk from root, collect all SlotLoad imm values, then check which freshly
    // allocated slots (those with index >= live_slot_count_at_start) are missing.
    if (!builder.has_error && root != NODE_NONE &&
        pool.live_slot_count > builder.live_slot_count_at_start) {
        // Collect referenced slot indices via DFS from root
        bool slot_referenced[MAX_LIVE_SLOTS] = {};
        uint16_t walk_stack[MAX_TOTAL_NODES];
        uint16_t walk_top = 0;
        bool visited[MAX_TOTAL_NODES] = {};
        walk_stack[walk_top++] = root;
        while (walk_top > 0) {
            uint16_t ni = walk_stack[--walk_top];
            if (ni == NODE_NONE || ni >= pool.node_count || visited[ni]) continue;
            visited[ni] = true;
            const Node& n = pool.nodes[ni];
            if (n.op == NodeOp::SlotLoad) {
                uint16_t slot_idx = (uint16_t)n.imm;
                if (slot_idx < MAX_LIVE_SLOTS) slot_referenced[slot_idx] = true;
            }
            if (n.input_a != NODE_NONE && walk_top < MAX_TOTAL_NODES) walk_stack[walk_top++] = n.input_a;
            if (n.input_b != NODE_NONE && walk_top < MAX_TOTAL_NODES) walk_stack[walk_top++] = n.input_b;
            if (n.input_c != NODE_NONE && walk_top < MAX_TOTAL_NODES) walk_stack[walk_top++] = n.input_c;
        }

        // Emit warnings for unreferenced freshly-allocated slots
        for (uint16_t s = builder.live_slot_count_at_start;
             s < pool.live_slot_count; s++) {
            if (!slot_referenced[s] &&
                builder.diagnostic_count < MAX_DIAGNOSTICS) {
                builder.diagnostics[builder.diagnostic_count++] = {
                    DiagnosticSeverity::Warning,
                    DiagnosticCategory::Boundary,
                    0, 0,
                    "live-edit slot allocated but never read in this signal graph",
                    "Ensure the live-edit value is used in the output expression"
                };
            }
        }
    }

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
