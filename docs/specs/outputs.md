# Outputs

> Spec: standard output sinks (`a1`..`a8`, `d1`..`d8`, `s1`..`s8`), `q0`,
> active program / LKG / last sample slots. Counterpart to [MAIN.md](MAIN.md).
> See [prev.md](prev.md) for cross-output reads and
> [failure-model.md](failure-model.md) for LKG fallback.

1.1 An **output** is a named sink that consumes a signal and produces hardware effect. The standard outputs are:
- `a1`..`a8` — continuous (analog voltage / PWM), nominally `[0, 1]` mapped to the module's voltage range;
- `d1`..`d8` — binary (gate / digital), thresholded at `0.5`;
- `s1`..`s8` — serial streams, emitted as binary `STREAM` frames over USB
  (`[0x1F][0x00][channel:u8][value:f64-LE]`, 11 bytes total; see
  [wire-protocol.md §3.2](wire-protocol.md)).

1.2 Output assignment is a top-level form: `(a1 expr)`. The expression is compiled and stored as the output's signal program. The output is sampled every tick.

1.3 Assigning a numeric literal to an output is the constant signal: `(a1 0.5)` holds 0.5 forever. `(a1 0)` clears the output.

1.4 Each output slot owns:
- An **active program** (current compiled signal).
- A **last-known-good (LKG) program** (most recent program that has produced ≥1 healthy sample batch).
- A **last sample value** (held during transitions).

1.5 Reassigning an output replaces the active program. The previous active program, if it had ever run a healthy batch, becomes LKG. See [failure-model.md](failure-model.md).

1.6 `q0` is a **scheduling callback**, not an output. `(q0 expr)` runs `expr` once per quantisation period (default: bar boundary). Use it for top-level effects synchronised to the bar.

1.7 Outputs not assigned by the user produce a **neutral default**:
`DEFAULT_OUTPUT_CV` for continuous outputs (currently `0.5` in firmware) and
`0` for digital and serial outputs. Hosts that need the exact startup value
must read the target's advertised/runtime-probed defaults once that surface
exists; until then, these constants are the compatibility contract.

1.8 Output programs that compile but error at runtime fall back to LKG (see [failure-model.md](failure-model.md)).
