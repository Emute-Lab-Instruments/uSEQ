# Failure Model

> Spec: compile-time vs runtime errors, LKG fallback, diagnostic survival,
> per-output health states, success feedback, REPL-vs-output channels,
> batch-eval isolation, cascade-noise mitigation, chain-of-blame.
> Counterpart to [MAIN.md](MAIN.md). See [compilation.md §3](compilation.md)
> for the compile-time rejection rules in signal context, and
> [diagnostics.md](diagnostics.md) for the on-wire diagnostic data shapes.

### Source Files

- `uSEQ/src/signal_engine/executor.{h,cpp}` — `execute_all_outputs()` (per-output isolation, LKG fallback via `OutputSlot.lkg_value`), `commit_outputs()` (LKG promotion), `commit_state()` (state-slot updates)
- `uSEQ/src/signal_engine/node_pool.h` — `OutputSlot` (`root_node`, active-assignment `valid`, `lkg_value`, independent `has_lkg`); runtime output/state failure masks
- `uSEQ/src/signal_engine/graph_builder.{h,cpp}` — compile-time error reporting (`report_error`, `report_error_cat`, `report_warning`), `GraphBuildResult.has_error`, dependency tracking for chain-of-blame (`dep_cells`)
- `uSEQ/src/signal_engine/cold_eval.{h,cpp}` — `EvalResult` (diagnostic array, error kind), `on_cell_changed()` (dependency-triggered recompilation — chain of blame source)
- `uSEQ/src/signal_engine/diagnostics.{h,cpp}` — `Diagnostic` struct (severity, category, span, message, suggestion), `DiagnosticSeverity`, `DiagnosticCategory`
- `wasm/wasm_wrapper.cpp` — `useq_last_diagnostics()`, `useq_active_diagnostics()`, batch-eval isolation in `useq_eval_outputs_time_window_into()`
- `uSEQ/src/firmware/firmware.{h,cpp}` — tick-loop error handling, watchdog recovery
- `test/signal_engine/test_signal_engine_robustness.cpp` — robustness and error-recovery tests

## 1. Error Categories

1.1 ModuLisp distinguishes **compile-time errors** (program never produces a value) from **runtime errors** (program ran but a sample was unhealthy).

1.2 **Compile-time errors** include: parse errors, unresolved symbols (with fuzzy-match suggestions), arity mismatches, side-effect forms inside signal context, recursion in signal context, oversized `for` collections, type errors that the compiler can prove (e.g. arithmetic on a string with no type-promoting operator), unsupported builtins for the current target, and structurally invalid forms.

1.3 Compile-time errors are **structured diagnostics**: they carry severity, category, source span, human-readable message, and a suggestion (often a working example). They surface in the editor as inline annotations and in the firmware as serial-protocol messages. The data shapes live in [diagnostics.md](diagnostics.md).

1.4 **A compile-time error does not stop the music.** The previously active program for that output (if any) keeps running. Other outputs are unaffected.

1.5 **Runtime errors** are: division by zero, out-of-bounds vector access on an unbounded path, integer-modulo by zero, NaN/Inf propagation that survives to the output root, and invalid local-clock deltas in state-bearing contexts.

1.6 `rate-as` rates follow ordinary numeric health rules. A non-finite rate is a runtime error. Zero rate is valid: the local clock stops. Negative rates are valid only for primitives that explicitly document reverse-time state semantics; otherwise they are compile-time errors when provable or runtime errors when only discovered from inputs.

1.7 **Runtime errors are time-phase-dependent.** A signal is a pure function of time and inputs; an expression valid at `beat = 0.3` may produce `NaN` at `beat = 0.95`. Runtime diagnostics describe *when* something went wrong, not just *that* it went wrong. A diagnostic carrying `transient = true` ([diagnostics.md §3.4](diagnostics.md)) signals "this fails on some samples and succeeds on others"; `transient = false` signals "this fails on every sample observed so far". The user should treat transient errors as soft signals (the output is mostly running on LKG but recovers periodically) and persistent errors as hard ones (the output has been on LKG continuously since the failure began).

1.8 **Runtime errors are deduplicated and rate-limited.** A signal sampled hundreds of times per second per output must not produce hundreds of diagnostics. The runtime collapses runtime errors by `(output, category)` and reports at most **one diagnostic per output per frame**. The diagnostic's `last_occurrence` field is updated when the same `(output, category)` recurs without a healthy sample in between. A diagnostic that persists across multiple frames may be annotated with phrasing such as "occurring frequently" — the message text is implementation-defined, the rate-limit is normative.

## 2. Last-Good-Sample Hold

2.1 **Per-sample output hold.** When an active output graph produces a
non-finite root, the runtime substitutes that output's stored last finite
sample for this sample. The active graph is retried on the next sample; a
phase-dependent failure can therefore recover without reassignment. The
runtime does not retain or execute an older compiled program.

2.2 **What is stored.** Each output has one `lkg_value` scalar. A healthy
committed sample replaces it. A substituted sample leaves the same scalar in
place; NaN/Inf is never promoted.

