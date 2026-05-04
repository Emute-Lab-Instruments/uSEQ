# Visualisation Projection

> Spec: WASM/runtime support for editor visualisation projection. Counterpart
> to `../../../docs/specs/visualisation.md` in `useq-perform`. This document
> is not a user-facing language feature; it defines the engine invariants that
> let the editor render faithful past values and projected future values
> without corrupting live output state.

## Source files

- `wasm/wasm_wrapper.cpp` — primary implementation:
  - `ProjectionFork` struct (state_values, prev_output_values, lkg_values, prev_tick_time, cell_values, hw_inputs, frontier_time, start_time, valid).
  - `reset_projection_fork()` — clones post-tick live state into the fork (spec section 3).
  - `project_from_fork()` — saves live state, installs fork state, runs sequential sample loop, saves fork advances, restores live state (spec section 4).
  - `useq_tick_and_project()` — combined ABI: phase 1 state-advancing tick, phase 2 projection (mode 0=none, 1=reset-fill, 2=extend-frontier) (spec section 7).
  - `execute_batch_sequential()` — stateful batch evaluation with state save/restore.
- `uSEQ/src/signal_engine/executor.cpp` — `execute_all_outputs()`, `commit_state()`, `commit_outputs()` used by both live ticks and projection ticks.
- `uSEQ/src/signal_engine/executor.h` — `ExecutionContext` struct (t, dt, cell_values, hw_inputs, prev_outputs, output_values, workspace).
- `uSEQ/src/signal_engine/node_pool.h` — `NodePool` state arrays (state_values, prev_output_values, OutputSlot::lkg_value) that the fork clones.

---

## 1. Frame

1.1 The editor visualisation renders two different streams:
- **Past** — values the live engine actually produced as time advanced.
- **Future** — values the engine would produce if the current program, cells,
  declared state, and external inputs continued from now.

1.2 The WASM engine must support this without advancing live output state during
future projection. A future projection is observational: it may consume CPU and
temporary memory, but after the call returns the live engine must behave as if
only the live tick happened. The efficient path is a persistent projection fork:
clone once on invalidation, then extend the fork's frontier in small batches.

1.3 Projection correctness is especially important for declared state
([state.md](state.md)), `prev` ([prev.md](prev.md)), and external inputs
([inputs.md](inputs.md)). Pure expressions can be resampled at arbitrary times;
stateful expressions cannot be faked by repeatedly evaluating isolated points.

---

## 2. Live State vs Projection Fork

2.1 The engine owns two conceptual state shapes:
- **Live state** — the authoritative runtime state advanced by firmware ticks
  or WASM live ticks.
- **Projection fork** — a cloned state used only to generate future samples for
  the editor.

2.2 Live ticks update:
- declared state slots;
- `prev_output_values`;
- last-known-good / last-sample bookkeeping;
- the previous tick time used for `dt`.

2.3 Projection ticks update only the projection fork. They must not mutate live
declared state, live `prev_output_values`, live output LKG state, or live
previous tick time.

2.4 The projection fork records:
- `projectionStartTime` — the live time at which the fork was created;
- `projectionFrontierTime` — the newest projected sample time;
- the current compiled output graph identity / program revision;
- current cell values and live-edit values;
- a clone of declared state slots;
- a clone of `prev_output_values`;
- a clone of the previous tick time used for `dt`;
- output validity / last-known-good state required to evaluate active outputs;
- frozen external input values captured at fork creation.

(See `wasm_wrapper.cpp` `ProjectionFork` struct which implements all of these fields.)

2.5 The clone operation must be complete enough that stepping the projection
fork produces the same values that live state would have produced if time had
advanced under the frozen inputs.

2.6 The projection fork should share immutable compiled graph structures with
live state where possible. Mutable runtime vectors must be copied or otherwise
isolated so projection ticks cannot affect live ticks.

---

## 3. Reset-Fill

3.1 A reset-fill is used when the editor invalidates future projection: code
changed, projection-affecting settings changed, or a relevant external input /
live-edit value changed.

3.2 Reset-fill steps:
1. Tick live state at `tick_time` if the combined ABI call includes a live tick.
2. Clone post-tick live state into a fresh projection fork.
3. Freeze current external inputs and live-edit values into the fork.
4. Sequentially evaluate active outputs from just after `tick_time` to
   `projection_end`.
5. Store the newest projected sample time as `projectionFrontierTime`.

(See `wasm_wrapper.cpp` `reset_projection_fork()` for steps 2-3 and `useq_tick_and_project()` projection_mode==1 for the full sequence.)

3.3 The live tick owns the exact `tick_time` boundary value. Reset-fill does not
advance state a second time at `tick_time`. If the editor needs a visual
boundary anchor, it uses the live tick value returned by the combined call.

3.4 `dt` inside the projection fork is computed between adjacent projection
sample times. For the first projected sample after reset, `dt` is
`first_projection_time - tick_time`.

