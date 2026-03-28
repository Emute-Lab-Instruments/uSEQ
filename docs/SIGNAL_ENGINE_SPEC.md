# Signal Engine Spec

## Overview

This spec defines a ground-up redesign of the signal execution engine. The core diagnosis: runtime bridges from compiled signal graphs back into interpreter-shaped evaluation are the central architectural problem. The redesign goes further — eliminating temporal operations, closures, environments, and the Value type as separate concepts entirely.

An earlier proposal (6-stage pipeline, explicit IR, full tagged VM) was rejected as over-scoped. The validated insight was the bridge problem; the solution is simpler than the proposal imagined.

The result: a reactive graph of arithmetic nodes with one input (`t`), cells, and hardware inputs. It replaces ~8,000 lines of compiler, VM, value system, and environment machinery with ~1,200 lines of graph builder and executor.

---

## 1. Core Model

### 1.1 What the System Is

The system is a **reactive signal machine** that turns text into voltages (on hardware) or sample streams (in WASM). It is not a general-purpose programming language runtime. It is a spreadsheet where some cells are sampled at control rates (or, where possible, approaching audio rates).

Every output expression is a **pure function of time and named parameters**:

```
f(t, cells, inputs) → double
```

There are no side effects, no mutable state, no allocation, and no control flow on the signal path. "Stateful" constructs like step sequencers are pure functions of time (`step([1,0,1,0]) = data[floor(beat * len) % len]`, which is a pure function of `t` and `bpm`).

### 1.2 Implicit Lifting — Every Expression Is a Signal

The fundamental computational model is **implicit lifting**: every expression in output context is implicitly a function of time.

```lisp
t             == (fn [t] t)           ;; the identity signal
(+ 1 t)       == (fn [t] (+ 1 t))    ;; offset: constant lifted to constant signal
(fast 2 t)    == (fn [t] (* 2 t))    ;; time scaling: arithmetic on t
(sin (* t f)) == (fn [t] (sin (* t f))) ;; compound signal
```

Constants are "lifted" to constant signals. Operators combine signals pointwise. Time transforms (`fast`, `slow`, `offset`) are arithmetic transformations of the implicit `t` parameter. This is the standard functional reactive programming (FRP) model, but users never see the `fn [t]` wrapper — it is always implicit.

### 1.3 The Only Input

There is exactly **one true input variable**: `t` (raw time in seconds, provided by the system each sample).

Everything else is derived:
- `beat`, `bar`, `phrase`, `section` are expressions of `t` and timing cells (`bpm`, `beats-per-bar`, etc.)
- `fast`, `slow`, `offset` are arithmetic transformations of `t`
- Hardware inputs (`in1`, `ain1`, knobs, switches) are external values, read per sample
- Named parameters (`freq`, `scale`, etc.) are cells in a global table

The signal executor has zero concept of "time," "beats," or "tempo." It sees: one double input, a cell table, hardware input values, and arithmetic nodes.

### 1.3 Four Operations

The entire user interface consists of four operations:

1. **Define** — create or update a named cell: `(define freq 440)`
2. **Connect** — wire an expression to an output: `(a1 (sin (* t freq)))`
3. **Change** — redefine a cell; dependents update automatically: `(define freq 880)`
4. **Trigger** — fire a side effect: `(set-bpm 120)`, `(useq-play)`

Nothing else exists at the system level.

### 1.5 Key Design Principles

These principles are load-bearing — the architecture depends on them.

**1. One language, compiler limitations not language restrictions.** The user experiences ModuLisp as one language. When an expression can't be used in an output, the diagnostic says "this expression is not yet supported in outputs" — framing it as a compiler limitation that may improve, not a permanent language rule.

**2. Pervasive constant folding + aggressive inlining + dirty recompilation = reactive behavior.** Every node constructor (unary, binary, ternary) folds constant inputs immediately — no separate optimization pass. Because folding is transitive, arbitrarily deep pure subexpressions collapse to single `Const` nodes at build time. This is what makes `(for x (range 1 8) ...)` work: `range` compiles to arithmetic that folds to constants. The system also aggressively inlines user-defined functions and bakes cell values. When a dependency changes, the dependency tracker marks affected outputs dirty and triggers recompilation. Sub-millisecond recompilation (50-100 nodes) makes this indistinguishable from live cell reads. This is the "sea of nodes" model: hardcode everything, recompile incrementally.

**3. The numeric fast-path cliff is eliminated by construction.** In the old system, a single `CALL_INTRINSIC` forced the entire program off the fast numeric executor onto the slow tagged executor — a system-wide performance cliff. The new design has no intrinsics, no bridges, no tagged executor. Every signal program is a flat array of arithmetic nodes. If it compiles, it runs at full speed. If it can't compile, it's a compile error, not a runtime fallback.

**4. Worker-ready compiled artifacts.** Worker-based WASM execution is a near-term goal. Compiled graphs are inherently worker-transferable: they contain no `std::function`, no `Environment*`, no pointers. A `CompiledGraph` + cell table snapshot can be sent to a Web Worker and executed there. This property falls out of the design, not as an optimization added later.

**5. Firmware is primary, WASM is secondary.** The firmware path is the most important. WASM is secondary. The two can diverge in implementation details (e.g., WASM may use batch/SOA execution while firmware uses single-sample) as long as they share a common test suite that verifies semantic equivalence.

**6. Fixed memory, no heap on the hot path.** All hot-path data structures (node pool, cell table, data pool, workspace) are fixed-size arrays. No `std::vector`, no `std::string`, no `std::shared_ptr`, no `new`, no `malloc` on the signal path. The `USE_FIXED_MEMORY_POOLS` build flag in the current system is aspirational (no allocator behind it). The new design makes it structural — heap allocation is impossible by construction.

**7. Never stop the music.** LKG (last-known-good) fallback ensures outputs keep producing sound even during errors. Parse errors preserve the previous output. Compile errors fall back to LKG. Runtime NaN/Inf is replaced with 0.0 per node. The output is never silent unless the user explicitly clears it.

### 1.6 What the Current System Got Right (Preserved)

The current system got these things right. The new design preserves all of them:

- Unboxed numeric execution (doubles, not tagged values, on the signal path)
- Structured diagnostics with source spans, human-readable messages, suggestions
- Dependency tracking by symbol ID and revision counter
- Batch execution APIs for WASM visualization
- LKG fallback strategy
- Compile-time time-transform lowering (now generalized to arbitrary expressions)
- Direct lowering for common math and vector operations (sin, cos, step, gates, etc.)

---

## 2. Data Structures

### 2.1 Cell Table

Replaces `Value`, `Environment`, and `shared_ptr<Environment>`.

```cpp
constexpr size_t MAX_CELLS = 512;
constexpr size_t MAX_CALLABLE_PARAMS = 8;
constexpr size_t MAX_DATA_ENTRIES = 2048;   // total doubles across all data tables
constexpr size_t MAX_DATA_TABLES = 64;

enum class CellKind : uint8_t {
    Empty,       // unused slot
    Number,      // double constant
    Data,        // numeric array (for vectors, step patterns, scale tables)
    Callable,    // user function template (params + body token offset)
    Nil          // explicitly nil
};

struct Cell {
    CellKind kind;
    uint8_t flags;          // 0x01 = frozen (won't change during performance)
    uint16_t data_table_id; // for Data cells: index into shared data pool
    uint32_t revision;      // bumped on every change; used for dirty detection
    double value;           // for Number cells; for Data cells: length as double
};
// sizeof(Cell) = 16 bytes. Fixed array, no heap.

struct CallableInfo {
    SymbolID params[MAX_CALLABLE_PARAMS];
    uint8_t param_count;
    uint8_t pad[3];
    uint32_t source_offset; // byte offset into source arena where body tokens start
    uint32_t source_length; // byte length of body tokens
};
// sizeof(CallableInfo) = 24 bytes. Separate array indexed by cell index.
// Only populated for Callable cells.

struct CellStore {
    Cell cells[MAX_CELLS];
    CallableInfo callables[MAX_CELLS]; // parallel array; only valid when cells[i].kind == Callable

    // Shared data pool for all Data cells
    double data_pool[MAX_DATA_ENTRIES];
    uint16_t data_offsets[MAX_DATA_TABLES]; // where each table starts in data_pool
    uint16_t data_lengths[MAX_DATA_TABLES]; // how many entries in each table
    uint8_t data_table_count;

    // Symbol interning (reuse existing SymbolIntern)
    // cells are indexed by SymbolID
};
```

**What this replaces:**
- `Value` (88 bytes, 13 type variants, heap allocations) → `Cell` (16 bytes, 5 kinds, no heap)
- `Environment` (string-keyed `std::map`, parent scope chains, full-copy semantics) → flat `Cell` array indexed by `SymbolID`
- `shared_ptr<Environment>` for closures → `CallableInfo` with token offsets; inlined at graph build time
- `std::vector<Value>` for lists/vectors → `data_pool` with offset/length pairs

### 2.2 Node Pool (Shared Hash-Consed Graph)

Replaces `NumericVmProgram`, `NumericVmInstruction`, `std::vector<TaggedVmIntrinsic>`, and `std::vector<std::shared_ptr<NumericVmProgram>>`.

```cpp
constexpr size_t MAX_TOTAL_NODES = 1024;  // across all outputs combined
constexpr size_t MAX_OUTPUTS = 42;        // 14 analog + 14 digital + 14 serial (configurable per variant)

enum class NodeOp : uint8_t {
    // Constants and loads
    Const,          // imm = value
    RawTimeLoad,    // imm = unused (loads the single 't' input)
    CellLoad,       // imm = cell_id (loads cell's current double value)
    InputLoad,      // imm = input_index (loads hardware input channel)
    DataLoad,       // input_a = index_node, imm = data_table_id

    // Binary arithmetic
    Add, Sub, Mul, Div, Mod, Pow, Min, Max,

    // Unary math
    Neg, Abs, Floor, Ceil, Frac, Sqrt, Clamp, // Clamp uses input_a=val, input_b=min, input_c=max

    // Trigonometry
    Sin, Cos, Tan,

    // Domain waveforms (operate on a phase [0,1) input)
    USin,       // unipolar sine: (sin(2π × phase) + 1) / 2
    UCos,       // unipolar cosine
    USinBi,     // bipolar unipolar sine (signed unipolar)
    UCosBi,     // bipolar unipolar cosine
    Tri,        // triangle wave from phase
    Sqr,        // square wave from phase (0 or 1)
    Pulse,      // pulse wave: input_a = phase, input_b = width

    // Comparison (return 1.0 for true, 0.0 for false)
    CmpGt, CmpLt, CmpGe, CmpLe, CmpEq,

    // Logic (truthy = non-zero)
    Not, And, Or,

    // Control flow
    Select,     // input_a = condition, input_b = true_val, input_c = false_val

    // Vector/data operations
    VecIndex,   // input_a = fractional_index, imm = table_id; floor index, bounds wrap
    VecLerp,    // input_a = fractional_index, imm = table_id; linear interpolation

    // Modular arithmetic (for phasor computation)
    Fmod,       // input_a = value, input_b = modulus; result = fmod(a, b)

    // Range conversion
    BiToUni,    // bipolar [-1,1] → unipolar [0,1]
    UniToBi,    // unipolar [0,1] → bipolar [-1,1]
    Scale,      // 3 inputs: value, out_min, out_max (maps [0,1] to [min,max])
    Scale5,     // 5 inputs: value, in_min, in_max, out_min, out_max (full range map)
    Lerp,       // 3 inputs: a, b, t → a + (b - a) * t
};

struct Node {
    NodeOp op;
    uint8_t flags;      // FLAG_TIME_INVARIANT = 0x01 (computed once per batch, not per sample)
    uint16_t input_a;   // index into node pool (0xFFFF = unused)
    uint16_t input_b;   // index into node pool (0xFFFF = unused)
    uint16_t input_c;   // index into node pool (0xFFFF = unused)
    double imm;         // immediate value (constant, cell_id, table_id, input_index)
    uint16_t span_start;// source location for diagnostics
    uint16_t span_len;  // source location for diagnostics
};
// sizeof(Node) = 20 bytes. Compare to NumericVmInstruction at 32 bytes.

struct NodePool {
    Node nodes[MAX_TOTAL_NODES];
    uint16_t node_count;

    // Hash table for CSE (hash of node content → node index)
    // Implementation: open-addressing hash map, fixed size
    uint32_t cse_hashes[MAX_TOTAL_NODES * 2]; // ~2× load factor
    uint16_t cse_indices[MAX_TOTAL_NODES * 2];

    // Topologically sorted execution order (computed once per recompilation)
    uint16_t exec_order[MAX_TOTAL_NODES];
    uint16_t exec_count; // may be less than node_count (dead nodes excluded)

    // Per-output metadata
    struct OutputSlot {
        uint16_t root_node;         // index into nodes[] (0xFFFF = no expression assigned)
        double lkg_value;           // last known good output value
        bool valid;                 // did this output succeed on last sample?
        uint8_t pad[7];
    };
    OutputSlot outputs[MAX_OUTPUTS];

    // Error isolation: which outputs depend on each node?
    // Bit i set → output i depends on this node
    uint64_t output_deps[MAX_TOTAL_NODES]; // supports up to 64 outputs

    // Shared data tables (same as in CellStore, or referencing CellStore's data)
    // The node pool can reference CellStore's data_pool directly via DataLoad nodes
};
```

