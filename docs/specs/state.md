# State

> Spec: how ModuLisp expresses cross-sample state without abandoning the
> stateless-by-default signal model. Covers the foundation form
> (`define-state` / `defstate`), the canonical primitive (`integrate`),
> the named UGen catalogue (`osc`/`phasor`/`slew`/...), the orthogonal
> time-substitution form (`time-as`), the integrated-rate local-clock form
> (`rate-as`), and how all of these interact with warps. Counterpart to
> [MAIN.md](MAIN.md). Closely related to
> [time-warps.md](time-warps.md) (which stays mathematically pure — phase
> coherence lives here, not there) and [cells.md](cells.md) (purely
> functional cells; state-bearing cells are a distinct construct
> introduced here). Stable identity for anonymous stateful expressions across
> source edits and alternate output variants lives in
> [state-identity.md](state-identity.md).
>
> Intent over implementation. Where a design decision had real
> alternatives, those alternatives are recorded as `*.alt` sub-points
> alongside the decision so future revisits know what was considered.
> Implementation suggestions are flagged inline with `> *Suggestion:*`.
> The *what* is normative; the *how* is advisory.

### Source Files

- `uSEQ/src/signal_engine/node_pool.h` — `NodeOp::LoadState` (read state slot), `NodeOp::LoadDt` (local dt leaf), `NodePool::state_values[]` (state slot storage), `state_update_roots[]` (per-slot update graph roots), `state_slot_count`, `make_state_load()`, `make_dt_load()`
- `uSEQ/src/signal_engine/graph_builder.{h,cpp}` — `compile_integrate()` (allocates state slot, builds update graph), state-slot allocation during compilation
- `uSEQ/src/signal_engine/executor.{h,cpp}` — `commit_state()` (post-tick state update from workspace), `LoadState`/`LoadDt` evaluation in the per-node dispatch
- `uSEQ/src/signal_engine/cold_eval.{h,cpp}` — `SignalEngine::state_sources[]` (`StateUpdateSource` for recompilation), state migration across recompilation in `on_cell_changed()`
- `uSEQ/src/signal_engine/types.h` — `MAX_STATE_SLOTS` (32)
- `wasm/wasm_wrapper.cpp` — state save/restore in `execute_batch_sequential()` (visualisation does not corrupt live state), projection fork state cloning in `reset_projection_fork()`/`project_from_fork()`
- `test/signal_engine/test_signal_engine_phase4.cpp` — state-bearing construct tests

---

## 1. Frame

1.1 ModuLisp's signal model is stateless by default: every signal is a pure function of `t`, cells, and external inputs ([signal-model.md](signal-model.md)). This is load-bearing — it makes recompilation safe, time-warping composable, batch evaluation correct, and live-coding edits reactive.

1.2 But musical signals genuinely need cross-sample state for a small, well-known set of operations: oscillator phase under modulation, integration of rate signals, slew limiting, envelope following, one-pole filtering, sample-and-hold. These cannot be expressed as closed-form functions of `t` and inputs alone — they're recursive in time.

1.3 This spec adds **declared, transparent state** to the signal model. The new contract:

> Signals are pure functions of `(t, cells, inputs, prev_outputs, declared_state)`. All state on the signal path lives in slots the compiler knows about and the user has named or implicitly declared. There is no hidden state and no escape hatch — every cross-sample dependency is visible at the call site.

1.4 The mechanism is layered. The user picks the layer that matches the abstraction they want:

```
┌─────────────────────────────────────────────────┐
│ Layer 3 (orthogonal): time-as / rate-as         │
│   time-as substitutes a time position; rate-as  │
│   runs a local clock at an integrated rate.     │
├─────────────────────────────────────────────────┤
│ Layer 2: Named UGen catalogue                   │
│   osc, phasor, tri-osc, saw, sqr-osc,           │
│   slew, one-pole, envelope-follower, latch, ... │
├─────────────────────────────────────────────────┤
│ Layer 1: Bare state primitives                  │
│   integrate, and a small extensible set of      │
│   canonical state-bearing operations.           │
├─────────────────────────────────────────────────┤
│ Layer 0: define-state / defstate                │
│   The foundation. Declares a named cell whose   │
│   value is computed recursively from its own    │
│   previous value.                               │
└─────────────────────────────────────────────────┘
```

1.5 The layers are not alternatives. They're entry points at different abstraction levels into the same machinery. A user who never touches `define-state` can still write `(osc freq)`. A user who wants something the catalogue doesn't provide can drop to `define-state` to invent it.

