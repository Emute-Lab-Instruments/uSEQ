# Compilation and Reactivity

> Spec: node-graph model, compile passes, time-warp flattening, loop
> unrolling, state-slot allocation, signal-context rejection rules.
> Counterpart to [MAIN.md](MAIN.md). See [failure-model.md](failure-model.md)
> for the contract that wraps compilation/runtime errors, and
> [state.md](state.md) and [state-identity.md](state-identity.md) for the state-bearing constructs whose
> compile-time treatment is summarised here.

### Source Files

- `uSEQ/src/signal_engine/graph_builder.{h,cpp}` — `GraphBuilder`: compilation entry points, form dispatch table, constant folding, time-context propagation, CSE, dependency tracking, signal-context rejection, error reporting with fuzzy match
- `uSEQ/src/signal_engine/node_pool.{h,cpp}` — `NodePool`, `Node` (the node-graph representation), `NodeOp` enum (all node operations), hash-cons CSE table, topological sort (`rebuild_execution_order`), state slots, constant-folding helpers (`eval_unary`, `eval_binop`, `eval_ternary`)
- `uSEQ/src/signal_engine/token.{h,cpp}` — `TokenStream`: zero-allocation tokenizer, `Token` with source spans
- `uSEQ/src/signal_engine/cold_eval.{h,cpp}` — `eval_cold()`: parse + compile + dispatch, `on_cell_changed()` for dependency-driven recompilation
- `uSEQ/src/signal_engine/executor.{h,cpp}` — `execute_all_outputs()`: single-sample forward pass (the hot path), `commit_state()`
- `uSEQ/src/signal_engine/types.h` — limits (`MAX_TOTAL_NODES`, `MAX_STATE_SLOTS`, `MAX_INLINE_DEPTH`, etc.)
- `uSEQ/src/signal_engine/symbols.def` — X-macro symbol table (builtins, side-effect forms, output names)
- `uSEQ/src/signal_engine/diagnostics.{h,cpp}` — `Diagnostic` struct, `find_fuzzy_match()` for "did you mean" suggestions
- `test/signal_engine/test_signal_engine.cpp` — compilation and execution tests
- `test/signal_engine/test_signal_engine_golden.cpp` — golden semantic tests

1.1 Every signal expression is compiled to a **node graph** — a finite, acyclic, topologically-sortable structure of pure-arithmetic nodes. The exact graph format is engine-specific (register bytecode in the current VM; flat node array in the redesign); the abstract model is the same. (See `uSEQ/src/signal_engine/node_pool.h` — Node struct, NodeOp enum, the flat node array; `uSEQ/src/signal_engine/graph_builder.{h,cpp}` — compilation to node graph.)

1.2 Compilation is **always at least these passes**: parse → name resolution (cells, locals, hardware inputs, builtins) → constant folding → time-context propagation → builtin lowering → dead-code elimination → CSE / hash-consing → register/node allocation. (See `uSEQ/src/signal_engine/token.cpp` — parse/tokenize; `uSEQ/src/signal_engine/graph_builder.cpp` — name resolution in compile_symbol, constant folding in make_binop/make_unary, time-context via TimeContext, builtin lowering in form_table dispatch, CSE in NodePool::intern_node; `uSEQ/src/signal_engine/node_pool.cpp` — hash-consing, rebuild_execution_order, gc_unreachable_nodes.)

1.3 **Pervasive constant folding.** Any pure operation on constant inputs is evaluated at compile time. This composes transitively: deeply nested pure subexpressions collapse to a single `Const` node.

1.4 **Time-substitution flattening.** `fast`/`slow`/`offset`/`shift` compile as pure time substitution. Constant affine chains compose into a single `(scale, offset)` pair that is baked into the temporal-leaf load. Dynamic arguments remain ordinary graph nodes unless the compiler can prove a safe simplification. General `time-as`/`premap` forms compile as explicit local time substitution and do not imply rate integration. See [time-warps.md](time-warps.md).

1.5 **A node graph is not always a signal.** Most node graphs are persistent and re-run per sample (the signal case). Some are one-off — the engine compiles a top-level form to a graph, executes it once, discards it. The compile-time rejection rules in §3 apply to any code being compiled to a node graph, regardless of whether the graph will be re-run or executed once.