**What this replaces:**
- `NumericVmProgram` (vectors of instructions, constants, data segments, functions, intrinsics) → flat `Node` array with hash-consed sharing
- `NumericVmInstruction` (32 bytes) → `Node` (20 bytes)
- Per-output separate programs → shared pool with per-output root indices
- `CALL_INTRINSIC` + `std::function` bridges → gone entirely; if it can't be a node, it's a compile error
- `StoredOutput` (heavy, with `Value expr`, `shared_ptr<NumericVmProgram>`) → `OutputSlot` (16 bytes)
- Two separate executors (fast numeric + tagged) → one executor

### 2.3 Source Arena

Stores source text for callable bodies and diagnostic messages. Replaces the role of `Value` trees as callable storage.

```cpp
constexpr size_t SOURCE_ARENA_SIZE = 16384; // 16KB; sufficient for typical live-coding sessions

struct SourceArena {
    char data[SOURCE_ARENA_SIZE];
    uint32_t write_head;

    // Store source text, return offset
    uint32_t store(const char* text, uint32_t length);

    // Read back source text
    const char* read(uint32_t offset, uint32_t length) const;

    // Compaction: copy live entries to front, update cell references
    void compact(CellStore& cells);
};
```

When the user writes `(defn osc [f ph] (sin (* ph f)))`, the body `(sin (* ph f))` is copied into the source arena. The callable cell stores the offset and length. When the graph builder inlines the callable, it reads from the arena.

### 2.4 Token Stream

Replaces the `Value` AST as the parser output. Zero-allocation tokenization.

```cpp
enum class TokenKind : uint8_t {
    LParen, RParen, LBracket, RBracket,
    Number, Symbol, String,
    Eof, Error
};

struct Token {
    TokenKind kind;
    uint8_t pad;
    uint16_t span_start; // offset in source text
    uint16_t span_len;
    union {
        double number;
        SymbolID symbol;
        struct { uint32_t offset; uint16_t length; } string; // into source arena
    };
};
// sizeof(Token) = 16 bytes.

struct TokenStream {
    Token tokens[256]; // fixed buffer; sufficient for any single expression
    uint16_t count;
    uint16_t pos;      // current read position

    Token peek() const;
    Token consume();
    void rewind(uint16_t position);

    // Tokenize from source text
    static uint16_t tokenize(const char* source, uint32_t length,
                             Token* out, uint16_t max_tokens,
                             Diagnostic* errors, uint8_t* error_count);
};
```

**Key property:** Tokenization is zero-allocation. The token buffer is stack-allocated or statically allocated. No `std::vector`, no `std::string`, no heap.

---

## 3. The Graph Builder

Replaces the 6,200-line bytecode compiler (`bytecode_vm.cpp`). Estimated size: ~600-800 lines.

### 3.1 Architecture

The graph builder is a recursive-descent compiler that reads tokens and emits nodes directly into the shared node pool. It combines parsing, name resolution, constant folding, CSE, closure inlining, and time-transform application into a single pass.

```cpp
struct Scope {
    struct Binding {
        SymbolID name;
        uint16_t node_index; // which node produces this local's value
    };
    Binding locals[32];  // max local bindings per scope level
    uint8_t local_count;
    Scope* parent;       // for nested let/lambda scopes
};

struct TimeContext {
    uint16_t t_node; // node index representing "current t" (may be transformed)
};

struct GraphBuildResult {
    uint16_t root_node;        // root of the compiled expression (0xFFFF on failure)
    Diagnostic diagnostics[16]; // compile-time diagnostics
    uint8_t diagnostic_count;
    bool has_error;
};

GraphBuildResult build_output_graph(
    NodePool& pool,
    const Token* tokens, uint16_t token_count,
    const CellStore& cells,
    const SourceArena& source
);
```

### 3.2 Node Construction with Automatic CSE and Constant Folding

Every node is created through helper functions that check for optimization opportunities. **Constant folding is pervasive**: any node whose inputs are all `Const` is evaluated immediately and replaced with a single `Const` node. This applies uniformly to binary ops, unary math, comparisons, ternary ops — every pure operation. Most ModuLisp functions *are* pure, so this collapses large constant subgraphs at build time for free.

```cpp
uint16_t make_const(NodePool& pool, double value) {
    Node n = { NodeOp::Const, FLAG_TIME_INVARIANT, 0xFFFF, 0xFFFF, 0xFFFF, value, 0, 0 };
    return intern_node(pool, n); // hash-cons: if identical Const exists, reuse it
}

uint16_t make_unary(NodePool& pool, NodeOp op, uint16_t a) {
    Node& na = pool.nodes[a];

    // Constant folding: pure unary on constant → evaluate immediately
    if (na.op == NodeOp::Const) {
        return make_const(pool, eval_unary(op, na.imm));
    }

    uint8_t flags = na.flags & FLAG_TIME_INVARIANT; // propagate
    Node n = { op, flags, (uint16_t)a, 0xFFFF, 0xFFFF, 0.0, 0, 0 };
    return intern_node(pool, n);
}

uint16_t make_binop(NodePool& pool, NodeOp op, uint16_t a, uint16_t b) {
    Node& na = pool.nodes[a];
    Node& nb = pool.nodes[b];

    // Constant folding: pure binary on two constants → evaluate immediately
    if (na.op == NodeOp::Const && nb.op == NodeOp::Const) {
        return make_const(pool, eval_binop(op, na.imm, nb.imm));
    }

    // Algebraic simplification
    if (op == NodeOp::Add && nb.op == NodeOp::Const && nb.imm == 0.0) return a;
    if (op == NodeOp::Mul && nb.op == NodeOp::Const && nb.imm == 1.0) return a;
    if (op == NodeOp::Mul && nb.op == NodeOp::Const && nb.imm == 0.0) return make_const(pool, 0.0);
    if (op == NodeOp::Sub && a == b) return make_const(pool, 0.0);
    if (op == NodeOp::Div && a == b) return make_const(pool, 1.0);
    // ... additional identities

    // Time-invariance propagation
    uint8_t flags = 0;
    if ((na.flags & FLAG_TIME_INVARIANT) && (nb.flags & FLAG_TIME_INVARIANT)) {
        flags |= FLAG_TIME_INVARIANT;
    }

    Node n = { op, flags, (uint16_t)a, (uint16_t)b, 0xFFFF, 0.0, 0, 0 };
    return intern_node(pool, n); // CSE: if identical node exists, reuse it
}

uint16_t make_ternary(NodePool& pool, NodeOp op, uint16_t a, uint16_t b, uint16_t c) {
    Node& na = pool.nodes[a];
    Node& nb = pool.nodes[b];
    Node& nc = pool.nodes[c];

    // Constant folding: pure ternary on three constants → evaluate immediately
    if (na.op == NodeOp::Const && nb.op == NodeOp::Const && nc.op == NodeOp::Const) {
        return make_const(pool, eval_ternary(op, na.imm, nb.imm, nc.imm));
    }

    uint8_t flags = 0;
    if ((na.flags & FLAG_TIME_INVARIANT) && (nb.flags & FLAG_TIME_INVARIANT)
        && (nc.flags & FLAG_TIME_INVARIANT)) {
        flags |= FLAG_TIME_INVARIANT;
    }

    Node n = { op, flags, (uint16_t)a, (uint16_t)b, (uint16_t)c, 0.0, 0, 0 };
    return intern_node(pool, n);
}
```

The `eval_unary`, `eval_binop`, and `eval_ternary` functions implement the same semantics as the executor's switch statement — `sin`, `cos`, `floor`, `abs`, `clamp`, `lerp`, etc. They are trivial lookup tables from `NodeOp` to the corresponding `<cmath>` call. Because every node constructor folds constants, **constant folding composes transitively**: `(sin (* 2 pi))` folds `Mul(Const(2), Const(pi))` → `Const(6.283...)` → `Sin(Const(6.283...))` → `Const(-0.0)` without any explicit multi-pass optimization.

The `intern_node` function hashes the node's content (op, inputs, imm) and checks the CSE table. Identical nodes are deduplicated automatically.

**Why this matters beyond simple arithmetic**: Because constant folding fires at every node construction site, it naturally handles sequence-producing pure functions compiled as node subgraphs. `(range 1 8)` compiles to a sequence of constant nodes `[1, 2, 3, 4, 5, 6, 7]` — each element is the result of folded arithmetic. `(for x (range 1 8) (* x beat))` unrolls with each iteration binding `x` to a `Const` node. No special "compile-time evaluation" pass is needed; the existing per-node folding does all the work.

### 3.3 Expression Compilation

```cpp
uint16_t compile_expr(GraphBuilder& b, TokenStream& ts, Scope& scope, TimeContext& ctx) {
    Token tok = ts.peek();

    // Number literal
    if (tok.kind == TokenKind::Number) {
        ts.consume();
        return b.make_const(tok.number);
    }

    // Symbol
    if (tok.kind == TokenKind::Symbol) {
        ts.consume();
        return compile_symbol(b, tok.symbol, scope, ctx, tok.span_start, tok.span_len);
    }

    // Vector literal [1 2 3 4]
    if (tok.kind == TokenKind::LBracket) {
        return compile_vector_literal(b, ts, scope, ctx);
    }

    // List form (op args...)
    if (tok.kind == TokenKind::LParen) {
        ts.consume(); // eat '('
        Token op_tok = ts.consume();
        if (op_tok.kind != TokenKind::Symbol) {
            return b.report_error(op_tok, "Expected a function name after '('",
                                  "Try: (sin (* t 440))");
        }
        SymbolID op = op_tok.symbol;
        uint16_t result = compile_form(b, op, ts, scope, ctx, op_tok);
        ts.expect(TokenKind::RParen);
        return result;
    }

    return b.report_error(tok, "Unexpected token", "Expected a number, name, or '('");
}
```

### 3.4 Symbol Resolution

When the graph builder encounters a symbol, it resolves it in this order:

