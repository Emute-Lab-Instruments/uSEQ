# ModuLisp Semantics

> Engine-agnostic semantic spec. Describes the language as the user experiences
> it and as any conforming implementation must behave. The current bytecode VM
> (`BYTECODE_VM_SPEC.md`) and the proposed signal-engine redesign
> (`SIGNAL_ENGINE_SPEC.md`) are both treated as implementation candidates that
> must satisfy this spec. Implementation differences are footnotes, not
> semantics.
>
> Read top-down. Each numbered point is self-contained. Subsections go from
> mental-model to implementer-level invariants; you can stop at whichever
> depth you need. The catalogue of built-in functions lives in `useq.md`.

---

## 1. Frame

1.1 ModuLisp is the live-coding language for the uSEQ eurorack module. It is a Lisp dialect that runs both on firmware (RP2040/RP2350) and in the browser via WASM, with identical observable semantics.

1.2 The product use case is a performer typing expressions and pressing an eval key while the module produces voltages. The language is shaped by that constraint: outputs must keep producing values across edits, errors, and recompilation.

1.3 ModuLisp is **not** a general-purpose programming language. It is a substrate for describing time-varying signals that drive control voltages, gates, and serial streams (or other arbitrary numerical streams in software).

1.4 The user-facing manual (`useq.md`) is descriptive — it lists what each builtin does. This document is normative — it defines what programs *mean* and what runtimes must guarantee.

---

## 2. Two Dialects

2.1 ModuLisp has **two mutually-exclusive evaluation dialects**: **Reactive** and **Imperative**. A session runs in exactly one. The dialect is a runtime mode, not per-form.

2.2 **Reactive** is the default and the dialect this spec mostly describes. It uses FRP semantics: every output expression is implicitly a function of time, cells are reactive bindings, dependents auto-recompile when bindings change, the engine re-runs each output graph as fast as it can.

2.3 **Imperative** is a traditional Lisp REPL. `define` mutates eagerly and does not invalidate anything. There is no implicit time, no auto-re-evaluation, no FRP. If the user wants periodic execution, they set it up themselves (e.g. via `schedule` or external tooling). Outputs hold whatever value was last written until the user writes again.

2.4 The two dialects share the same parser, the same value tower (§3), the same standard library where it is meaningful, and the same compile/eval pipeline. They differ in **what the engine does between user evals** — re-runs everything (Reactive) vs. nothing (Imperative).

2.5 Switching dialects mid-session is allowed but resets all output bindings and dependency state. There is no "mixed" mode (although the imperative mode can be set up to immitate some of the semantics and functionality of the reactive mode).

2.6 Unless qualified, the rest of this document describes **Reactive** semantics. Imperative-mode behaviour is addressed in §15.

---

## 3. Values and Types

3.1 ModuLisp has a **Clojure-shaped value tower**: numbers, strings, symbols, keywords, lists, vectors, maps (where supported), nil, and callables (lambdas / closures).

3.2 At the parser level all of these are first-class. `define` can bind any of them. `defn` produces a callable. Vectors `[…]` are data, lists `(…)` are code; this is the convention, not a hard rule.

3.3 **Numbers are doubles.** There is no separate integer type. `1` and `1.0` are the same value. Integer-valued doubles are produced by `floor`, `ceil`, indexing, etc. The compiler may **internally** specialise integer-valued node chains (counters, vector indices, `floor`/`ceil`/comparison/`mod` chains where it can prove integer-ness) to integer arithmetic on targets without an FPU; this is invisible to the user. Whether to expose user-visible integer literals or an `(int x)` coercion is open (§17).

3.4 **Truthiness in numeric context**: non-zero is true, `0.0` is false. Comparison and logic operators return `1.0` or `0.0`. To threshold a phasor to a gate, use the explicit operator (`sqr`, `pulse`, `(> phasor 0.5)`); truthiness is not redefined to "≥ 0.5", because that would silently invert the meaning of `(if x …)` for bipolar signals near zero (`sin`, audio).

3.5 **Truthiness in non-numeric context**: `nil` and `0`/`0.0` are false; everything else is true. This is consistent with Clojure modulo the absence of a dedicated boolean type.

