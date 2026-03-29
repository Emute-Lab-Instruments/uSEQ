# Firmware Architecture Spec

## Product Context

uSEQ is a **livecodeable eurorack module** — a hardware music sequencer that musicians program in real-time during performance using a custom LISP dialect called ModuLisp. The module sits in a modular synthesizer rack, producing control voltages and gate signals that drive other modules (oscillators, filters, envelopes, etc.).

### The use case

A performer sits in front of their modular synth. uSEQ is running. They open the web editor on their laptop, which connects to uSEQ over USB serial. They type `(a1 (sin (* t 440)))` and press Ctrl+L. The module's first analog output starts producing a 440Hz sine wave — a smooth voltage that sweeps between 0V and 5V. They type `(d1 (gates [1 0 1 0] beat))` — the first digital output starts firing gate pulses on beats 1 and 3 at the current tempo. They tweak: `(define freq 220)`, `(a1 (sin (* t freq)))`. The output changes instantly. The music never stops.

### What matters

- **Never stop the music.** Outputs keep producing signal even during errors, compilation, flash saves. LKG (last-known-good) fallback is not optional.
- **Sub-millisecond output update.** The tick loop runs as fast as possible (~1kHz+). No allocation, no string processing, no blocking on the hot path.
- **Instant response to code.** When the user sends new code, it compiles and takes effect within one tick. Recompilation of dependent outputs is automatic.
- **Works without a computer.** Saved state loads from flash on boot. The module is fully functional standalone — the editor is optional.
- **Multiple hardware variants.** The same firmware supports different PCB revisions with different numbers of outputs, inputs, and controls. Hardware differences are resolved at compile time.

### What doesn't matter (yet)

- Audio-rate output (future — current update rate is control-rate ~1kHz).
- Multi-user / network editing.
- Firmware OTA updates.

---

## 1. Architecture Overview

The firmware is a **composition of independent modules**, not an inheritance hierarchy. Each module has one responsibility, communicates through explicit data, and knows nothing about the others.

```
Firmware
├── SignalEngine        (sig::SignalEngine — owns all interpreter state)
├── HardwareIO          (pin access, input sampling, output writing, LEDs)
├── SerialProtocol      (JSON wire protocol over USB serial)
├── FlashStorage        (preset save/load to on-chip flash)
├── I2CNetwork          (optional — multi-module communication)
└── DSPEngine           (optional — audio synthesis on core 1)
```

The `Firmware` struct is the composition root. It owns all modules and orchestrates the tick loop. There is exactly one `Firmware` instance, created in `setup()` and ticked in `loop()`.

### 1.1 Tick Loop

Every cycle, core 0 runs this sequence:

```
tick():
    1. time ← read_system_clock()
    2. io.read_inputs()                          // sample ADC, filter, store
    3. if serial.has_incoming():
           code ← serial.read_command()
           result ← engine.eval_cold(code)       // cold path: compile + maybe recompile
           serial.send_response(result)
    4. engine.execute(time, io.inputs) → values   // hot path: one forward pass
    5. io.write_outputs(values)                   // PWM, DAC, digital pins
    6. engine.swap_prev_outputs(values)            // copy current → prev for cross-output refs
    7. serial.maybe_send_stream()                 // periodic output value streaming
```

Steps 1–6 are the critical path. Step 7 is opportunistic (skip if tick budget exceeded).

**Invariants:**
- Steps 1–6 never allocate heap memory.
- Steps 1–6 never block on I/O (serial read is non-blocking).
- Steps 1–6 run sequentially on core 0. No concurrent access to engine state. `eval_cold` writes directly to the live pool. Double-buffering is deferred until multi-threaded execution is needed (e.g., WASM Web Workers).

### 1.2 Dual-Core Split (RP2040)

```
Core 0: Firmware.tick()         — signal engine + I/O + serial
Core 1: DSPEngine.tick()        — audio synthesis (optional)
Communication: lock-free queues only. No shared mutable state.
```

Core 1 is independent. It reads from a queue of DSP commands and writes audio samples to DAC queues. The signal engine on core 0 can enqueue commands (e.g., `dsp-create`, `dsp-connect`) but never waits for core 1.