```cpp
uint16_t compile_symbol(GraphBuilder& b, SymbolID sym, Scope& scope,
                        TimeContext& ctx, uint16_t span_start, uint16_t span_len) {
    // 1. Local scope (let bindings, lambda params, for variables)
    if (auto local = scope.find(sym)) {
        return local->node_index;
    }

    // 2. Raw time input
    if (sym == SYM_T) {
        return ctx.t_node; // the current (possibly transformed) time node
    }

    // 3. Well-known temporal templates
    if (sym == SYM_BEAT)    return expand_beat(b, ctx);
    if (sym == SYM_BAR)     return expand_bar(b, ctx);
    if (sym == SYM_PHRASE)  return expand_phrase(b, ctx);
    if (sym == SYM_SECTION) return expand_section(b, ctx);
    if (sym == SYM_BEAT_NUM) return expand_beat_num(b, ctx);
    if (sym == SYM_BAR_NUM)  return expand_bar_num(b, ctx);

    // 4. Hardware inputs
    if (auto input_idx = resolve_hardware_input(sym)) {
        return b.make_node(NodeOp::InputLoad, 0xFFFF, 0xFFFF, 0xFFFF, (double)*input_idx);
    }

    // 5. Cell table
    Cell& cell = b.cells[sym];
    switch (cell.kind) {
        case CellKind::Number:
            b.add_dependency(sym);
            return b.make_const(cell.value); // bake constant; dependency triggers recompile

        case CellKind::Data:
            b.add_dependency(sym);
            // Data cells aren't loaded as a single value; they're referenced by DataLoad/VecIndex
            // If the symbol appears bare (not inside 'for' or indexing), return length
            return b.make_const(cell.value); // cell.value = length for Data cells

        case CellKind::Callable:
            // Callables are only valid in call position, not as bare symbols
            return b.report_error_at(span_start, span_len,
                   "'" + get_symbol_name(sym) + "' is a function — it needs arguments",
                   "Try: (" + get_symbol_name(sym) + " arg1 arg2)");

        case CellKind::Empty:
            return b.report_error_with_fuzzy_match(sym, span_start, span_len);

        default:
            return b.make_const(0.0); // nil
    }
}
```

### 3.5 Temporal Templates

`beat`, `bar`, etc. are not primitives. They are expression templates expanded inline by the graph builder, using the current `TimeContext` (which may have been transformed by `fast`/`slow`/`offset`).

```cpp
// beat = fmod(t_warped * (bpm / 60), 1.0)
uint16_t expand_beat(GraphBuilder& b, TimeContext& ctx) {
    uint16_t bpm = b.make_cell_load(SYM_BPM);
    uint16_t rate = b.make_binop(NodeOp::Div, bpm, b.make_const(60.0));
    uint16_t phase = b.make_binop(NodeOp::Mul, ctx.t_node, rate);
    return b.make_binop(NodeOp::Fmod, phase, b.make_const(1.0));
}

// bar = fmod(t_warped * (bpm / 60 / beats_per_bar), 1.0)
uint16_t expand_bar(GraphBuilder& b, TimeContext& ctx) {
    uint16_t bpm = b.make_cell_load(SYM_BPM);
    uint16_t bpb = b.make_cell_load(SYM_BEATS_PER_BAR);
    uint16_t rate = b.make_binop(NodeOp::Div,
                        b.make_binop(NodeOp::Div, bpm, b.make_const(60.0)), bpb);
    uint16_t phase = b.make_binop(NodeOp::Mul, ctx.t_node, rate);
    return b.make_binop(NodeOp::Fmod, phase, b.make_const(1.0));
}

// phrase = fmod(t_warped * (bpm / 60 / beats_per_bar / bars_per_phrase), 1.0)
uint16_t expand_phrase(GraphBuilder& b, TimeContext& ctx) {
    uint16_t bpm = b.make_cell_load(SYM_BPM);
    uint16_t bpb = b.make_cell_load(SYM_BEATS_PER_BAR);
    uint16_t bpp = b.make_cell_load(SYM_BARS_PER_PHRASE);
    uint16_t rate = b.make_binop(NodeOp::Div,
                        b.make_binop(NodeOp::Div,
                            b.make_binop(NodeOp::Div, bpm, b.make_const(60.0)), bpb), bpp);
    uint16_t phase = b.make_binop(NodeOp::Mul, ctx.t_node, rate);
    return b.make_binop(NodeOp::Fmod, phase, b.make_const(1.0));
}

// beat_num = floor(t_warped * bpm / 60)
uint16_t expand_beat_num(GraphBuilder& b, TimeContext& ctx) {
    uint16_t bpm = b.make_cell_load(SYM_BPM);
    uint16_t rate = b.make_binop(NodeOp::Div, bpm, b.make_const(60.0));
    uint16_t count = b.make_binop(NodeOp::Mul, ctx.t_node, rate);
    return b.make_unary(NodeOp::Floor, count);
}
```

CSE handles sharing automatically: if `a1` and `a2` both use `beat`, and `bpm` is the same cell, the entire `bpm/60` and `fmod` computation is deduplicated.

### 3.6 Time Transforms

`fast`, `slow`, `offset` are syntactic forms that modify the `TimeContext` before compiling their body. They are not VM operations — they are graph builder directives.

```cpp
uint16_t compile_fast(GraphBuilder& b, TokenStream& ts, Scope& scope, TimeContext& ctx) {
    uint16_t factor = compile_expr(b, ts, scope, ctx); // factor can be any expression
    TimeContext inner = { b.make_binop(NodeOp::Mul, ctx.t_node, factor) };
    return compile_expr(b, ts, scope, inner);
}

uint16_t compile_slow(GraphBuilder& b, TokenStream& ts, Scope& scope, TimeContext& ctx) {
    uint16_t factor = compile_expr(b, ts, scope, ctx);
    TimeContext inner = { b.make_binop(NodeOp::Div, ctx.t_node, factor) };
    return compile_expr(b, ts, scope, inner);
}

uint16_t compile_offset(GraphBuilder& b, TokenStream& ts, Scope& scope, TimeContext& ctx) {
    uint16_t amount = compile_expr(b, ts, scope, ctx);
    TimeContext inner = { b.make_binop(NodeOp::Add, ctx.t_node, amount) };
    return compile_expr(b, ts, scope, inner);
}
```

**Dynamic factors are free.** The factor expression compiles to nodes just like anything else. Whether it's `Const(2.0)` or a complex LFO subgraph, the graph builder doesn't care — it's just another node index used as an input to the Mul/Div/Add.

**Nested transforms compose via graph structure.** `(fast 2 (slow 3 expr))` produces: `t_inner = Div(Mul(t, Const(2)), Const(3))`. Constant folding reduces `Mul(Const(2), ...)` then `Div(..., Const(3))` → `Mul(t, Const(0.667))`. CSE shares the result if used in multiple subexpressions.

**`shift` is an alias for `offset`.**

### 3.7 Form Compilation

The `compile_form` function dispatches on the operator symbol:

```cpp
uint16_t compile_form(GraphBuilder& b, SymbolID op, TokenStream& ts,
                      Scope& scope, TimeContext& ctx, Token op_tok) {
    // Time transforms
    if (op == SYM_FAST)   return compile_fast(b, ts, scope, ctx);
    if (op == SYM_SLOW)   return compile_slow(b, ts, scope, ctx);
    if (op == SYM_OFFSET || op == SYM_SHIFT) return compile_offset(b, ts, scope, ctx);

    // Control flow
    if (op == SYM_IF)     return compile_if(b, ts, scope, ctx);
    if (op == SYM_LET)    return compile_let(b, ts, scope, ctx);
    if (op == SYM_DO)     return compile_do(b, ts, scope, ctx);
    if (op == SYM_FOR)    return compile_for(b, ts, scope, ctx);
    if (op == SYM_WHILE)  return compile_while_gate(b, ts, scope, ctx);
    if (op == SYM_FN || op == SYM_LAMBDA) return compile_lambda(b, ts, scope, ctx);

    // Side effects → compile-time error in signal context
    if (is_side_effect_form(op)) {
        return b.report_error_at(op_tok,
            "'" + name(op) + "' can't be used inside an output expression",
            "Use it at the top level instead");
    }

    // Variadic arithmetic: (+ a b c ...) → left-fold
    if (is_arithmetic_op(op)) return compile_variadic_arithmetic(b, op, ts, scope, ctx);

    // Comparison operators
    if (is_comparison_op(op)) return compile_comparison(b, op, ts, scope, ctx);

    // Logic operators
    if (is_logic_op(op)) return compile_logic(b, op, ts, scope, ctx);

    // Unary math functions
    if (is_unary_math(op)) return compile_unary(b, op, ts, scope, ctx);

    // Binary math functions (min, max, pow)
    if (is_binary_math(op)) return compile_binary_math(b, op, ts, scope, ctx);

    // Ternary math (clamp, lerp, scale)
    if (is_ternary_math(op)) return compile_ternary_math(b, op, ts, scope, ctx);

    // Sequence-producing functions
    if (op == SYM_RANGE)  return compile_range(b, ts, scope, ctx);

    // Domain-specific signal functions
    if (op == SYM_STEP)   return compile_step(b, ts, scope, ctx);
    if (op == SYM_GATES || op == SYM_TRIGS) return compile_gates(b, op, ts, scope, ctx);
    if (op == SYM_EUCLID || op == SYM_EU) return compile_euclid(b, ts, scope, ctx);
    if (op == SYM_SEQ || op == SYM_FROM_LIST) return compile_seq(b, ts, scope, ctx);
    if (op == SYM_INTERP || op == SYM_FLATSEQ) return compile_interp(b, ts, scope, ctx);
    if (op == SYM_DM)     return compile_dm(b, ts, scope, ctx);

    // Range conversion
    if (op == SYM_BI_TO_UNI || op == SYM_B_TO_U) return compile_unary(b, NodeOp::BiToUni, ts, scope, ctx);
    if (op == SYM_UNI_TO_BI || op == SYM_U_TO_B) return compile_unary(b, NodeOp::UniToBi, ts, scope, ctx);

    // Waveform generators
    if (op == SYM_TRI)    return compile_unary(b, NodeOp::Tri, ts, scope, ctx);
    if (op == SYM_SQR)    return compile_unary(b, NodeOp::Sqr, ts, scope, ctx);
    if (op == SYM_PULSE)  return compile_binary(b, NodeOp::Pulse, ts, scope, ctx);
    if (op == SYM_USIN)   return compile_unary(b, NodeOp::USin, ts, scope, ctx);
    if (op == SYM_UCOS)   return compile_unary(b, NodeOp::UCos, ts, scope, ctx);

    // Type predicates (return 1.0 or 0.0)
    if (is_type_predicate(op)) return compile_type_predicate(b, op, ts, scope, ctx);

    // Hardware input
    if (op == SYM_INPUT)  return compile_input_read(b, ts, scope, ctx);

    // User-defined function call
    return compile_call(b, op, ts, scope, ctx, op_tok);
}
```

### 3.8 Closure / Function Inlining

User-defined functions are inlined at the call site. No closure objects, no `Environment` capture, no `shared_ptr`.

```cpp
uint16_t compile_call(GraphBuilder& b, SymbolID fn_sym, TokenStream& ts,
                      Scope& scope, TimeContext& ctx, Token op_tok) {
    // Look up the callable
    Cell& cell = b.cells[fn_sym];
    if (cell.kind != CellKind::Callable) {
        return b.report_error_with_fuzzy_match(fn_sym, op_tok.span_start, op_tok.span_len);
    }

    CallableInfo& info = b.callables[fn_sym];
    b.add_dependency(fn_sym); // recompile if function definition changes

    // Recursion guard
    if (b.is_in_inline_stack(fn_sym)) {
        return b.report_error_at(op_tok,
            "'" + name(fn_sym) + "' calls itself — recursive functions can't be used in outputs",
            "Try using 'for' over a fixed collection instead");
    }
    b.push_inline_stack(fn_sym);

    // Compile arguments
    uint16_t arg_nodes[MAX_CALLABLE_PARAMS];
    uint8_t arg_count = 0;
    while (ts.peek().kind != TokenKind::RParen && arg_count < info.param_count) {
        arg_nodes[arg_count++] = compile_expr(b, ts, scope, ctx);
    }

    // Arity check
    if (arg_count != info.param_count) {
        b.pop_inline_stack();
        return b.report_error_at(op_tok,
            "'" + name(fn_sym) + "' needs " + std::to_string(info.param_count) +
            " values, but got " + std::to_string(arg_count),
            "Try: (" + name(fn_sym) + example_args(info) + ")");
    }

    // Create local scope with param bindings
    Scope inner_scope = { .parent = &scope };
    for (uint8_t i = 0; i < arg_count; i++) {
        inner_scope.bind(info.params[i], arg_nodes[i]);
    }

    // Tokenize the callable body from the source arena
    Token body_tokens[256];
    uint16_t body_count = TokenStream::tokenize(
        b.source.read(info.source_offset, info.source_length),
        info.source_length, body_tokens, 256, nullptr, nullptr);

    TokenStream body_ts = { body_tokens, body_count, 0 };

    // Compile the body with the new scope
    uint16_t result = compile_expr(b, body_ts, inner_scope, ctx);

    b.pop_inline_stack();
    return result;
}
```