1.6 The mathematical-purity contract of `premap`, `time-as`, and `fast`/`slow`/`offset`/`shift` is **preserved** ([time-warps.md](time-warps.md)). Phase coherence is not a property of pure time substitution; it is a property of expressions the user has explicitly built using the constructs in this spec. Dynamic substitution may use dynamic `fast`/`slow`/`offset`; dynamic phase-coherent speed changes use `rate-as`, `integrate`, or UGens.

1.7 The two-dialect framing ([dialects.md](dialects.md)) still applies. State constructs in Reactive mode behave as described here. In Imperative mode their semantics relax — see §12.

1.8 **Decision: Layer 0 is `define-state`/`defstate`, not `prev`-on-cells.**

&nbsp;&nbsp;&nbsp;&nbsp;1.8.alt **Alternative considered:** generalising `prev` so a cell that self-references via `prev` becomes implicitly stateful. Smaller language-surface delta — no new keyword. Rejected because the syntactic distinction between stateful and stateless cells matters: forgetting `prev` silently produces a pure cell, remembering it silently produces a stateful one. An explicit `defstate` form announces stateful intent at the declaration site and lets the parser reject mistakes early.

---

## 2. `define-state` / `defstate` — the foundation

2.1 `define-state` declares a cell whose value at each tick is computed from its *previous* value (and any other signals). It is the only construct in ModuLisp that introduces user-named cross-sample state.

2.2 **Surface syntax** (the spec admits more than one form; the implementation may choose. Semantics are identical):

```lisp
;; Long form, separate initial and update:
(define-state phase
  :initial 0
  :update (+ phase (* freq dt)))

;; Concise form, init then update body:
(defstate phase 0
  (+ phase (* freq dt)))

;; The two are aliases. defstate is the everyday short form.
```

2.3 **Semantics.** `(defstate name init update)` introduces a cell `name` with these properties:
- At the first tick after the cell is declared, `name`'s value is `init` evaluated once in the current environment.
- At every subsequent tick, `name`'s new value is `update` evaluated as a signal expression, where references to `name` *inside `update`* read the **previous tick's** value.
- References to `name` from *other* expressions (other cells, output bodies) read the **current tick's** value (the just-computed update result).

2.4 The "previous tick / current tick" split is what makes recursive updates well-defined without requiring evaluation-order rules. The update body is a fixpoint over time, not over the per-tick evaluation order.

2.5 **`dt` is a leaf** inside a `defstate` body. It refers to the duration since the previous tick in the current local-clock context. Outside `rate-as`, `dt` is equivalent to `dt-wall`. Inside `rate-as`, `dt` is scaled by the local rate. Authors who explicitly want real elapsed time regardless of local clock write `dt-wall`.

2.6 The `update` body is a signal expression with the same compile-time rejection rules as any signal expression ([compilation.md](compilation.md)). Side effects, dynamic `eval`, recursion through other callables, and unbounded loops are rejected. The only thing the body can do that an output body cannot is reference its own `name`.

2.7 **The init value is evaluated once, in the current environment.** It can reference cells, constants, and any expression resolvable at the moment of declaration. It does not re-evaluate on subsequent ticks.

2.8 **Stateful cells are distinct from purely-functional cells.** A regular `define`d cell ([cells.md](cells.md)) has no notion of "previous value"; redefining it is a pure rebinding. A `defstate` cell carries a value across ticks and is a different kind of object. The two should not share a name.

2.9 **`defstate` is a top-level form.** It is rejected inside output bodies, callable bodies, `let`, or other signal contexts. State must be named at the document level so the user can reason about its identity and lifetime.

&nbsp;&nbsp;&nbsp;&nbsp;2.9.alt **Alternative considered:** allowing `defstate` (or a variant) inside `let` / `defn` bodies, with per-call-site identity. The use cases are real but small (locally-scoped slew limiters, etc.). Deferred until first user need; locally-scoped state would be syntactically distinct from top-level `defstate` to avoid identity confusion.

> *Implementation suggestion:* enforce the top-level-only rule at parse time, before signal-context analysis. The error message should point at the equivalent stateful primitive (`integrate`, `osc`, etc.) for the inline case the user probably wanted.

2.10 **A stateful cell is observable from any signal expression** as if it were an ordinary cell. Outputs read it, other state cells read it, callables that get inlined into signal contexts read it. The reader gets the current-tick value (post-update for the current tick).

2.11 Stateful cells participate in dependency tracking: when a cell the *update body* depends on is redefined, the state cell's compiled update is invalidated and recompiled (§4).

---

## 3. What `defstate` Lets the User Build

3.1 `defstate` is the foundation; `integrate` and the UGens are sugar over it. The user can express any of these directly:

```lisp
;; Integrator (Layer 1's `integrate` desugared):
(defstate phase 0
  (+ phase (* rate dt)))

;; Slew limiter — change toward target at most `slew-rate` per second:
(defstate slewed 0
  (let [step (* slew-rate dt)
        delta (- target slewed)]
    (+ slewed (clamp delta (- step) step))))

;; One-pole low-pass filter at `cutoff` Hz:
(defstate lowpass 0
  (let [alpha (min 1 (* 2 π cutoff dt))]
    (+ lowpass (* alpha (- input lowpass)))))

;; Envelope follower:
(defstate env 0
  (let [rate (if (> (abs input) env)
               (/ dt attack)
               (/ dt release))]
    (+ env (* rate (- (abs input) env)))))

;; Sample-and-hold:
(defstate held 0
  (if (> (rising trigger) 0) input held))

;; Counter that resets on a phasor wrap:
(defstate cycles 0
  (if (< bar (prev bar)) (+ cycles 1) cycles))
```

3.2 These examples are illustrative. The standard library should provide the common cases as named primitives (Layer 1 / Layer 2) so users don't reinvent them. `defstate` is for new state shapes that the catalogue doesn't yet cover.

3.3 The cost the user pays for this expressivity is responsibility: a `defstate` body is a recursive update equation. The user must reason about its stability, its rate-dependence, and its initial conditions. Bad equations produce bad music. The language can't catch every dynamical-systems error at compile time.

---

## 4. State Identity and Recompilation

4.1 The signal engine recompiles output graphs sub-tick whenever a dependency changes ([MAIN.md §3](MAIN.md)). For stateful cells to be useful, **state must survive recompilation**, otherwise every unrelated edit zeroes every integrator and the language is worse than no-state-at-all. (See `uSEQ/src/signal_engine/cold_eval.h` — StateUpdateSource stores source and deps for recompilation; `uSEQ/src/signal_engine/cold_eval.cpp` — on_cell_changed recompiles state update bodies, preserves state_values; `uSEQ/src/signal_engine/node_pool.h` — NodePool::state_values[] survives across recompilation.)

4.2 **State identity is the cell's symbol.** A `defstate` named `phase` is *the same state cell* across recompilations as long as the symbol `phase` continues to refer to a `defstate` declaration. Edits to the update body do not reset the cell.