3.6 **Signal context is type-restricted by reachability, not by syntax.** Most signals reduce to a numeric value at the root; that is the common case and the one the engine optimises. Signals of non-numeric types (e.g. a string-valued signal driving a visual live-coding panel) are permitted in principle and a conforming engine may support them; engines that only target audio/CV outputs may reject non-numeric roots at compile time.

3.7 **Vectors of numbers are first-class signal data.** They lower to fixed-size data tables and are consumed by `step`, `gates`, `seq`, `interp`, `for`, etc. A vector containing time-varying expressions (e.g. `[1 (sin beat) 3]`) is also valid; each slot is its own signal.

3.8 **Callables in signal context are pure.** They may close over compile-time-resolvable values but not over mutable state. They are inlined at every call site, unless it's beneficial to performance otherwise.

3.9 **`nil` is a value, not an error.** `(define x nil)` is fine. Reading an unbound symbol is a compile-time error, not a `nil`-returning operation.

---

## 4. The Signal Model (Implicit Lifting)

4.1 The fundamental semantic claim of Reactive ModuLisp is **implicit lifting**: every expression appearing in signal position is automatically interpreted as a (pure) function of a numerical argument representing time, plus optionally some external inputs inserted into the graph by e.g. reading hardware/MIDI/OSC/etc inputs.

4.2 Concretely, in signal position:
- a literal `1` denotes the constant signal `λt. 1`;
- the symbol `t` denotes the identity signal `λt. t`;
- `(+ 1 t)` denotes `λt. 1 + t`;
- `(sin (* t 440))` denotes `λt. sin(t · 440)`;
- vectors like `[1 2 3]` denote constant data signals (time-invariant tables).
- vectors like `[1 t 3]` denote mixed-data tables, with the time-varying ones resolving to a "pointer" to the corresponding node.

4.3 The user never writes the `λt.` wrapper. There is no surface-level `fn [t] ...` around output expressions. The lifting is always implicit.

4.4 **Operators combine signals pointwise.** `(+ a b)` where `a` and `b` are signals produces a signal whose value at time `t` is `a(t) + b(t)`. This generalises to all pure operators.

4.5 **The only true input is `t`** — raw time in seconds since transport start. All other temporal variables (`beat`, `bar`, `phrase`, `section`, `beat-num`, `bar-num`) are derived from `t` and timing cells (`bpm`, `beats-per-bar`, etc.).

4.6 A signal's *value at a moment* is computed by sampling. Sampling boundaries are an engine concern (§13); the language model is "the signal exists for every `t`".

4.7 Signals are **pure**: they have no hidden state, no allocation, no side effects, no I/O. A signal that "looks stateful" (e.g. a step sequencer counting through positions) is in fact a closed-form function of phasor position.

4.8 Pseudo-randomness, where present, must be deterministically seeded (typically from `t` or a hash) so that two evaluations at the same `t` return the same value. Different interface functions might choose to expose this seeding, such that the user might choose to sequence seeds and potentially return to pseudo-random streams that appeared in earlier moments.

---

## 5. Time and Phasors

5.1 `t` is real-valued time in seconds. Monotonic while playing, frozen while paused, reset to zero on stop or rewind.
  5.1.1 `t0` is the same as `t` but remains unaffected by any and all time modifications like `fast`, `slow`, `offset` etc; it simply monotonically increases or resets to zero on stop or rewind.
  5.1.2 `ground-time` is like `t0` but does not reset to zero; it simply keeps track of the number of seconds since the session was booted.

5.2 **Phasors** are signals that ramp `0 → 1` and (typically) wrap. The standard phasors are derived from `t` and timing cells:
- `beat = fmod(t · (bpm / 60), 1)`
- `bar = fmod(t · (bpm / 60 / beats-per-bar), 1)`
- `phrase = fmod(t · (bpm / 60 / beats-per-bar / bars-per-phrase), 1)`
- `section = fmod(t · (bpm / 60 / beats-per-bar / bars-per-phrase / phrases-per-section), 1)`

5.3 **Beat counters** are integer-valued signals: `beat-num = floor(t · bpm / 60)`, `bar-num = floor(t · bpm / 60 / beats-per-bar)`. They never wrap.

5.4 **Durations** are scalar cells: `beat-dur = 60 / bpm`, `bar-dur = beat-dur · beats-per-bar`. Use them to express musical offsets in seconds.