1.6 **Dependency set.** Every compiled graph carries the set of cell symbols it inlined or loaded — including stateful (`defstate`) cells whose update bodies are recompiled when their dependencies change. The runtime indexes outputs by their dependency sets so cell mutations can target precisely the affected graphs. (See `uSEQ/src/signal_engine/graph_builder.h` — GraphBuildResult.dep_cells, GraphBuilder.dep_cells, add_dependency; `uSEQ/src/signal_engine/node_pool.h` — OutputDeps; `uSEQ/src/signal_engine/cold_eval.cpp` — on_cell_changed walks output_deps to find affected graphs.)

1.7 **Invalidation is proactive, recompilation is lazy.** Cell mutation marks affected graphs dirty immediately. Dirty graphs are recompiled at the next sampling boundary, never on the per-sample hot path. (See `uSEQ/src/signal_engine/cold_eval.cpp` — on_cell_changed triggers recompilation for dependent outputs.)

1.8 **Loops.** `for` is unrolled at compile time when the collection is compile-time-resolvable (literal vector of constants, cell-bound numeric vector, `(range a b)` with constant args, etc.). Maximum unroll: 64 iterations. Larger or dynamic collections are an error in signal context. `while` with non-trivial dynamic exit conditions is currently deferred; treat it as effectively top-level only for now.

1.9 **Higher-order operations** (`map`, `reduce`, `filter`) on constant collections are unrolled the same way `for` is. On dynamic collections in signal context they are errors until generalised loop support arrives.

1.10 **Numerical health.** Per-sample evaluation produces an `IEEE 754 double`. NaN/Inf produced by an individual node is the engine's signal that the program is unhealthy this sample. Whole-output LKG fallback is the canonical recovery; see [failure-model.md](failure-model.md).

1.11 **Sub-tick guarantees.** The hot path (sampling) never allocates, never does string-keyed lookup, never compiles. All compilation work happens between ticks.

## 2. State-Bearing Constructs

The semantics of `defstate`/`integrate`/UGens/`time-as`/`rate-as` and their interaction with local time contexts live in [state.md](state.md). This section captures only what the compilation pipeline needs to know.

2.1 **State-slot allocation is a compile pass.** After name resolution and CSE, the compiler walks the graph and assigns a stable slot index to every state-bearing node (`defstate` cell, `integrate` call, UGen instance, `rate-as` local clock, and any per-call-site scratch required by a primitive). The slot index is part of the compiled graph; the slot vector is part of the runtime state. (See `uSEQ/src/signal_engine/graph_builder.cpp` — compile_integrate allocates state_slot_count++; `uSEQ/src/signal_engine/node_pool.h` — NodePool::state_values[], state_update_roots[], state_slot_count, NodeOp::LoadState.)

2.2 **CSE applies to state-bearing nodes.** Two referentially-equal state-bearing nodes (same operator, same input subgraph, same local-clock context, same identity keywords) hash-cons to a single node and share one state slot. Different inputs, different `rate-as` contexts, or different `:id` / `:fresh` options produce distinct slots. This is FRP-correct: structurally identical state-bearing constructs *are* the same accumulator unless the user supplies distinct identity.

2.3 **State migration across recompilation.** When a cell mutation triggers recompilation, the compiler builds a fresh slot table and migrates state values from the old table to the new one. Identity rules: named `defstate` cells migrate by symbol name; anonymous state slots (`rate-as`, UGens, `integrate`) migrate by explicit state ID plus resource schema when one is available, and by best-effort anonymous structural identity otherwise. Slots without a match in the old table initialise to `init` (for `defstate`) or to the appropriate neutral value. See [state.md section 4](state.md), [state.md section 6.9](state.md), and [state-identity.md](state-identity.md).

2.4 **`dt` is a per-tick input.** Alongside `t`, the executor receives `dt-wall` per tick (the wall-clock duration since the previous tick). The unqualified `dt` leaf inside a state-bearing body resolves to the current local-clock delta: outside `rate-as`, `dt == dt-wall`; inside `rate-as r`, `dt == r * dt-wall`. `time-as` changes the local time position but does not itself scale `dt`.

2.5 **Constant folding has limited reach across state.** `(integrate 0)` folds to `0`. `(integrate const)` folds to `(* const t)`. Beyond these, state-bearing nodes are opaque to constant folding — their state evolves over time and isn't resolvable at compile time.

2.6 **No symbolic-integration pass for substitution factors.** The compiler does not symbolically integrate dynamic time-substitution expressions to give closed-form `(sin (* k t))` expressions phase coherence. The intended path for phase coherence is `rate-as`, `integrate`, or a UGen. See [state.md §10.5](state.md).