&nbsp;&nbsp;&nbsp;&nbsp;4.2.alt **Alternative considered:** structural-hash identity (hash the update body's compiled graph; identical structure across recompiles preserves state). Rejected because the hash is brittle to formatting and to small body edits, both of which the user would expect to leave the running cell alone. Symbol-name identity is more robust to user-perceived "this is still the same cell." Anonymous state slots (UGens, §6) use structural hashing because they have no name to anchor on.

4.3 **State resets on identity break.** A cell's state resets to its `init` value when:
- The cell is undeclared and re-declared (e.g. `phase` removed, then `defstate phase` added back later).
- The cell's *kind* changes (a `defstate phase` replaced by a `(define phase …)`, or vice versa).
- The user explicitly resets it (mechanism TBD; see §13).

4.4 **Editing the update body does not reset state**, even if the update produces wildly different values from the previous version. The user is changing the law of motion; the system continues from wherever it was. This matches the modular-synth intuition (turning a knob on a running module changes its behaviour going forward; it doesn't snap the module's internal state to zero).

> *Implementation suggestion:* on each compilation, build a new state-slot table. Map old cells to new slots by symbol identity; copy old values into the new slots. Cells without a match in the old table get `init`. This is a small hash-table walk at recompile time; should fit comfortably inside the sub-millisecond compile budget.

4.5 **The init expression is re-evaluated only on identity reset**, not on every recompilation. A `defstate` whose `init` references a cell that changes does not re-init when that cell changes — only the update body recompiles.

4.6 **Stateful cells participate in dependency tracking** the same way pure cells do: their update body's dependencies are tracked, and changes to those dependencies invalidate and recompile the update. The state value carries through.

4.7 **Renaming a `defstate` is logically a delete-and-create.** The old name's state vanishes; the new name starts at `init`. There is no rename-preserves-identity contract. A future migration story for live-coded sessions could change this; for V1, keep it simple.

4.8 **Anonymous state identity is specified separately.** UGens and other
stateful primitives inside expressions are not identified by a top-level
symbol. Their stable identity, explicit `:id` / `with-state-id` surface,
resource-schema compatibility, duplicate-active validation, and cold-eval
behaviour are specified in [state-identity.md](state-identity.md). This section
only owns named `defstate` identity.

4.9 **Projection forks clone state, they do not redefine it.** The browser WASM
runtime may clone declared state into a temporary projection fork for editor
visualisation ([visualisation-projection.md](visualisation-projection.md)). That
fork follows the same state identity and `dt` rules as live execution, but its
updates are discarded or kept only in the fork. Projection must never reset or
mutate the live state slots described in this section.

---

## 5. `integrate` — the Canonical Primitive

5.1 `integrate` is the most common state-bearing primitive, and the spec promotes it to a first-class operator. **`(integrate x)` is the signal `∫₀ᵗ x(τ) dτ`** — the running total of `x` over time. (See `uSEQ/src/signal_engine/graph_builder.cpp` — compile_integrate allocates state slot, builds LoadState + LoadDt + Mul + Add update graph, sets state_update_roots.)

5.2 Semantically equivalent to:

```lisp
(defstate _anonymous 0 (+ _anonymous (* x dt)))
```

…with `_anonymous` being a per-call-site internal name the user does not see and cannot reference.

5.3 **Why `integrate` is a primitive, not just sugar:** the compiler can specialise it. `(integrate const)` folds to `(* const t)`. `(integrate (+ a b))` may decompose by linearity into `(+ (integrate a) (integrate b))` if that exposes more sharing. `(integrate (* k x))` for constant `k` becomes `(* k (integrate x))`. Sugar over `defstate` would not see these opportunities, because the compiler treats `defstate` bodies as opaque update equations.

> *Implementation suggestion:* implement `integrate` as a dedicated node op with its own state slot. CSE deduplicates referentially-equal `integrate` calls (same input subgraph → same accumulator).

5.4 **Each `integrate` call site has its own accumulator.** Two distinct calls — even with identical inputs — share state if and only if CSE proves them referentially equal:
- `(+ (integrate x) (integrate x))` — CSE collapses to one accumulator, output is `2 · ∫x`.
- `(+ (integrate x) (integrate y))` — two accumulators.
- `(let [a (integrate x)] (+ a a))` — one accumulator, output `2a`.

5.5 **`integrate` higher up the tree is well-defined but rarely musical.** `(integrate (sin t))` ramps upward at average rate `0` for a centred sine and at the DC offset for a unipolar one. `(integrate (integrate x))` is double integration. The spec doesn't restrict where `integrate` may appear; the user is responsible for whether it makes sense there.

5.6 **`integrate` is the natural building block for phase**, but it is not specifically tied to oscillators. It can be used for any cumulative quantity: distance traveled, heat accumulated, etc. The musical interpretation lives at the call site, not in `integrate` itself.

5.7 The same primitive-promotion logic applies to other Layer 1 operations as the catalogue grows: a primitive `slew`, a primitive `one-pole`, etc., each with compiler-known semantics that beat the equivalent `defstate`. The list of Layer 1 primitives is small and grows by explicit decision, not by user-facing addition.

---

## 6. Stateful Unit Generators (UGens)

6.1 The UGen catalogue is the user's everyday entry point. Each UGen is a named, self-contained state-bearing operation with a clear musical purpose. The user writes `(osc freq)`, not `(usin (* 2 π (integrate freq)))`, when they want a sine oscillator.

6.2 **Initial proposed catalogue.** The list is intentionally small. Additions require explicit decisions about scope, semantics, and naming.

| UGen | Purpose | Approximate semantics |
|---|---|---|
| `(phasor freq)` | Phase-coherent ramp `[0,1)` at frequency `freq` Hz | `(frac (integrate freq))` |
| `(osc freq)` | Phase-coherent unipolar sine at `freq` Hz | `(usin (phasor freq))` |
| `(tri-osc freq)` | Phase-coherent triangle wave | `(tri (phasor freq))` |
| `(saw freq)` | Phase-coherent sawtooth `[0,1)` | `(phasor freq)` |
| `(sqr-osc freq pulse-width)` | Phase-coherent square / pulse | `(if (< (phasor freq) pulse-width) 1 0)` |
| `(slew target rate)` | Slew-limited tracker of `target`, max change `rate` per second | recursive update on `target - prev` |
| `(one-pole input cutoff)` | First-order low-pass at `cutoff` Hz | recursive `α(input - prev)` |
| `(envelope-follower input attack release)` | Asymmetric leaky integrator on `\|input\|` | branching one-pole |
| `(latch input trigger)` | Sample `input` whenever `trigger` rises through zero | `(if (rising trigger) input prev)` |

6.3 The "approximate semantics" column is descriptive, not prescriptive. Each UGen has a precise spec entry (TBD per-UGen). The user-facing contract is the name and signature; the recursive form is an implementation hint.

6.4 **Frequency unit convention.** UGens that take a frequency parameter use **Hz** (cycles per second) as the canonical unit. To express musical-relative frequencies, divide by `bar-dur` or `beat-dur`:

```lisp
(osc 1)                ; 1 Hz
(osc (/ 1 bar-dur))    ; 1 cycle per bar
(osc (/ 4 bar-dur))    ; 4 cycles per bar
```

&nbsp;&nbsp;&nbsp;&nbsp;6.4.alt **Alternative considered:** a parallel `osc-bar`/`osc-beat` family that takes cycles-per-bar / cycles-per-beat directly. Ergonomic for live-coding patterns but doubles the UGen surface. Deferred pending real-use evidence; if the divide-by-`bar-dur` idiom proves persistently awkward, reopen.

6.5 **UGens are primitives, not user-space sugar.** Each is a distinct compiler-known operation with optimisation opportunities (waveform tables, SIMD vectorisation, hardware-specific oscillator units on RP2350). A user *could* write `(usin (* 2 π (integrate freq)))` and get the same logical result, but the compiler may specialise `(osc freq)` more aggressively.

6.6 **CSE applies to UGens identically to other state-bearing nodes by default.** Two `(osc freq)` calls with referentially-equal arguments share one oscillator (one phase accumulator, one sine evaluation). This is FRP-correct: same operation, same inputs, same identity, same accumulator.

6.7 **UGens compose with all other signal operators.** `(+ (osc 1) (osc 2))`, `(* (slew (ain1) 5) (osc 4))`, `(usin (* 2 π (phasor (* 2 (ain1)))))` are all well-formed.

6.8 **Catalogue growth policy.** New UGens enter the catalogue by explicit design decision. Criteria:
- The operation has a clear musical purpose with a recognised name in the modular-synth / DSL canon.
- The recursive form is non-trivial enough that users would otherwise reinvent it incorrectly.
- The compiler has specialisation opportunities or hardware support that justify primitive status.

Operations that fail these criteria are left for users to express via `defstate`.

6.9 **UGen keyword options.** UGens may accept trailing keyword arguments after
their positional arguments. Keyword arguments are ordinary keyword values at the
parser/type level ([values-types.md](values-types.md)), but primitive UGens
interpret a small standard set at compile time:

```lisp
(osc freq :phase 0.25)   ; initial phase / phase offset in cycles
(osc freq :id :left)     ; stable user-supplied state identity
(osc freq :fresh true)   ; force a distinct anonymous state slot
```

6.9.1 Keyword names are part of the UGen's signature. Unknown keywords or
duplicate keywords are compile-time errors with suggestions.

6.9.2 `:phase` sets the oscillator's initial phase or phase offset, depending
on the UGen's precise entry. It is the preferred way to make two otherwise
identical oscillators musically distinct:

```lisp
(+ (osc freq :phase 0.00)
   (osc freq :phase 0.25))
```

6.9.3 `:id` participates in state-slot identity. Two structurally identical
UGens with different `:id` values do **not** share state. The `:id` value must
be compile-time-resolvable, normally a keyword or symbol.

6.9.4 `:fresh true` is an explicit request for a new anonymous state slot at
that call site. It is useful for quick live-coding, but `:id` is preferred for
state that should survive document edits predictably.

---

## 7. `time-as` and `rate-as`

7.1 `(time-as time-signal body)` is pure time substitution. It evaluates
`body` with the time leaf `t` rebound to `time-signal` at the current sample.
Phasors derived from `t` (`beat`, `bar`, etc.) are re-derived from that local
time. External leaves (`ain1`, etc.) and cells are unaffected.

7.2 `time-as` does not introduce state by itself. It is a position/scrubbing
operator: if `time-signal` jumps, `body` jumps. Use it for loops, scrubbing,
reverse closed-form gestures, and explicit local time expressions.

```lisp
;; Read the body at twice the current time.
(time-as (* 2 t) body)

;; Read a gesture at a knob-selected position.
(time-as (ain1) gesture)

;; Hard-loop local time every bar.
(time-as (fmod t bar-dur) body)
```

7.3 `(rate-as rate-signal body)` is an integrated-rate local-clock form. It has
two semantic effects:

- The local time leaf `t` is rebound to a clock that accumulates
  `rate-signal` over wall time.
- The local state delta `dt` inside state-bearing constructs in `body` is
  scaled by `rate-signal`.

For closed-form bodies with no state-bearing constructs, this is equivalent to:

```lisp
(time-as (integrate rate-signal) body)
```

For stateful bodies, `rate-as` is stronger than plain `time-as`: the state
updates also follow the local clock. An implementation may lower it directly to
a specialised local clock state slot. The user model is "run this subexpression
at this speed." The local clock is continuous across rate changes because the
rate is accumulated over time.

```lisp
;; Phase-coherent speed changes.
(rate-as (step [1 2 4 8] bar)
  (usin t))

;; Equivalent explicit spelling.
(time-as (integrate (step [1 2 4 8] bar))
  (usin t))
```

7.4 `osc` and `phasor` are the everyday vocabulary for the common oscillator
case. Prefer `(osc freq)` over `(rate-as freq (usin t))` unless the explicit
local clock is part of the musical idea.

7.5 The point of the split is vocabulary:

- `time-as` means "choose a position in time"; it may jump.
- `rate-as` means "choose the speed of a local clock"; it accumulates.
- `osc`/`phasor` mean "use the standard phase-coherent oscillator machinery."

---

## 8. Local `dt`, State, and Time Contexts

8.1 The signal graph exposes two explicit delta-time leaves. (See `uSEQ/src/signal_engine/node_pool.h` — NodeOp::LoadDt; `uSEQ/src/signal_engine/executor.h` — ExecutionContext.dt, wall-clock delta passed per tick.)

- **`dt-wall`** is the actual wall-clock duration since the previous tick.
  It is independent of any local time context.
- **`dt`** inside a state-bearing update is the local-clock delta for the
  current context. Outside `rate-as`, `dt == dt-wall`. Inside `rate-as r`,
  `dt == r * dt-wall`.

8.2 `time-as` does not scale `dt` merely because the substituted time changes.
Substitution changes what `t` means; it does not by itself say that state inside
the body should evolve faster. If a user wants the body to run under a local
rate, they should use `rate-as`.

8.3 State-bearing constructs inside `rate-as` follow the local clock:

```lisp
(rate-as 2
  (osc freq))
;; Equivalent user intent: (osc (* 2 freq))

(rate-as (ain1)
  (slew target 4))
;; The slew's local `dt` follows the knob-controlled rate.
```

8.4 State-bearing constructs that must ignore local musical time should use
`dt-wall` explicitly in their update equation or expose a wall-clock variant in
the standard library:

```lisp
(defstate env 0
  (+ env (* (- target env) dt-wall)))
```

8.5 Applying `time-as` to a referenced `defstate` cell is not closed-form
time travel. A state cell is history, not a function that can be sampled at an
arbitrary past/future time. V1 should reject or warn on this pattern:

```lisp
(time-as (* 2 t) phase) ; suspicious if phase is a defstate cell
```

8.6 Applying `rate-as` to state-bearing expressions is coherent: it evaluates
their updates under the local `dt`. For V1, derivative state cells for arbitrary
named `defstate` references are deferred. The initial supported surface should
focus on UGens, `integrate`, and state introduced directly inside the
rate-context by compiler-known primitives.

8.7 Negative rates are semantically allowed only for pure substitution and
closed-form expressions unless the specific state-bearing primitive documents
reverse-time behaviour. For V1, stateful negative local `dt` may be rejected
with a diagnostic.

---

## 9. The User's Motivating Example, Written Five Ways

9.1 Goal: a sine whose frequency steps through `[1, 2, 3, 4]` cycles per bar, **phase-coherent** at the boundaries.

9.2 At each layer:

```lisp
;; Layer 2 (UGen): everyday call.
(a1 (osc (/ (from-list [1 2 3 4] (slow 4 bar)) bar-dur)))

;; Layer 3 (time-as) + Layer 1 (integrate): explicit phase plumbing.
(a1 (time-as (integrate (/ (from-list [1 2 3 4] (slow 4 bar)) bar-dur))
      (usin (* 2 π t))))

;; Layer 1 (integrate) directly:
(a1 (usin (* 2 π (integrate (/ (from-list [1 2 3 4] (slow 4 bar)) bar-dur)))))

;; Layer 0 (defstate): the foundation.
(defstate phase 0
  (+ phase (* (/ (from-list [1 2 3 4] (slow 4 bar)) bar-dur) dt)))
(a1 (usin (* 2 π phase)))

;; Compare: bare arithmetic, *not* phase-coherent (intentionally).
(a1 (usin (* 2 π bar (from-list [1 2 3 4] (slow 4 bar)))))
```

9.3 The first four produce identical output. The fifth produces something different and well-defined (frequency-stepped LFO with phase resets); the user wrote pointwise multiplication, the system delivered pointwise multiplication. No inference about intent.

---

## 10. Compile-Time Properties

10.1 **CSE applies uniformly.** Two referentially-equal state-bearing nodes (same operator, same input subgraph) hash-cons to one node and share state. This is FRP-correct: same input + same start time → same accumulator.

10.2 **Constant folding has limited reach across state boundaries.** `(integrate 0)` folds to `0`. `(integrate const)` folds to `(* const t)` (because the integral of a constant is `const · t` and the initial state is 0). `(osc 0)` folds to a constant `0.5` (or whatever the unipolar sine evaluates to at phase 0). Beyond these, state-bearing nodes are opaque to constant folding — their state evolves over time and isn't resolvable at compile time.

10.3 **Time-substitution flattening is unaffected** because `fast`/`slow`/`offset`/`shift` and `premap` remain mathematically pure ([time-warps.md](time-warps.md)). Constant affine substitutions may flatten completely; dynamic substitutions remain graph expressions. Integrated local-clock behaviour is not inferred from substitution forms; it is explicit in `rate-as`, `integrate`, and UGens.

10.4 **Linearity decomposition for `integrate`.** When the compiler can prove pure-arithmetic structure: `(integrate (+ a b))` may be rewritten as `(+ (integrate a) (integrate b))` if it exposes more sharing or simpler antiderivatives downstream. `(integrate (* k x))` for constant `k` becomes `(* k (integrate x))`. This is a permission, not a requirement; an engine may skip these transformations if its CSE already handles the case adequately.

10.5 **No "symbolic integration of substitution factors" pass is required.** Earlier discussions explored this as a way to give phase-coherent behaviour to closed-form `(sin (* k t))` patterns under dynamic `k`. Under this design, the user reaches for `rate-as`, `integrate`, or a UGen instead. The compiler need not guess that pointwise time multiplication was intended as rate accumulation.

&nbsp;&nbsp;&nbsp;&nbsp;10.5.alt **Alternative considered: implement symbolic-integration eligibility class for substitution factors anyway.** Would let the compiler produce phase-coherent behaviour for patterns where `factor` is in a recognised closed-form-integrable class. Rejected because (a) it requires non-trivial compiler machinery; (b) the user can always reach for `rate-as` or a UGen instead, which is simpler and more uniform; (c) the class boundary is opaque to the user — they'd have to learn which factor expressions are eligible.

10.6 **Worker-transferable.** State slots are arrays of doubles, just like the existing `prev_output_values` buffer. They contain no pointers, no closures, no environments. A compiled graph plus its state vectors can be shipped to a Web Worker without serialisation overhead.

---

## 11. Failure Model and LKG

11.1 There is one live state-slot vector, with one update-writer compiler
context per resource. Outputs do not retain parallel fallback programs or
fallback state vectors; [failure-model.md](failure-model.md) defines a scalar
last-good-sample hold.

11.2 **On a runtime failure at an output root,** state resources owned by that
output do not commit their candidate next values. They resume from the last
finite, audible state on the next healthy sample. Resources owned by another
output or by an explicit named/shared state source are unaffected.

11.3 **On successful recompilation,** matching resource keys owned by the same
compiler context preserve their state. Removed keys are retired and their
slots become reusable. A rejected candidate restores values, update roots,
owners, registry entries, and capacity exactly.

11.4 **A NaN or non-finite value in a state cell is treated as a runtime error** at the moment it would propagate to an output root. The output falls back to LKG. The state cell may continue to hold the bad value internally; the next healthy compilation supersedes it.

> *Implementation suggestion:* the runtime may opt to clamp `defstate` updates that produce non-finite values to the previous tick's value, preventing a single bad sample from poisoning subsequent ticks. This is a hygiene measure, not a substitute for LKG. Document precisely which it does.

11.5 **Stateful cells survive the failure of any individual output.** If `(a1 ...)` fails and falls back to LKG, but `(a2 ...)` references `phase` (a `defstate` cell), `phase`'s state continues to advance based on its own update body. State-cell state is not coupled to the health of any one output.

11.6 **Stateful cells whose update body errors at runtime** stop updating: the state holds at its previous value until the user fixes the update. The rest of the signal graph continues to read the held value. A diagnostic surfaces the broken update.

11.7 **Local-clock failures are isolated.** If a `rate-as` local clock produces a non-finite or otherwise invalid local `dt`, only state-bearing nodes in that local-clock context stop updating or fall back according to their output's LKG rules. The source cells and other local-clock contexts continue.

---

## 12. Imperative Mode

12.1 In Imperative mode ([dialects.md §2](dialects.md)), the FRP machinery is disabled. State-bearing constructs adapt:

- `defstate` is permitted; the cell still has an `init` and an `update`. The `update` runs once per *manual eval* of the form (or via `schedule`, when implemented), not once per tick. The user is in control of when state advances.
- `integrate`, `osc`, and other UGens behave as values evaluated at the moment of eval. They effectively freeze; without a tick loop, there is no `dt` to drive them. They produce their `init` value (or a sensible default) until the user invokes them in an explicit time-stepping context.
- `time-as` is a pure substitution at the leaf level, unaffected by mode.
- `rate-as` requires a ticking context to advance its integrated clock. Without one, it behaves like `time-as` over a clock that has not advanced.

12.2 The intended use of state in Imperative mode is not "live performance" — it is debugging, exploration, or building bespoke evaluation loops. The semantics are defined but not load-bearing for the live-coding path.

---

## 13. Open / Deferred

13.1 **Frequency unit conventions for UGens.** Hz is the canonical unit (§6.4). Whether to add `osc-bar`/`osc-beat` variants for musical-relative frequencies is unresolved — they are ergonomic for live-coding patterns but expand the UGen surface. Pending evidence from real use.

13.2 **Explicit state reset.** The user occasionally wants to force a `defstate` to its `init` value (e.g. on a transport rewind, or as a deliberate "reset everything" gesture). Mechanism options: a `(reset! state-name)` form, a transport-tied automatic reset, or implicit reset on the cell's identity break (§4.3). Not yet decided.

13.3 **State scope below top level.** This spec restricts `defstate` to top-level form (§2.9). A future extension might allow stateful bindings inside `let` or `defn` bodies, with per-call-site identity. The use cases are unclear; the implementation cost is real (locally-scoped state slots, identity hashing). Defer.

13.4 **`dt-wall` exposure outside state-bearing bodies.** `dt-wall` is described as available inside state-bearing forms. Whether it should also be available in arbitrary signal expressions (for users who want to roll their own state via `prev` plus arithmetic, even though `defstate` is the recommended path) is open. Probably yes, with light caveats; defer until first user need.

13.5 **`defstate` ↔ UGen identity.** Whether UGens are implemented as anonymous `defstate` instances with local-clock `dt` semantics, or as a distinct primitive layer that happens to share the state-slot machinery, is **intentionally left open**. Both are coherent with the spec; the observable behaviour is identical (both follow `rate-as`, both share state under CSE unless `:id`/`:fresh` says otherwise, both interact with LKG identically). Implementations may unify or distinguish them as suits the engine.

&nbsp;&nbsp;&nbsp;&nbsp;13.5.alt1 **Possible future direction:** treat UGens as compiler-known primitives with bespoke optimisations (waveform tables, SIMD), distinct from `defstate`. Best for performance.

&nbsp;&nbsp;&nbsp;&nbsp;13.5.alt2 **Possible future direction:** treat UGens as a small standard library on top of `defstate`, with special CSE rules for anonymous state slots. Best for spec simplicity.

&nbsp;&nbsp;&nbsp;&nbsp;13.5.alt3 **Possible future direction:** allow user-defined UGens via a `defugen` form that wraps `defstate` with anonymous, per-call-site identity. Most extensible; pushes complexity onto the user-facing surface.

13.6 **CSE granularity for UGens with same-shaped but logically distinct inputs.** `(+ (osc freq) (osc freq))` shares one accumulator by default. If the user wants two distinct oscillators with the same frequency, they should use `:phase`, `:id`, or `:fresh`. The exact per-UGen keyword catalogue remains to be finalised.

13.7 **State-cell debugging surface.** The user benefits from being able to inspect `defstate` cell values from the editor (visualisation panel, REPL probe, etc.). Whether this is part of the spec or a runtime concern is unclear. Probably runtime; `state.md` should at minimum specify that cell values are observable through the existing reactive read path.

13.8 **Migration of state across cell-name changes.** A user renames `phase` to `lfo-phase` mid-session. State resets (§4.3) — they lose their accumulated value. Whether the editor / runtime can offer a "rename, preserve state" affordance is open. Not a language concern per se, but the spec should not preclude it.

13.9 **Catalogue ordering.** The proposed UGens in §6.2 cover the obvious musical canon. A larger catalogue (FM operators, comb filters, sample-and-hold variants, feedback delays) might fall out of real use. Growth happens by explicit decision, not on demand.

13.10 **Derivative state cells for named `defstate` under `rate-as`.** V1 can support rate contexts for UGens and `integrate` first. Recompiling arbitrary named `defstate` cells under multiple local rates is coherent but implementation-heavy; defer until a concrete use case demands it.

13.11 **Symbolic integration of substitution factors.** §10.5 declines this as compiler machinery. If a future use case demonstrates that closed-form `(sin (* factor t))` patterns under dynamic `factor` need phase coherence *without* the user reaching for `rate-as` or a UGen, the eligibility-class design from earlier discussions is the starting point. For now, the answer is "use `rate-as` or a UGen."

13.12 **Stateful sub-spec interactions.** `defstate` interacts with `prev`, `schedule`, `eval`, and the imperative dialect in subtle ways. As those specs mature, this one will need targeted updates rather than a rewrite.