---

## 2. Module Contracts

### 2.1 HardwareIO

**Responsibility:** All physical pin access. Reading inputs (ADC, digital, encoder), writing outputs (PWM, DAC, digital), updating LEDs. No engine knowledge.

```cpp
struct HardwareIO {
    // ── Configuration (set at init, immutable after) ───────────────────
    uint8_t num_continuous_outs;    // e.g., 4 for MusicThing
    uint8_t num_binary_outs;        // e.g., 2 for MusicThing
    uint8_t num_serial_outs;        // e.g., 8 (virtual, over protocol)
    uint8_t num_hw_inputs;          // e.g., 14 for MusicThing

    // ── Live state (updated every tick) ────────────────────────────────
    double inputs[MAX_HW_INPUTS];   // sampled input values, normalized [0,1]
    double outputs[MAX_OUTPUTS];    // output values to write to hardware

    // ── Lifecycle ──────────────────────────────────────────────────────
    void init();                    // configure pins, interrupts, ADC, LEDs
    void read_inputs();             // sample all inputs, apply filtering
    void write_outputs();           // write outputs[] to hardware pins
    void update_leds();             // reflect output values on indicator LEDs
};
```

**Contracts:**
- `read_inputs()` populates `inputs[]` with filtered, normalized values. Called once per tick before execution.
- `write_outputs()` reads `outputs[]` and writes to hardware. Analog outputs are clamped to [0,1] and scaled to voltage. Digital outputs threshold at 0.5. Called once per tick after execution.
- `init()` is called once. After init, `read_inputs()` and `write_outputs()` are safe to call every tick.
- HardwareIO never reads from or writes to the signal engine. It only touches `inputs[]` and `outputs[]`.

**Hardware variant handling:** HardwareIO uses `configure.h` and `pinmap.h` at compile time. The `init()` implementation is `#ifdef`-gated per variant. The `num_*` counts are set in init based on the variant.

**Input index convention:**
```
Index  Name         Description
0      in1          Digital gate input 1
1      in2          Digital gate input 2
2      ain1         Analog CV input 1
3      ain2         Analog CV input 2
4+     variant      Hardware-specific (knobs, switches, encoder)
```

The signal engine's `InputLoad` node references these indices directly.

**Output index convention (matches signal engine):**
```
Index    Name    Type
0–7      a1–a8   Continuous (analog voltage, 0–5V or 0–10V)
8–15     d1–d8   Binary (gate, 0V or 5V)
16–23    s1–s8   Serial (virtual, streamed over protocol)
```

Not all indices are physically present on every variant. `write_outputs()` skips indices beyond the variant's actual output count.

### 2.2 SerialProtocol

**Responsibility:** JSON-framed communication over USB serial. Receives code from the editor, sends results, diagnostics, and output stream data back.

```cpp
struct SerialProtocol {
    // ── Lifecycle ──────────────────────────────────────────────────────
    void init(uint32_t baud_rate = 115200);

    // ── Receiving ──────────────────────────────────────────────────────
    // Non-blocking. Returns true if a complete command is ready.
    bool has_incoming();

    // Read the next command. Fills `code` with the expression text.
    // Returns message length (caller must NOT use strlen on the result).
    // `request_id` is the protocol tracking ID (for response correlation).
    // `immediate` is true if prefixed with "@" (execute now, don't quantize).
    uint32_t read_command(char* code, uint32_t max_len,
                          uint32_t* request_id, bool* immediate);

    // ── Sending ────────────────────────────────────────────────────────
    // Send eval response (result or error) correlated to a request.
    void send_eval_response(uint32_t request_id, const sig::EvalResult& result);

    // Send diagnostics (independent of eval — e.g., per-output health).
    void send_diagnostics(const sig::Diagnostic* diags, uint8_t count);

    // Send periodic output value stream (for editor visualization).
    // Called opportunistically when tick budget allows.
    void send_stream_data(const double* outputs, uint16_t count,
                          const StreamConfig& config);

    // Handle hello/ping handshake (returns hardware config to editor).
    void handle_handshake();
};
```

