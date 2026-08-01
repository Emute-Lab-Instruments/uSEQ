# Time and Phasors

> Spec: time leaves (`t`/`t0`/`ground-time`), derived phasors and counters,
> durations, bipolar/unipolar conventions. Counterpart to [MAIN.md](MAIN.md).
> See [time-warps.md](time-warps.md) for pure time substitution and
> [state.md](state.md) for integrated local clocks (`rate-as`, `integrate`,
> UGens).

## Source files

- `uSEQ/src/signal_engine/node_pool.h` — `NodeOp::RawTimeLoad` (the `t` leaf); `NodeOp::LoadDt`; `NodeOp::USin`/`UCos`/`Sin`/`Cos` (unipolar/bipolar waveforms); `NodeOp::Fmod` (phasor wrapping); `NodeOp::BiToUni`/`UniToBi`.
- `uSEQ/src/signal_engine/graph_builder.cpp` — `compile_symbol()` emits `RawTimeLoad` for `t`; phasor derivation from timing cells (`beat`, `bar`, `phrase`, `section`, `beat-num`, `bar-num`, `beat-dur`, `bar-dur`).
- `uSEQ/src/signal_engine/graph_builder.h` — `TimeContext` struct carries the local time node through compilation.
- `uSEQ/src/signal_engine/cell_store.h` / `cell_store.cpp` — `init_timing_defaults()` populates `bpm`, `beats-per-bar`, `bars-per-phrase`, `phrases-per-section` cells.
- `uSEQ/src/signal_engine/executor.cpp` — `ExecutionContext::t` and `ExecutionContext::dt`; `RawTimeLoad` returns `ctx.t`.
- `uSEQ/src/firmware/firmware.cpp` — sets `ctx.t` from the hardware clock before each tick.
- `wasm/wasm_wrapper.cpp` — `g_current_time`; `useq_update_time()`.
- `uSEQ/src/dsp/tempoEstimator.h` / `tempoEstimator.cpp` — external tempo estimation from clock inputs.
- `test/signal_engine/test_signal_engine_golden.cpp` — "beat phasor", "bar phasor", "phrase phasor", "section phasor", "beat-num", "bar-num", "beat-dur" golden tests.

---

1.1 `t` is real-valued time in seconds. Monotonic while playing, frozen while paused, reset to zero on stop or rewind.
&nbsp;&nbsp;&nbsp;&nbsp;1.1.1 `t0` is the same as `t` but remains unaffected by any and all time modifications like `fast`, `slow`, `offset` etc; it simply monotonically increases or resets to zero on stop or rewind.
&nbsp;&nbsp;&nbsp;&nbsp;1.1.2 `ground-time` is like `t0` but does not reset to zero; it simply keeps track of the number of seconds since the session was booted.

1.1.3 Transport logical time is derived from, but distinct from, the host or
hardware monotonic wall clock. Pause records a logical-time anchor. Resume
adjusts the wall-to-logical offset so the paused wall interval is excluded;
the first resumed tick has `dt = 0`. Rewind changes the logical origin to zero
without clearing programs or state and likewise gives the first post-rewind
tick `dt = 0`. Stop is pause plus rewind. Play, pause, rewind, and stop never
clear definitions, graphs, state resources, or their current values. The
transport-origin correction is separate from the user-visible
`useq-set-time-offset` value, so transport operations do not silently rewrite
that setting.

1.2 **Phasors** are signals that ramp `0 → 1` and (typically) wrap. The standard phasors are derived from `t` and timing cells:
- `beat = fmod(t · (bpm / 60), 1)`
- `bar = fmod(t · (bpm / 60 / beats-per-bar), 1)`
- `phrase = fmod(t · (bpm / 60 / beats-per-bar / bars-per-phrase), 1)`
- `section = fmod(t · (bpm / 60 / beats-per-bar / bars-per-phrase / phrases-per-section), 1)`

1.3 **Beat counters** are integer-valued signals: `beat-num = floor(t · bpm / 60)`, `bar-num = floor(t · bpm / 60 / beats-per-bar)`. They never wrap.

1.3.1 Inside `time-as`, phasors and counters are re-derived from the substituted
local `t`. Inside `rate-as`, phasors and counters are re-derived from the
integrated local clock. This is why `time-as` is the "position" tool and
`rate-as` is the "speed" tool.

1.4 **Durations** are scalar cells: `beat-dur = 60 / bpm`, `bar-dur = beat-dur · beats-per-bar`. Use them to express musical offsets in seconds.

1.5 The timing cells (`bpm`, `beats-per-bar`, etc.) are ordinary cells (see [cells.md](cells.md)). Redefining `bpm` reactively updates every signal that derives from it. (See `cell_store.cpp` `init_timing_defaults()` and `cold_eval.cpp` `do_set_bpm()` / `on_cell_changed()` for reactive invalidation.)

1.6 Phasors are bipolar-domain agnostic. Whether a phasor maps to a unipolar `[0,1]` or bipolar `[-1,1]` output is decided by the operator that consumes it (e.g. `usin` is unipolar, `sin` is mathematical, `sqr` thresholds).

1.7 **Mathematical primitives are bipolar by default.** `(sin x)` is the standard `sin(x)`; `(u-sin p)` is `sin(2π·p)/2 + 1/2` — the unipolar phasor-domain convenience operator. The user picks which they want; the engine does not silently rescale. (See `node_pool.h` `NodeOp::Sin` vs `NodeOp::USin`; `node_pool.cpp` `eval_unary()` for the math.)
