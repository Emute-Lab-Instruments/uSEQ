# Time and Phasors

> Spec: time leaves (`t`/`t0`/`ground-time`), derived phasors and counters,
> durations, bipolar/unipolar conventions. Counterpart to [MAIN.md](MAIN.md).
> See [time-warps.md](time-warps.md) for pure time substitution and
> [state.md](state.md) for integrated local clocks (`rate-as`, `integrate`,
> UGens).

1.1 `t` is real-valued time in seconds. Monotonic while playing, frozen while paused, reset to zero on stop or rewind.
&nbsp;&nbsp;&nbsp;&nbsp;1.1.1 `t0` is the same as `t` but remains unaffected by any and all time modifications like `fast`, `slow`, `offset` etc; it simply monotonically increases or resets to zero on stop or rewind.
&nbsp;&nbsp;&nbsp;&nbsp;1.1.2 `ground-time` is like `t0` but does not reset to zero; it simply keeps track of the number of seconds since the session was booted.

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

1.5 The timing cells (`bpm`, `beats-per-bar`, etc.) are ordinary cells (see [cells.md](cells.md)). Redefining `bpm` reactively updates every signal that derives from it.

1.6 Phasors are bipolar-domain agnostic. Whether a phasor maps to a unipolar `[0,1]` or bipolar `[-1,1]` output is decided by the operator that consumes it (e.g. `usin` is unipolar, `sin` is mathematical, `sqr` thresholds).

1.7 **Mathematical primitives are bipolar by default.** `(sin x)` is the standard `sin(x)`; `(u-sin p)` is `sin(2π·p)/2 + 1/2` — the unipolar phasor-domain convenience operator. The user picks which they want; the engine does not silently rescale.