**Expression cells** (e.g., `(define sweep (+ 200 (* 200 t)))`) are inlined identically — the graph builder re-tokenizes and compiles the body, with dependencies tracked. CSE deduplicates across multiple references.

### 3.9 `for` Compilation

`for` unrolls at graph construction time. The collection must be resolvable.

```cpp
uint16_t compile_for(GraphBuilder& b, TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // (for var collection body...)
    Token var_tok = ts.consume();
    if (var_tok.kind != TokenKind::Symbol) {
        return b.report_error_at(var_tok, "for needs a variable name", "Try: (for x [1 2 3] (* x 2))");
    }
    SymbolID var = var_tok.symbol;

    // Save position for body re-parsing
    uint16_t body_start = 0; // will be set after collection

    // Compile/resolve the collection
    auto collection = resolve_collection(b, ts, scope, ctx);

    body_start = ts.pos; // body starts here

    if (!collection.ok) {
        return b.report_error_at(var_tok,
            "for's collection couldn't be resolved at compile time",
            "Try using a literal vector: (for x [1 2 3 4] body)");
    }

    if (collection.count > 64) {
        return b.report_error_at(var_tok,
            "for's collection is too large (max 64 elements)",
            "Try a shorter collection or precompute the result");
    }

    // Unroll: compile body once per collection element
    uint16_t result = b.make_const(0.0); // default for empty collection
    for (uint16_t i = 0; i < collection.count; i++) {
        Scope iter_scope = { .parent = &scope };
        iter_scope.bind(var, collection.element_nodes[i]);

        ts.rewind(body_start);
        result = compile_expr(b, ts, iter_scope, ctx);
    }

    // Skip past body tokens (we've already consumed them in the loop)
    // ... handle RParen
    return result;
}
```

**Collection resolution** handles:
- Literal vectors `[1 2 3 4]` → each element compiled as an expression (not just constants — `[1 2 t 3]` works, producing `Const(1)`, `Const(2)`, `ctx.t_node`, `Const(3)`)
- Symbol references `items` → look up cell, resolve if Data or Number
- Pure function calls that produce sequences → compile the call, check if result is a known-size collection of `Const` nodes (see below)
- Nested expressions → compile and check if the result is a known-size collection

**`range` and other sequence-producing pure functions work naturally.** `(for x (range 1 8) (* x beat))` compiles the `range` call first, which produces a sequence of `Const` nodes `[1, 2, 3, 4, 5, 6, 7]` via pervasive constant folding (section 3.2). The collection resolver then iterates over these nodes. No special-casing of `range` is needed in the collection resolver — it just checks whether the compiled result is a known-size sequence of nodes. The constant folding infrastructure handles the rest.

More generally, any pure expression that reduces to constants at build time is a valid `for` collection: `(for x (map (fn [i] (* i 2)) [1 2 3 4]) ...)` would work if `map` were compiled (it isn't — it's cold-path only). But `(for x [2 4 6 8] ...)` achieves the same thing and is clearer. The key unlocked case is `range`, which is the most common source of computed collections in practice.

#### `range` Compilation

`range` is a sequence-producing form compiled directly by the graph builder:

```cpp
// (range end) or (range start end) or (range start end step)
// All arguments must fold to constants. Produces a data table.
uint16_t compile_range(GraphBuilder& b, TokenStream& ts, Scope& scope, TimeContext& ctx) {
    uint16_t arg1 = compile_expr(b, ts, scope, ctx);
    double start = 0, end_val, step_val = 1;

    if (ts.peek().kind == TokenKind::RParen) {
        // (range end) — single arg
        if (!b.is_const(arg1)) return b.report_error(...);
        end_val = b.const_value(arg1);
    } else {
        uint16_t arg2 = compile_expr(b, ts, scope, ctx);
        if (ts.peek().kind == TokenKind::RParen) {
            // (range start end)
            if (!b.is_const(arg1) || !b.is_const(arg2)) return b.report_error(...);
            start = b.const_value(arg1);
            end_val = b.const_value(arg2);
        } else {
            // (range start end step)
            uint16_t arg3 = compile_expr(b, ts, scope, ctx);
            if (!b.is_const(arg1) || !b.is_const(arg2) || !b.is_const(arg3))
                return b.report_error(...);
            start = b.const_value(arg1);
            end_val = b.const_value(arg2);
            step_val = b.const_value(arg3);
        }
    }

    // Produce sequence of Const nodes (capped at 64)
    b.collection.count = 0;
    for (double v = start; step_val > 0 ? v < end_val : v > end_val; v += step_val) {
        if (b.collection.count >= 64) break;
        b.collection.element_nodes[b.collection.count++] = b.make_const(v);
    }
    b.collection.ok = true;
    return COLLECTION_SENTINEL; // signals that result is a collection, not a scalar
}
```

The error when arguments aren't constant:
```
"range needs values known at compile time"
"Try: (range 1 8) or use a literal vector [1 2 3 4 5 6 7]"
```

This pattern generalises: any future sequence-producing function (e.g., `linspace`, `geom-series`) follows the same shape — compile arguments, check they're `Const`, produce a sequence of `Const` nodes. The constant folding infrastructure means the arguments can themselves be arbitrarily complex pure expressions: `(range 1 (* 4 beats-per-bar))` works as long as `beats-per-bar` is a frozen Number cell whose value can be baked at compile time.

### 3.10 `while` as Conditional Gate (Deferred)

`while` has conditional-gate semantics: it returns the body value when the condition is true, and the monoidal identity when false. The identity depends on the enclosing operator context.

```cpp
// Operator context is threaded through compilation
enum class OpContext : uint8_t { None, Add, Mul, Other };

uint16_t compile_while_gate(GraphBuilder& b, TokenStream& ts, Scope& scope,
                            TimeContext& ctx, OpContext enclosing_op) {
    uint16_t cond = compile_expr(b, ts, scope, ctx);
    uint16_t value = compile_expr(b, ts, scope, ctx);

    double identity;
    switch (enclosing_op) {
        case OpContext::Mul: identity = 1.0; break;
        case OpContext::Add:
        default:             identity = 0.0; break;
    }

    uint16_t id_node = b.make_const(identity);
    return b.make_select(cond, value, id_node);
}
```

**Status: deferred.** The context propagation is non-trivial for deeply nested expressions. For initial implementation, default to identity `0.0` and revisit when usage patterns are clearer.

### 3.11 Domain-Specific Signal Functions

These are the musically important operations. Each compiles to a small subgraph of nodes.

```cpp
// (step data) or (step data phase)
// Indexes into a data table at floor(phase * length) % length
// Default phase: beat (from current time context)
uint16_t compile_step(GraphBuilder& b, TokenStream& ts, Scope& scope, TimeContext& ctx) {
    auto data = resolve_data_table(b, ts, scope, ctx);

    uint16_t phase;
    if (ts.peek().kind != TokenKind::RParen) {
        phase = compile_expr(b, ts, scope, ctx);
    } else {
        phase = expand_beat(b, ctx); // default: beat phasor
    }

    // floor(phase * length) % length → index into data table
    uint16_t len = b.make_const((double)data.length);
    uint16_t scaled = b.make_binop(NodeOp::Mul, phase, len);
    uint16_t idx = b.make_unary(NodeOp::Floor, scaled);
    return b.make_node(NodeOp::VecIndex, idx, 0xFFFF, 0xFFFF, (double)data.table_id);
}

// (gates data) or (gates data phase)
// Same as step but returns 1.0 for truthy values, 0.0 for falsy
uint16_t compile_gates(GraphBuilder& b, SymbolID op, TokenStream& ts,
                       Scope& scope, TimeContext& ctx) {
    uint16_t raw = compile_step(b, ts, scope, ctx);

    if (op == SYM_TRIGS) {
        // trigs: 1.0 only on the rising edge (first sample of each step)
        // This requires comparing current and previous step indices
        // Implemented as: (and (> step_value 0) (< fractional_part threshold))
        // ... detailed implementation
    }

    // gates: just threshold at > 0
    return b.make_binop(NodeOp::CmpGt, raw, b.make_const(0.0));
}

// (euclid phase total_steps active_steps) or (euclid phase total active rotation)
// Bjorklund/Euclidean rhythm — can be computed as a closed-form expression
uint16_t compile_euclid(GraphBuilder& b, TokenStream& ts, Scope& scope, TimeContext& ctx) {
    // ... compile arguments
    // The Euclidean pattern can be precomputed as a data table at compile time
    // if total and active are constants
    // Otherwise: use the Bresenham closed-form: floor(i * active / total) != floor((i-1) * active / total)
}

// (interp data phase) — linearly interpolating step sequencer
uint16_t compile_interp(GraphBuilder& b, TokenStream& ts, Scope& scope, TimeContext& ctx) {
    auto data = resolve_data_table(b, ts, scope, ctx);
    uint16_t phase = (ts.peek().kind != TokenKind::RParen)
        ? compile_expr(b, ts, scope, ctx) : expand_beat(b, ctx);
    return b.make_node(NodeOp::VecLerp, phase, 0xFFFF, 0xFFFF, (double)data.table_id);
}
```

---

## 4. The Graph Executor

Replaces both `execute_numeric_program` and `execute_tagged_program`. Estimated size: ~80 lines.

### 4.1 Single-Sample Execution

