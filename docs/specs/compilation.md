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
- `test/signal_engine/test_node_pool_traversal.cpp` — adversarial reachability,
  execution-order, and GC tests for high-sharing DAGs

1.1 Every signal expression is compiled to a **node graph** — a finite,
acyclic, topologically-sortable structure stored as a flat node array. The hot
runtime is a graph executor, not a bytecode virtual machine. (See
`uSEQ/src/signal_engine/node_pool.h` — `Node`, `NodeOp`, and the flat node
array; `uSEQ/src/signal_engine/graph_builder.{h,cpp}` — compilation to the node
graph.)

1.2 Compilation is **always at least these passes**: parse → name resolution (cells, locals, hardware inputs, builtins) → constant folding → time-context propagation → builtin lowering → dead-code elimination → CSE / hash-consing → register/node allocation. (See `uSEQ/src/signal_engine/token.cpp` — parse/tokenize; `uSEQ/src/signal_engine/graph_builder.cpp` — name resolution in compile_symbol, constant folding in make_binop/make_unary, time-context via TimeContext, builtin lowering in form_table dispatch, CSE in NodePool::intern_node; `uSEQ/src/signal_engine/node_pool.cpp` — hash-consing, rebuild_execution_order, gc_unreachable_nodes.)

1.3 **Pervasive, IEEE-sound constant folding.** Any pure operation whose
required inputs are compile-time constants is evaluated at compile time. This
composes transitively. Rewrites that discard a dynamic operand are permitted
only when they preserve observable non-finite behavior: in particular,
dynamic `x * 0`, `0 * x`, and `x - x` remain runtime nodes because NaN/Inf at
the output root participates in the failure model.

1.4 **Time-substitution flattening.** `fast`/`slow`/`offset`/`shift` compile as pure time substitution. Constant affine chains compose into a single `(scale, offset)` pair that is baked into the temporal-leaf load. Dynamic arguments remain ordinary graph nodes unless the compiler can prove a safe simplification. General `time-as`/`premap` forms compile as explicit local time substitution and do not imply rate integration. See [time-warps.md](time-warps.md).

1.5 **A node graph is not always a signal.** Most node graphs are persistent and re-run per sample (the signal case). Some are one-off — the engine compiles a top-level form to a graph, executes it once, discards it. The compile-time rejection rules in §3 apply to any code being compiled to a node graph, regardless of whether the graph will be re-run or executed once.

1.6 **Dependency set.** Every compiled graph carries the complete set of cell
symbols it inlined or loaded — including stateful (`defstate`) cells whose
update bodies are recompiled when their dependencies change. The runtime
indexes outputs by these sets so cell mutations can target precisely the
affected graphs. A set that exceeds `MAX_OUTPUT_DEPS` is an `Overflow` compile
error; truncation is forbidden because it would make later mutations
silently fail to recompile the graph.

1.7 **Reactive recompilation is cold-path and immediate.** Cell mutation
recompiles affected stored output and state-update sources during the same
cold eval, never on the per-sample hot path. Each candidate uses the
publication rule in §1.12: a rejected dependent retains its old root, source,
dependencies, state ownership, validity, and capacity.

1.8 **Loops.** `for` is unrolled at compile time when the collection is compile-time-resolvable (literal vector of constants, cell-bound numeric vector, `(range a b)` with constant args, etc.). Maximum unroll: 64 iterations. Larger or dynamic collections are an error in signal context. `while` with non-trivial dynamic exit conditions is currently deferred; treat it as effectively top-level only for now.

1.9 **Higher-order operations** (`map`, `reduce`, `filter`) on constant collections are unrolled the same way `for` is. On dynamic collections in signal context they are errors until generalised loop support arrives.

1.10 **Numerical health.** Per-sample evaluation produces an `IEEE 754 double`. NaN/Inf produced by an individual node is the engine's signal that the program is unhealthy this sample. Whole-output LKG fallback is the canonical recovery; see [failure-model.md](failure-model.md).

Division and modulo use IEEE operations without a zero-divisor shortcut.
Therefore constant folding and hot execution both preserve `Inf`/`NaN` for a
zero divisor; the configured output failure policy, not the arithmetic
primitive or optimizer, performs any LKG substitution or legacy zero squash.

1.11 **Sub-tick guarantees.** The hot path (sampling) never allocates, never does string-keyed lookup, never compiles. All compilation work happens between ticks.

1.12 **Per-form publication transaction.** A top-level form validates its
complete syntax/arity and builds a candidate before publishing any cell,
callable source, output source/root/dependencies, vector table, live-edit
slot, state registry/update root, synth declaration/control root, execution
order, or classification. A rejected form is observationally equivalent to
not submitting it.

1.13 **Sequential submissions.** A submission containing multiple forms, and
the children of `do`, is an ordered sequence of §1.12 transactions. Evaluation
stops at the first rejected form. Earlier committed siblings remain; the
rejected form and all later siblings publish nothing.

1.14 **Bounded rollback.** Candidate graph construction may intern directly
into the fixed live pool, but it snapshots every structure the builder can
mutate. Failure restores the snapshot and reachability-GCs candidate nodes.
Repeating a rejected form therefore cannot consume bounded capacity.

1.15 **Exact syntax identity.** Delimiters are typed: `)` cannot close `[` and
vice versa. Identifiers longer than the tokenizer's representable limit are
syntax errors; they are never truncated and interned under an aliased name.

1.16 **Published references are validated at both ends.** Capacity must be
proved before a definition is installed, and consumers independently reject
out-of-range table/slot references. Fixed-store overflow never yields a
sentinel that can later be interpreted as a valid index.

1.17 **Reachability is bounded by nodes, not edges.** Execution-order rebuilds
and node GC discover each valid node at most once before enqueueing it. The
fixed traversal stack is therefore bounded by `node_count <= MAX_TOTAL_NODES`
even when CSE creates a high-sharing DAG with more incoming edges than nodes.
Traversal capacity must never silently drop a reachable dependency.

## 2. State-Bearing Constructs

The semantics of `defstate`/`integrate`/UGens/`time-as`/`rate-as` and their interaction with local time contexts live in [state.md](state.md). This section captures only what the compilation pipeline needs to know.

2.1 **State-slot resolution is a compile pass.** After name resolution, each
state-bearing node (`defstate`, `integrate`, UGens, and stateful local clocks)
resolves a stable resource key to a dense slot. The graph contains slot
indices; values, update roots, and owning compiler contexts live in the
runtime snapshot.

2.2 **Identity, not algebraic CSE, owns state.** A resource key is explicit
state ID (or deterministic program-context/ordinal fallback) plus resource
kind and role. Recompiling the same owning context reuses its value. Two
update writers for the same key in one or different published programs are a
compile-time boundary error; sharing is expressed as one state source plus
pure readers.

2.3 **State preservation and reclamation.** A successful recompilation keeps
values for keys still owned by that compiler context. Keys omitted by the new
program are retired, their update roots are cleared, and their slots enter a
bounded free list for later UGen or `defstate` allocation. A rejected
recompilation restores the prior registry, roots, values, owners, and free
list. See [state-identity.md](state-identity.md).

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
