# Signal Model (Implicit Lifting)

> Spec: the fundamental semantic claim of Reactive ModuLisp — every
> expression in signal position is implicitly a pure function of time.
> Counterpart to [MAIN.md](MAIN.md). Imperative-mode behaviour is in
> [dialects.md §2](dialects.md).

### Source Files

- `uSEQ/src/signal_engine/node_pool.h` — `NodeOp` enum: `Const` (constant signal), `RawTimeLoad` (the `t` leaf), `CellLoad`, `InputLoad`, `PrevOutputLoad`, `LoadState`, `LoadDt`, plus all pointwise arithmetic/math/comparison/logic ops
- `uSEQ/src/signal_engine/graph_builder.{h,cpp}` — implicit lifting via graph compilation: `compile_expr()`, `compile_symbol()`, time-context threading (`TimeContext`), `expand_beat()`/`expand_bar()`/etc. (derived phasors from local `t`)
- `uSEQ/src/signal_engine/executor.{h,cpp}` — `execute_all_outputs()` (single-sample evaluation — the "sampling" of signals), `execute_batch()` (SOA batch sampling for visualisation)
- `uSEQ/src/signal_engine/types.h` — `MAX_TOTAL_NODES`, `FLAG_TIME_INVARIANT`
- `wasm/wasm_wrapper.cpp` — `useq_eval_output()` (sample a single output at a time), projection fork (visualisation lookahead without mutating live state)
- `test/signal_engine/test_signal_engine_golden.cpp` — golden tests verifying signal semantics at the language boundary

1.1 The fundamental semantic claim of Reactive ModuLisp is **implicit lifting**: every expression appearing in signal position is automatically interpreted as a function of a numerical argument representing time, plus cells, declared state, previous-output buffers, and external inputs inserted into the graph by e.g. reading hardware/MIDI/OSC/etc inputs.

1.2 Concretely, in signal position:
- a literal `1` denotes the constant signal `λt. 1`;
- the symbol `t` denotes the identity signal `λt. t`;
- `(+ 1 t)` denotes `λt. 1 + t`;
- `(sin (* t 440))` denotes `λt. sin(t · 440)`;
- vectors like `[1 2 3]` denote constant data signals (time-invariant tables);
- vectors like `[1 t 3]` denote mixed-data tables, with the time-varying ones resolving to a "pointer" to the corresponding node.

1.3 The user never writes the `λt.` wrapper. There is no surface-level `fn [t] ...` around output expressions. The lifting is always implicit.

1.4 **Operators combine signals pointwise.** `(+ a b)` where `a` and `b` are signals produces a signal whose value at time `t` is `a(t) + b(t)`. This generalises to all pure operators. (See `uSEQ/src/signal_engine/node_pool.h` — NodeOp: Add, Sub, Mul, Div, etc., all pointwise binary ops; `uSEQ/src/signal_engine/executor.cpp` — eval_node dispatches each NodeOp per sample.)

1.5 **The canonical time input is `t`** — raw time in seconds since transport start. All other temporal variables (`beat`, `bar`, `phrase`, `section`, `beat-num`, `bar-num`) are derived from the current local `t` and timing cells (`bpm`, `beats-per-bar`, etc.). `time-as` substitutes a different local `t`; `rate-as` constructs an integrated local `t`. See [time.md](time.md), [time-warps.md](time-warps.md), and [state.md](state.md).

1.6 A signal's *value at a moment* is computed by sampling. Sampling boundaries are an engine concern (see [compilation.md](compilation.md)); the language model is "the signal exists for every `t`".

1.7 Signals have **no hidden state, no allocation, no side effects, no I/O** on the hot path. Most signals are closed-form functions of local time and inputs. Cross-sample memory is allowed only through declared, compiler-visible state (`defstate`, `integrate`, UGens, `rate-as`) or the explicit previous-output buffer (`prev`). (See `uSEQ/src/signal_engine/executor.cpp` — execute_all_outputs, zero-allocation hot path: reads from flat arrays, writes to pre-allocated workspace; `uSEQ/src/signal_engine/node_pool.h` — NodeOp::LoadState, PrevOutputLoad, the only cross-sample memory paths.)

1.8 Pseudo-randomness, where present, must be deterministically seeded (typically from `t` or a hash) so that two evaluations at the same `t` return the same value. Different interface functions might choose to expose this seeding, such that the user might choose to sequence seeds and potentially return to pseudo-random streams that appeared in earlier moments.

1.9 Editor visualisation may sample future values through a temporary WASM
projection fork ([visualisation-projection.md](visualisation-projection.md)).
That fork is an implementation strategy for lookahead, not a language feature:
it must preserve the same observable signal semantics as live sampling under
frozen inputs, and it must not mutate live state.
