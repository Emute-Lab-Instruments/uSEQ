# Top-Level Forms (Imperative Surface)

> Spec: the surface the user types at and the editor evaluates. Each
> top-level eval is a single form (or a `do` of multiple forms) that runs
> immediately. Counterpart to [MAIN.md](MAIN.md). See
> [compilation.md](compilation.md) for what is rejected when these forms
> appear inside signal context.

1.1 The "top level" is the surface the user types at and the editor evaluates. Each top-level eval is a single form (or a `do` of multiple forms) that runs immediately.

1.2 **There is no `@` prefix.** All eval is immediate by default. Earlier docs in `../useq.md` describe an `@` operator that no longer exists; treat that section as historical.

1.3 **Quantised eval is an editor concern, not a language operator.** A keybinding variation lets the user submit code to be evaluated at the next musical boundary (default: next bar). The submitted code itself is ordinary top-level code; the editor holds it until the boundary.

1.4 Top-level forms that have meaning beyond signal compilation:
- `(define name expr)` — bind a cell.
- `(defn name [params...] body)` — bind a callable cell.
- `(let [bindings...] body)` — local scope. At top level, also a value-producing expression.
- `(do form1 form2 ...)` — sequence top-level forms; result is the last form's value. Each child is itself a top-level form.
- `(if cond then else)` — conditional. The unselected branch is not evaluated.
- Output assignment forms `a1`..`s8`, `q0`.
- Transport: `(useq-play)`, `(useq-pause)`, `(useq-stop)`, `(useq-rewind)`, `(useq-clear)`, `(useq-get-transport-state)`.
- Tempo / metre: `(setbpm bpm)`, `(getbpm n)`, `(settimesig num denom)`.
- `(schedule name body period)` / `(unschedule name)` — see open question below.
- `(eval string)` — dynamic evaluation. Top-level only; rejected in signal context.
- `(print value)`, `(perf)`, `(timeit expr)` — diagnostic side effects.

1.5 Top-level forms are **eagerly evaluated for their side effects**. The result of a top-level form may also be returned to the editor for display.

1.6 The compiler may **compile top-level forms to a node graph for performance** even when they only run once. This does not change their semantics; it just means "compiles to a node graph" is not synonymous with "is a signal" — see [compilation.md §1.5](compilation.md).

## Open / Deferred

2.1 **`schedule` and `unschedule`.** The historical contract (`(schedule name body period)` runs `body` `period` times per bar) is documented in `../useq.md`. Whether it survives in the same shape, becomes a thin wrapper over editor-side quantised eval, or is replaced by something else is not yet decided. Treat current behaviour as the historical contract; the spec needs updating once the design lands. See [MAIN.md §5.1](MAIN.md).
