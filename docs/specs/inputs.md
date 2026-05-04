# Hardware Inputs

> Spec: external leaves of the signal graph — gate inputs, CV inputs,
> switches, encoders. Counterpart to [MAIN.md](MAIN.md).

## Source files

- `uSEQ/src/signal_engine/graph_builder.cpp` — `input_index_for_name()` maps `"in1"`/`"in2"`/`"ain1"`/`"ain2"` to integer indices; `compile_symbol()` emits `InputLoad` nodes for input names.
- `uSEQ/src/signal_engine/node_pool.h` — `NodeOp::InputLoad` (imm = input_index); `make_input_load()`.
- `uSEQ/src/signal_engine/executor.cpp` — `InputLoad` case reads from `hw_inputs[]` array during sampling.
- `uSEQ/src/firmware/hardware_io.h` / `hardware_io.cpp` — `HardwareIO::read_inputs()` samples physical pins into `inputs[]`; `INP_I1`..`INP_ZSWITCH` enum mapping hardware channels to array indices.
- `uSEQ/src/input.h` — `HardwareInput` struct and `HardwareInputType` enum (not yet wired in).
- `wasm/wasm_wrapper.cpp` — `g_hw_inputs[32]` static array; `useq_set_input_value()` for host injection in WASM mode.
- `test/signal_engine/test_signal_engine_golden.cpp` — "hardware input leaves read injected input values" section.

---

1.1 The signal graph has external leaves besides `t`: hardware input channels.

1.2 `in1`, `in2` — digital gate inputs, `0` or `1`.

1.3 `ain1`, `ain2` — analog CV inputs, normalised to `[0, 1]`.

1.4 `(swm n)` / `(swt n)` — momentary / toggle switch values (variant-dependent).

1.5 `(swr)` — encoder switch (variant-dependent).

1.6 `(rot)` — encoder position (variant-dependent).

1.7 Hardware inputs are sampled once per tick before the signal graph runs. Their values are constant within a single sample but can change every tick. (See `firmware.cpp` which calls `io.read_inputs()` before passing `io.inputs` into `ExecutionContext::hw_inputs`.)

1.8 In WASM mode, hardware inputs default to neutral values (typically `0`) unless the host injects them via the eval-with-inputs ABI. (See `wasm_wrapper.cpp` `g_hw_inputs[32]` zero-initialised array and `useq_set_input_value()`.)

1.9 An input symbol that is unsupported on the current variant is a compile-time error, not a silent zero.
