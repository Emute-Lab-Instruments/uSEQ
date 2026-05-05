# Outputs

> Spec: standard output sinks (`a1`..`a8`, `d1`..`d8`, `s1`..`s8`), `q0`,
> active program / LKG / last sample slots. Counterpart to [MAIN.md](MAIN.md).
> See [prev.md](prev.md) for cross-output reads and
> [failure-model.md](failure-model.md) for LKG fallback.

## Source files

- `uSEQ/src/signal_engine/node_pool.h` — `OutputSlot` struct (root_node, lkg_value, valid); `NodePool::outputs[MAX_OUTPUTS]`; `prev_output_values[]`.
- `uSEQ/src/signal_engine/graph_builder.cpp` — `resolve_output_index()` maps `a1`..`s8` to 0..23; output assignment compilation.
- `uSEQ/src/signal_engine/graph_builder.h` — `resolve_output_index()` declaration.
- `uSEQ/src/signal_engine/cold_eval.cpp` — top-level output assignment dispatch; `do_useq_clear()`; output source storage for recompilation.
- `uSEQ/src/signal_engine/cold_eval.h` — `OutputSource` struct (arena offset/length for stored output programs).
- `uSEQ/src/signal_engine/executor.cpp` — `execute_all_outputs()` samples every valid output; `commit_outputs()` updates `prev_output_values` and `lkg_value`.
- `uSEQ/src/signal_engine/executor.h` — `ExecutionContext` (output_values, prev_outputs).
- `uSEQ/src/uSEQ/output_manager.h` / `output_manager.cpp` — firmware-level output routing to hardware pins.
- `uSEQ/src/output.h` — `HardwareOutput` struct, `DEFAULT_OUTPUT_CV`, `DEFAULT_OUTPUT_GATE` (not yet wired).
- `uSEQ/src/firmware/hardware_io.cpp` — `write_outputs()` writes sampled values to DAC/GPIO.
- `wasm/wasm_wrapper.cpp` — `resolve_output_name()` maps name strings to indices; `execute_at_time()` evaluates all outputs.
- `test/signal_engine/test_signal_engine_golden.cpp` — output assignment, sampling, batch execution tests.

---

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

(See `node_pool.h` `OutputSlot` — `root_node` is the active program, `lkg_value` is the LKG sample; `cold_eval.h` `OutputSource` stores source text for recompilation.)

1.5 Reassigning an output replaces the active program. The previous active program, if it had ever run a healthy batch, becomes LKG. See [failure-model.md](failure-model.md).

1.6 `q0` is a **scheduling callback**, not an output. `(q0 expr)` runs `expr` once per quantisation period (default: bar boundary). Use it for top-level effects synchronised to the bar.

1.7 Outputs not assigned by the user produce a **neutral default**:
`DEFAULT_OUTPUT_CV` for continuous outputs (currently `0.5` in firmware) and
`0` for digital and serial outputs. (See `output.h` for `DEFAULT_OUTPUT_CV` / `DEFAULT_OUTPUT_GATE` constants.) Hosts that need the exact startup value
must read the target's advertised/runtime-probed defaults once that surface
exists; until then, these constants are the compatibility contract.

1.8 Output programs that compile but error at runtime fall back to LKG (see [failure-model.md](failure-model.md)).
