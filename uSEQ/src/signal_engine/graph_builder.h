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

    // Cell dependencies discovered during graph compilation
    SymbolID dep_cells[MAX_OUTPUT_DEPS] = {};
    uint8_t dep_count = 0;
};

// ── Graph Builder ───────────────────────────────────────────────────────────

struct GraphBuilder {
    NodePool& pool;
    CellStore& cells;
    const SourceArena& source;
    const char* source_base = nullptr; // raw tokenized text for string resolution

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

    // Live-edit: slot count at build start (to detect fresh allocations vs pre-existing)
    uint16_t live_slot_count_at_start = 0;
    // Live-edit ids seen during this build (duplicate detection within one graph)
    char live_edit_ids_seen[MAX_LIVE_SLOTS][MAX_LIVE_SLOT_ID] = {};
    uint8_t live_edit_ids_count = 0;

    // ── Well-known symbol IDs (populated at init) ───────────────────────
    // Generated from symbols.def — do not edit by hand.
    struct Symbols {
        #define SYM(field, str, cat) SymbolID field;
        #include "symbols.def"
        #undef SYM
    };
    static Symbols sym;
    static void init_symbols();
    static bool symbols_initialized;

    // ── Form dispatch table ─────────────────────────────────────────────
    struct FormEntry {
        SymbolID sym;
        uint16_t (GraphBuilder::*handler)(TokenStream&, Scope&, TimeContext&);
    };
    static constexpr uint16_t FORM_TABLE_CAPACITY = 64;
    static FormEntry form_table[FORM_TABLE_CAPACITY];
    static uint16_t form_table_count;
    static bool form_table_sorted;
    static void init_form_table();

    // ── Construction ────────────────────────────────────────────────────

    GraphBuilder(NodePool& pool, CellStore& cells, const SourceArena& source);

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
    uint16_t compile_loop_at(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_eval_at_time(TokenStream& ts, Scope& scope, TimeContext& ctx);

    // Control flow
    uint16_t compile_if(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_let(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_do(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_for(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_while_gate(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_lambda(TokenStream& ts, Scope& scope, TimeContext& ctx);

    // Output feedback
    uint16_t compile_prev(TokenStream& ts, Scope& scope, TimeContext& ctx);

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
    uint16_t compile_gatesw(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_range(TokenStream& ts, Scope& scope, TimeContext& ctx);

    // Ratio-rhythm functions
    uint16_t compile_rpulse(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_rstep(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_ridx(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_rwarp(TokenStream& ts, Scope& scope, TimeContext& ctx);

    // State
    uint16_t compile_integrate(TokenStream& ts, Scope& scope, TimeContext& ctx);

    // UGens
    uint16_t compile_phasor(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_lfo(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_lfo_sin(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_lfo_tri(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_lfo_saw(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_lfo_sqr(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_slew(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_one_pole(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_env_follow(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_sah(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_noise(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_toggle(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_count(TokenStream& ts, Scope& scope, TimeContext& ctx);

    // UGen helpers
    uint16_t alloc_state_slot(double init_value);
    uint16_t build_lfo(TokenStream& ts, Scope& scope, TimeContext& ctx, uint16_t default_wave);
    uint16_t build_osc_output(uint16_t state_load, uint16_t wave_type, uint16_t pw_node);

    // Live-edit
    uint16_t compile_live_edit(TokenStream& ts, Scope& scope, TimeContext& ctx);

    // Random / hash
    uint16_t compile_random(TokenStream& ts, Scope& scope, TimeContext& ctx);
    uint16_t compile_index_rand(TokenStream& ts, Scope& scope, TimeContext& ctx);

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

    // Category-aware error reporting — prefer these over the Runtime-defaulting overloads
    uint16_t report_error_cat(DiagnosticCategory cat, const Token& tok,
                              const char* message, const char* suggestion = nullptr);
    uint16_t report_error_at_cat(DiagnosticCategory cat,
                                 uint16_t span_start, uint16_t span_len,
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
    CellStore& cells,
    const SourceArena& source,
    const char* source_base = nullptr
);

} // namespace sig

#endif // SIGNAL_ENGINE_GRAPH_BUILDER_H
