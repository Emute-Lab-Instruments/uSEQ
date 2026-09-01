# Time and Phasors

> Spec: time leaves (`t`/`t0`/`ground-time`), derived phasors and counters,
> durations, bipolar/unipolar conventions. Counterpart to [MAIN.md](MAIN.md).
> See [time-warps.md](time-warps.md) for pure time substitution and
> [state.md](state.md) for integrated local clocks (`rate-as`, `integrate`,
> UGens).

## Source files

- `uSEQ/src/signal_engine/node_pool.h` — `NodeOp::RawTimeLoad` (`t`/`t0`), `NodeOp::GroundTimeLoad` (`ground-time`), and `NodeOp::LoadDt` (`dt-wall`, with local `dt` represented by the current time context); `NodeOp::USin`/`UCos`/`Sin`/`Cos` (unipolar/bipolar waveforms); `NodeOp::Fmod` (phasor wrapping); `NodeOp::BiToUni`/`UniToBi`.
- `uSEQ/src/signal_engine/graph_builder.cpp` — `compile_symbol()` resolves `t`, `t0`, `ground-time`, `dt`, and `dt-wall`; phasor derivation from timing cells (`beat`, `bar`, `phrase`, `section`, `beat-num`, `bar-num`, `beat-dur`, `bar-dur`).
- `uSEQ/src/signal_engine/graph_builder.h` — `TimeContext` carries local/base/ground time and local/wall delta nodes through compilation.
- `uSEQ/src/signal_engine/cell_store.h` / `cell_store.cpp` — `init_timing_defaults()` populates `bpm`, `beats-per-bar`, `bars-per-phrase`, `phrases-per-section` cells.
- `uSEQ/src/signal_engine/executor.{h,cpp}` — `ExecutionContext::t`, `dt`, and `ground_t`; `RawTimeLoad`, `GroundTimeLoad`, and `LoadDt` evaluation.
- `uSEQ/src/firmware/firmware.cpp` — sets `ctx.t` from the hardware clock before each tick.
- `wasm/wasm_host_session.cpp`, `wasm/wasm_wrapper.cpp`, and
  `wasm/wasm_evaluation.cpp` — generated-runtime wall/logical time state,
  `useq_update_time()`, and authoritative live-tick entry points.
- `uSEQ/src/firmware/clock_sync.{h,cpp}` — external tempo estimation and phase alignment from clock inputs.
- `test/signal_engine/test_signal_engine_golden.cpp` — phasor/counter/duration and explicit time-context golden tests.
- `test/signal_engine/test_program_semantics.cpp` — independent reference evaluation and local/ground/delta-time effect witnesses.

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

1.1.4 In the generated runtime, `useq_tick_synth_controls` and the
state-advancing mode of `useq_tick_and_project` share one adapter-owned wall
frontier. The first accepted tick may use any finite wall time; later accepted
ticks must strictly increase it. Observational sampling does not acquire or
advance the frontier. The firmware has one device-clock-owned live loop and
assumes that clock is finite and monotonic; the compiler core does not impose a
cross-adapter wall-time rejection rule. A successful `useq-clear` restores the
compiler timing state and the generated-runtime frontier/time origin. The
physical device clock remains an external input, while the first post-clear
core tick receives `dt = 0`. (`TIME-C-001`, `CLEAR-001`)

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

1.5.1 `(set-time-sig beats subdivision)` accepts a positive whole beat count
and subdivision `4`. Other subdivisions are rejected because the current
clock model stores quarter-note `beats-per-bar` and has no denominator cell;
silently accepting (for example) `6/8` would produce a six-quarter-note bar.

1.6 Phasors are polarity-agnostic. The consuming operator chooses the output
range: `sin[uni norm]` maps a normalized phasor to `[0,1]`, `sin[bi norm]`
maps it to `[-1,1]`, and `sqr` thresholds it. Raw angle-domain trigonometry is
explicit as `sin[raw rad]` or `cos[raw rad]`.

1.7 **Normalised waveform primitives are unipolar by default.**
`(sin[uni norm] p) = (/ (+ (sin[raw rad] (* 2 π p)) 1) 2)` — one cycle per
unit of phasor input, mapped to `[0,1]`. `(sin[bi norm] p)` is its bipolar
counterpart. `(sin[raw rad] x)` is mathematical `sin(x)` for an angle in
radians. The user picks the required domain and range; the engine does not
silently infer either from the value. See
[operator-modifiers.md](operator-modifiers.md) §3.
