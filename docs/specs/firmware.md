# Firmware Architecture

> Spec: composition, tick loop, hardware contracts, persistence surface,
> hardware variants, boot/recovery behaviour. Counterpart to
> [MAIN.md](MAIN.md). The user-observable surface lives here; module C++
> contracts (struct layouts and method signatures) are implementation
> details, not independent specs.

## 1. Composition

1.1 The firmware is a **composition of independent modules**, not an inheritance hierarchy. Each module has one responsibility and communicates through explicit data:

- `SignalEngine` — owns all interpreter state (cells, node graphs, output slots).
- `HardwareIO` — pin access, input sampling, output writing, indicator LEDs.
- `SerialProtocol` — JSON wire protocol over USB serial.
- `FlashStorage` — preset save/load to on-chip flash.
- `I2CNetwork` — optional, multi-module communication.
- `DSPEngine` — optional, audio synthesis on core 1.

The `Firmware` struct is the composition root, instantiated once. `HardwareIO` never reads or writes the signal engine; it only touches its own input/output buffers, which the tick loop bridges to the engine.

## 2. Tick Loop

2.1 Each cycle on core 0 runs the following sequence in order:

1. Read system time.
2. `io.read_inputs()` — sample all hardware inputs once per tick.
3. If a serial command is pending: `engine.eval_cold(code)` and send response.
4. `engine.execute(time, inputs)` — one forward pass over all output graphs.
5. `io.write_outputs(values)` — write to PWM / DAC / digital pins.
6. **Swap `current → prev`** for cross-output `prev` reads (see [prev.md](prev.md)).
7. Opportunistic streaming back to the editor (skipped when budget exceeded).

2.2 **Hot-path invariants.** Steps 1–6 never allocate, never block on I/O (serial reads are non-blocking), and never compile. All compilation work happens in step 3 (cold path). Hardware inputs are sampled exactly once per tick; multiple reads of the same input within a tick see the same value.

2.3 **Dual-core split (RP2040 / RP2350).** Core 0 runs the tick loop above. Core 1 runs `DSPEngine.tick()` for optional audio-rate synthesis. Communication is through lock-free queues only — no shared mutable state. The signal engine can enqueue DSP commands but never waits for core 1.

## 3. Boot, Recovery, Watchdog

3.1 **Boot sequence:**

1. Hardware init (pins, ADC, LEDs, serial).
2. LED → amber (booting).
3. Signal engine init: register builtins, set defaults (BPM 120, 4/4).
4. If flash has a saved state with a valid CRC32, load it and recompile all outputs. On checksum mismatch the saved state is treated as absent and the engine starts with defaults.
5. LED → green (ready).
6. Send `{"type":"ready","version":"..."}` over serial. This frame is advisory; the editor does not wait for it and may send `hello` immediately after opening the port.
7. Enable the watchdog and enter the tick loop.

3.2 **LED indicator states** (user-visible minimum contract):

| Colour | Meaning |
|---|---|
| amber | booting; or compile error on most recent eval (transient flash) |
| green | running |
| red | flash load failure (transient flash); or persistent runtime error (held) |

Implementations may add finer-grained states (e.g. RGB channels per output health); the three-state contract above is the floor.

3.3 **Watchdog.** A hardware watchdog with a **200 ms timeout** is kicked at the end of every tick. If a tick exceeds the timeout (catastrophic bug, infinite loop in the cold path), the watchdog resets the MCU. On reset, the firmware boots fresh — flash-saved state is preserved and reloaded normally. The reset is invisible to the performer beyond a brief LED amber-flash.

## 4. Hardware Variants

4.1 Hardware differences are resolved at **compile time**. There is no runtime variant detection, no virtual dispatch, and no dead variant code in the binary. Variant flags map to feature flags in `configure.h` and pin numbers in `pinmap.h`; only `HardwareIO` (and the underlying `IOManager`) contains variant-specific code.

4.2 Variant table:

| Variant | Continuous outs | Binary outs | Inputs | Controls | Flash | Notes |
|---|---|---|---|---|---|---|
| MUSICTHING | 4 | 2 | 2 CV + 2 gate | 3 knobs, 1 switch | 16 MB | Inverted outputs; SPI DAC |
| USEQHARDWARE_0_2 | 2 | 4 | 2 CV + 2 gate | Rotary encoder | 8 MB | |
| USEQHARDWARE_1_0 | 3 | 3 | 2 CV + 2 gate | — | 8 MB | PDM output |
| MINIMAL | 2 | 2 | — | — | 2 MB | Testing only |

