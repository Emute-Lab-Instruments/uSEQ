# Top-Level Forms (Imperative Surface)

> Spec: the surface the user types at and the editor evaluates. Each
> top-level eval is a single form (or a `do` of multiple forms) that runs
> immediately. Counterpart to [MAIN.md](MAIN.md). See
> [compilation.md](compilation.md) for what is rejected when these forms
> appear inside signal context.

## Source files

- `uSEQ/src/signal_engine/cold_eval.cpp` — main top-level eval dispatch: `define`, `defn`, `do`, `if`, `let`, output assignment, transport commands (`useq_play`/`useq_pause`/`useq_stop`/`useq_rewind`/`useq_clear`), `do_set_bpm()`, cell mutation and dependency invalidation (`on_cell_changed()`).
- `uSEQ/src/signal_engine/cold_eval.h` — `eval_cold()` entry point; `SignalEngine` struct; `EngineState` (transport state); `OutputSource` / `StateUpdateSource` (stored programs for recompilation).
- `uSEQ/src/signal_engine/graph_builder.cpp` — `resolve_output_index()` maps output names to indices; output-assignment compilation.
- `uSEQ/src/signal_engine/executor.{h,cpp}` — one-off execution of compiled top-level signal expressions.
- `uSEQ/src/signal_engine/cell_store.h` / `cell_store.cpp` — `define`/`defn` write to cell and callable stores; `init_timing_defaults()` for `setbpm`/`settimesig`.
- `uSEQ/src/firmware/serial_protocol.cpp` — wire-protocol dispatch for eval requests, quantised eval queue.
- `wasm/wasm_wrapper.cpp` — `useq_eval()` calls `sig::eval_cold()`.
- `test/signal_engine/test_signal_engine.cpp` — cold eval tests for define, defn, transport.
- `test/signal_engine/test_signal_engine_golden.cpp` — golden semantics for define, defn, output assignment.

---

1.1 The "top level" is the surface the user types at and the editor evaluates. Each top-level eval is a single form (or a `do` of multiple forms) that runs immediately.

1.2 **There is no `@` prefix.** All eval is immediate by default. The
historical `@` immediate-eval marker is removed from the protocol and from
the language surface.

1.3 **Quantised eval is a wire-level flag plus a runtime-side queue, not a language operator.** A keybinding variation submits the form with `quant: true` on the eval request ([wire-protocol.md §5.7](wire-protocol.md)); the runtime buffers the form and drains the queue on the next wrap of the **global quant phasor** ([firmware.md §6](firmware.md)). The phasor defaults to `bar` and is changed via the ModuLisp builtin `(set-quant-phasor expr)` — e.g. `(set-quant-phasor (slow 2 bar))`. The submitted code itself is ordinary top-level code; quantisation is purely about *when* the runtime evaluates it, not what it is.

1.4 Top-level forms that have meaning beyond signal compilation:
- `(define name expr)` — bind a cell.
- `(defn name [params...] body)` — bind a callable cell.
- `(let [bindings...] body)` — local scope. At top level, also a value-producing expression.
- `(do form1 form2 ...)` — sequence top-level forms; result is the last form's value. Each child is itself a top-level form.
- `(if cond then else)` — conditional. The unselected branch is not evaluated.
- Output assignment forms `a1`..`s8`, `q0`.
- Transport: `(useq-play)`, `(useq-pause)`, `(useq-stop)`, `(useq-rewind)`, `(useq-clear)`, `(useq-get-transport-state)`. (See `cold_eval.cpp` transport command handlers.)
- Tempo / metre: `(setbpm bpm)`, `(getbpm n)`, `(settimesig num denom)`. (See `cold_eval.cpp` `do_set_bpm()` and `cell_store.cpp` `init_timing_defaults()`.)
- `(set-quant-phasor expr)` — set the global phasor that gates quantised eval (§1.3 / [firmware.md §6](firmware.md)). Defaults to `bar`.
- `(schedule name body period)` / `(unschedule name)` — see open question below.
- `(eval string)` — dynamic evaluation. Top-level only; rejected in signal context.
- `(print value)`, `(perf)`, `(timeit expr)` — diagnostic side effects.

1.4.1 `(useq-clear)` is a full in-memory session reset. It removes user
definitions and callables, callable/output/state source text, data tables,
active graphs and dependencies, state resources, live-edit slots, synth
declarations/control roots, scratch compiler state, and diagnostics/cache
generations. It then restores only the standard timing cells at their default
values. Names and bounded resource IDs are immediately reusable as in a fresh
session. It does not erase a persistent flash save and is not an immediate
electrical output command; see [outputs.md §1.9](outputs.md).

1.4.2 `(useq-pause)` freezes logical time and state. `(useq-play)` resumes
without counting paused wall time. `(useq-rewind)` resets logical time to zero
while preserving programs and state. `(useq-stop)` is exactly pause plus
rewind and also preserves programs and state. See [time.md §1.1.3](time.md).

1.5 Top-level forms are **eagerly evaluated for their side effects**. The result of a top-level form may also be returned to the editor for display.

1.6 The compiler may **compile top-level forms to a node graph for performance** even when they only run once. This does not change their semantics; it just means "compiles to a node graph" is not synonymous with "is a signal" — see [compilation.md §1.5](compilation.md).

## 2. Top-Level Signal Expression Results

2.1 If a top-level form is not a recognised side-effect form, transport form,
definition form, output assignment, or other eager command, the runtime should
treat it as a signal expression query. It compiles the expression, executes it
at the runtime's current time, and returns the current value.

Examples:

```lisp
bar
(* bar 0.5)
(eval-at-time 2 bar)
(from-list [1 2 3] bar)
```

2.2 A bare top-level temporal symbol such as `bar`, `beat`, `phrase`, `section`,
`beat-num`, `bar-num`, `beat-dur`, or `bar-dur` is a signal expression query,
not an unknown symbol to be silently ignored. It returns the value it would
have inside an output expression at the current eval time.

2.3 A top-level vector evaluates each element and returns a printable vector
when every element can be represented as a value:

```lisp
[(eval-at-time 0 bar) (eval-at-time 0.5 bar)]
```

may return:

```lisp
[0 0.25]
```

This supports editor probe batching, where the host submits a vector of
`eval-at-time` expressions in one `useq_eval()` call.

2.4 One-off expression compilation must not mutate active output programs.
Specifically, it must not leak unreachable nodes into the live `NodePool`, must
not corrupt the live CSE table, and must not append literal vector data tables
to the live `CellStore`.

2.5 A conforming implementation therefore uses isolated scratch compilation
state for top-level expression queries. It may still read live cells, live
named state, previous outputs, hardware inputs, and current timing values.

2.6 Stateful top-level expressions follow the identity rules in
[state-identity.md](state-identity.md). A named `defstate` reference reads live
state. An anonymous stateful expression without a state ID may use fresh
scratch state. A stateful expression with a state ID may use the persistent
state-resource registry.

2.7 Compile errors in top-level expression queries are ordinary eval errors
with diagnostics. A genuinely unknown top-level form may remain a compatibility
no-op only if it cannot be parsed as a valid signal expression and preserving
that behaviour is required by an existing host.

## Open / Deferred

3.1 **`schedule` and `unschedule`.** The historical contract is
`(schedule name body period)`, where `body` runs `period` times per bar,
and `(unschedule name)` removes it. Whether that survives in the same shape,
gets folded into the global-quant-phasor mechanism (§1.3), or is replaced by
something else is not yet decided. Treat current behaviour as historical
compatibility; the spec needs updating once the design lands. See
[MAIN.md §5.1](MAIN.md).