**Contracts:**
- `has_incoming()` and `read_command()` are non-blocking. They read from an internal ring buffer filled by the serial interrupt.
- `read_command()` returns the message length. Callers must use this length, not `strlen()`, to determine code size.
- `send_eval_response()` must NEVER drop an eval response. If the TX buffer is full, block briefly until space is available. Eval responses are the user's confirmation that their code was received and processed — dropping them silently breaks the contract with the editor.
- `send_stream_data()` and `send_diagnostics()` may be dropped if the TX buffer is full (they are periodic/opportunistic).
- The protocol layer does NOT evaluate code. It only shuttles bytes. The Firmware connects it to `engine.eval_cold()`.
- Wire format: JSON messages framed with a start-of-message byte (0x1F) and message-type byte, matching the existing protocol in `uSEQ.cpp`.
- **Boot-ready signal:** After `init()` completes, firmware sends `{"type":"ready","version":"..."}` on serial. The editor waits for this before sending commands.

**Message types (preserved from existing protocol):**
```
ready       → sent once after boot, includes firmware version
hello       → hardware config (output names, counts, features)
ping        → keepalive
eval        → code string, returns result + diagnostics
stream-config → configure which outputs to stream and at what rate
```

### 2.3 FlashStorage

**Responsibility:** Save and load the signal engine's state (cells, output expressions, timing parameters) to RP2040 on-chip flash.

```cpp
struct FlashStorage {
    void init();

    // Save current engine state to flash.
    // Serializes: all Number/Data/Callable cells with source text,
    // all output source expressions, timing state.
    void save(const sig::SignalEngine& engine);

    // Load saved state into engine. Returns true if state was found.
    // After loading, caller must recompile all outputs.
    bool load(sig::SignalEngine& engine);

    // Check if flash has a saved state (without loading).
    bool has_saved_state() const;

    // Erase saved state.
    void erase();
};
```

**Contracts:**
- `save()` is called from the cold path (in response to `(memory-save)`). It may take several milliseconds. Outputs continue running from the live pool during save. Before writing flash, the serial buffer must be drained and a `"save-in-progress"` status sent to the editor so it knows to expect a brief pause.
- `load()` is called once during `Firmware::init()`, before the tick loop starts. On load, the CRC32 checksum in the metadata header is validated; `load()` returns `false` on mismatch (corrupted data treated as no saved state).
- Format: each cell is serialized as `{symbol_name}\0{source_text}\0`. Output expressions are serialized as `{output_name}\0{source_text}\0`. A magic header identifies valid data.
- **No backward compatibility** with the old firmware's flash format. Upgrading to the new firmware requires a clean flash (erase). This is a deliberate break — the old format serialized `Value` objects and environment maps, which have no equivalent in the signal engine.
- Flash writes are sector-aligned (4KB on RP2040). Interrupts are disabled during the actual flash write (hardware requirement).
- `set` variables (no source text) are NOT persisted — only `define`d values with source expressions.
- **Flash wear lifetime:** At typical usage (a few saves per session, ~10 sessions/week), the RP2040's 100K erase-cycle flash endurance gives ~6 years of life. No wear-leveling is needed yet.

### 2.4 I2CNetwork (Optional)

**Responsibility:** Send and receive ModuLisp expressions to/from other uSEQ modules over I2C.

```cpp
struct I2CNetwork {
    void init();

    // Send a code string to another module.
    void send_to(uint8_t address, const char* code, uint32_t length);

    // Check for incoming messages. Returns true if one is ready.
    bool has_incoming();

    // Read incoming message into buffer.
    void read_incoming(char* code, uint32_t max_len);
};
```

**Contracts:**
- `send_to()` is called from the cold path (in response to `(send-to addr expr)`).
- Incoming messages are processed in the tick loop, same as serial commands: `engine.eval_cold(code)`.
- I2C is optional — compiled out when `ENABLE_I2C_NETWORKING` is not defined.
- **Timeout/error handling:** I2C transactions must have a bounded timeout (e.g., 1ms). On timeout or NACK, `send_to()` returns an error status — it never blocks the tick loop. Repeated failures to a given address should be rate-limited (e.g., back off for N ticks) to avoid starving the main loop.

