# ModuLisp Semantics

> Engine-agnostic semantic spec for ModuLisp — the live-coding language for
> the uSEQ eurorack module. Describes the language as the user experiences
> it and as any conforming implementation must behave. Implementation
> documents are footnotes, not semantics.
>
> Counterpart to `../../../docs/specs/MAIN.md` (app semantics, in
> `useq-perform`) and `../../../docs/specs/runtime-contract.md` (runtime/firmware
> contract). This document and its sub-specs are the canonical source for
> what programs *mean* and what runtimes must guarantee.
>
> This is the **main spec**. It holds the frame, language-wide failure and
> performance contracts, the stable compatibility surface, cross-cutting
> open questions, and an index of feature-specific sub-specs (§6). Each
> sub-spec is self-contained and numbered from 1.1.

### Source Files

Core signal engine (shared by firmware and WASM):

- `uSEQ/src/signal_engine/signal_engine.h` — convenience header, includes all signal engine components
- `uSEQ/src/signal_engine/types.h` — limits, constants, type aliases (`SymbolID`, `NODE_NONE`, `MAX_OUTPUTS`, etc.)
- `uSEQ/src/signal_engine/cold_eval.{h,cpp}` — cold-path evaluation (`eval_cold`), `SignalEngine` struct, dependency tracking (`on_cell_changed`), bulk recompilation
- `uSEQ/src/signal_engine/cell_store.{h,cpp}` — `CellStore`, `Cell`, `CallableInfo`, `SourceArena`, data pool
- `uSEQ/src/signal_engine/node_pool.{h,cpp}` — `NodePool`, `Node`, `OutputSlot`, CSE hash-cons table, state slots, topological sort
- `uSEQ/src/signal_engine/graph_builder.{h,cpp}` — `GraphBuilder`, compilation passes, form dispatch, dependency tracking, error reporting
- `uSEQ/src/signal_engine/executor.{h,cpp}` — `execute_all_outputs`, `execute_batch`, `commit_outputs`, `commit_state`
- `uSEQ/src/signal_engine/token.{h,cpp}` — tokenizer, `Token`, `TokenStream`
- `uSEQ/src/signal_engine/diagnostics.{h,cpp}` — `Diagnostic`, severity/category enums, `find_fuzzy_match`
- `uSEQ/src/signal_engine/symbols.def` — X-macro symbol table (builtins, keywords, output names)

Firmware composition:

- `uSEQ/src/firmware/firmware.{h,cpp}` — `Firmware` struct (composition root), `init()`, `tick()`, watchdog
- `uSEQ/src/firmware/serial_protocol.{h,cpp}` — `SerialProtocol`, JSON wire protocol, hello/ping/eval/stream-config handlers
- `uSEQ/src/firmware/hardware_io.{h,cpp}` — `HardwareIO`, pin access, input sampling, output writing, LED states
- `uSEQ/src/firmware/flash_storage.{h,cpp}` — `FlashStorage`, binary save/load, CRC32 checksum
- `uSEQ/src/firmware/dsp_engine.{h,cpp}` — `DSPEngine`, core 1 audio-rate DSP (optional)
- `uSEQ/src/firmware/i2c_network.{h,cpp}` — `I2CNetwork`, multi-module I2C communication (optional)

WASM build:

- `wasm/wasm_wrapper.cpp` — WASM ABI: `useq_init`, `useq_eval`, `useq_last_diagnostics`, `useq_active_diagnostics`, batch evaluation, projection fork

Port abstractions:

- `uSEQ/src/ports/IStorage.h` — testable storage interface for flash persistence
- `uSEQ/src/ports/mocks/MockStorage.h` — in-memory storage for desktop tests

Tests:

- `test/signal_engine/test_signal_engine.cpp` — comprehensive signal engine tests (tokenizer, graph builder, executor, cold eval)
- `test/signal_engine/test_signal_engine_golden.cpp` — golden semantic tests at the language boundary
- `test/firmware/test_firmware_e2e.cpp` — full tick-loop end-to-end tests
- `test/firmware/test_wire_protocol_contract.cpp` — wire protocol contract tests

---

## 1. Frame

1.1 ModuLisp is the live-coding language for the uSEQ eurorack module. It is a Lisp dialect that runs both on firmware (RP2040/RP2350) and in the browser via WASM, with identical observable semantics.

1.2 The product use case is a performer typing expressions and pressing an eval key while the module produces voltages. The language is shaped by that constraint: outputs must keep producing values across edits, errors, and recompilation. Feedback latency must be low; nothing the user types should ever silently lose music.

