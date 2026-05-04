# Values and Types

> Spec: the value tower, numeric model, truthiness, vectors, callables,
> `nil`. Counterpart to [MAIN.md](MAIN.md).

### Source Files

- `uSEQ/src/signal_engine/cell_store.{h,cpp}` — `CellKind` enum (`Number`, `Data`, `Callable`, `Nil`), `Cell.value` (double), `CellStore::store_data_table()` / `get_data_table()` (vector data)
- `uSEQ/src/signal_engine/node_pool.h` — `NodeOp::Const` (numeric constants), `NodeOp::DataLoad` / `VecIndex` / `VecLerp` (vector access), `NodeOp::CmpGt` / `CmpEq` / etc. (comparison returns 1.0/0.0), `NodeOp::Not` / `And` / `Or` (logic on truthiness)
- `uSEQ/src/signal_engine/graph_builder.{h,cpp}` — `compile_vector_literal()` (vector data compilation), `compile_call()` (callable inlining), `compile_lambda()`, keyword argument validation
- `uSEQ/src/signal_engine/token.{h,cpp}` — `TokenKind` (`Number`, `Symbol`, `String`, `LBracket`/`RBracket` for vectors), parser-level value representation
- `uSEQ/src/modulisp/lisp/symbol_intern.h` — `SymbolIntern`: symbol interning (symbols as integer IDs)

1.1 ModuLisp has a **Clojure-shaped value tower**: numbers, strings, symbols, keywords, lists, vectors, maps (where supported), nil, and callables (lambdas / closures).

1.2 At the parser level all of these are first-class. `define` can bind any of them. `defn` produces a callable. Vectors `[…]` are data, lists `(…)` are code; this is the convention, not a hard rule.

1.3 **Numbers are doubles.** There is no separate integer type. `1` and `1.0` are the same value. Integer-valued doubles are produced by `floor`, `ceil`, indexing, etc. The compiler may **internally** specialise integer-valued node chains (counters, vector indices, `floor`/`ceil`/comparison/`mod` chains where it can prove integer-ness) to integer arithmetic on targets without an FPU; this is invisible to the user. Whether to expose user-visible integer literals or an `(int x)` coercion is open ([MAIN.md §5.7](MAIN.md)). (See `uSEQ/src/signal_engine/cell_store.h` — Cell.value is double; `uSEQ/src/signal_engine/node_pool.h` — Node.imm is double, all NodeOps operate on doubles.)

1.4 **Truthiness in numeric context**: non-zero is true, `0.0` is false. Comparison and logic operators return `1.0` or `0.0`. To threshold a phasor to a gate, use the explicit operator (`sqr`, `pulse`, `(> phasor 0.5)`); truthiness is not redefined to "≥ 0.5", because that would silently invert the meaning of `(if x …)` for bipolar signals near zero (`sin`, audio).

1.5 **Truthiness in non-numeric context**: `nil` and `0`/`0.0` are false; everything else is true. This is consistent with Clojure modulo the absence of a dedicated boolean type.

1.6 **Signal context is type-restricted by reachability, not by syntax.** Most signals reduce to a numeric value at the root; that is the common case and the one the engine optimises. Signals of non-numeric types (e.g. a string-valued signal driving a visual live-coding panel) are permitted in principle and a conforming engine may support them; engines that only target audio/CV outputs may reject non-numeric roots at compile time.

1.7 **Vectors of numbers are first-class signal data.** They lower to fixed-size data tables and are consumed by `step`, `gates`, `seq`, `interp`, `for`, etc. A vector containing time-varying expressions (e.g. `[1 (sin beat) 3]`) is also valid; each slot is its own signal. (See `uSEQ/src/signal_engine/cell_store.{h,cpp}` — CellStore::store_data_table, data_pool, data_offsets, data_lengths, flat double arrays; `uSEQ/src/signal_engine/graph_builder.cpp` — compile_vector_literal, resolve_data_table; `uSEQ/src/signal_engine/node_pool.h` — NodeOp::DataLoad, VecIndex, VecLerp.)

1.8 **Callables in signal context are pure.** They may close over compile-time-resolvable values but not over mutable state. They are inlined at every call site, unless it's beneficial to performance otherwise. See [functions.md](functions.md) for the full callable contract.

1.9 **`nil` is a value, not an error.** `(define x nil)` is fine. Reading an unbound symbol is a compile-time error, not a `nil`-returning operation.

## 2. Keyword Arguments

2.1 Keywords are first-class values. A keyword literal begins with `:` and
evaluates to itself: `:phase`, `:id`, `:fresh`.

2.2 Function and primitive calls may accept trailing keyword arguments when the
callee's signature declares them. The general shape is:

```lisp
(callee positional1 positional2 :option value :flag true)
```

2.3 Keyword arguments are parsed as ordinary alternating keyword/value pairs
after the positional arguments. Unknown keywords, duplicate keywords, missing
values, or keyword arguments before required positional arguments are
compile-time errors.

2.4 Keyword arguments are part of a primitive's compile-time contract. In signal
context, a primitive may require some keyword values to be compile-time
resolvable because they affect lowering or state-resource identity. For
example, UGens may use `:phase`, `:id`, and `:fresh` to control initial phase
and state-sharing ([state.md section 6.9](state.md),
[state-identity.md](state-identity.md)).

## Open / Deferred

3.1 **Map / dict values.** Whether ModuLisp has Clojure-style maps as first-class values, and what their signal-context semantics would be, is open.

3.2 **Non-numeric signal roots.** See [MAIN.md §5.3](MAIN.md). Strings flowing through signal graphs to non-CV sinks (visual live-coding, MIDI text) is intended capability but not currently implemented in either engine.

3.3 **User-visible integer types.** See [MAIN.md §5.7](MAIN.md).