2.3 **State coherence.** A state resource owned by the failing output does not
commit its candidate next value on a fallback sample. It resumes from its
previous finite state when that owner next produces a healthy root. This
prevents oscillators and integrators from advancing inaudibly behind a held
sample. Named/shared state sources have their own writer context and continue
according to their own finite-update rule.

2.4 **Bootstrap.** If no healthy sample has ever been committed, no LKG exists.
The published substitute is still the compiler-domain neutral output, numeric
`0`, but health is `error`, not `fallback`; committing that substitute does
not manufacture an LKG.

2.5 **No fallback chains.** There is exactly one held scalar per output, not a
chain of candidate programs or values. Every runtime sample either publishes
its finite root or reuses that scalar.

2.6 **Compile-time errors do not consume fallback.** A program that fails to
compile is not promoted, demoted, or substituted; the active graph, held
sample, source, dependencies, state ownership, and health are unchanged. The
same publication rule applies to a rejected reactive synth-control candidate:
its prior root and control LKG remain live.

## 3. Numerical Hygiene

3.1 **Numerical errors are not silently zeroed at the output level.** Per-node NaN/Inf clamping (if any engine performs it) is an internal hygiene mechanism; the user-observable contract is "if the output goes unhealthy, you fall back to LKG, and you see a diagnostic." The engine must not silently produce subtly-wrong output instead of declaring failure.

3.2 **Configurable failure mode.** The runtime exposes two non-finite policies as a single engine-global option (`sig::FailureMode`, `uSEQ/src/signal_engine/executor.h`):

| Mode | Behaviour |
|---|---|
| `lkg` (**default**) | §3.1 semantics: values propagate freely through the node graph; a non-finite value reaching an *output root* substitutes that output's LKG value (or the neutral default per §2.4), marks the output as `fallback` (§5), and surfaces a runtime diagnostic (`useq_active_diagnostics()` on WASM). |
| `zero` (legacy) | Pre-v1.2 behaviour: every non-finite node result is clamped to `0.0` at the node level. No fallback, no diagnostic. Retained for compatibility with patches that exploited the squash. |

The mode is set over the serial wire protocol via `set-failure-mode` ([wire-protocol.md §5.18](wire-protocol.md)) and on the WASM runtime via `useq_set_failure_mode(0|1)` / `useq_get_failure_mode()`. It is global, not per-output (a per-output variant was considered and rejected as disproportionate wire/UI complexity). It defaults to `lkg` at boot/init and is not persisted by the engine; the editor re-sends its setting on connect.

3.3 **Fallback tracking is per-pass.** The engine recomputes the per-output
fallback set (`NodePool::runtime_fallback_mask`) on every live execution pass:
a sample whose root is finite clears that output's fallback state. Read-only
sampling and projection restore this diagnostic field with the rest of the
runtime snapshot and cannot create or clear live health. State commits follow
§2.3 and never accept non-finite update roots. Each rejected state update sets
its slot in `state_update_failure_mask`; a later finite candidate commits and
clears that same bit. Projection snapshots preserve both masks.

> **Status note.** Prior to v1.2.0 the implementation only performed the `zero` squash (spec drift flagged by two independent audits). The `lkg` path above is now implemented and is the default; the squash survives solely as the opt-in legacy mode.

## 4. Diagnostic Survival

4.1 **Diagnostics survive across evals.** Per-output health is queryable by the editor and shown to the user. A successful eval clears prior diagnostics for *that output* — not for the whole document. The clearing policy is detailed in [diagnostics.md §4.4](diagnostics.md).

## 5. Per-Output Health States

5.1 Every output is in exactly one of four states at any time. (See
`uSEQ/src/signal_engine/node_pool.h` — `valid`/`root_node` identify an active
assignment while `has_lkg` independently identifies a committed finite root;
`uSEQ/src/signal_engine/executor.cpp` — `output_health()` and
`commit_outputs()`.)

| State | Meaning |
|---|---|
| `idle` | No program assigned. Output emits the neutral default (`0`). |
| `running` | Active program is healthy and producing samples. |
| `fallback` | The latest live sample was unhealthy; output held its last finite sample. |
| `error` | Latest live sample was unhealthy and no LKG has ever existed; output holds neutral zero. |

5.2 Transitions:

```
idle ── (assign) ─────────► running
running ── (compile fail on new assignment) ─► running   (active unchanged)
running ── (runtime error) ──────────────────► fallback
running-with-no-LKG ── (first runtime error) ─► error
fallback ── (next healthy sample or assignment) ─► running
error ── (next healthy committed sample) ────► running
* ── (clear) ────────────────────────────────► idle
```

5.3 The state is queryable by the editor (typically once per animation frame).
It drives output-pane indicators and a "holding last good sample — view
error" badge on `fallback`.

## 6. Success Feedback

6.1 **Silence on success is also a bug.** A user typing `(a1 (sin beat))` needs to know it *worked*. If nothing visibly changes — no echo, no indicator, no waveform redraw — they cannot tell whether the code was received, compiled, or is running. The system MUST close the loop.

