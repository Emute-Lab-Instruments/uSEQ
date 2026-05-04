# Cells and Reactivity

> Spec: cells as named bindings, reactive invalidation, dependency tracking,
> cascade propagation. Counterpart to [MAIN.md](MAIN.md). Imperative-mode
> cell semantics are in [dialects.md §2](dialects.md).

### Source Files

- `uSEQ/src/signal_engine/cell_store.{h,cpp}` — `CellStore`, `Cell` (kind, value, revision), `CellKind` enum, `CallableInfo`, `SourceArena`, data pool, `snapshot_values()`, `init_timing_defaults()`
- `uSEQ/src/signal_engine/cold_eval.{h,cpp}` — `eval_cold()` (handles `define`/`defn`), `on_cell_changed()` (reactive invalidation + cascading recompilation), `OutputSource` (stored source for recompilation)
- `uSEQ/src/signal_engine/graph_builder.{h,cpp}` — `compile_symbol()` (cell inlining), `add_dependency()` (dependency tracking), `inline_expression_cell()` (callable inlining), `OutputDeps` tracking
- `uSEQ/src/signal_engine/node_pool.h` — `OutputDeps` (per-output cell dependency set), `NodeOp::CellLoad`
- `uSEQ/src/signal_engine/types.h` — `MAX_CELLS`, `MAX_OUTPUT_DEPS`
- `test/signal_engine/test_signal_engine.cpp` — cell definition, redefinition, and dependency tests
- `test/signal_engine/test_signal_engine_golden.cpp` — golden tests for cell-dependent signals

1.1 A **cell** is a named binding from a symbol to a value. `(define name expr)` creates or replaces a cell. `(defn name [params...] body)` creates a cell whose value is a callable.

1.2 **Cells are reactive bindings** in Reactive mode. When the user redefines a cell, every output whose compiled graph references that cell is invalidated and recompiled with the new value. (See `uSEQ/src/signal_engine/cold_eval.cpp` — on_cell_changed walks OutputDeps to find and recompile affected outputs.)

1.3 The user model is "I changed `freq`, all signals using it follow." How that is achieved (constant-folding the new value into recompiled graphs vs. live-loading the cell value at runtime) is an implementation detail; the observable behaviour is identical for pure-numeric cells.

1.4 **Recompilation is sub-tick.** A typical signal recompiles in well under one millisecond. The user perceives redefinition as instantaneous.

1.5 **Dependency tracking is by symbol identity, not by source text.** Renaming a cell does not preserve dependencies; it is logically a new cell. (See `uSEQ/src/signal_engine/graph_builder.h` — GraphBuilder.dep_cells, add_dependency tracks SymbolIDs; `uSEQ/src/signal_engine/node_pool.h` — OutputDeps.cells SymbolID array.)

1.6 **Diamond / cascading dependencies** propagate transitively. `(define a 1) (define b (+ a 1)) (define c (+ b 1)) (a1 (* c beat))` — redefining `a` invalidates the entire chain to `a1`. (See `uSEQ/src/signal_engine/cold_eval.cpp` — on_cell_changed recompiles outputs and state cells whose dep sets contain the changed cell.)

1.7 Redefining a cell that no signal depends on is a no-op for the signal engine.

1.8 **Cells are not stateful in the imperative sense in Reactive mode.** A cell's "value" at any moment is whatever its current definition evaluates to in the current environment. There is no "previous value" of a cell.

1.9 A cell's body in Reactive mode is itself a (possibly trivial) signal expression. `(define sweep (+ 200 (* 200 t)))` creates a time-varying cell. References to `sweep` from output expressions inline its body. (See `uSEQ/src/signal_engine/graph_builder.cpp` — inline_expression_cell inlines callable body text into the graph at the reference site.)

1.10 Cell redefinitions can come from any source — user REPL evals, `schedule`d code (when implemented; see [MAIN.md §5.1](MAIN.md)), or programmatic eval. All sources go through the same dependency-invalidation path.