1.3 ModuLisp is **not** a general-purpose programming language. It is a substrate for describing time-varying signals that drive control voltages, gates, and serial streams (or other arbitrary numerical streams in software).

1.4 ModuLisp has **two mutually-exclusive evaluation dialects**: **Reactive** (default, FRP) and **Imperative** (traditional Lisp REPL). A session runs in exactly one. Unless qualified, this spec describes Reactive semantics. See [dialects.md](dialects.md).

1.5 Implementations target two interchangeable shapes — firmware on the RP2040/RP2350 and an in-browser WASM build of the same interpreter. Both are first-class. The user's code, edits, and evaluations behave the same against either, modulo per-target performance budgets and documented numerical tolerance.

---

## 2. Failure Model

Language-wide degradation contracts. Cited from feature sub-specs.

2.1 **A compile-time error must not stop the music.** The previously active program for that output (if any) keeps running. Other outputs are unaffected. See [failure-model.md](failure-model.md).

2.2 **A runtime error falls back to last-known-good (LKG).** When an active output program errors at runtime — including non-finite values reaching the root — that output switches to its LKG program until the user replaces or clears it.

2.3 **LKG is observed safety, not provable safety.** A graph that succeeded once may fail later under different time / inputs. Cascading failures don't promote unhealthy programs.

2.4 **Diagnostics are structured.** Every diagnostic carries severity (`info`/`warning`/`error`), a category, a source span (when applicable), a human-readable message in plain language, and an optional suggestion with a working example. No jargon ("arity mismatch") in user-facing strings. The data shapes and ABI are in [diagnostics.md](diagnostics.md); the failure semantics (LKG, health states, REPL-vs-output channels) are in [failure-model.md](failure-model.md).

2.5 **Diagnostics survive across evals.** Per-output health (`idle`/`running`/`fallback`/`error`) is queryable and rendered. A successful eval clears prior diagnostics for the affected outputs, not for the whole document.

2.6 **The diagnostic framing is "the compiler doesn't support this here"**, not "the language forbids this." Some restrictions are temporary (dynamic loops, non-numeric signal roots, etc.) and may relax in future versions.

2.7 **Numerical errors are not silently zeroed at the output level.** Per-node NaN/Inf clamping (if any engine performs it) is an internal hygiene mechanism; the user-observable contract is "if the output goes unhealthy, you fall back to LKG, and you see a diagnostic." The engine must not silently produce subtly-wrong output instead of declaring failure.

---

## 3. Performance Targets

3.1 **The hot path (per-sample evaluation) never allocates, never does string-keyed lookup, never compiles.** All compilation work happens between ticks. (See `uSEQ/src/signal_engine/executor.cpp` — execute_all_outputs, flat array traversal, no allocation; `uSEQ/src/signal_engine/cold_eval.cpp` — eval_cold, all compilation here, not on hot path.)

3.2 **Recompilation is sub-tick.** A typical signal recompiles in well under one millisecond. The user perceives cell redefinition as instantaneous.

3.3 **Invalidation is proactive, recompilation is lazy.** Cell mutation marks affected graphs dirty immediately. Dirty graphs are recompiled at the next sampling boundary, never on the per-sample hot path.

3.4 **Pervasive constant folding.** Any pure operation on constant inputs is evaluated at compile time. This composes transitively: deeply nested pure subexpressions collapse to a single `Const` node. (See `uSEQ/src/signal_engine/node_pool.cpp` — make_binop, make_unary fold to Const when both/single input is Const; `uSEQ/src/signal_engine/node_pool.h` — eval_unary, eval_binop, eval_ternary, constant evaluation helpers.)

3.5 **Tick rate.** All current targets are control-rate (~1 kHz tick). Audio-rate (≥ 44.1 kHz) is a future direction; semantics are unchanged but per-sample budget tightens dramatically.

---

## 4. Stable Compatibility Surface

4.1 The **stable core** is committed language surface. Breaking changes require explicit decision and migration path.

4.2 The stable core comprises:
- The Reactive dialect, the value tower (numbers/strings/symbols/keywords/lists/vectors/nil/callables), and the implicit-lifting signal model.
- Standard outputs (`a1`..`a8`, `d1`..`d8`, `s1`..`s8`, `q0`) and their wire contracts.
- Standard time leaves (`t`, `t0`, `ground-time`) and derived phasors (`beat`/`bar`/`phrase`/`section` and their `-num`/`-dur` variants).
- Pure time substitution via `premap`/`time-as` and substitution sugars (`fast`/`slow`/`offset`/`shift`), plus integrated local clocks via `rate-as`.
- Cell reactivity: redefining a cell invalidates and recompiles every output that references it.
- Cross-output `prev` reads, including the "bare output name = `prev`" sugar.
- Hardware input leaves (`in1`/`in2`/`ain1`/`ain2`/`swm`/`swt`/`swr`/`rot`) on supported variants.
- The compile-time rejection rules for signal context (no side effects, no recursion, no unbounded loops, no dynamic `eval`).
- The LKG fallback contract for runtime errors.