5.5 The timing cells (`bpm`, `beats-per-bar`, etc.) are ordinary cells (§7). Redefining `bpm` reactively updates every signal that derives from it.

5.6 Phasors are bipolar-domain agnostic. Whether a phasor maps to a unipolar `[0,1]` or bipolar `[-1,1]` output is decided by the operator that consumes it (e.g. `usin` is unipolar, `sin` is mathematical, `sqr` thresholds).

5.7 **Mathematical primitives are bipolar by default.** `(sin x)` is the standard `sin(x)`; `(u-sin p)` is `sin(2π·p)/2 + 1/2` — the unipolar phasor-domain convenience operator. The user picks which they want; the engine does not silently rescale.

---

## 6. Time Warps

6.1 ModuLisp signals form a **profunctor over time**. Two mapping operations exist:
  6.1.1 **postmap** — output mapping. `(postmap g signal)` ≡ `(g signal)`. This is just ordinary function application; no special form is needed and the compiler treats it as such.
  6.1.2 **premap** — input mapping. `(premap f signal)` denotes the signal `λt. signal(f(t))`. This is the primitive time-warp operation; it requires compiler support because it changes the time context for the inner subgraph.

6.2 **The canonical warps are sugar over `premap`.** They are not magical primitives:
- `(fast k expr)` ≡ `(premap (fn [t] (* k t)) expr)`
- `(slow k expr)` ≡ `(premap (fn [t] (/ t k)) expr)` ≡ `(fast (/ 1 k) expr)`
- `(offset k expr)` ≡ `(premap (fn [t] (+ t k)) expr)`. `k` is in **seconds**.
- `(shift k expr)` — alias of `offset`.

6.3 Use `beat-dur` and `bar-dur` to express warps in musical units: `(offset (* 0.5 beat-dur) expr)` shifts by half a beat regardless of BPM.

6.4 **Affine warps compose and flatten at compile time.** `(fast a (fast b expr))` ≡ `(fast (* a b) expr)`. `(slow k (fast k expr))` ≡ `expr`. When `f` in `(premap f expr)` is affine (`λt. αt + β`), the compiler folds nested affine `premap`s into a single `(scale, offset)` baked into temporal-leaf loads — the warp forms vanish from the runtime graph. Non-affine `premap` is permitted but does not get this optimisation.

6.5 **The warp factor may itself be a signal.** `(fast (sin section) expr)` is well-formed: at each sample, the inner subgraph reads time as `t · sin(section(t))`. This is **instantaneous warp** — the warp function is sampled point-by-point and the inner expression is recomputed from scratch under the new time map. There is no time-integration, no continuity guarantee across samples, and no notion of "the LFO that was running" persisting under a changing rate. (See §6.7 for why this matters and what to do about it.)

6.6 **Integrated warps must be built explicitly.** Because signals are pure and stateless, the language does not give you `t' = ∫ k(τ) dτ` for free. To integrate a rate signal across time you have to reserve an output channel as an accumulator and use `prev` (§10) to read its previous-sample value:

```lisp
;; Goal: musical time t' that runs at rate k(t) = 1 + 0.5·sin(0.1·t).
;; a7 snapshots t for the dt computation; a8 accumulates the warped time.
(a7 t)
(a8 (+ (prev a8)
       (* (+ 1 (* 0.5 (sin (* 0.1 t))))   ; rate k(t)
          (- t (prev a7)))))               ; dt
;; a8 is now usable as the warped time leaf:
(a1 (sin (* (prev a8) 440)))
```

The cost is two output channels per integrator and the indirection through `prev`. A future built-in `(integrate factor)` primitive that allocates the scratch state under the hood is a candidate (§17); semantically it would be exactly this construction.

6.7 **Warps do not preserve oscillator phase.** This is a load-bearing consequence of stateless evaluation, deserving its own section (§19). Read it before designing modulation patches.

6.8 V1 has no non-affine time-warp primitives in the core language *as sugar* — but `premap` accepts arbitrary pure functions, so non-affine warps are constructible directly. The compiler simply doesn't get the affine-flattening optimisation for them. Wrap-around / non-monotonic / discontinuous warps are admissible as user code but the engine makes no continuity claims about the resulting signal.

---

## 7. Cells and Reactivity

