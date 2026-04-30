# Hardware Inputs

> Spec: external leaves of the signal graph — gate inputs, CV inputs,
> switches, encoders. Counterpart to [MAIN.md](MAIN.md).

1.1 The signal graph has external leaves besides `t`: hardware input channels.

1.2 `in1`, `in2` — digital gate inputs, `0` or `1`.

1.3 `ain1`, `ain2` — analog CV inputs, normalised to `[0, 1]`.

1.4 `(swm n)` / `(swt n)` — momentary / toggle switch values (variant-dependent).

1.5 `(swr)` — encoder switch (variant-dependent).

1.6 `(rot)` — encoder position (variant-dependent).

1.7 Hardware inputs are sampled once per tick before the signal graph runs. Their values are constant within a single sample but can change every tick.

1.8 In WASM mode, hardware inputs default to neutral values (typically `0`) unless the host injects them via the eval-with-inputs ABI.

1.9 An input symbol that is unsupported on the current variant is a compile-time error, not a silent zero.