4.3 **Compatibility cuts** (kept only as bridges, may shrink without replacement): the historical `@`-prefix immediate-eval surface (no longer present; treat earlier sections of `useq.md` as historical).

4.4 **Out of scope** (not compatibility targets, never returning without a mission case): pre-1.2.0 firmware text-serial protocol, hidden allocation in signal context, implicit threading, cooperative scheduling primitives, multi-tenant evaluation. *Note:* declared cross-sample state via [state.md](state.md) is in-scope — it is transparent (every slot is named or compiler-tracked), bounded, and visible at the call site; the "no hidden state" property is preserved.

---

## 5. Open / Deferred (cross-cutting)

Items that span multiple sub-specs. Feature-specific open questions live in the corresponding sub-spec.

5.1 **`schedule` and `unschedule`.** Whether the historical contract (`(schedule name body period)`) survives in the same shape, gets folded into the global-quant-phasor mechanism ([top-level.md §1.3](top-level.md), [firmware.md §6](firmware.md)), or is replaced by something else is not yet decided. Treat current behaviour as the historical contract; the spec needs updating once the design lands.

5.2 **Imperative-mode boundaries.** Whether the dialect is set per-session, per-tab, or via a `?mode=imperative` URL param is a UI/runtime decision pending product input. The semantics in [dialects.md](dialects.md) hold once a session is in imperative mode.

5.3 **Non-numeric signal roots.** Strings (and possibly other types) flowing through signal graphs to non-CV sinks (visual live-coding, MIDI text) is an intended capability but not currently implemented in either engine. The spec describes the intent; current engines may reject non-numeric roots.

5.4 **Audio-rate signals.** All current targets are control-rate (~1 kHz tick). Audio-rate (≥ 44.1 kHz) is a future direction; semantics are unchanged but per-sample budget tightens dramatically.

5.5 **Cross-target floating-point determinism.** Soft-float ARM vs. x86/WASM hard-float can diverge on transcendentals. The contract is "within tolerance" rather than "bit-identical"; the precise tolerance ladder is not currently specified — it lived in the deleted bytecode-VM spec and needs to be re-stated against the current engine.

5.6 **Source of truth for builtins.** The specs name and define the builtins
needed for each semantic area, but a complete canonical catalogue (with
signal/imperative-mode availability flags, arity, types, semantics, keyword
options, and statefulness) is still missing. Existing golden tests are a
partial implementation check, not a substitute for that catalogue.

5.7 **User-visible integer types.** Currently all numbers are doubles. The compiler may infer integer-ness internally for indices/counters. Whether to surface integer literals (`1i`?), an `(int x)` coercion, or stay doubles-only is open. Triggers for revisiting: concrete pattern bugs caused by FP rounding at vector indexing boundaries; sustained perf concerns on RP2040 soft-float (mostly absorbed by the move to RP2350 hard-float).

5.8 **State-preserving signal abstractions.** The semantic design lives in [state.md](state.md). Stable identity for anonymous stateful expressions lives in [state-identity.md](state-identity.md). Implementation is pending; until it lands, oscillators-under-modulation and DSP-style state must still be expressed via `prev`-on-output gymnastics. The cross-cutting open questions (catalogue growth, `rate-as` implementation scope, state reset mechanics, etc.) live in [state.md §13](state.md).

5.9 **`prev` window across batches.** The `prev` contract is "previous sample within the current batch / previous tick on firmware". The exact semantics at batch boundaries (does `prev` at the first sample of a new batch read the last sample of the previous batch, or the neutral default?) needs an explicit answer; current engines tend to carry forward, but this should be normalised. See [prev.md](prev.md).

---

## 6. Sub-Specs

Read each as a self-contained spec. Internal numbering restarts at 1.1.

6.1 [dialects.md](dialects.md) — Reactive vs Imperative, dialect switching, imperative-mode semantics.

6.2 [values-types.md](values-types.md) — value tower, numeric model, truthiness, keyword arguments, signal-context type restrictions.