```cpp
void execute_all_outputs(
    const NodePool& pool,
    double t,                      // THE input: raw time in seconds
    const double* cell_values,     // snapshot of cell numeric values (indexed by SymbolID)
    const double* hw_inputs,       // hardware input channel values
    const double* data_pool,       // shared data tables
    const uint16_t* data_offsets,  // data table offsets
    const uint16_t* data_lengths,  // data table lengths
    double* output_values,         // [MAX_OUTPUTS] results written here
    double* node_values            // [MAX_TOTAL_NODES] workspace (can be stack-allocated)
) {
    // One forward pass through topologically sorted nodes
    for (uint16_t i = 0; i < pool.exec_count; i++) {
        uint16_t idx = pool.exec_order[i];
        const Node& n = pool.nodes[idx];

        double a = (n.input_a != 0xFFFF) ? node_values[n.input_a] : 0.0;
        double b = (n.input_b != 0xFFFF) ? node_values[n.input_b] : 0.0;
        double c = (n.input_c != 0xFFFF) ? node_values[n.input_c] : 0.0;

        double result;
        switch (n.op) {
            case NodeOp::Const:       result = n.imm; break;
            case NodeOp::RawTimeLoad: result = t; break;
            case NodeOp::CellLoad:    result = cell_values[(uint16_t)n.imm]; break;
            case NodeOp::InputLoad:   result = hw_inputs[(uint16_t)n.imm]; break;
            case NodeOp::DataLoad: {
                uint16_t tid = (uint16_t)n.imm;
                uint16_t off = data_offsets[tid];
                uint16_t len = data_lengths[tid];
                int index = ((int)a % len + len) % len; // wrap-around
                result = data_pool[off + index];
                break;
            }

            case NodeOp::Add:  result = a + b; break;
            case NodeOp::Sub:  result = a - b; break;
            case NodeOp::Mul:  result = a * b; break;
            case NodeOp::Div:  result = (b != 0.0) ? a / b : 0.0; break;
            case NodeOp::Mod:  result = (b != 0.0) ? fmod(a, b) : 0.0; break;
            case NodeOp::Neg:  result = -a; break;
            case NodeOp::Abs:  result = fabs(a); break;
            case NodeOp::Floor: result = floor(a); break;
            case NodeOp::Ceil:  result = ceil(a); break;
            case NodeOp::Frac:  result = a - floor(a); break;
            case NodeOp::Sqrt:  result = sqrt(fabs(a)); break;
            case NodeOp::Min:   result = (a < b) ? a : b; break;
            case NodeOp::Max:   result = (a > b) ? a : b; break;
            case NodeOp::Pow:   result = pow(a, b); break;
            case NodeOp::Clamp: result = (a < b) ? b : (a > c) ? c : a; break;

            case NodeOp::Sin:  result = sin(a); break;
            case NodeOp::Cos:  result = cos(a); break;
            case NodeOp::Tan:  result = tan(a); break;

            case NodeOp::USin:   result = (sin(a * 2.0 * M_PI) + 1.0) * 0.5; break;
            case NodeOp::UCos:   result = (cos(a * 2.0 * M_PI) + 1.0) * 0.5; break;
            case NodeOp::Tri:    result = 1.0 - fabs(2.0 * (a - floor(a)) - 1.0); break;
            case NodeOp::Sqr:    result = ((a - floor(a)) < 0.5) ? 1.0 : 0.0; break;
            case NodeOp::Pulse:  result = ((a - floor(a)) < b) ? 1.0 : 0.0; break;

            case NodeOp::CmpGt: result = (a > b) ? 1.0 : 0.0; break;
            case NodeOp::CmpLt: result = (a < b) ? 1.0 : 0.0; break;
            case NodeOp::CmpGe: result = (a >= b) ? 1.0 : 0.0; break;
            case NodeOp::CmpLe: result = (a <= b) ? 1.0 : 0.0; break;
            case NodeOp::CmpEq: result = (a == b) ? 1.0 : 0.0; break;

            case NodeOp::Not:    result = (a == 0.0) ? 1.0 : 0.0; break;
            case NodeOp::And:    result = (a != 0.0 && b != 0.0) ? 1.0 : 0.0; break;
            case NodeOp::Or:     result = (a != 0.0 || b != 0.0) ? 1.0 : 0.0; break;

            case NodeOp::Select: result = (a != 0.0) ? b : c; break;

            case NodeOp::Fmod:   result = (b != 0.0) ? fmod(a, b) : 0.0; break;

            case NodeOp::BiToUni: result = (a + 1.0) * 0.5; break;
            case NodeOp::UniToBi: result = a * 2.0 - 1.0; break;
            case NodeOp::Lerp:    result = a + (b - a) * c; break;
            case NodeOp::Scale:   result = a * (c - b) + b; break; // [0,1] → [b, c]

            case NodeOp::VecIndex: {
                uint16_t tid = (uint16_t)n.imm;
                uint16_t off = data_offsets[tid];
                uint16_t len = data_lengths[tid];
                int index = ((int)floor(a) % len + len) % len;
                result = data_pool[off + index];
                break;
            }
            case NodeOp::VecLerp: {
                uint16_t tid = (uint16_t)n.imm;
                uint16_t off = data_offsets[tid];
                uint16_t len = data_lengths[tid];
                double scaled = a * len;
                int i0 = ((int)floor(scaled) % len + len) % len;
                int i1 = (i0 + 1) % len;
                double frac = scaled - floor(scaled);
                result = data_pool[off + i0] + (data_pool[off + i1] - data_pool[off + i0]) * frac;
                break;
            }

            default: result = 0.0; break;
        }

        // NaN/Inf guard
        if (!std::isfinite(result)) result = 0.0;

        node_values[idx] = result;
    }

    // Read output values
    for (uint16_t i = 0; i < MAX_OUTPUTS; i++) {
        if (pool.outputs[i].root_node != 0xFFFF) {
            output_values[i] = node_values[pool.outputs[i].root_node];
        }
    }
}
```

### 4.2 Batched Execution (WASM Visualization)

For the browser, sample all outputs across a time window efficiently using SOA (struct-of-arrays) execution:

```cpp
void execute_batch(
    const NodePool& pool,
    const double* t_array,         // array of time points
    size_t sample_count,
    const double* cell_values,
    const double* hw_inputs,
    const double* data_pool,
    const uint16_t* data_offsets,
    const uint16_t* data_lengths,
    double* output_buffer,         // [num_outputs × sample_count], row-major
    uint16_t num_outputs
) {
    // Workspace: one array per node, each of length sample_count
    // For large batches, chunk to limit memory usage
    constexpr size_t CHUNK = 256;
    double regs[MAX_TOTAL_NODES][CHUNK];

    for (size_t chunk_start = 0; chunk_start < sample_count; chunk_start += CHUNK) {
        size_t chunk_size = std::min(CHUNK, sample_count - chunk_start);

        for (uint16_t ni = 0; ni < pool.exec_count; ni++) {
            uint16_t idx = pool.exec_order[ni];
            const Node& n = pool.nodes[idx];

            if (n.flags & FLAG_TIME_INVARIANT) {
                // Compute once, broadcast
                double val = compute_single(n, regs, 0, cell_values, hw_inputs,
                                            data_pool, data_offsets, data_lengths, 0.0);
                for (size_t s = 0; s < chunk_size; s++) regs[idx][s] = val;
            } else {
                // Compute per sample — this inner loop auto-vectorizes with -O3 / SIMD
                for (size_t s = 0; s < chunk_size; s++) {
                    double t = t_array[chunk_start + s];
                    regs[idx][s] = compute_single(n, regs, s, cell_values, hw_inputs,
                                                  data_pool, data_offsets, data_lengths, t);
                }
            }
        }

        // Copy output values for this chunk
        for (uint16_t o = 0; o < num_outputs; o++) {
            if (pool.outputs[o].root_node != 0xFFFF) {
                memcpy(&output_buffer[o * sample_count + chunk_start],
                       regs[pool.outputs[o].root_node], chunk_size * sizeof(double));
            }
        }
    }
}
```

**Time-invariant optimization:** Nodes whose entire input chain is free of `RawTimeLoad` and `InputLoad` are flagged `TIME_INVARIANT` during graph construction. In batch mode, they're computed once and broadcast. This typically saves 30-50% of computation for expressions with constant subexpressions.

---

## 5. Cold-Path Interpreter

Handles everything that isn't signal sampling: `define`, `defn`, `set-bpm`, REPL evaluation, etc. Estimated size: ~400 lines.

```cpp
struct EvalResult {
    enum Kind : uint8_t { Number, Text, DataRef, Ok, Error } kind;
    double number;
    const char* text;        // points into source arena or static string
    uint16_t text_length;
    Diagnostic diagnostics[8];
    uint8_t diagnostic_count;
};

EvalResult eval_cold(const char* source, uint32_t length,
                     CellStore& cells, SourceArena& arena, NodePool& pool) {
    Token tokens[256];
    Diagnostic parse_errors[8];
    uint8_t parse_error_count = 0;

    uint16_t count = TokenStream::tokenize(source, length, tokens, 256,
                                            parse_errors, &parse_error_count);
    if (parse_error_count > 0) {
        // Return parse errors
        return make_error_result(parse_errors, parse_error_count);
    }

    TokenStream ts = { tokens, count, 0 };
    return eval_form(ts, cells, arena, pool);
}

EvalResult eval_form(TokenStream& ts, CellStore& cells, SourceArena& arena, NodePool& pool) {
    Token tok = ts.peek();

    if (tok.kind == TokenKind::Number) { ts.consume(); return make_number(tok.number); }
    if (tok.kind == TokenKind::Symbol) { ts.consume(); return eval_symbol(tok.symbol, cells); }

    if (tok.kind == TokenKind::LParen) {
        ts.consume();
        Token op_tok = ts.consume();
        SymbolID op = op_tok.symbol;

        // Cell mutations
        if (op == SYM_DEFINE || op == SYM_DEF) return do_define(ts, cells, arena, pool);
        if (op == SYM_DEFN || op == SYM_DEFUN) return do_defn(ts, cells, arena, pool);
        if (op == SYM_DEFS) return do_defs(ts, cells, arena, pool);
        if (op == SYM_SET)  return do_set(ts, cells, arena, pool);

        // Output assignment → delegate to graph builder
        if (is_output_symbol(op)) return do_output_assign(op, ts, cells, arena, pool);

        // Transport / side effects
        if (op == SYM_SET_BPM)     return do_set_bpm(ts, cells);
        if (op == SYM_SET_TIME_SIG) return do_set_time_sig(ts, cells);
        if (op == SYM_USEQ_PLAY)   return do_transport(TransportCmd::Play);
        if (op == SYM_USEQ_PAUSE)  return do_transport(TransportCmd::Pause);
        if (op == SYM_USEQ_STOP)   return do_transport(TransportCmd::Stop);
        if (op == SYM_USEQ_REWIND) return do_transport(TransportCmd::Rewind);
        if (op == SYM_SCHEDULE)    return do_schedule(ts, cells, arena);
        if (op == SYM_UNSCHEDULE)  return do_unschedule(ts, cells);

        // Control flow on cold path
        if (op == SYM_IF)   return eval_if_cold(ts, cells, arena, pool);
        if (op == SYM_DO)   return eval_do_cold(ts, cells, arena, pool);
        if (op == SYM_LET)  return eval_let_cold(ts, cells, arena, pool);

        // Cold-path list/utility operations
        if (op == SYM_PRINT || op == SYM_PRINTLN) return do_print(ts, cells);
        if (op == SYM_DISPLAY) return do_display(ts, cells);

        // Numeric expression at REPL → build graph, execute once, return result
        ts.rewind(op_tok); // rewind to include the operator
        // ... build a temporary graph and execute at current time
        return eval_numeric_expression(ts, cells, arena, pool);
    }

    return make_error("Unexpected input", "Try: (define name value) or (a1 expression)");
}
```

### 5.1 `define` Implementation

```cpp
EvalResult do_define(TokenStream& ts, CellStore& cells, SourceArena& arena, NodePool& pool) {
    Token name_tok = ts.consume();
    if (name_tok.kind != TokenKind::Symbol) {
        return make_error("define needs a name", "Try: (define freq 440)");
    }
    SymbolID sym = name_tok.symbol;

    // Peek at what we're defining
    Token val_tok = ts.peek();

    if (val_tok.kind == TokenKind::Number) {
        // Simple numeric constant
        ts.consume();
        cells.cells[sym] = { CellKind::Number, 0, 0, cells.cells[sym].revision + 1, val_tok.number };
    }
    else if (val_tok.kind == TokenKind::LBracket) {
        // Vector data: [1 2 3 4]
        auto data = parse_data_table(ts);
        uint16_t table_id = cells.store_data_table(data.values, data.count);
        cells.cells[sym] = { CellKind::Data, 0, table_id, cells.cells[sym].revision + 1, (double)data.count };
    }
    else {
        // Expression — store the source text as a callable with 0 params (expression cell)
        uint16_t expr_start = ts.pos;
        skip_form(ts); // skip past the expression
        uint16_t expr_end = ts.pos;

        uint32_t src_offset = arena.store_tokens(ts.tokens + expr_start, expr_end - expr_start);
        cells.callables[sym] = { {}, 0, {}, src_offset, (uint32_t)(expr_end - expr_start) };
        cells.cells[sym] = { CellKind::Callable, 0, 0, cells.cells[sym].revision + 1, 0.0 };
        // Note: expression cells with 0 params are inlined by the graph builder
        // just like callable cells — the difference is they're referenced as values, not calls
    }

    ts.expect(TokenKind::RParen);

    // Notify dependents
    on_cell_changed(sym, cells, pool);

    return make_ok();
}
```

### 5.2 Output Assignment