### 2.5 DSPEngine (Optional, Core 1)

**Responsibility:** Audio-rate synthesis running on RP2040 core 1. Independent from the signal engine.

```cpp
struct DSPEngine {
    void init();          // called from setup1()
    void tick();          // called from loop1()

    // Command queue (lock-free, core 0 → core 1)
    void enqueue_command(const DSPCommand& cmd);
};
```

**Contracts:**
- DSPEngine runs on core 1. It never touches signal engine state.
- Communication is via lock-free queues only (RP2040 `queue_t`).
- Signal engine can enqueue commands via cold-path builtins (`dsp-create`, `dsp-connect`, etc.).
- DSPEngine writes audio samples to hardware DAC queues.

---

## 3. Firmware Struct

```cpp
struct Firmware {
    // ── Modules ────────────────────────────────────────────────────────
    sig::SignalEngine engine;
    HardwareIO io;
    SerialProtocol serial;
    FlashStorage flash;
#ifdef ENABLE_I2C_NETWORKING
    I2CNetwork i2c;
#endif
#ifdef ENABLE_DSP_ENGINE
    DSPEngine dsp;
#endif

    // ── Tick state (struct members, not stack — keeps tick() frame small)
    char   code_buffer[2048];
    double cell_snapshot[sig::MAX_CELLS];
    double output_values[sig::MAX_OUTPUTS];
    double workspace[sig::MAX_TOTAL_NODES];

    // ── Quantization (see §3.7) ───────────────────────────────────────
    struct PendingCommand {
        char     code[2048];
        uint32_t length;
        uint32_t request_id;
    };
    PendingCommand pending_commands[8];       // ring buffer
    uint8_t        pending_head = 0;
    uint8_t        pending_tail = 0;
    double         last_bar_phasor = 0.0;

    // ── Lifecycle ──────────────────────────────────────────────────────
    void init();
    void tick();
};
```

### 3.1 init()

```cpp
void Firmware::init() {
    // 1. Hardware
    io.init();
    io.set_led(LED_AMBER);  // booting
    serial.init();
    flash.init();
#ifdef ENABLE_I2C_NETWORKING
    i2c.init();
#endif

    // 2. Signal engine
    sig::GraphBuilder::init_symbols();
    engine.init_defaults();  // bpm=120, 4/4

    // 3. Restore saved state (CRC32 validated; mismatch → skip)
    if (flash.has_saved_state()) {
        if (flash.load(engine)) {
            recompile_all_outputs();
        }
        // load() returns false on checksum mismatch — start with defaults
    }

    // 4. Ready
    io.set_led(LED_GREEN);
    serial.send_ready(FIRMWARE_VERSION);  // {"type":"ready","version":"..."}

    // 5. Enable watchdog (200ms timeout)
    watchdog_enable(200, true);
}
```

### 3.2 tick()