6.3 [signal-model.md](signal-model.md) — implicit lifting, pointwise combination, purity, deterministic pseudorandomness.

6.4 [time.md](time.md) — `t`/`t0`/`ground-time`, phasors, beat counters, durations, bipolar/unipolar conventions.

6.5 [time-warps.md](time-warps.md) — pure substitution (`premap`/`time-as`) and substitution sugars (`fast`/`slow`/`offset`/`shift`). Phase coherence is **not** a property of substitution — see [state.md](state.md).

6.6 [cells.md](cells.md) — reactive bindings, dependency tracking, redefinition invalidation, cascading dependencies. Purely-functional cells; for stateful cells see [state.md](state.md).

6.6.1 [state.md](state.md) — declared cross-sample state. The `define-state`/`defstate` foundation, `integrate` and the bare primitives, the named UGen catalogue (`osc`/`phasor`/`slew`/...), keyword UGen options, and the `rate-as` local-clock form. Phase coherence lives here.

6.6.2 [state-identity.md](state-identity.md) — stable IDs and resource schemas for anonymous stateful expressions, cold-eval expression execution, duplicate-active ID validation, and stateful probe projection.

6.7 [functions.md](functions.md) — `defn`/`fn`/`lambda`, inlining in signal context, recursion rules, variadic arithmetic.

6.8 [outputs.md](outputs.md) — output sinks (`a1`..`a8`, `d1`..`d8`, `s1`..`s8`), `q0` scheduling, active program / LKG / last sample slots.

6.9 [prev.md](prev.md) — cross-output reads, the "bare name = prev" sugar, batch semantics, feedback loops.

6.10 [inputs.md](inputs.md) — hardware input leaves, variant gating, WASM-host injection.

6.11 [top-level.md](top-level.md) — top-level forms, eager evaluation, transport/tempo commands, dynamic `eval`.

6.12 [compilation.md](compilation.md) — node-graph model, compile passes, time-warp flattening, loop unrolling, signal-context rejection rules.

6.13 [failure-model.md](failure-model.md) — compile-time vs runtime errors, LKG fallback, per-output health states, success feedback, REPL-vs-output channels, batch-eval isolation, cascade-noise mitigation, chain of blame.

6.14 [diagnostics.md](diagnostics.md) — diagnostic data shapes, source-span coordinates, WASM ABI exports, firmware serial JSON embedding, fuzzy-name matching.

6.15 [firmware.md](firmware.md) — firmware composition, tick loop, dual-core split, boot/recovery, watchdog, hardware variants, persistence surface.

6.15.1 [hardware-io.md](hardware-io.md) — physical pin maps, input sampling (filtering, interrupts, ADC), output writing (PWM, SPI DAC, PIO), LED feedback system, boot LED sequences.

6.16 [wire-protocol.md](wire-protocol.md) — byte-level and message-level contract over USB CDC serial between the firmware and a host editor. Single source of truth for what crosses the wire.

6.17 [visualisation-projection.md](visualisation-projection.md) — WASM/runtime support for editor visualisation projection: live-state ticks, projection forks, reset-fill, frontier extension, and the combined tick/project ABI semantics.

6.18 [devtools.md](devtools.md) — compile-time-gated firmware instrumentation and runtime-controllable telemetry. `dt::` namespace API for tick profiling, events, counters, gauges. Wire protocol `debug` message type for agent-driven debugging.

6.19 [synth-nodes.md](synth-nodes.md) — (accepted v1) the `(synth ...)` top-level form: declarative instantiation of audio-rate NodeDefs with signal-controlled params, inline audio routing, auto identity with optional names, vector-valued params as the polyphony surface, declarative lifecycle with fades. Host/engine counterpart: `../../../docs/specs/synthesis.md` (in `useq-perform`).

---

## 7. Cross-References

7.1 `../../../docs/specs/runtime-contract.md` (in `useq-perform`) — runtime/firmware/WASM contract from the editor's perspective.

7.2 `../../../docs/specs/MAIN.md` (in `useq-perform`) — app semantics. Counterpart to this doc; where they disagree on app behaviour, that doc wins.

7.3 `../../../docs/specs/visualisation.md` (in `useq-perform`) — app-side visualisation contract. The app spec owns user-visible rendering semantics; [visualisation-projection.md](visualisation-projection.md) owns the WASM-side projection invariants needed to support them.

7.4 If this spec disagrees with any of the above on a point of language semantics, **this spec wins** by intent. Implementations should be brought into line. If this spec disagrees with the actually-deployed firmware, that is a bug — file it.