```cpp
EvalResult do_output_assign(SymbolID output_sym, TokenStream& ts,
                            CellStore& cells, SourceArena& arena, NodePool& pool) {
    uint16_t output_index = resolve_output_index(output_sym);

    // Build the signal graph
    Scope root_scope = {};
    TimeContext ctx = { make_raw_time_load(pool) };

    GraphBuildResult result = compile_expr_from_stream(pool, ts, root_scope, ctx, cells, arena);

    ts.expect(TokenKind::RParen);

    if (result.has_error) {
        // Store diagnostics but keep LKG running
        pool.outputs[output_index].valid = false;
        return make_error_result(result.diagnostics, result.diagnostic_count);
    }

    // Install the new graph root
    uint16_t old_root = pool.outputs[output_index].root_node;
    pool.outputs[output_index].root_node = result.root_node;
    pool.outputs[output_index].valid = true;

    // Re-sort the execution order and recompute dependency bitmasks
    pool.rebuild_execution_order();

    // Prune dead nodes (old subgraph that may no longer be reachable)
    pool.gc_unreachable_nodes();

    return make_ok();
}
```

### 5.3 Dependency Tracking and Recompilation

```cpp
void on_cell_changed(SymbolID cell_id, CellStore& cells, NodePool& pool) {
    // For each output, check if it depends on this cell
    for (uint16_t i = 0; i < MAX_OUTPUTS; i++) {
        if (pool.outputs[i].root_node == 0xFFFF) continue;
        if (!output_depends_on_cell(pool, i, cell_id)) continue;

        // This output needs recompilation
        // Save LKG
        if (pool.outputs[i].valid) {
            pool.outputs[i].lkg_value = /* last sampled value */;
        }

        // Recompile from the stored output expression
        // ... rebuild this output's subgraph in the pool
        // ... re-sort, recompute bitmasks
    }
}

bool output_depends_on_cell(const NodePool& pool, uint16_t output_idx, SymbolID cell_id) {
    // Walk the output's reachable nodes, check for CellLoad with matching cell_id
    for (uint16_t i = 0; i < pool.exec_count; i++) {
        uint16_t idx = pool.exec_order[i];
        if (!(pool.output_deps[idx] & (1ULL << output_idx))) continue;
        if (pool.nodes[idx].op == NodeOp::CellLoad && (uint16_t)pool.nodes[idx].imm == cell_id) {
            return true;
        }
    }
    return false;
}
```

---

## 6. Diagnostics

### 6.1 Design Principles

From the existing error handling spec (which is excellent and should be preserved):

- **Errors are a conversation, not a verdict.** Messages are written for someone who may not know what "arity mismatch" means.
- **Never stop the music.** LKG fallback keeps outputs running even during errors.
- **Every error has a suggestion** with working example code.
- **Fuzzy matching** catches typos: `(sni (* t 440))` → *"Unknown 'sni'. Did you mean 'sin'?"*
- **Progressive disclosure**: colored underline → one-sentence summary → expandable detail.
- **Rate limiting**: runtime errors deduplicated per output per frame.

### 6.2 Diagnostic Structure

```cpp
enum class DiagnosticSeverity : uint8_t { Hint, Warning, Error };

enum class DiagnosticCategory : uint8_t {
    Syntax,        // parse errors
    UndefinedName, // unknown symbol
    Arity,         // wrong argument count
    Type,          // wrong argument type
    Boundary,      // side effect in signal context
    Arithmetic,    // division by zero, NaN
    Runtime,       // loop budget, recursion depth
    Overflow       // value out of range
};

struct Diagnostic {
    DiagnosticSeverity severity;
    DiagnosticCategory category;
    uint16_t span_start;
    uint16_t span_len;
    const char* message;      // static string literal (no allocation)
    const char* suggestion;   // static string literal with working example
};
```

### 6.3 Key Error Messages

| Situation | Message | Suggestion |
|-----------|---------|------------|
| Unknown symbol | "'frq' isn't defined. Did you mean 'freq'?" | "Try: freq (defined as 440)" |
| Wrong arity | "'sin' needs 1 value, but got 3" | "Try: (sin (* t 440))" |
| Side effect in output | "'define' can't be used inside an output" | "Use it at the top level instead" |
| Recursion in output | "'fibonacci' calls itself — that can't be used in outputs" | "Try using 'for' over a fixed collection" |
| Unresolvable for | "for's collection couldn't be resolved at compile time" | "Try: (for x [1 2 3 4] body) or (for x (range 1 8) body)" |
| Non-constant range | "range needs values known at compile time" | "Try: (range 1 8) or use a literal vector [1 2 3 4 5 6 7]" |
| Bare callable | "'osc' is a function — it needs arguments" | "Try: (osc 440)" |
| Swapped args | "'fast' expects a number first, then an expression" | "Try: (fast 2 beat)" |
| Division by zero | "Division by zero" | "Check that the denominator isn't zero" |
| Undefined with context | "'frq' isn't defined. Did you mean 'freq'? (defined as 440)" | Include the value of the suggested match |
| While context hint | "'while' inside '+' returns 0 when false. Inside '*' it returns 1." | "If you need a different default, use 'if' directly" |
| Slow by zero | "'slow' by 0 would divide time by zero" | "Try: (slow 2 beat)" |
| Negative fast | "'fast' by a negative number reverses time — was that intentional?" | (hint, not error) |
| Quote in output | "'quote' can't be used inside an output expression" | "Use a literal vector instead: [1 0 1 0]" |

---

## 7. WASM ABI

The WASM interface must remain stable. The new engine implements the same exported functions:

```cpp
extern "C" {
    // Initialization
    void useq_init();

    // Evaluate a command (cold path)
    // Returns: result string (JSON for structured results, plain text for values)
    const char* useq_eval(const char* source);

    // Update the raw time input
    void useq_update_time(double t);

    // Set a hardware input channel value
    void useq_set_input_value(int channel, double value);

    // Evaluate a single output at a specific time
    double useq_eval_output(const char* output_name, double t);

    // Get last error
    const char* useq_last_error();

    // Diagnostics from last eval (JSON array)
    const char* useq_last_diagnostics();

    // Per-output active diagnostics (JSON object)
    const char* useq_active_diagnostics();

    // Batch evaluation: evaluate outputs across a time window
    // Returns JSON with sample data
    const char* useq_eval_outputs_time_window(const char* output_names,
                                               double start_time, double end_time,
                                               int sample_count);

    // Zero-copy batch evaluation into caller's Float64Array
    int useq_eval_outputs_time_window_into(const char* output_names,
                                            double start_time, double end_time,
                                            int sample_count,
                                            int heap_offset, int max_bytes);
}
```

**Diagnostic JSON format** (unchanged from current system):

```json
[
  {
    "severity": "error",
    "category": "undefinedName",
    "span": { "start": 5, "end": 8 },
    "message": "'frq' isn't defined. Did you mean 'freq'?",
    "suggestion": "Try: freq"
  }
]
```

---

## 8. Firmware Integration

### 8.1 Output Sampling Loop

The firmware main loop calls the executor directly. No `eval()` per tick. No recompilation per tick.

```cpp
// In uSEQ_update.cpp — called per tick (~1kHz)
void uSEQ::updateOutputs() {
    double t = getCurrentTimeSeconds();

    // Snapshot cell values for this tick (atomic read)
    double cell_snapshot[MAX_CELLS];
    snapshot_cell_values(m_cells, cell_snapshot);

    // Read hardware inputs
    double hw_inputs[NUM_INPUTS];
    read_hardware_inputs(hw_inputs);

    // Execute all output graphs in one pass
    double output_values[MAX_OUTPUTS];
    double workspace[MAX_TOTAL_NODES]; // stack-allocated

    execute_all_outputs(m_pool, t, cell_snapshot, hw_inputs,
                        m_cells.data_pool, m_cells.data_offsets, m_cells.data_lengths,
                        output_values, workspace);

    // Write to hardware
    for (int i = 0; i < NUM_CONTINUOUS_OUTS; i++) {
        writeAnalogOutput(i, output_values[i]);
    }
    for (int i = 0; i < NUM_BINARY_OUTS; i++) {
        writeDigitalOutput(i, output_values[NUM_CONTINUOUS_OUTS + i] > 0.5 ? 1 : 0);
    }
    for (int i = 0; i < NUM_SERIAL_OUTS; i++) {
        // Serial outputs handled via protocol
    }
}
```

### 8.2 Cold-Path Command Processing

User commands arrive via serial (JSON protocol). Processed on the cold path:

```cpp
// Called when a new command arrives via serial
void uSEQ::processCommand(const char* source, uint32_t length) {
    EvalResult result = eval_cold(source, length, m_cells, m_arena, m_pool);

    // Send response via serial protocol
    sendEvalResponse(result);
}
```

Recompilation (triggered by `on_cell_changed`) happens here, on the cold path. The hot path (output sampling) never compiles — it only executes.

---

## 9. Memory Budget (RP2040)

| Structure | Size | Count | Total |
|-----------|------|-------|-------|
| Cell table | 16 bytes/cell | 512 cells | 8 KB |
| Callable info | 24 bytes/callable | 512 (parallel) | 12 KB |
| Data pool | 8 bytes/double | 2048 entries | 16 KB |
| Node pool | 20 bytes/node | 1024 nodes | 20 KB |
| CSE hash table | 6 bytes/entry | 2048 entries | 12 KB |
| Execution order | 2 bytes/entry | 1024 entries | 2 KB |
| Output deps bitmask | 8 bytes/node | 1024 nodes | 8 KB |
| Output slots | 16 bytes/output | 42 outputs | ~1 KB |
| Source arena | 1 byte/char | 16384 chars | 16 KB |
| Token buffer (stack) | 16 bytes/token | 256 tokens | 4 KB |
| Node values workspace (stack) | 8 bytes/node | 1024 nodes | 8 KB |
| **Total** | | | **~107 KB** |

RP2040 has 264 KB SRAM. This budget leaves ~157 KB for the rest of the firmware (stack, serial buffers, DSP, I2C, flash filesystem). Generous.

The current system's memory usage is higher due to `Value` (88 bytes × hundreds), `Environment` chains (144 bytes each + heap), `std::vector` and `std::shared_ptr` overhead, and `NumericVmInstruction` at 32 bytes.

---

## 10. What This Replaces

| Current | Lines | Replacement | Lines (est.) |
|---------|-------|-------------|--------------|
| `Value` class | ~600 | `Cell` struct | ~30 |
| `Environment` class | ~300 | Cell table (flat array) | ~20 |
| `bytecode_vm.cpp` (compiler) | ~6,200 | Graph builder | ~600-800 |
| `bytecode_vm.h` (types) | ~170 | Node/Pool types | ~80 |
| Fast + tagged executors | ~800 | Single executor | ~80 |
| `TemporalContext` | ~100 | Temporal templates in graph builder | ~30 |
| `StoredOutput` + caching | ~200 | OutputSlot + dependency tracking | ~50 |
| `modulisp_eval.cpp` | ~400 | Cold-path interpreter | ~400 |
| Bridge intrinsics | ~300 | (eliminated) | 0 |
| **Total** | **~9,000** | | **~1,200-1,400** |

---

## 11. Migration Strategy

### Phase 0: Build the New Engine Alongside the Old (2-3 weeks)

Build the new signal engine as a separate compilation unit. Do not modify the existing code. The new engine lives in a new directory (e.g., `uSEQ/src/signal_engine/`):

```
signal_engine/
  cell_store.h         // Cell table
  node_pool.h          // Node types and pool
  graph_builder.h/cpp  // Token-to-graph compilation
  executor.h/cpp       // Graph execution (single + batch)
  cold_eval.h/cpp      // Cold-path interpreter
  source_arena.h       // Source text storage
  diagnostics.h        // Diagnostic types
  token.h              // Token types and tokenizer
```

Test against the existing golden test suite (`basic_semantics.yaml`) and the existing integration tests.

### Phase 1: Wire In for WASM (1 week)