2.7 **Keyword arguments affect lowering.** Primitive calls may include trailing keyword arguments. The compiler validates keyword names and values during builtin lowering. For state-bearing primitives, identity-related keyword arguments such as `:id` and `:fresh` participate in state-resource identity. Duplicate active IDs are validated according to [state-identity.md](state-identity.md).

## 3. Compile-Time Rejection in Signal Context

3.1 An expression is in **signal context** iff it is being compiled to a node graph (§1.5). The same rejection rules apply whether the graph will be sampled forever or executed once for performance.

3.2 **Side-effect forms are rejected.** This includes (non-exhaustive): `define`, `defn`, `defstate` (state declaration is a top-level act, not a signal-context one — see [state.md §2.9](state.md)), output assignments (`a1`..`s8`, `q0`), `schedule`, `unschedule`, transport commands (`useq-play`, etc.), `eval`, `setbpm`, `settimesig`, `print`, `perf`, `timeit`. (See `uSEQ/src/signal_engine/graph_builder.cpp` — is_side_effect_form checks symbols tagged "side_effect" in symbols.def; `uSEQ/src/signal_engine/symbols.def` — side-effect symbol tags.)

3.3 **Side effects are rejected anywhere in the subtree**, not just at the top of the form. `(a1 (if (> beat 0.5) (define x 1) 0))` is an error.

3.4 **Reachable callables are checked transitively.** If a signal calls `(my-fn beat)` and `my-fn` contains a `define`, the compile error is raised against the signal call site.

3.5 **Recursion is rejected in signal context.** Direct (`f` calls `f`) and mutual (`f` → `g` → `f`) recursion both fail with a "calls itself / each other" diagnostic. Note that this is recursion among *callables*; recursive *update equations* in `defstate` bodies are explicitly allowed and are the whole point of state declaration ([state.md §2.3](state.md)).

3.6 **Dynamic `eval` is rejected in signal context.** `(eval string)` is top-level only.

3.7 **Unresolvable symbols are rejected** with a fuzzy-match suggestion ("did you mean `beat`?") when one is plausible. (See `uSEQ/src/signal_engine/graph_builder.cpp` — report_error_with_fuzzy_match; `uSEQ/src/signal_engine/diagnostics.cpp` — find_fuzzy_match, Levenshtein distance, prefix match, case-insensitive match.)

3.8 **Type errors the compiler can prove** are rejected (e.g. arithmetic on a string-cell where the operator has no string overload).

3.9 **Dynamic affine sugar arguments are allowed as substitution.** `fast`/`slow`/`offset`/`shift` may take dynamic signal arguments, but they always lower to stateless time substitution. They never imply phase-coherent rate integration. Dynamic phase-coherent speed changes should be written with `rate-as`, `integrate`, or a UGen.

3.10 The diagnostic for a rejected form must use **plain language**, not jargon. "This form can only be used at the top level, not inside an output." "This function calls itself — recursive functions can't be used in outputs." Suggestions should include a working example. For an inline `defstate` rejection, the diagnostic should suggest the equivalent stateful primitive (`integrate`, `osc`, etc.) for the inline case. For dynamic `fast`/`slow` around oscillator-like closed forms, the compiler/editor may emit a non-blocking hint: "this is stateless time substitution; use `rate-as` for phase-coherent speed changes."

3.11 The diagnostic framing is **"the compiler doesn't support this here"**, not "the language forbids this." Some restrictions are temporary (dynamic loops, non-numeric signal roots, etc.) and may relax in future versions.

## 4. Open / Deferred

4.1 **General `while` and dynamic loops.** Currently rejected in signal context beyond the trivially-bounded cases. A future loop-completeness pass will define what cycles, exit conditions, and counter types are admissible.

4.2 **Cross-target floating-point determinism.** Soft-float ARM vs. x86/WASM hard-float can diverge on transcendentals. The contract is "within tolerance" rather than "bit-identical"; the precise tolerance ladder is not currently specified — it lived in the deleted bytecode-VM spec and needs to be re-stated against the current engine.

4.3 **Locally-scoped state.** `defstate` is currently top-level only ([state.md §2.9](state.md)). Future extension to allow stateful bindings inside `let`/`defn` bodies would change the rejection rules here. Defer until a use case demands it.