7.1 A **cell** is a named binding from a symbol to a value. `(define name expr)` creates or replaces a cell. `(defn name [params...] body)` creates a cell whose value is a callable.

7.2 **Cells are reactive bindings** in Reactive mode. When the user redefines a cell, every output whose compiled graph references that cell is invalidated and recompiled with the new value.

7.3 The user model is "I changed `freq`, all signals using it follow." How that is achieved (constant-folding the new value into recompiled graphs vs. live-loading the cell value at runtime) is an implementation detail; the observable behaviour is identical for pure-numeric cells.

7.4 **Recompilation is sub-tick.** A typical signal recompiles in well under one millisecond. The user perceives redefinition as instantaneous.

7.5 **Dependency t
racking is by symbol identity, not by source text.** Renaming a cell does not preserve dependencies; it is logically a new cell.

7.6 **Diamond / cascading dependencies** propagate transitively. `(define a 1) (define b (+ a 1)) (define c (+ b 1)) (a1 (* c beat))` — redefining `a` invalidates the entire chain to `a1`.

7.7 Redefining a cell that no signal depends on is a no-op for the signal engine.

7.8 **Cells are not stateful in the imperative sense in Reactive mode.** A cell's "value" at any moment is whatever its current definition evaluates to in the current environment. There is no "previous value" of a cell.

7.9 A cell's body in Reactive mode is itself a (possibly trivial) signal expression. `(define sweep (+ 200 (* 200 t)))` creates a time-varying cell. References to `sweep` from output expressions inline its body.

7.10 Cell redefinitions can come from any source — user REPL evals, `schedule`d code (when implemented; §17), or programmatic eval. All sources go through the same dependency-invalidation path.

---

## 8. Functions

8.1 `(defn name [p1 p2 ...] body)` defines a callable cell. `(fn [p1 p2 ...] body)` and `(lambda [p1 p2 ...] body)` produce anonymous callables.

8.2 **In signal context, callables are inlined at every call site.** No closure object is allocated, no environment is captured at runtime. The function's body is re-compiled with parameter symbols bound to the argument node graphs.

8.3 **Recursion in signal context is a compile-time error.** A signal must compile to a finite, acyclic graph. Recursive functions are top-level only.

8.4 **Higher-order functions returning callables** (e.g. `((fn [x] (fn [y] (* x y))) 3)`) are valid where the outer call's result is fully resolvable at compile time. Returning a callable from a runtime branch in signal context is rejected.

8.5 **Callables stored in cells** are referenced by name at the call site. The graph builder resolves the cell, inlines the body, records a dependency. Redefining the function invalidates every signal that calls it.

8.6 At top level (and in imperative mode), callables are first-class values: stored, passed, returned, called dynamically.

8.7 Variadic arithmetic operators (`+`, `*`, `-`, `/`, `min`, `max`) are left-folded into pairwise nodes during compilation. `(+)` is `0`, `(*)` is `1`, `(- x)` is `(- 0 x)`, `(/ x)` is `(/ 1 x)`.

---

## 9. Outputs

9.1 An **output** is a named sink that consumes a signal and produces hardware effect. The standard outputs are:
- `a1`..`a8` — continuous (analog voltage / PWM), nominally `[0, 1]` mapped to the module's voltage range;
- `d1`..`d8` — binary (gate / digital), thresholded at `0.5`;
- `s1`..`s8` — serial streams (10-byte framed messages over USB).

9.2 Output assignment is a top-level form: `(a1 expr)`. The expression is compiled and stored as the output's signal program. The output is sampled every tick.

9.3 Assigning a numeric literal to an output is the constant signal: `(a1 0.5)` holds 0.5 forever. `(a1 0)` clears the output.

9.4 Each output slot owns:
- An **active program** (current compiled signal).
- A **last-known-good (LKG) program** (most recent program that has produced ≥1 healthy sample batch).
- A **last sample value** (held during transitions).

9.5 Reassigning an output replaces the active program. The previous active program, if it had ever run a healthy batch, becomes LKG.

9.6 `q0` is a **scheduling callback**, not an output. `(q0 expr)` runs `expr` once per quantisation period (default: bar boundary). Use it for top-level effects synchronised to the bar.

