# Outputs

> Spec: standard output sinks (`a1`..`a8`, `d1`..`d8`, `s1`..`s8`),
> active program / LKG / last sample slots. Counterpart to [MAIN.md](MAIN.md).
> See [prev.md](prev.md) for cross-output reads and
> [failure-model.md](failure-model.md) for LKG fallback.

## Source files

- `uSEQ/src/signal_engine/node_pool.h` — `OutputSlot` struct (root_node, lkg_value, active-assignment `valid`, independent `has_lkg`); `NodePool::outputs[MAX_OUTPUTS]`; `prev_output_values[]`.
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

1.3 Assigning a numeric literal to an output is the constant signal: `(a1 0.5)` holds 0.5 forever. `(a1 0)` installs a running constant-zero program; it does not unassign the output.

1.3.1 `(unassign a1)` is the explicit per-output removal form (and likewise
for every `a1`..`a8`, `d1`..`d8`, and `s1`..`s8` sink). It clears that slot's
active program, stored source, dependencies, previous/LKG sample, and
owner-scoped state/live resources. It validates the complete form before
mutation; an invalid name or extra argument leaves all outputs unchanged.

1.4 Each output slot owns:
- An **active program** (current compiled signal).
- Its stored source and dependency set for reactive recompilation.
- A **last healthy sample value** used only for scalar runtime fallback.

(See `node_pool.h` `OutputSlot` — `root_node`/`valid` identify the active
assignment, `has_lkg` records whether a finite root has ever committed, and
`lkg_value` is the scalar fallback sample; `cold_eval.h` `OutputSource` stores
source text for recompilation.) There is no separately retained LKG program.

1.5 A successfully compiled reassignment atomically replaces the active
program. A failed compile leaves the previous active program and source
unchanged. A runtime failure retains the active program, substitutes the last
healthy scalar sample for that tick, and retries the program on the next tick.
See [failure-model.md](failure-model.md).

1.6 `q0` is not a recognised name in the hardened language. Scheduling and
quantised callback forms are outside this compiler's stable surface.

1.7 Outputs not assigned by the user produce the compiler/runtime **neutral
default**, numeric `0`, for every output class. This value is written
explicitly on each running executor pass, so reusing a caller-owned output
buffer cannot preserve a sample from a program removed by `useq-clear`.
Electrical boot voltages and gate levels are a separate hardware-mapping
policy; they do not change the language-level neutral value.

1.8 Output programs that compile but error at runtime fall back to LKG (see [failure-model.md](failure-model.md)).

1.9 `(useq-clear)` removes every active output graph, stored source,
dependency set, classification, previous sample, and LKG sample. Clear itself
is not an electrical write: while transport is paused the physical outputs
remain held; the next running executor pass emits neutral zero.