---

## 4. Frontier Extension

4.1 Frontier extension is the steady-state operation between invalidations. It
does not rewind to live `now`; it advances the projection fork from its current
frontier.

4.2 Extension steps:
1. Read `projectionFrontierTime`.
2. Choose a small batch of future sample times strictly greater than the
   current frontier.
3. Sequentially step the projection fork across those times.
4. Return requested output samples to the editor as append-only future data.
5. Update `projectionFrontierTime` to the newest returned sample time.

4.3 The extension batch should contain more than one sample by default. A target
of 4 samples per extension is a good default; implementations may tune within
2-8 samples. Larger batches amortize the JS/WASM boundary but increase burst
cost and latency after input changes.

4.4 A one-sample extension, if an implementation allows it, must return a sample
at the next frontier time, not at live `tick_time`. The historical
`num_samples == 1` time-window convention of `dt = 0` and `sample_time =
start_time` is not acceptable for frontier extension unless `start_time` is
already the desired frontier sample time.

4.5 Extension must be monotonic. Returned sample times for a given projection
fork are strictly increasing. Duplicate times are rejected or omitted before
they reach the editor.

4.6 Extension sample times are deterministic. For old frontier `F`, requested
end `E`, and count `N`, samples are at `F + step`, `F + 2*step`, ..., `E`,
where `step = (E - F) / N`.

4.7 Projection stepping must update all runtime-active outputs needed for
correct `prev_output_values` and shared state. The initial implementation should
step all active outputs and return only requested channels; dependency-closure
stepping is an optimisation once dependency metadata is reliable.

---

## 5. Invalidation

5.1 The editor is responsible for deciding that a projection is stale. The WASM
runtime is responsible for making reset cheap and exact once requested.

5.2 Invalidation must discard the projection fork. It must not reset live state.

5.3 The minimum invalidation triggers are:
- any successful code evaluation that changes an active output or a dependency
  of an active output;
- changes to cells referenced by active outputs;
- changes to projection-affecting visualisation settings;
- changes to external inputs or live-edit values referenced by active outputs,
  once dependency metadata is available.

5.4 Failed evals do not create a new projection from broken code. The previous
projection may remain visible as last-known-good future data, or the editor may
visually mark it stale, but the engine must not promote failed code into the
projection fork.

5.5 Until dependency tracking is reliable, successful evals should invalidate
the whole projection fork. Per-output future-buffer preservation is allowed only
when the engine can prove that the output and its dependency closure are
unaffected.

---

## 6. Failure Atomicity

6.1 Frontier extension is atomic. If any projection step fails before the batch
completes, the engine restores the projection fork to its previous frontier,
returns an error, and the editor appends no samples.

6.2 Reset-fill may return a valid prefix only if the fork can be left in the
state corresponding to the last valid sample. If not, reset-fill fails
atomically and the previous successful projection remains the editor's last
known future.

---

## 7. ABI Shape

7.1 The editor-facing ABI may expose one combined call or separate operations.
The semantic operations are:
- **tick live** at `tick_time`;
- **reset-fill projection** from post-tick live state at `tick_time`;
- **extend projection frontier** from `projectionFrontierTime`;
- **read projection metadata** (`projectionStartTime`,
  `projectionFrontierTime`, replacement-vs-append mode).

7.2 A combined call is preferred for the editor hot path (implemented in `wasm_wrapper.cpp` `useq_tick_and_project()`):

```c
int useq_tick_and_project(
    const char* outputs_json,
    double tick_time,
    int projection_mode,          // 0 none, 1 reset-fill, 2 extend-frontier
    double projection_end,
    int projection_sample_count,
    int buffer_ptr,
    int buffer_length);
```

7.3 Buffer layout:
- tick row first: `num_channels` doubles;
- projection rows after that: `num_channels * projection_sample_count` doubles;
- each projection row is contiguous for one requested output.

7.4 Metadata may be returned by a companion export or an out-parameter struct.
The editor must be able to distinguish:
- no projection returned;
- replacement projection from reset-fill;
- append projection from frontier extension.

7.5 The ABI must fail with `-1` and populate `useq_last_error` when:
- the output JSON cannot be parsed;
- the caller-provided buffer is too small;
- extension is requested before a projection fork exists;
- projection times are non-finite or non-monotonic.

---

## 8. Non-Goals

8.1 The projection fork is not a second audio/control engine. It exists only for
editor visualisation and probe lookahead.

8.2 The projection fork does not predict future external input changes. Frozen
input values are a deliberate approximation.

8.3 The projection fork does not change language semantics. It is an evaluation
strategy for visual feedback, not a user-visible construct.

8.4 The projection fork is not required on hardware firmware unless hardware
itself later starts serving projected future data. For v1, the browser-local
WASM runtime owns this contract.
