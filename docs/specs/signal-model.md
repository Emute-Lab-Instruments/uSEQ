# Signal Model (Implicit Lifting)

> Spec: the fundamental semantic claim of Reactive ModuLisp — every
> expression in signal position is implicitly a pure function of time.
> Counterpart to [MAIN.md](MAIN.md). Imperative-mode behaviour is in
> [dialects.md §2](dialects.md).

1.1 The fundamental semantic claim of Reactive ModuLisp is **implicit lifting**: every expression appearing in signal position is automatically interpreted as a function of a numerical argument representing time, plus cells, declared state, previous-output buffers, and external inputs inserted into the graph by e.g. reading hardware/MIDI/OSC/etc inputs.

1.2 Concretely, in signal position:
- a literal `1` denotes the constant signal `λt. 1`;
- the symbol `t` denotes the identity signal `λt. t`;
- `(+ 1 t)` denotes `λt. 1 + t`;
- `(sin (* t 440))` denotes `λt. sin(t · 440)`;
- vectors like `[1 2 3]` denote constant data signals (time-invariant tables);
- vectors like `[1 t 3]` denote mixed-data tables, with the time-varying ones resolving to a "pointer" to the corresponding node.

1.3 The user never writes the `λt.` wrapper. There is no surface-level `fn [t] ...` around output expressions. The lifting is always implicit.

1.4 **Operators combine signals pointwise.** `(+ a b)` where `a` and `b` are signals produces a signal whose value at time `t` is `a(t) + b(t)`. This generalises to all pure operators.

1.5 **The canonical time input is `t`** — raw time in seconds since transport start. All other temporal variables (`beat`, `bar`, `phrase`, `section`, `beat-num`, `bar-num`) are derived from the current local `t` and timing cells (`bpm`, `beats-per-bar`, etc.). `time-as` substitutes a different local `t`; `rate-as` constructs an integrated local `t`. See [time.md](time.md), [time-warps.md](time-warps.md), and [state.md](state.md).

1.6 A signal's *value at a moment* is computed by sampling. Sampling boundaries are an engine concern (see [compilation.md](compilation.md)); the language model is "the signal exists for every `t`".

1.7 Signals have **no hidden state, no allocation, no side effects, no I/O** on the hot path. Most signals are closed-form functions of local time and inputs. Cross-sample memory is allowed only through declared, compiler-visible state (`defstate`, `integrate`, UGens, `rate-as`) or the explicit previous-output buffer (`prev`).

1.8 Pseudo-randomness, where present, must be deterministically seeded (typically from `t` or a hash) so that two evaluations at the same `t` return the same value. Different interface functions might choose to expose this seeding, such that the user might choose to sequence seeds and potentially return to pseudo-random streams that appeared in earlier moments.