9.7 Outputs not assigned by the user produce a **neutral default**: `0` for both continuous and digital. (Earlier docs specified `0.5` for analog; the current contract is `0` everywhere — verify against the firmware before relying on edge cases.)

9.8 Output programs that compile but error at runtime fall back to LKG (§14).

---

## 10. Cross-Output Reads (`prev`)

10.1 Output values produced in the current sample are not readable from other outputs in the same sample. To reference another output, you read its **previous sample** value.

10.2 `(prev a1)` reads the value `a1` produced one sample ago in the current sampling pass.

10.3 Bare output names in expression position are sugar for `prev`: `(a2 (* a1 0.5))` ≡ `(a2 (* (prev a1) 0.5))`.

10.4 **"Previous sample within batch"**: when the engine is rendering a window of samples (typical in WASM batch mode), `prev` reads the immediately preceding sample within that window. On firmware single-sample execution, this collapses to "previous tick".

10.5 The first sample of any batch (or first tick after a reset) reads the neutral default for any output that has never produced a value.

10.6 `prev` enables feedback loops between outputs: `(a2 (+ (prev a2) 0.01))` integrates by `0.01` per sample.

10.7 **`prev` does not introduce hidden state into the signal graph.** The "state" lives in the engine's per-output sample buffer; the graph itself remains a pure function of `(t, cells, inputs, prev_outputs)`.

10.8 `prev` is rejected in any context where the referenced output has not been declared. Self-reference (`(a1 (prev a1))`) is allowed.

---

## 11. Hardware Inputs

11.1 The signal graph has external leaves besides `t`: hardware input channels.

11.2 `in1`, `in2` — digital gate inputs, `0` or `1`.

11.3 `ain1`, `ain2` — analog CV inputs, normalised to `[0, 1]`.

11.4 `(swm n)` / `(swt n)` — momentary / toggle switch values (variant-dependent).

11.5 `(swr)` — encoder switch (variant-dependent).

11.6 `(rot)` — encoder position (variant-dependent).

11.7 Hardware inputs are sampled once per tick before the signal graph runs. Their values are constant within a single sample but can change every tick.

11.8 In WASM mode, hardware inputs default to neutral values (typically `0`) unless the host injects them via the eval-with-inputs ABI.

11.9 An input symbol that is unsupported on the current variant is a compile-time error, not a silent zero.

---

## 12. Top-Level Forms (Imperative Surface)

12.1 The "top level" is the surface the user types at and the editor evaluates. Each top-level eval is a single form (or a `do` of multiple forms) that runs immediately.

12.2 **There is no `@` prefix.** All eval is immediate by default. Earlier docs in `useq.md` describe an `@` operator that no longer exists; treat that section as historical.

12.3 **Quantised eval is an editor concern, not a language operator.** A keybinding variation lets the user submit code to be evaluated at the next musical boundary (default: next bar). The submitted code itself is ordinary top-level code; the editor holds it until the boundary.

12.4 Top-level forms that have meaning beyond signal compilation:
- `(define name expr)` — bind a cell.
- `(defn name [params...] body)` — bind a callable cell.
- `(let [bindings...] body)` — local scope. At top level, also a value-producing expression.
- `(do form1 form2 ...)` — sequence top-level forms; result is the last form's value. Each child is itself a top-level form.
- `(if cond then else)` — conditional. The unselected branch is not evaluated.
- Output assignment forms `a1`..`s8`, `q0`.
- Transport: `(useq-play)`, `(useq-pause)`, `(useq-stop)`, `(useq-rewind)`, `(useq-clear)`, `(useq-get-transport-state)`.
- Tempo / metre: `(setbpm bpm)`, `(getbpm n)`, `(settimesig num denom)`.
- `(schedule name body period)` / `(unschedule name)` — see §17 (open question).
- `(eval string)` — dynamic evaluation. Top-level only; rejected in signal context.
- `(print value)`, `(perf)`, `(timeit expr)` — diagnostic side effects.

12.5 Top-level forms are **eagerly evaluated for their side effects**. The result of a top-level form may also be returned to the editor for display.

12.6 The compiler may **compile top-level forms to a node graph for performance** even when they only run once. This does not change their semantics; it just means "compiles to a node graph" is not synonymous with "is a signal" (§13.5).

---

## 13. Compilation and Reactivity