Replace the WASM wrapper's internals to use the new engine. The exported ABI stays identical. The frontend doesn't change. Run the full contract test suite.

### Phase 2: Wire In for Firmware (1 week)

Replace the firmware output loop to use the new engine. Run hardware-in-the-loop tests. The old engine remains available behind a build flag (`USE_LEGACY_ENGINE`) for emergency rollback. Both engines can coexist in the same binary during this phase — the flag selects which one the output loop uses.

### Phase 2.5: Shared Semantic Test Suite

Before removing the old engine, establish a shared test suite that runs identical test cases against both engines and verifies identical output values. This suite becomes the long-term semantic contract and is shared between firmware and WASM builds. The golden test file (`basic_semantics.yaml`) is the starting point, extended with coverage for all features identified in the audit (section 15).

### Phase 3: Remove the Old Engine (1 week)

Once the new engine is proven in both WASM and firmware (all shared tests pass on both), remove the old code: `Value`, `Environment`, the 6,200-line compiler, the dual executors, bridge intrinsics. Remove the `USE_LEGACY_ENGINE` flag.

### Phase 4: Measure and Optimize (1 week)

Profile. Identify actual bottlenecks. Candidates:
- Instruction encoding (20 bytes → 12-16 bytes with split constant table)
- Batch SIMD for WASM
- Node pool compaction strategy
- Source arena GC policy

---

## 12. Test Strategy

### 12.1 Preserved Tests

All existing golden tests (`basic_semantics.yaml`) must pass with identical output values. These define the semantic contract.

### 12.2 New Tests

| Category | What to test |
|----------|-------------|
| **Graph construction** | CSE deduplication, constant folding, algebraic simplification |
| **Temporal templates** | `beat`, `bar`, `phrase`, `section` expand correctly; compose with `fast`/`slow`/`offset` |
| **Dynamic time warps** | `(fast lfo beat)` where lfo is a cell or expression |
| **Nested time warps** | `(fast 2 (slow 3 (offset 0.5 beat)))` flattens correctly |
| **CSE across outputs** | Two outputs sharing subexpressions produce one copy of shared nodes |
| **Dependency tracking** | Changing a cell triggers recompilation of dependent outputs only |
| **LKG fallback** | Compile error keeps previous output value running |
| **Error messages** | All diagnostic messages are human-readable, have suggestions, fuzzy-match |
| **Closure inlining** | `(defn f [x] body)` then `(a1 (f 440))` inlines correctly |
| **Recursion detection** | Recursive callable produces diagnostic, not infinite loop |
| **For unrolling** | Literal vectors, cell-backed vectors, vectors with time-varying elements, `range` with constant args, pure expressions that fold to sequences |
| **Memory bounds** | Node pool, cell table, data pool don't overflow under realistic workloads |
| **Batch execution** | `useq_eval_outputs_time_window_into` produces identical results to single-sample |
| **WASM ABI** | All 9 exported functions behave identically to the old engine |

---

## 13. Open Questions

### 13.1 Expression Cells vs. Number/Data Cells

When the user writes `(define sweep (+ 200 (* 200 t)))`, `sweep` is stored as a Callable cell with 0 params. When referenced in an output, the graph builder re-tokenizes and compiles the body inline. CSE deduplicates across multiple references to the same expression.

This is simple and consistent. If profiling shows re-tokenization overhead matters (unlikely for expressions of 5-20 tokens), an optimization would be to cache the compiled node subgraph per expression cell and clone/graft it into the output graph. But this is a Phase 4 concern.

**`for` over defined globals already works in the current system** — the compiler's `try_resolve_numeric_sequence()` looks up global definitions via `m_env.get_expr()` and recursively resolves. The new graph builder preserves this: when `for` encounters a symbol as its collection, look up the cell. If it's a Data cell, iterate over the data table. If it's a Callable/expression cell, compile it and check if the result is a known-size collection. The dependency tracker registers the collection cell so the output recompiles if the collection changes.

**`for` over pure function calls** (e.g., `(for x (range 1 8) ...)`) works via pervasive constant folding (section 3.2) and the `range` compilation form (section 3.9). Because every node constructor folds constants transitively, `range` with constant arguments produces a concrete sequence of `Const` nodes. The collection resolver in `compile_for` checks whether the compiled collection expression produced a known-size sequence, regardless of whether it came from a literal vector, a cell lookup, or a pure function call. This generalises to any future sequence-producing pure function.

### 13.2 `while` Operator Context Propagation

Deferred. Default identity is 0.0 (correct for addition). Full context-aware compilation requires threading `OpContext` through the graph builder, which adds complexity to every variadic arithmetic compilation path.

### 13.3 Source Arena Compaction Policy

When to compact? Options:
- On every `define` that overwrites a previous definition (simple, possibly wasteful)
- When arena exceeds 75% capacity (amortized, needs tracking)
- Never on RP2040 (16KB is enough for most sessions); compact on WASM where sessions are longer

### 13.4 Error Recovery During Graph Construction

If one subexpression fails (e.g., `(+ (sin beat) (undefined-fn 42))`), should the graph builder:
- Abort the entire output? (Current approach — LKG fallback)
- Substitute a placeholder (0.0) for the failing subexpression and continue?

Current spec: abort and LKG. This is simpler and avoids silent incorrect values.

### 13.5 Cross-Output CSE Invalidation

When output a1's subgraph is rebuilt, shared nodes with a2 must not be disturbed. The hash-cons pool handles this naturally (shared nodes persist as long as any output references them). But the execution order must be re-sorted, and dependency bitmasks recomputed. This is O(n) where n = total nodes — acceptable for ~300 nodes.

---

## 14. Glossary

| Term | Definition |
|------|-----------|
| **Cell** | A named slot in the global table. Holds a number, data array, callable, or nil. Indexed by SymbolID. |
| **Node** | A single computation step in a signal graph. Takes 0-3 inputs, produces one output. 20 bytes. |
| **Node Pool** | The shared, hash-consed collection of all nodes across all outputs. |
| **Graph Builder** | The recursive-descent compiler that reads tokens and emits nodes. Replaces the bytecode compiler. |
| **Executor** | The forward-pass evaluator that runs a topologically-sorted node array. Replaces both VM executors. |
| **Cold Path** | Code that runs once per user action: define, defn, set-bpm, output assignment. Can allocate. |
| **Hot Path** | Code that runs per sample: the executor. Zero allocation, zero string lookup, zero pointer chasing. |
| **Time Context** | A node index representing "what t means right now." Modified by fast/slow/offset during graph construction. Invisible to the executor. |
| **CSE** | Common Subexpression Elimination. Automatic via hash-consing — identical nodes deduplicate. |
| **Constant Folding** | Pervasive build-time evaluation: every node constructor checks if all inputs are `Const` and evaluates immediately. Composes transitively — arbitrarily deep pure subexpressions collapse to a single `Const` node. Enables `range`, computed collections, and complex constant expressions without a separate optimization pass. |
| **LKG** | Last Known Good. When an output fails to compile or errors at runtime, its last successful value is held. |
| **Source Arena** | Append-only string buffer storing callable body text. Referenced by token offset. |
| **Data Pool** | Shared flat array of doubles for vector/step/gates data tables. |

---

## 15. Audit Findings — Items Not Covered by Initial Spec

The following gaps were identified by a systematic audit of the full codebase against the spec. Each is categorized as either a spec addition (must be addressed), a design decision (needs input), or a known omission (intentionally deferred).

### 15.1 Features Missing from the Signal Path

**Random / noise functions:**
- `random` (0-2 args): Returns a deterministic per-beat hash, NOT per-sample noise. Hashes `beat-num` via `simple_hashing_function`. Range: `(random)` → [0,1], `(random lo hi)` → [lo,hi]. Currently NOT VM-compiled — falls back to tree-walker.
- `index-rand` (1-3 args): Pure hash function of its index argument. Stateless, deterministic. Same index always returns same value.
- **Spec addition:** Both must compile to graph nodes. `random` needs a `HashBeat` node op that hashes the beat-num value. `index-rand` needs a `HashIndex` node op. Neither requires mutable state — they're pure functions of their inputs.

**Ratio-rhythm functions (`rpulse`, `rstep`, `ridx`, `rwarp`):**
- These operate on phasors and ratio arrays. Currently NOT VM-compiled.
- `rpulse(ratios, pulseWidth, phase)`: trigger pulses at ratio-defined subdivisions
- `rstep(ratios, phase)`: step value at ratio boundaries
- `ridx(ratios, phase)`: which subdivision index we're in
- `rwarp(ratios, phase)`: phasor warping by ratio weights
- **Spec addition:** These should compile to node subgraphs using data tables + arithmetic. The ratio array is a data table. The subdivision lookup is floor/modulo arithmetic on the cumulative sum of ratios.

**`loop-at(duration, expr)`:**
- Wraps time: evaluates `expr` at `fmod(raw_t, duration)`. A time warp in seconds (not beats).
- **Spec addition:** Compiles identically to `offset`/`fast`/`slow` — it modifies the TimeContext: `inner.t_node = Fmod(ctx.t_node, duration_node)`.

**`dm(condition, default, value)` — decision map:**
- Binary conditional: returns `value` if `condition > 0`, else `default`.
- **Spec addition:** Compiles to `Select(CmpGt(condition, Const(0)), value, default)`. Two nodes.

**`eval-at-time(time_seconds, expr)`:**
- Evaluates `expr` with raw time set to `time_seconds`. User-facing time-manipulation primitive.
- **Spec addition:** Compiles as: `inner.t_node = time_node; compile body with inner`. Same mechanism as `fast`/`slow`/`offset`.

**`gatesw(data, phase)` — gate with width encoding:**
- Pattern values 1-9 control pulse width (value/9.0). Output is binary.
- **Spec addition:** Compiles to a subgraph: read pattern value, divide by 9 for width, compare fractional phase against width.

**`trigs` vs `gates` distinction:**
- `gates`: long rectangular pulses (default 50% width). Binary output.
- `trigs`: short pulses (default 10% width) with per-step velocity (pattern values 1-9 → amplitude). Output is velocity-scaled.
- **Spec addition:** Both compile to node subgraphs using VecIndex + phase comparison. `trigs` additionally scales by `clamp(value/9, 0, 1)`.

**`zeros(n)`:**
- Creates a vector of N zeros: `[0 0 0 ... 0]`.
- **Spec addition:** Cold-path only. Creates a Data cell with N zeros in the data pool.

### 15.2 Semantic Distinctions

**`set` vs `define` — critical difference:**
- `define`: stores BOTH the value AND the unevaluated source expression. Enables re-evaluation in outputs and flash persistence.
- `set`: stores ONLY the value. Explicitly REMOVES any stored expression. Variables created with `set` are NOT persisted to flash and NOT re-evaluated.
- **Spec addition:** The cell table needs to distinguish these. A `Number` cell created by `define` preserves the source expression (for re-evaluation). A `Number` cell created by `set` does not. This affects dependency tracking: `define`d expressions can be inlined and recompiled; `set` values are opaque constants.
- **Design decision:** In the new engine, `set` could simply be "define but without storing the expression." The cell has a value but no source offset.

**Output reassignment is silent replacement:**
- `(a1 (sin beat)) (a1 (cos beat))` — second overwrites first, no warning.
- Multiple forms are implicitly wrapped in `do`, so both execute sequentially.
- **Spec addition:** Document this behavior. Consider emitting a hint diagnostic: "a1 was just assigned — this overwrites it."

**`defs` — multi-define:**
- `(defs name1 expr1 name2 expr2 ...)` — syntactic sugar for multiple `define`s.
- **Spec addition:** Cold-path interpreter handles this by iterating pairs.

### 15.3 Cross-Output References

**Output getters (`get-a1` through `get-d8`) allow one output to read another's value.**

