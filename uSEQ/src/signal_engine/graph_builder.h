#ifndef SIGNAL_ENGINE_GRAPH_BUILDER_H
#define SIGNAL_ENGINE_GRAPH_BUILDER_H

#include "types.h"
#include "token.h"
#include "node_pool.h"
#include "cell_store.h"
#include "diagnostics.h"

namespace sig {

// ── Scope (local bindings for let, lambda params, for variables) ────────────

struct Scope {
    struct Binding {
        SymbolID name;
        uint16_t node_index;
    };
    Binding locals[MAX_LOCAL_BINDINGS] = {};
    uint8_t local_count = 0;
    Scope* parent       = nullptr;

    void bind(SymbolID name, uint16_t node_index);
    const Binding* find(SymbolID name) const;
};

// ── Time Context ────────────────────────────────────────────────────────────

struct TimeContext {
    uint16_t t_node; // node index representing "current t" (may be transformed)
};

// ── Graph Build Result ──────────────────────────────────────────────────────

struct GraphBuildResult {
    uint16_t root_node = NODE_NONE;
    Diagnostic diagnostics[MAX_DIAGNOSTICS] = {};
    uint8_t diagnostic_count = 0;
    bool has_error = false;
};

// ── Graph Builder ───────────────────────────────────────────────────────────

struct GraphBuilder {
    NodePool& pool;
    const CellStore& cells;
    const SourceArena& source;

    // Diagnostics output
    Diagnostic diagnostics[MAX_DIAGNOSTICS] = {};
    uint8_t diagnostic_count = 0;
    bool has_error = false;

    // Dependency tracking (populated during build)
    SymbolID dep_cells[MAX_OUTPUT_DEPS] = {};
    uint8_t dep_count = 0;

    // Recursion guard for inline stack
    SymbolID inline_stack[MAX_INLINE_DEPTH] = {};
    uint8_t inline_depth = 0;

    // ── Well-known symbol IDs (populated at init) ───────────────────────
    // These are cached lookups to avoid repeated intern() calls.
    struct Symbols {
        SymbolID t, beat, bar, phrase, section, beat_num, bar_num;
        SymbolID bpm, beats_per_bar, bars_per_phrase, phrases_per_section;
        SymbolID fast, slow, offset, shift;
        SymbolID if_, let_, do_, for_, while_, fn, lambda;
        SymbolID define, def, defn, defun, defs, set;
        SymbolID step, gates, trigs, euclid, eu;
        SymbolID seq, from_list, interp, flatseq, dm;
        SymbolID range;
        SymbolID sin_, cos_, tan_, abs_, floor_, ceil_, sqrt_, neg, frac_;
        SymbolID min_, max_, pow_, mod_, clamp;
        SymbolID mod_pct;
        SymbolID input;
        SymbolID not_, and_, or_;
        SymbolID tri, sqr, pulse, usin, ucos;
        SymbolID bi_to_uni, b_to_u, uni_to_bi, u_to_b;
        SymbolID scale, lerp;
        SymbolID quote;
        SymbolID scope;
    };
    static Symbols sym;
    static void init_symbols();
    static bool symbols_initialized;

    // ── Construction ────────────────────────────────────────────────────

    GraphBuilder(NodePool& pool, const CellStore& cells, const SourceArena& source);

    // ── Compilation entry points ────────────────────────────────────────