13.1 Every signal expression is compiled to a **node graph** — a finite, acyclic, topologically-sortable structure of pure-arithmetic nodes. The exact graph format is engine-specific (register bytecode in the current VM; flat node array in the redesign); the abstract model is the same.

13.2 Compilation is **always at least these passes**: parse → name resolution (cells, locals, hardware inputs, builtins) → constant folding → time-context propagation → builtin lowering → dead-code elimination → CSE / hash-consing → register/node allocation.

13.3 **Pervasive constant folding.** Any pure operation on constant inputs is evaluated at compile time. This composes transitively: deeply nested pure subexpressions collapse to a single `Const` node.

13.4 **Time-warp flattening.** Affine `fast`/`slow`/`offset`/`shift` chains compose into a single `(scale, offset)` pair that is baked into the temporal-leaf load. The warp forms vanish from the runtime graph.

13.5 **A node graph is not always a signal.** Most node graphs are persistent and re-run per sample (the signal case). Some are one-off — the engine compiles a top-level form to a graph, executes it once, discards it. The compile-time rejection rules in §15 apply to any code being compiled to a node graph, regardless of whether the graph will be re-run or executed once.

13.6 **Dependency set.** Every compiled graph carries the set of cell symbols it inlined or loaded. The runtime indexes outputs by their dependency sets so cell mutations can target precisely the affected graphs.

13.7 **Invalidation is proactive, recompilation is lazy.** Cell mutation marks affected graphs dirty immediately. Dirty graphs are recompiled at the next sampling boundary, never on the per-sample hot path.

13.8 **Loops.** `for` is unrolled at compile time when the collection is compile-time-resolvable (literal vector of constants, cell-bound numeric vector, `(range a b)` with constant args, etc.). Maximum unroll: 64 iterations. Larger or dynamic collections are an error in signal context. `while` with non-trivial dynamic exit conditions is currently deferred; treat it as effectively top-level only for now.

13.9 **Higher-order operations** (`map`, `reduce`, `filter`) on constant collections are unrolled the same way `for` is. On dynamic collections in signal context they are errors until generalised loop support arrives.

13.10 **Numerical health.** Per-sample evaluation produces an `IEEE 754 double`. NaN/Inf produced by an individual node is the engine's signal that the program is unhealthy this sample. Whole-output LKG fallback is the canonical recovery (§14).

13.11 **Sub-tick guarantees.** The hot path (sampling) never allocates, never does string-keyed lookup, never compiles. All compilation work happens between ticks.

---

## 14. Failure Model

14.1 ModuLisp distinguishes **compile-time errors** (program never produces a value) from **runtime errors** (program ran but a sample was unhealthy).

14.2 **Compile-time errors** include: parse errors, unresolved symbols (with fuzzy-match suggestions), arity mismatches, side-effect forms inside signal context, recursion in signal context, oversized `for` collections, type errors that the compiler can prove (e.g. arithmetic on a string with no type-promoting operator), unsupported builtins for the current target, and structurally invalid forms.

14.3 Compile-time errors are **structured diagnostics**: they carry severity, category, source span, human-readable message, and a suggestion (often a working example). They surface in the editor as inline annotations and in the firmware as serial-protocol messages.

14.4 **A compile-time error does not stop the music.** The previously active program for that output (if any) keeps running. Other outputs are unaffected.

14.5 **Runtime errors** are: division by zero, out-of-bounds vector access on an unbounded path, integer-modulo by zero, NaN/Inf propagation that survives to the output root.

14.6 **Whole-output LKG fallback.** When an active output program errors at runtime — including non-finite values reaching the root — that output **switches to its LKG program** for the rest of the current sampling pass and remains on LKG until either (a) the user replaces the broken program with a working one, or (b) the user explicitly clears the output.

14.7 **What is "last-known-good"**: the most recent program for that output that has completed at least one full healthy sample batch. LKG is observed safety, not provable safety — a graph that succeeded once may fail later under different time / inputs.

14.8 **LKG bindings are frozen.** When a graph becomes LKG, its inlined cell values are baked at the moment of LKG promotion. Subsequent cell mutations do **not** rebind LKG. This keeps fallback deterministic and avoids a recompile inside an error path.

14.9 **Bootstrap.** If an output has never had a healthy program, runtime error falls back to the last valid sample if one exists; otherwise to the neutral default (`0`).