- Evaluation order is sequential by index: a1, a2, a3, ..., d1, d2, ...
- `a2` reading `get-a1` gets the CURRENT tick's a1 value (already computed).
- `a1` reading `get-a2` gets the PREVIOUS tick's a2 value (not yet computed this tick).
- **Spec addition:** Add a `PrevOutputLoad` node op that reads from a previous-tick output buffer. All cross-output references use previous-tick values for consistency (eliminates evaluation-order dependency). Alternatively, maintain the sequential order but document it explicitly.
- **Design decision needed:** Should cross-output references see current-tick (order-dependent) or previous-tick (order-independent) values?

### 15.4 Transport and Time

**Pause behavior:**
- When paused, the firmware skips the entire update loop (`tick()` returns early). Hardware outputs hold their last PWM/DAC voltage via hardware latching.
- **Spec addition:** The new engine should explicitly hold LKG values during pause, not rely on hardware state persistence. When `is_playing == false`, skip executor but write last-known values to outputs.

**`useq-clear`:**
- Resets all output expressions to defaults (0.5 for analog, 0 for digital). Erases stored expressions.
- **Spec addition:** Cold-path command. Sets all output root nodes to a Const(default_value) node. Clears associated source and dependencies.

**Time offset (`useq-set-time-offset`, `useq-nudge-time`):**
- Global offset added to raw time before it enters the graph. Units: seconds (currently microseconds internally, but should be seconds in the new engine).
- `set-time-offset`: absolute. `nudge-time`: relative (adds to current offset).
- **Spec addition:** The `t` value passed to the executor is `raw_time + global_offset`. The offset is stored as a cell or a dedicated field.

**BPM changes are instantaneous:**
- No ramping or smoothing. Changing BPM mid-bar causes a phasor discontinuity.
- **Spec note:** This is the current behavior and is acceptable. Tempo ramping is a future enhancement.

### 15.5 Persistence (Flash Storage)

The firmware persists state to flash on command (`memory-save`) and auto-loads on boot:
- Stored: all `define`d variables (name + value + source expression) and output assignments.
- NOT stored: `set` variables (no source expression).
- Format: packed null-terminated strings for names and expressions.
- **Spec addition:** The new engine needs a serialization format for the cell table. Cells with source expressions (from `define`) are persisted. Cells without (from `set`) are not. Output expressions are persisted as source text.
- **Design decision:** Reuse the existing flash format for backward compatibility, or define a new compact format?

### 15.6 I2C Networking

- `send-to(index, expr)`: Serializes a Lisp expression as text and sends via I2C to another module.
- Unidirectional only. String-based protocol.
- **Spec note:** This is orthogonal to the signal engine. The cold-path interpreter handles `send-to` by serializing the expression and calling the I2C driver. No changes needed for the signal engine itself.

### 15.7 JSON Serial Protocol

Message types: `hello` (handshake), `ping` (heartbeat), `eval` (execute code), `stream-config` (configure output streaming).
- The `hello` response includes IO configuration (output/input names, counts).
- The `@` prefix triggers immediate execution (vs. code-quantized deferred execution).
- **Spec note:** The protocol layer sits above the signal engine. `eval` messages dispatch to `eval_cold()`. No changes to the protocol itself; only the internals change.

### 15.8 DSP Engine (Core 1)

- Separate audio DSP engine runs on core 1 (sample playback, effects).
- Communication via lock-free queues (`queue_t`).
- DAC output queues pass float values to audio DAC hardware.
- `list-samples` reads audio file headers from flash.
- **Spec note:** The DSP engine is orthogonal to the signal engine. The signal engine (new) runs on core 0 alongside the cold-path interpreter. DSP stays on core 1. No changes needed.

### 15.9 Output Clamping

- Analog outputs: clamped to [0, 1] at the hardware write stage (PWM scaling).
- Digital outputs: thresholded at >0 on hardware, >0.5 on desktop. **This is inconsistent.**
- Clamping happens at the I/O layer, NOT in the signal engine. Raw output values (from `get-aX`) can be outside [0,1].
- **Design decision:** Should the signal engine clamp output values, or leave it to the I/O layer? Current behavior (I/O layer clamping) is fine, but the digital threshold inconsistency should be fixed (standardize on >0.5).

### 15.10 Error Recovery

- **Parse errors**: preserve the previous output expression (the bad code never reaches the graph).
- **Compilation errors**: LKG fallback (output holds its last good value, new graph is rejected).
- **Runtime errors** (NaN, Inf): the executor guards every node with `isfinite()` and substitutes 0.0. The output continues running.
- **Current firmware behavior on eval failure**: resets the output to default expression (0.5 for analog). This is destructive and should NOT be carried forward.
- **Spec addition:** On any error, preserve LKG. Never reset to default. Only `useq-clear` should reset outputs.

### 15.11 Cold-Path-Only Operations

The following MUST NOT appear in signal expressions and should produce a `Boundary` diagnostic if attempted:

- `define`, `def`, `defn`, `defun`, `defs`, `set`, `eval`
- `schedule`, `unschedule`
- `useq-play`, `useq-pause`, `useq-stop`, `useq-rewind`, `useq-clear`
- `set-bpm`, `set-time-sig`, `useq-set-time-offset`, `useq-nudge-time`
- `print`, `println`, `display`
- `map`, `filter`, `reduce` (allocate, iterate with callable — too expensive for signal path)
- `delay`, `delayus` (block the update loop — fundamentally incompatible)
- `send-to` (I2C network send)
- `memory-save`, `memory-restore`, `memory-erase`
- `timeit`, `millis`, `micros`
- Any output assignment symbol (`a1`-`s8`) inside another output

### 15.12 `quote` in Signal Context

- The parser converts `'expr` to `(quote expr)`.
- `quote` is NOT VM-compiled in the current system.
- In the new engine: `quote` is not meaningful in signal context (it returns an unevaluated AST, which is not a number). It should produce a diagnostic: "'quote' can't be used inside an output expression."
- **Exception:** Patterns like `(seq '(1 0 1 0) beat)` use quoted lists as data. In the new engine, the `seq` compiler should handle this by resolving the quoted list as a data table at compile time, NOT by evaluating `quote` at runtime.

### 15.13 `scope` Builtin

- `(scope expr1 expr2 ... exprN)`: creates a temporary local scope, evaluates all expressions, returns last.
- Semantically identical to `do` in the current system (since `do` already creates implicit scope in some contexts).
- **Spec addition:** In signal context, `scope` compiles identically to `do` (sequential evaluation, return last). In cold path, it creates a temporary scope frame.

### 15.14 `get-expr` Builtin

- `(get-expr name)`: returns the source expression stored for a symbol.
- Cold-path only. Used for metaprogramming and debugging.
- **Spec addition:** In the new engine, this reads the source arena at the cell's source offset and returns the text. Cold-path only.

### 15.15 Multi-Form Input

- When the user sends multiple forms in one message (e.g., `(define a 1) (define b 2) (a1 (+ a b))`), they are implicitly wrapped in `(do ...)`.
- **Spec addition:** The cold-path interpreter must handle `do` at the top level by evaluating each child form sequentially. This is already covered by the `eval_do_cold` path.

### 15.16 `shift` vs `offset`

- `shift` and `offset` are aliases (both accepted by the compiler).
- In the current system, `shift` operates on the PHASOR (adds to the 0-1 phase value), while `offset` operates on TIME (adds seconds to the time input). The bytecode VM treats them identically (`compile_time_warp` with offset semantics for both).
- **Design decision:** Should the new engine distinguish phasor-shift from time-offset? The current system doesn't, treating both as time-offset. If they should differ, `shift` would add to the phasor output (post-wrap), while `offset` adds to time (pre-wrap).

---

## 16. Design Decisions Requiring Owner Input

These items were surfaced by the audit and need explicit decisions:

| # | Question | Options | Current behavior |
|---|----------|---------|-----------------|
| 1 | Cross-output references: current-tick or previous-tick? | (a) Previous-tick for all (order-independent) (b) Current-tick with documented order | Current-tick, sequential order |
| 2 | Digital output threshold: >0 or >0.5? | Standardize one | >0 on hardware, >0.5 on desktop |
| 3 | Output clamping: signal engine or I/O layer? | (a) Clamp in engine (b) Clamp at I/O | I/O layer only |
| 4 | Error recovery: always LKG or sometimes reset to default? | (a) Always LKG (b) Reset after N failures | Reset to default on runtime error (destructive) |
| 5 | `shift` vs `offset`: same semantics or different? | (a) Both = time offset (b) shift = phasor shift | Both = time offset |
| 6 | Flash format: reuse existing or new? | (a) Backward compatible (b) Clean break | Existing packed-string format |
| 7 | Per-sample noise: add a `noise` node op? | (a) Yes, seeded PRNG per sample (b) No, keep per-beat hash only | Per-beat hash only |

---

## 17. Risk Register

Risks identified during the design process, carried forward.

| # | Risk | Severity | Mitigation |
|---|------|----------|------------|
| 1 | **Regression during live performance** — expressions that currently work become compile errors in the new engine, breaking code during a performance. | Medium | Feature flag rollback (`USE_LEGACY_ENGINE`). Shared semantic test suite. LKG preserves running outputs. Expand graph builder coverage before restricting. Risk is lower than initially estimated because `for` over globals already works and `while` is deferred, not removed. |
| 2 | **Rewrite gravity** — the project expands from "build the new engine" to "rebuild everything." | Medium | Strict phase discipline. Each phase ships independently. Measurement gate (Phase 4) before optimization. The new engine is ~1,200 lines — scope is inherently limited. |
| 3 | **Closure semantic regression** — the new engine accidentally changes lexical capture behavior. | Medium | Preserve existing closure tests. Value capture (inlining with dependency tracking) is the mechanism. Cell references are for global access, not for rewriting closure semantics. |
| 4 | **WASM ABI drift** — internal changes break exported function signatures or diagnostic JSON format. | High | ABI is documented in section 7. Contract tests verify all 9 exported functions. Diagnostic JSON shape is frozen. |
| 5 | **Memory pressure on RP2040** — the new data structures exceed the 264KB SRAM budget. | Low | Memory budget (section 9) shows ~107KB total, leaving ~157KB. This is less than the current system uses. |
| 6 | **Hash-cons table collision** — CSE produces incorrect results due to hash collisions. | Low | Use a strong hash (e.g., FNV-1a or xxHash on node content). Verify equality on collision (not just hash match). Test with adversarial inputs. |
| 7 | **Source arena exhaustion** — long live-coding sessions fill the 16KB source arena. | Low | Compaction on demand (copy live entries to fresh arena). Monitor usage. 16KB is generous for typical sessions. WASM can use a larger arena. |
| 8 | **No profiling data** — all performance arguments are structural, not measured. | Low (process) | Phase 4 is explicitly a measurement gate. Profile before optimizing. |

---

## 18. RP2040 Target Constraints (Reference)

For quick reference during implementation:

- **CPU**: Dual Cortex-M0+ at 250MHz (overclocked from 133MHz)
- **SRAM**: 264KB total
- **Flash**: 2MB minimum (Pico), 16MB on Music Thing variant
- **FPU**: None (soft float). Every `sin()`, `cos()`, `*`, `+` is a function call.
- **Data cache**: None. Memory access patterns matter. Contiguous arrays are optimal.
- **Instruction cache**: 16KB per core. Keep hot-path code small.
- **Dual-core**: Core 0 = signal engine + cold-path interpreter. Core 1 = DSP audio engine. Communication via lock-free queues only. No shared mutable state between cores.
- **C++ standard**: gnu++17
- **Optimization**: -O3
- **Build system**: PlatformIO (firmware), Meson (desktop/test), Emscripten (WASM)
- **Key build flags**: `USE_FIXED_MEMORY_POOLS` (all embedded), `MUSICTHING` / `USEQHARDWARE_0_2` / `USEQHARDWARE_1_0` (hardware variant)
- **Future target**: RP2350 (hardware FPU, more SRAM). Phase 4 optimization decisions should consider this — e.g., the soft-float cost that dominates RP2040 profiling may disappear on RP2350.