    uint16_t compile_expr(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_symbol(SymbolID sym, Scope& scope, TimeContext& ctx,
                            uint16_t span_start, uint16_t span_len);
    uint16_t compile_form(SymbolID op, TokenStream& ts, Scope& scope,
                          TimeContext& ctx, Token op_tok);

    // ── Form handlers ───────────────────────────────────────────────────

    // Time transforms
    uint16_t compile_fast(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_slow(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_offset(TokenStream& ts, Scope& scope, TimeContext& ctx);

    // Control flow
    uint16_t compile_if(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_let(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_do(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_for(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_while_gate(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_lambda(TokenStream& ts, Scope& scope, TimeContext& ctx);

    // User-defined function call
    uint16_t compile_call(SymbolID fn_sym, TokenStream& ts, Scope& scope,
                          TimeContext& ctx, Token op_tok);

    // Arithmetic and math
    uint16_t compile_variadic_arithmetic(SymbolID op, TokenStream& ts,
                                         Scope& scope, TimeContext& ctx);
    uint16_t compile_comparison(SymbolID op, TokenStream& ts,
                                Scope& scope, TimeContext& ctx);
    uint16_t compile_logic(SymbolID op, TokenStream& ts,
                           Scope& scope, TimeContext& ctx);
    uint16_t compile_unary_math(NodeOp op, TokenStream& ts,
                                Scope& scope, TimeContext& ctx);
    uint16_t compile_binary_math(NodeOp op, TokenStream& ts,
                                 Scope& scope, TimeContext& ctx);
    uint16_t compile_ternary_math(NodeOp op, TokenStream& ts,
                                  Scope& scope, TimeContext& ctx);

    // Domain-specific signal functions
    uint16_t compile_step(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_gates(SymbolID op, TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_euclid(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_seq(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_interp(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_dm(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_range(TokenStream& ts, Scope& scope, TimeContext& ctx);

    // Vector literal [1 2 3]
    uint16_t compile_vector_literal(TokenStream& ts, Scope& scope, TimeContext& ctx);

    // ── Temporal templates ──────────────────────────────────────────────

    uint16_t expand_beat(TimeContext& ctx);
    uint16_t expand_bar(TimeContext& ctx);
    uint16_t expand_phrase(TimeContext& ctx);
    uint16_t expand_section(TimeContext& ctx);
    uint16_t expand_beat_num(TimeContext& ctx);
    uint16_t expand_bar_num(TimeContext& ctx);

    // ── Collection resolution (for `for` loops) ─────────────────────────

    struct Collection {
        uint16_t element_nodes[64] = {};
        uint16_t count = 0;
        bool ok = false;
    };
    Collection resolve_collection(TokenStream& ts, Scope& scope, TimeContext& ctx);

    // ── Data table resolution ───────────────────────────────────────────

    struct DataRef {
        uint16_t table_id;
        uint16_t length;
        bool ok;
    };
    DataRef resolve_data_table(TokenStream& ts, Scope& scope, TimeContext& ctx);

    // ── Dependency tracking ─────────────────────────────────────────────

    void add_dependency(SymbolID sym);

    // ── Inline stack ────────────────────────────────────────────────────

    bool is_in_inline_stack(SymbolID sym) const;
    void push_inline_stack(SymbolID sym);
    void pop_inline_stack();

    // ── Inline expression cell ──────────────────────────────────────────

    uint16_t inline_expression_cell(SymbolID sym, const CallableInfo& info,
                                    Scope& scope, TimeContext& ctx);

    // ── Error reporting ─────────────────────────────────────────────────

    uint16_t report_error(const Token& tok, const char* message,
                          const char* suggestion = nullptr);
    uint16_t report_error_at(uint16_t span_start, uint16_t span_len,
                             const char* message, const char* suggestion = nullptr);
    uint16_t report_error_with_fuzzy_match(SymbolID sym,
                                            uint16_t span_start, uint16_t span_len);
    uint16_t report_warning(uint16_t span_start, uint16_t span_len,
                            const char* message, const char* suggestion = nullptr);

    // ── Helpers ─────────────────────────────────────────────────────────

    bool is_const(uint16_t node_idx) const;
    double const_value(uint16_t node_idx) const;
    bool is_side_effect_form(SymbolID op) const;
    bool is_arithmetic_op(SymbolID op) const;
    bool is_comparison_op(SymbolID op) const;
    bool is_logic_op(SymbolID op) const;
    bool is_unary_math(SymbolID op) const;
    bool is_binary_math(SymbolID op) const;
    bool is_ternary_math(SymbolID op) const;
    static bool is_output_symbol(SymbolID op);
    static uint16_t resolve_output_index(SymbolID op);
    static uint16_t resolve_hardware_input(SymbolID sym);
    NodeOp arithmetic_sym_to_op(SymbolID op) const;
    NodeOp comparison_sym_to_op(SymbolID op) const;
    NodeOp unary_sym_to_op(SymbolID op) const;

    // Skip past a complete form in the token stream (for deferred parsing).
    static void skip_form(TokenStream& ts);
};

// ── Top-level build function ────────────────────────────────────────────────

GraphBuildResult build_output_graph(
    NodePool& pool,
    TokenStream& ts,
    const CellStore& cells,
    const SourceArena& source
);

} // namespace sig

#endif // SIGNAL_ENGINE_GRAPH_BUILDER_H