14.10 **Cascading failures don't promote unhealthy programs.** If A is healthy and becomes LKG, then B replaces A and immediately fails, the engine falls back to A — not to a "B → fall back" chain. Only programs that have completed a healthy batch are eligible to be LKG.

14.11 **Compile-time errors do not consume LKG.** A program that fails to compile is not promoted, demoted, or substituted; the active program is unchanged.

14.12 **Numerical errors are not silently zeroed at the output level.** Per-node NaN/Inf clamping (if any engine performs it) is an internal hygiene mechanism; the user-observable contract is "if the output goes unhealthy, you fall back to LKG, and you see a diagnostic." The engine must not silently produce subtly-wrong output instead of declaring failure.

14.13 **Diagnostics survive across evals.** Per-output health (`idle` / `running` / `fallback` / `error`) is queryable by the editor and shown to the user. A successful eval clears prior diagnostics for that output.

---

## 15. Compile-Time Rejection in Signal Context

15.1 An expression is in **signal context** iff it is being compiled to a node graph (§13.5). The same rejection rules apply whether the graph will be sampled forever or executed once for performance.

15.2 **Side-effect forms are rejected.** This includes (non-exhaustive): `define`, `defn`, output assignments (`a1`..`s8`, `q0`), `schedule`, `unschedule`, transport commands (`useq-play`, etc.), `eval`, `setbpm`, `settimesig`, `print`, `perf`, `timeit`.

15.3 **Side effects are rejected anywhere in the subtree**, not just at the top of the form. `(a1 (if (> beat 0.5) (define x 1) 0))` is an error.

15.4 **Reachable callables are checked transitively.** If a signal calls `(my-fn beat)` and `my-fn` contains a `define`, the compile error is raised against the signal call site.

15.5 **Recursion is rejected in signal context.** Direct (`f` calls `f`) and mutual (`f` → `g` → `f`) recursion both fail with a "calls itself / each other" diagnostic.

15.6 **Dynamic `eval` is rejected in signal context.** `(eval string)` is top-level only.

15.7 **Unresolvable symbols are rejected** with a fuzzy-match suggestion ("did you mean `beat`?") when one is plausible.

15.8 **Type errors the compiler can prove** are rejected (e.g. arithmetic on a string-cell where the operator has no string overload).

15.9 The diagnostic for a rejected form must use **plain language**, not jargon. "This form can only be used at the top level, not inside an output." "This function calls itself — recursive functions can't be used in outputs." Suggestions should include a working example.

15.10 The diagnostic framing is **"the compiler doesn't support this here"**, not "the language forbids this." Some restrictions are temporary (dynamic loops, non-numeric signal roots, etc.) and may relax in future versions.

---

## 16. Imperative-Mode Semantics

16.1 In Imperative mode, the FRP machinery is disabled. The language is a traditional Lisp REPL.

16.2 `(define x …)` mutates eagerly. Existing output programs are **not invalidated**. They retain whatever value of `x` they were compiled against (or, if cell-loads are dynamic, observe the new value at the next manual eval).

16.3 There is no automatic re-evaluation. Outputs hold whatever the user last wrote until the user writes again.

16.4 Time variables (`t`, `beat`, `bar`, …) are still available as queryable values, but no automatic time-stepping drives output recomputation. Periodic execution must be set up explicitly (e.g. via `schedule`, an external host loop, or a manual eval triggered on a timer).

16.5 Compile-time rejection rules (§15) still apply to any form being compiled to a node graph. Top-level mutation forms remain top-level only.

16.6 LKG fallback (§14) still applies to outputs whose programs are sampled by the engine, but it is much less load-bearing because outputs change only when the user explicitly writes them.

16.7 Imperative mode is intended for: language exploration, debugging stuck reactive graphs, experiments where the user wants explicit control of when things happen, and interop with external sequencing.

---

## 17. Open / Deferred

17.1 **`schedule` and `unschedule`.** The historical contract (`(schedule name body period)` runs `body` `period` times per bar) is documented in `useq.md`. Whether it survives in the same shape, becomes a thin wrapper over editor-side quantised eval, or is replaced by something else is not yet decided. Treat current behaviour as the historical contract; the spec needs updating once the design lands.