```cpp
void Firmware::tick() {
    // 1. Time
    double t = get_system_time_seconds() + engine.state.time_offset;

    // 2. Inputs
    io.read_inputs();

    // 2b. Quantization: drain pending commands on bar boundary
    maybe_drain_pending(t);

    // 3. Serial commands (cold path)
    if (serial.has_incoming()) {
        uint32_t request_id;
        bool immediate;
        uint32_t len = serial.read_command(code_buffer, sizeof(code_buffer),
                                           &request_id, &immediate);

        if (immediate) {
            // @ prefix: execute now
            sig::EvalResult result = sig::eval_cold(code_buffer, len, engine);
            serial.send_eval_response(request_id, result);
        } else {
            // Queue for next bar boundary (see §3.7 Quantization)
            enqueue_pending(code_buffer, len, request_id);
        }
    }

#ifdef ENABLE_I2C_NETWORKING
    // 3b. I2C commands (same cold path)
    if (i2c.has_incoming()) {
        char i2c_buf[512];
        uint32_t len = i2c.read_incoming(i2c_buf, sizeof(i2c_buf));
        sig::eval_cold(i2c_buf, len, engine);
    }
#endif

    // 4. Execute signal graph (hot path)
    if (engine.state.is_playing) {
        engine.cells.snapshot_values(cell_snapshot, sig::MAX_CELLS);

        sig::ExecutionContext ctx;
        ctx.t             = t;
        ctx.cell_values   = cell_snapshot;
        ctx.hw_inputs     = io.inputs;
        ctx.data_pool     = engine.cells.data_pool;
        ctx.data_offsets  = engine.cells.data_offsets;
        ctx.data_lengths  = engine.cells.data_lengths;
        ctx.prev_outputs  = engine.pool.prev_output_values;
        ctx.output_values = output_values;
        ctx.workspace     = workspace;

        sig::execute_all_outputs(engine.pool, ctx);

        // 5. Write to hardware
        memcpy(io.outputs, output_values, sizeof(double) * sig::MAX_OUTPUTS);
        io.write_outputs();
        io.update_leds();

        // 6. Copy current → prev for cross-output refs
        memcpy(engine.pool.prev_output_values, output_values,
               sizeof(double) * sig::MAX_OUTPUTS);
    } else {
        // Paused: hold LKG values on hardware
        io.write_outputs();  // outputs[] unchanged from last tick
    }

    // 7. Opportunistic streaming
    serial.send_stream_data(output_values, io.num_continuous_outs + io.num_binary_outs,
                            stream_config);

    // 8. Watchdog kick (see §3.5)
    watchdog_update();
}
```

**Notes:**
- `code_buffer[2048]`, `cell_snapshot[512]`, `output_values[42]`, and `workspace[1024]` are `Firmware` struct members (in `.bss`), not stack variables. This keeps tick()'s stack frame small.
- `snapshot_values` could be optimized with a high-water mark to avoid copying unused cells. Deferred as a future optimization.
- The `memcpy` from `output_values` to `io.outputs` could be eliminated by aliasing `io.outputs` to `output_values`. Deferred as a future optimization.

### 3.3 Memory Layout (RP2040)

```
Component                        Size        Location
─────────────────────────────────────────────────────────
SignalEngine
  CellStore.cells[512]           8 KB        .bss
  CellStore.callables[512]       22 KB       .bss
    (CallableInfo = 44 B each: SymbolID uint32_t + params[8] = 32 B + 4+4+4)
  CellStore.data_pool[2048]      16 KB       .bss
  NodePool.nodes[1024]           20 KB       .bss
    (std::unique_ptr adds 8 B per node; null on firmware, present on desktop)
  NodePool.cse_hashes[2048]      8 KB        .bss
  NodePool.cse_indices[2048]     4 KB        .bss
  NodePool.exec_order[1024]      2 KB        .bss
  SourceArena.data[16384]        16 KB       .bss
  OutputSlot[42]                 ~1 KB       .bss
  OutputDeps[42]                 ~11 KB      .bss
    (~260 B each: cells[64] = 256 B + count + padding)
  prev_output_values[42]         336 B       .bss
HardwareIO
  inputs[32]                     256 B       .bss
  outputs[42]                    336 B       .bss
Firmware struct members
  code_buffer[2048]              2 KB        .bss
  cell_snapshot[512]             4 KB        .bss
  output_values[42]              336 B       .bss
  workspace[1024]                8 KB        .bss
─────────────────────────────────────────────────────────
Total                            ~121 KB

RP2040 SRAM: 264 KB
Remaining for stack + serial buffers + DSP + I2C: ~143 KB
```

### 3.4 Tick Budget

Target **1 kHz** tick rate for typical patches (< 200 total nodes). Complex patches degrade gracefully — the tick rate drops but outputs never stop. There is no hard deadline; the system is soft-real-time.

