# Time Warps

> Spec: pure time substitution (`premap`, `time-as`) and the affine-style
> substitution sugars (`fast`/`slow`/`offset`/`shift`). Integrated-rate / phase-coherent local
> clocks live in [state.md](state.md) via `rate-as`, `integrate`, and UGens.
> Counterpart to [MAIN.md](MAIN.md). See [time.md](time.md) for the time
> leaves themselves.

---

## 1. Two Different Ideas

1.1 ModuLisp has two related but distinct ways to change how a subexpression
experiences time:

- **Time substitution**: "read this signal at another time coordinate."
- **Rate integration**: "run a local clock at this speed."

1.2 These are identical for constant affine scaling, but diverge for dynamic
signals. If `k` changes over time:

```text
substitution: t' = k(t) * t
rate clock:   t' = integral(k(t) dt)
```

1.3 The vocabulary keeps the distinction visible. `premap`, `time-as`, and
`fast`/`slow`/`offset`/`shift` are substitution forms. `rate-as`, `integrate`,
`phasor`, and `osc` are rate / phase-coherent forms. The language must never
reinterpret dynamic `fast` as rate integration.

---

## 2. Pure Substitution

2.1 ModuLisp signals form a **profunctor over time**. Two mapping operations
exist:

- **postmap**: output mapping. `(postmap g signal)` is equivalent to `(g signal)`.
  This is ordinary function application; no special form is needed.
- **premap**: input mapping. `(premap f signal)` denotes `lambda t: signal(f(t))`.
  This is the primitive time-substitution operation; it changes the time context
  for the inner subgraph.

2.2 `(time-as time-signal body)` is the user-facing substitution form. It
evaluates `body` with the time leaf `t` rebound to `time-signal` at the current
sample. It is equivalent in spirit to `(premap (fn [_] time-signal) body)`, but
clearer when the user has already constructed a named clock/position signal.

2.3 Substitution is pointwise and mathematically pure. It performs no
integration, stores no history, and provides no continuity guarantee beyond the
continuity of the supplied `time-signal`.

```lisp
;; Reads the body at twice the current time.
(time-as (* 2 t)
  (usin t))

;; Scrubs a gesture by knob position. Moving the knob jumps around in time.
(time-as (ain1)
  gesture)

;; Loops local time every bar. The boundary reset is intentional.
(time-as (fmod t bar-dur)
  (step [1 0 1 0] beat))
```

2.4 Dynamic substitution is valid but not phase-coherent. This expression is
well-defined and intentionally jumpy when the selected factor changes:

```lisp
(time-as (* (step [1 2 4 8] bar) t)
  (usin t))
```

The phase is computed from the current pointwise time expression, not from an
accumulated rate. If the user wanted continuity, they wanted a rate clock
([state.md §7](state.md)).

---

## 3. Substitution Sugars

3.1 `fast`/`slow`/`offset`/`shift` are canonical sugars for pointwise time
substitution:

- `(fast k expr)` means `(time-as (* k t) expr)`.
- `(slow k expr)` means `(time-as (/ t k) expr)`.
- `(offset k expr)` means `(time-as (+ t k) expr)`. `k` is in seconds.
- `(shift k expr)` is an alias of `offset`.

3.2 Dynamic arguments are allowed, but they are always stateless substitution.
If `k` is a signal, `(fast k expr)` means `expr` sees `t' = k(t) * t` at each
sample. It does **not** mean `t' = integral(k(t) dt)`.

3.3 A compiler/editor may emit a hint, not an error, when dynamic
`fast`/`slow` is used around an oscillator-like closed-form expression:

```text
"fast with a signal is pointwise time substitution and may jump. Use rate-as
if you want phase-coherent speed changes."
```

3.4 Use `beat-dur` and `bar-dur` to express musical offsets in seconds:

```lisp
(offset (* 0.5 beat-dur) expr) ; half a beat
```

If `beat-dur` is baked, redefining `bpm` invalidates and recompiles dependents.
If it is live-loaded, the offset is dynamic pure substitution and may jump when
tempo changes.

3.5 Constant affine substitutions compose and flatten at compile time.
`(fast 2 (slow 4 expr))` is equivalent to `(fast 0.5 expr)`. Dynamic
substitutions remain ordinary graph nodes unless the compiler can prove a safe
simplification.

---

## 4. Rate Clocks Are Not Warps

4.1 Phase-coherent speed changes are expressed with state-bearing clock
constructs:

```lisp
;; Everyday oscillator vocabulary.
(osc (step [1 2 4 8] bar))

;; Explicit local clock.
(time-as (integrate (step [1 2 4 8] bar))
  (usin t))

;; Proposed user-facing sugar.
(rate-as (step [1 2 4 8] bar)
  (usin t))
```

4.2 `rate-as` is specified in [state.md](state.md) because it introduces an
accumulated local time signal and therefore needs state-slot identity, LKG, and
runtime failure semantics.

4.3 `offset` is still substitution, not a phase-offset UGen operation. To offset
oscillator phase, write phase arithmetic explicitly:

```lisp
(usin (+ (phasor 2) 0.25))
```

---

## 5. Open / Deferred

5.1 **Hint policy for dynamic sugar.** Dynamic `fast`/`slow`/`offset` are valid
pure substitution. The precise editor/compiler hint policy for oscillator-like
uses is UX work, not a semantic restriction.

5.2 **Symbolic integration of substitution factors** is not part of the spec.
The answer for phase coherence is to use a state-bearing form, not for the
compiler to guess when `(* k t)` should be treated as `integral(k dt)`.