6.2 Success feedback travels through the same channels as error feedback but is more subdued:

- A brief confirmation that fades after roughly one second ("a1 updated").
- An output-state transition (idle/error/fallback → running), visible on the indicator.
- A waveform redraw if the output is being visualised.

6.3 Success feedback must not be noisy. It should be visible enough for a beginner to notice, quiet enough that an expert performing on stage never thinks about it.

## 7. REPL Errors vs Output Errors

7.1 The user types in two distinct contexts. **Output assignments** (`(a1 ...)`, `(d3 ...)`, etc.) install a persistent signal expression; **REPL evals** (e.g. `(+ 1 2)`, `(define foo 42)`, exploratory `(sin 0.5)`) run once and return.

7.2 The two contexts have different failure semantics:

| | Output assignment | REPL eval |
|---|---|---|
| Persistent program? | Yes | No |
| LKG fallback applies? | Yes | No — the expression either works or doesn't |
| Failure surface | Editor inline annotation; fallback indicator on the output | Inline in the console output stream |
| Cleared by | Successful reassignment of the same output | The next REPL line |

7.3 Both contexts use the same `Diagnostic` data model. Only the delivery channel differs: output errors are read from `useq_active_diagnostics()` and `useq_output_diagnostics()`; REPL errors are read from `useq_last_diagnostics()` immediately after the eval call.

## 8. Cascade Noise and Display Cap

8.1 **Candidate-local report-and-continue.** Within one candidate graph the
compiler may collect multiple independent diagnostics using typed
placeholders. The candidate remains rejected and publishes nothing. At the
top-level submission boundary, evaluation stops at the first rejected form;
later forms are not executed.

8.2 **Typed placeholders.** When a non-fatal error is reported in a numeric context, the compiler substitutes `0.0`, not a sentinel `nil`. This prevents a single error from triggering a cascade of spurious type errors in downstream arithmetic.

8.3 **Placeholder-register suppression.** The compiler tracks which registers hold placeholder values. A diagnostic whose *only* failing input is a placeholder register is suppressed — it is a consequence of the primary error, not an independent problem.

8.4 **Speculative compilation discards diagnostics on rollback.** Where the compiler does speculative compilation (e.g. trying to specialise a callable, attempting numeric while-loop lowering), checkpoint state must include the diagnostic-vector size and rollback must truncate any speculative diagnostics. The user must not see error messages about bytecode that was discarded.

8.5 **Frontend display cap.** Even when the compiler produces many diagnostics, the editor shows at most **5 errors per eval** by default; additional diagnostics are reachable via "show all". Warnings render separately and do not count against this cap.

## 9. Chain of Blame

9.1 When a cell mutation triggers recompilation of a dependent output, named
state, or synth control and that recompilation fails, the error is *caused by*
the cell change but *manifests in* the persistent consumer expression. The user
needs to see both halves to know what to fix.

9.2 The diagnostic carries a `triggered_by` field ([diagnostics.md §2](diagnostics.md)) holding the symbol name of the mutated cell that triggered the recompile. The frontend renders it as context — *"this broke because `x` changed"* — alongside the primary message.

9.3 **Span attribution.** The diagnostic's source span points at the *reference
site* where the mutated symbol is used inside the consumer expression, not at
the redefinition site. The user needs to see which part of the persistent
program depends on the symbol whose meaning changed; the redefinition is in
another pane and is by definition the recent action.

9.4 A rejected background output, named-state, or synth-control recompile
publishes one active compile diagnostic indexed by the consumer, including
`triggered_by`. The old root, dependencies, state and resources remain live. A
subsequent successful recompile of that consumer clears its diagnostic;
unrelated evals do not. A later rejection replaces the consumer's complete
record rather than accumulating multiple causes.

9.5 Active diagnostics serialize in stable ownership order: output index,
dense named-state slot, then synth artifact control order. Direct replacement,
parameter removal, and full clear remove the corresponding synth-control slots
atomically with the public artifact.

## 10. Batch-Eval Isolation

10.1 When the runtime evaluates multiple outputs in a single batch (`useq_eval_outputs_time_window()` and equivalents), a runtime error on one output **must not** abort the others. Each output is independent: errors are recorded per-output, healthy outputs continue producing samples to the end of the batch. This is the per-output expression of the language-wide "never stop the music" guarantee. (See `wasm/wasm_wrapper.cpp` — useq_eval_outputs_time_window_into, per-output loop with errors isolated; `uSEQ/src/signal_engine/executor.cpp` — execute_all_outputs iterates outputs independently.)

10.2 When an output errors mid-batch the runtime:

1. Records the runtime diagnostic against that output.
2. Substitutes its held sample (or neutral default per §2.4) for each failing
   sample; later samples are still evaluated and may recover.
3. Continues evaluating the next output.

10.3 The batch return value indicates overall success only as a coarse summary. Individual output errors are retrieved via `useq_active_diagnostics()` / `useq_output_diagnostics()`.