17.2 **Imperative-mode boundaries.** Whether the dialect is set per-session, per-tab, or via a `?mode=imperative` URL param is a UI/runtime decision pending product input. The semantics in §16 hold once a session is in imperative mode.

17.3 **Non-numeric signal roots.** Strings (and possibly other types) flowing through signal graphs to non-CV sinks (visual live-coding, MIDI text) is an intended capability but not currently implemented in either engine. The spec describes the intent; current engines may reject non-numeric roots.

17.4 **Audio-rate signals.** All current targets are control-rate (~1 kHz tick). Audio-rate (≥ 44.1 kHz) is a future direction; semantics are unchanged but per-sample budget tightens dramatically.

17.5 **General `while` and dynamic loops.** Currently rejected in signal context beyond the trivially-bounded cases. A future loop-completeness pass will define what cycles, exit conditions, and counter types are admissible.

17.6 **Closures escaping signal context.** Returning a callable from a runtime branch is rejected today. Whether some restricted form (e.g. closures over compile-time constants only, transformed into inlined alternates) is allowed is open.

17.7 **Cross-target floating-point determinism.** Soft-float ARM vs. x86/WASM hard-float can diverge on transcendentals. The contract is "within tolerance" rather than "bit-identical"; the tolerance ladder is in `BYTECODE_VM_SPEC.md` §7.5.

17.8 **`prev` window across batches.** The `prev` contract is "previous sample within the current batch / previous tick on firmware". The exact semantics at batch boundaries (does `prev` at the first sample of a new batch read the last sample of the previous batch, or the neutral default?) needs an explicit answer; current engines tend to carry forward, but this should be normalised.

17.9 **Map / dict values.** Whether ModuLisp has Clojure-style maps as first-class values, and what their signal-context semantics would be, is open.

17.10 **Source of truth for builtins.** `useq.md` is currently descriptive. A canonical, machine-readable catalogue (with signal/imperative-mode availability flags, arity, types, semantics) is needed but does not exist yet. The YAML golden test suite (`BYTECODE_VM_SPEC.md` §7) is a partial step toward this.

17.11 **User-visible integer types.** Currently all numbers are doubles (§3.3). The compiler may infer integer-ness internally for indices/counters. Whether to surface integer literals (`1i`?), an `(int x)` coercion, or stay doubles-only is open. Triggers for revisiting: concrete pattern bugs caused by FP rounding at vector indexing boundaries; sustained perf concerns on RP2040 soft-float (mostly absorbed by the move to RP2350 hard-float).

17.12 **Built-in `(integrate factor)` primitive.** §6.6 shows that integrating a rate signal across time is constructible today via two scratch outputs and `prev`. A first-class `(integrate factor)` would allocate the per-call scratch state under the hood, returning a node usable as a time-leaf. Required for ergonomic continuous-modulation patches (§19).

17.13 **State-preserving signal abstractions.** Integrated warps are one instance of a broader gap: any pattern that needs cross-sample state (oscillators with continuous phase under modulation, slew limiters, envelope followers, one-pole filters) is currently expressed via `prev`-on-output gymnastics. A clean abstraction here — phase-preserving oscillators (§19), state objects, or richer `prev` semantics for cells — needs design work.

---

## 18. Cross-References

18.1 `useq.md` — user-facing manual, builtin catalogue, language tutorial. Authoritative for *what* operators do; this spec is authoritative for *how* expressions are evaluated.

18.2 `BYTECODE_VM_SPEC.md` — current engine implementation. Authoritative for the active runtime's instruction set, register allocation, and dispatch.

18.3 `SIGNAL_ENGINE_SPEC.md` — proposed engine redesign. Authoritative for the intended replacement runtime's node-graph model.

18.4 `ERROR_HANDLING_SPEC.md` — diagnostic format, source spans, ABI surface for diagnostics.

18.5 `FIRMWARE_SPEC.md` — module composition, tick loop, hardware contracts, dual-core split.

18.6 `docs/RUNTIME_CONTRACT.md` (in `useq-perform`) — runtime/firmware/WASM contract from the editor's perspective.

18.7 If this spec disagrees with any of the above on a point of language semantics, **this spec wins** by intent. Implementations should be brought into line. If this spec disagrees with the actually-deployed firmware, that is a bug — file it.