**Floating-point cost:** The RP2040 has no FPU. All `double` arithmetic is soft-float, which is the dominant cost per tick. The current choice is `double` (matching the signal engine's value type). Profile before considering a `float` fast-path — the RP2350's hardware FPU will eliminate this constraint entirely.

**Heap fragmentation:** `SymbolIntern` uses `std::map` internally, which heap-allocates on the cold path (symbol registration). This is acceptable because symbol interning only happens during `eval_cold`, never on the hot path. A fixed-memory intern table is a future optimization.

### 3.5 Watchdog

The RP2040 hardware watchdog is configured with a **200 ms timeout**. It is kicked at the end of every `tick()` (step 8). If a tick takes longer than 200 ms (catastrophic bug, infinite loop in cold path), the watchdog resets the MCU. On reset, the firmware boots fresh — flash-saved state is preserved and reloaded normally.

### 3.6 Boot Sequence

```
1. Hardware init (pins, ADC, LEDs, serial)
2. LED → amber (booting)
3. Signal engine init (symbols, defaults)
4. Flash load (if saved state exists, recompile all outputs)
5. LED → green (ready)
6. Send {"type":"ready","version":"..."} on serial
7. Enter tick loop
```

On error during boot (e.g., flash load fails), the LED flashes red briefly, then continues to step 5 with default state. The module is always usable after boot.

### 3.7 Quantization

Non-immediate commands are **bar-quantized**: they take effect at the next bar boundary, allowing the performer to queue changes that land on the beat.

**Mechanism:**
- Incoming commands without the `@` (immediate) prefix are enqueued in `pending_commands[8]`, a ring buffer on the `Firmware` struct.
- At each tick, before serial processing (step 2b), `maybe_drain_pending()` checks if the bar phasor has wrapped (crossed from near-1.0 back to near-0.0) since the last tick.
- If the bar wrapped, all pending commands are drained: each is passed to `eval_cold()` and its eval response is sent back via serial.
- If the ring buffer is full when a new non-immediate command arrives, the oldest pending command is evicted (dropped with an error response).
- Immediate commands (`@` prefix) bypass the queue entirely and execute in step 3 as before.

---

## 4. Error Handling

### 4.1 Never Stop the Music

Every output has an LKG (last-known-good) value. Error handling at every layer preserves it:

| Error | Behavior | LKG preserved? |
|---|---|---|
| Parse error in serial command | Respond with diagnostic. No output change. | Yes |
| Compile error in output expression | Respond with diagnostic. Output keeps running LKG. | Yes |
| Cell change breaks dependent output | Recompile fails → output holds LKG. Diagnostic sent. | Yes |
| NaN/Inf during execution | Per-node guard substitutes 0.0. Output continues. | Yes |
| Node pool exhaustion | Compile error. Output keeps LKG. | Yes |
| Flash load failure | Engine starts with defaults. No crash. | N/A (boot) |
| Serial buffer overflow (stream/diag) | Message dropped. No crash. | Yes |
| Serial TX full during eval response | Block briefly until space available. Never drop. | Yes |
| Flash checksum mismatch on load | Treated as no saved state. Engine starts with defaults. | N/A (boot) |

### 4.2 Diagnostic Flow

```
User code → Tokenizer → Graph Builder → Diagnostics[]
    → eval_cold returns EvalResult with diagnostics
    → serial.send_eval_response() includes diagnostics as JSON
    → Editor renders inline annotations
```

Diagnostics are compile-time only. The executor (hot path) never produces diagnostics — it guards NaN/Inf silently.

---

## 5. Persistence Format

### 5.1 Flash Layout

```
Flash offset (from end of firmware):
┌────────────────────────────────────────┐
│ Sector N-1: Metadata (4 KB)            │
│   Magic: "uSEQ\0" (5 bytes)           │
│   Version: uint16_t (format version)   │
│   Cell count: uint16_t                 │
│   Output count: uint16_t              │
│   Data size: uint32_t (total bytes)    │
│   Checksum: uint32_t (CRC32)           │
│   Reserved: 4064 bytes                 │
├────────────────────────────────────────┤
│ Sector N-2...: Cell + Output data      │
│   For each cell:                       │
│     Symbol name (null-terminated)      │
│     Cell kind (1 byte)                 │
│     Value (8 bytes, for Number)        │
│     Source text (null-terminated,       │
│       for Callable/Expression cells)   │
│   For each output with source:         │
│     Output name (null-terminated)      │
│     Source text (null-terminated)       │
└────────────────────────────────────────┘
```

### 5.2 What is persisted

- All `define`d cells (Number with source, Data with values, Callable with body)
- All output expressions (source text)
- Timing state (bpm, time signature)
- Module ID (for I2C addressing)

### 5.3 What is NOT persisted

- `set` variables (no source expression)
- Runtime state (current time, prev_output_values)
- Hardware input values
- Diagnostic history

---

## 6. Hardware Variant System

Hardware differences are resolved at **compile time** via preprocessor flags. No runtime variant detection, no vtables, no unused code in the binary.

### 6.1 Variant definitions

| Variant | Outs (C/B) | Inputs | Controls | Flash | Special |
|---|---|---|---|---|---|
| MUSICTHING | 4C + 2B | 2 CV, 2 gate | 3 knobs, 1 switch | 16 MB | Inverted outputs, SPI DAC |
| USEQHARDWARE_0_2 | 2C + 4B | 2 CV, 2 gate | Rotary encoder | 8 MB | |
| USEQHARDWARE_1_0 | 3C + 3B | 2 CV, 2 gate | — | 8 MB | PDM output |
| MINIMAL | 2C + 2B | — | — | 2 MB | Testing only |

### 6.2 How it works

```
platformio.ini          → defines -DMUSICTHING (or variant flag)
configure.h             → maps variant flag to feature flags
pinmap.h                → maps variant to pin numbers + output counts
HardwareIO::init()      → #ifdef-gated pin configuration
IOManager internals     → #ifdef-gated read/write implementations
```

The Firmware struct, SignalEngine, SerialProtocol, and FlashStorage are **variant-agnostic**. Only HardwareIO (and its internal IOManager) contain variant-specific code.

---

## 7. File Structure

```
uSEQ/src/
├── firmware/
│   ├── firmware.h              // Firmware struct + tick loop
│   ├── firmware.cpp            // init() + tick() implementation
│   ├── hardware_io.h           // HardwareIO struct
│   ├── hardware_io.cpp         // Pin setup, read/write, LEDs
│   ├── serial_protocol.h       // SerialProtocol struct
│   ├── serial_protocol.cpp     // JSON framing, command parsing
│   ├── flash_storage.h         // FlashStorage struct
│   ├── flash_storage.cpp       // Serialize/deserialize to flash
│   ├── i2c_network.h           // I2CNetwork struct (optional)
│   ├── i2c_network.cpp         // I2C host/client (optional)
│   └── dsp_engine.h/cpp        // DSPEngine (optional, core 1)
├── signal_engine/              // (existing, unchanged)
│   ├── types.h, token.h/cpp, cell_store.h/cpp, node_pool.h/cpp
│   ├── graph_builder.h/cpp, executor.h/cpp, cold_eval.h/cpp
│   ├── diagnostics.h/cpp, symbols.def, signal_engine.h
├── uSEQ/                       // (existing hardware abstractions)
│   ├── configure.h             // Feature flags
│   ├── pinmap.h                // Pin definitions
│   ├── io_manager.h/cpp        // Reused by HardwareIO
│   ├── output_manager.h/cpp    // Reused by HardwareIO
│   └── ResponsiveAnalogRead.*  // Analog filtering library
├── utils/                      // (existing, unchanged)
│   ├── string.h, json_builder.h, log.h, etc.
└── pch.h                       // Precompiled header

uSEQ/uSEQ.ino                  // Arduino entry point (simplified)
```

### 7.1 Arduino entry point (simplified)

```cpp
#include "src/firmware/firmware.h"

static Firmware firmware;

void setup()  { Serial.begin(115200); firmware.init(); }  // init() sends ready signal
void loop()   { firmware.tick(); }                        // tick() kicks watchdog

#ifdef ENABLE_DSP_ENGINE
void setup1() { firmware.dsp.init(); }
void loop1()  { firmware.dsp.tick(); }
#endif
```

---

## 8. Migration Strategy

### Phase 1: Scaffold firmware modules

Create the `firmware/` directory with header files defining the module interfaces. Stub implementations that compile but don't do anything. Wire into Meson and PlatformIO build systems. Verify empty build succeeds for all variants.

### Phase 2: HardwareIO

Extract IOManager's read/write logic into HardwareIO. The existing IOManager code is solid — this is mostly a wrapping exercise. Test on desktop (mock I/O), then on hardware.

### Phase 3: SerialProtocol

Extract the JSON protocol code from `uSEQ.cpp` into SerialProtocol. The existing protocol code handles hello, ping, eval, stream-config — it just needs to be decoupled from the uSEQ class.

### Phase 4: Firmware.tick()

Wire the tick loop: HardwareIO → SignalEngine → HardwareIO. This is the critical integration. Test with the golden suite first (desktop), then on hardware.

### Phase 5: FlashStorage

Rewrite persistence for the signal engine's CellStore/SourceArena format. Test save/load round-trip. Verify backward compatibility or document the break.

### Phase 6: I2C + DSP

Wire optional modules. These are lower priority and can follow the same extract-and-wrap pattern.

### Phase 7: Delete old firmware

Remove `uSEQ.h/cpp`, `uSEQ_update.cpp`, `uSEQ_io.cpp`, `uSEQ_api.cpp`, `uSEQ_flash.cpp`, `uSEQ_i2c.cpp`. The old 8,100-line firmware is replaced by ~2,000 lines of clean, modular code.

---

## 9. Testing Strategy

### 9.1 Desktop testing (Meson)

The firmware modules are testable on desktop without hardware:

- **HardwareIO**: mock `IIo` adapter (already exists in old codebase)
- **SerialProtocol**: feed test JSON strings, verify parsed commands and responses
- **FlashStorage**: mock `IStorage` adapter, test save/load round-trip
- **Firmware.tick()**: compose all mocks, verify the full tick cycle

### 9.2 Hardware testing

- **Golden test via probe**: the signal_engine_probe already validates semantic correctness
- **Hardware-in-the-loop**: send eval commands over serial, measure output voltages with scope/multimeter
- **Flash persistence**: save state, power cycle, verify outputs resume

### 9.3 What the signal engine tests already cover

The 717 assertions in the signal engine test suite verify:
- All arithmetic, comparison, logic, math operations
- Temporal phasors (beat, bar, phrase, section) at various BPMs
- Time transforms (fast, slow, offset)
- Control flow (if, let, do, for, while)
- Domain signal functions (step, gates, euclid, interp, etc.)
- Define, defn, set, output assignment, dependency recompilation
- Error cases, fuzzy matching, node GC, batch execution

The firmware tests need to cover the INTEGRATION: does the tick loop correctly wire inputs → engine → outputs?

---

## 10. Resolved Design Decisions

These were originally open questions. Decisions are recorded here for posterity.

1. **Flash format backward compatibility.** **Decision: No backward compat.** Upgrading to the new firmware requires a clean flash erase. The old format serialized `Value` objects and environment maps which have no equivalent in the signal engine. A clean break is simpler and safer than a fragile migration path. (See §2.3.)

2. **Scheduling / quantization.** **Decision: In the tick loop.** Quantization lives in `Firmware::tick()` via a `pending_commands[8]` ring buffer. Non-immediate commands are queued and drained when the bar phasor wraps. This keeps SerialProtocol and the signal engine unaware of scheduling. (See §3.7.)

3. **DSP engine integration depth.** **Decision: `eval_cold` builtins.** DSP commands (`dsp-create`, `dsp-connect`, etc.) are implemented as signal engine cold-path builtins. They translate ModuLisp calls into `DSPCommand` structs and enqueue them to core 1. The DSP engine itself remains a dumb command consumer with no LISP knowledge.

4. **Serial output streaming.** **Decision: In `outputs[]` array, indices 16–23.** Serial outputs (`s1`–`s8`) are virtual entries in the same `outputs[]` array as physical outputs. `HardwareIO::write_outputs()` skips indices beyond the physical count. `SerialProtocol::send_stream_data()` reads from the same array. This keeps the signal engine uniform — an output is an output regardless of destination.

5. **Time source.** **Decision: Standalone utility in `utils/time.h`.** A simple `get_system_time_seconds()` function wraps the platform timer (`micros() / 1e6` on RP2040, `std::chrono` on desktop). Called by `Firmware::tick()` at step 1. Neither Firmware nor HardwareIO owns the clock — it is a pure utility.
