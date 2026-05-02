# Dialects

> Spec: Reactive vs Imperative dialects and the rules for switching between
> them. Counterpart to [MAIN.md](MAIN.md).

1.1 ModuLisp has **two mutually-exclusive evaluation dialects**: **Reactive** and **Imperative**. A session runs in exactly one. The dialect is a runtime mode, not per-form.

1.2 **Reactive** is the default and the dialect [MAIN.md](MAIN.md) and most sub-specs describe. It uses FRP semantics: every output expression is implicitly a function of time, cells are reactive bindings, dependents auto-recompile when bindings change, the engine re-runs each output graph as fast as it can.

1.3 **Imperative** is a traditional Lisp REPL. `define` mutates eagerly and does not invalidate anything. There is no implicit time, no auto-re-evaluation, no FRP. If the user wants periodic execution, they set it up themselves (e.g. via `schedule` or external tooling). Outputs hold whatever value was last written until the user writes again.

1.4 The two dialects share the same parser, the same value tower (see [values-types.md](values-types.md)), the same standard library where it is meaningful, and the same compile/eval pipeline. They differ in **what the engine does between user evals** — re-runs everything (Reactive) vs. nothing (Imperative).

1.5 Switching dialects mid-session is allowed but resets all output bindings and dependency state. There is no "mixed" mode (although the imperative mode can be set up to imitate some of the semantics and functionality of the reactive mode).

## 2. Imperative-Mode Semantics

2.1 In Imperative mode, the FRP machinery is disabled. The language is a traditional Lisp REPL.

2.2 `(define x …)` mutates eagerly. Existing output programs are **not invalidated**. They retain whatever value of `x` they were compiled against (or, if cell-loads are dynamic, observe the new value at the next manual eval).

2.3 There is no automatic re-evaluation. Outputs hold whatever the user last wrote until the user writes again.

2.4 Time variables (`t`, `beat`, `bar`, …) are still available as queryable values, but no automatic time-stepping drives output recomputation. Periodic execution must be set up explicitly (e.g. via `schedule`, an external host loop, or a manual eval triggered on a timer).

2.5 Compile-time rejection rules (see [compilation.md](compilation.md) §2) still apply to any form being compiled to a node graph. Top-level mutation forms remain top-level only.

2.6 LKG fallback (see [failure-model.md](failure-model.md)) still applies to outputs whose programs are sampled by the engine, but it is much less load-bearing because outputs change only when the user explicitly writes them.

2.7 Imperative mode is intended for: language exploration, debugging stuck reactive graphs, experiments where the user wants explicit control of when things happen, and interop with external sequencing.

## Open / Deferred

3.1 **How dialect is selected.** Per-session, per-tab, or via a `?mode=imperative` URL param is a UI/runtime decision pending product input. The semantics here hold once a session is in imperative mode.

3.2 **`schedule` shape under imperative mode.** Whether the historical `(schedule name body period)` survives, gets folded into the global-quant-phasor mechanism, or is replaced is undecided. See [MAIN.md §5.1](MAIN.md).