4.3 **Variant gating of language surface.** Hardware input leaves and outputs that don't exist on a given variant are compile-time errors when referenced in code. See [inputs.md](inputs.md) and [outputs.md](outputs.md). The signal engine queries `HardwareIO::num_*` counts at compile time to validate references.

4.4 **Output index convention** (matches the signal engine's slot numbering):

| Index range | Slot names | Type |
|---|---|---|
| 0–7 | `a1`–`a8` | continuous (analog voltage, 0–5 V or 0–10 V depending on variant) |
| 8–15 | `d1`–`d8` | binary (gate, 0 V or 5 V) |
| 16–23 | `s1`–`s8` | serial (virtual, streamed over the wire protocol) |

Indices beyond the variant's physical count are skipped by `write_outputs()`; they remain valid as serial-stream destinations.

## 5. Persistence Surface

5.1 The user-observable persistence surface — what survives a power cycle:

| Persisted | Not persisted |
|---|---|
| `(define name expr)` cells (source text) | `(set name value)` mutations (no source text) |
| Output expressions (source text) | Runtime state (current `t`, prev-output values, hardware inputs) |
| Tempo / metre (`bpm`, `time-sig`) | Diagnostic history |
| Module ID (for I2C addressing) | LKG slots (rebuilt by re-evaluation) |

5.2 **`save` is a cold-path action.** Triggered by `(memory-save)`. The serial buffer is drained first and a `"save-in-progress"` status sent so the editor knows to expect a brief pause (interrupts are disabled during the actual flash write — hardware requirement). Outputs continue running from the live pool throughout the save.

5.3 **No backward compatibility with pre-2.0 firmware flash.** Upgrading requires a clean flash erase. The old format serialised `Value` objects and environment maps that have no equivalent in the current signal engine.

5.4 **Wear lifetime.** The RP2040's 100K erase-cycle endurance gives several years of typical-use savings. No wear-levelling is implemented; if it ever becomes necessary, that is a firmware-internal change and does not affect the persistence surface above.

## 6. Quantised Eval

6.1 Quantised eval is **runtime-driven**, gated by a single **global quant phasor**. The editor sets `quant: true` on the eval request (see [wire-protocol.md §5.7](wire-protocol.md)); the firmware buffers the form in a quantised-eval queue and drains the queue on the next wrap of the quant phasor. The eval response is sent only after the deferred evaluation completes, so the request's `requestId` round-trip is bounded by the current quant period. Within a single wrap, queued evals execute in submission order with no "latest wins" coalescing — every queued eval runs.

6.2 The **global quant phasor** defaults to `bar`. It is changed via the ModuLisp builtin `(set-quant-phasor expr)` — e.g. `(set-quant-phasor (slow 2 bar))` for a two-bar period. The phasor is global runtime state, not per-request: the wire only conveys the on/off bit for each eval. **Phasor changes are an atomic switch:** calling `(set-quant-phasor …)` immediately replaces the gating phasor for the entire queue — all already-queued entries, plus any subsequent ones, drain together on the next wrap of the new phasor. The previous phasor is dropped without firing; queued entries are never lost, only retimed.

6.3 The historical `@`-prefix immediate-eval surface and the legacy `pending_commands` ring buffer (drained on bar-phasor wrap, populated by `@`-prefix text) are both **gone**. The new quantised-eval queue replaces the ring buffer with a JSON-driven, phasor-configurable mechanism. Editor-side hold is no longer used; the editor sends `quant: true` and lets the runtime do the timing — keeping hardware and WASM behaviour identical by construction.

## 7. LKG and Error Handling

7.1 The "never stop the music" contract is firmware-relevant in several places beyond the language-level LKG fallback ([failure-model.md §2](failure-model.md)):

| Source of failure | Firmware behaviour | Outputs |
|---|---|---|
| Parse / compile error in serial command | Diagnostic in eval response. No engine state change. | Active program unchanged. |
| Flash load failure on boot | Engine starts with defaults. | All outputs neutral until user evaluates code. |
| Flash CRC32 mismatch | Treated as no saved state. | Same as above. |
| Serial TX buffer full during `eval` response | Block briefly until space available. Never drop. | Unchanged. |
| Serial TX buffer full during stream / diagnostic | Drop the message. | Unchanged. |
| Watchdog reset | Fresh boot; saved state reloaded. | Brief discontinuity then resume. |

7.2 `eval` responses are the user's confirmation that their code was received and processed. The serial protocol guarantees they are never silently dropped.

7.3 Diagnostics on the wire follow the format in [diagnostics.md](diagnostics.md). The firmware's serial JSON eval response embeds them as a `diagnostics` field.
