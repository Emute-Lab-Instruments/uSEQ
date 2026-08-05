# DevTools

> Spec: compile-time-gated instrumentation and runtime-controllable
> telemetry for the uSEQ firmware. Provides structured observability
> into the tick loop, signal engine, eval pipeline, and resource usage
> via a JSON wire-protocol extension. Designed for agent-driven
> debugging: an AI agent connects, discovers available channels,
> enables what it needs, and queries or streams structured data.
>
> Counterpart to [wire-protocol.md](wire-protocol.md) (transport layer),
> [firmware.md](firmware.md) (tick loop), [diagnostics.md](diagnostics.md)
> (compiler diagnostics — a separate system).
>
> The devtools system is **measurement-image-only**. It compiles to nothing in
> production firmware. It is distinct from the existing `USEQ_DEBUG` printf
> system, which remains available for human-readable debug logging.

### Source Files

- `uSEQ/src/devtools/devtools.h` — public `dt::` namespace API; inline no-op stubs when `USEQ_DEVTOOLS` is not defined
- `uSEQ/src/devtools/devtools.cpp` — state struct, collection logic, serialization, protocol handler
- `uSEQ/src/firmware/firmware.cpp` — tick-loop instrumentation call sites (`dt::tick_begin`, `dt::mark`, `dt::tick_end`)
- `uSEQ/src/firmware/serial_protocol.cpp` — `debug` message dispatch to `dt::handle_debug_message()`
- `uSEQ/src/signal_engine/cold_eval.cpp` — eval-event instrumentation (`dt::event`)

---

## 1. Frame

1.1 The devtools system provides **structured, machine-readable telemetry**
from the firmware to a connected host (editor or autonomous agent). It
is not a logging system — it produces typed data on named channels,
controllable at runtime.

1.2 The system has three layers:

- **Instrumentation** — `dt::` namespace calls at key points in the
  firmware source. These are the only visible footprint in non-devtools
  code. When `USEQ_DEVTOOLS` is not defined, every call compiles to
  nothing.
- **Collection** — internal fixed-size buffers that accumulate timing,
  events, counters, and gauges. Collection runs unconditionally when
  compiled in; the cost is writing to struct fields (no allocation, no
  I/O).
- **Emission** — serialization of collected data to JSON, gated by
  per-channel runtime configuration. Emission is driven by protocol
  messages (`debug` request type) and optional periodic streaming.

1.3 The devtools system is **orthogonal to the diagnostics system**
([diagnostics.md](diagnostics.md)). Diagnostics are compiler output
(errors, warnings, suggestions) that flow to the user. DevTools is
system telemetry (timing, state, resources) that flows to a debugger
or agent.

1.4 The devtools system is **orthogonal to the existing `USEQ_DEBUG`
system** (`uSEQ/src/utils/log.h`). `USEQ_DEBUG` is unstructured
printf-style logging for human consumption. Both may be compiled in
simultaneously; they do not interact.

---

## 2. Compile-Time Gate

2.1 All devtools functionality is gated behind `USEQ_DEVTOOLS`. When the
macro is not defined (or defined to `0`), every `dt::` call is an empty
inline function that the compiler eliminates entirely at `-O1` or above.
No string literals, no branches, no struct fields survive.

2.2 Build configuration:

- **PlatformIO** (`platformio.ini`): `musicthing-observe` adds
  `-DUSEQ_DEVTOOLS=1` to the production `musicthing` flags. The production
  `musicthing` and capacity-reduced `minimal` images exclude it.
- **Meson** (`meson.build`): boolean option `enable_devtools`
  (default: `true` for desktop/test builds) maps to
  `-DUSEQ_DEVTOOLS=1`.
- **WASM**: not compiled in. The browser has its own devtools; firmware
  devtools are for the physical device.

2.3 The `USEQ_DEVTOOLS` gate is independent of `USEQ_DEBUG`,
`ENABLE_PROFILING`, and `ENABLE_SERIAL_DEBUG`. Those flags are legacy
and may be deprecated once devtools is stable.

---

## 3. Instrumentation API (`dt::` namespace)

3.1 The public API is a set of free functions in the `dt` namespace,
declared in `devtools.h`. When `USEQ_DEVTOOLS` is not defined, each
function is an empty `inline` body. When defined, each function is
implemented in `devtools.cpp`.

### 3.1 Tick Profiling

```cpp
namespace dt {
    void tick_begin();
    void mark(const char* phase);
    void tick_end();
}
```

- `tick_begin()` — records the tick start timestamp (`micros()`).
- `mark(phase)` — records the end of the current phase and the start of
  the named phase. The first `mark()` after `tick_begin()` ends the
  implicit first phase.
- `tick_end()` — closes the last phase, computes total tick duration,
  and updates rolling statistics.

Phase names are string literals defined at the call site. The
implementation stores pointers (not copies) — the caller must pass
string literals or static strings.

**Call sites** (in `firmware.cpp`):

```cpp
void Firmware::tick()
{
    dt::tick_begin();

    const double t = get_system_time_seconds() + engine.state.time_offset;

    dt::mark("input");
    io.read_inputs();

    dt::mark("serial");
    if (serial.has_incoming()) { /* ... */ }

    dt::mark("execute");
    // ... execute_all_outputs ...

    dt::mark("output");
    io.write_outputs();
    io.update_leds();

    dt::mark("lkg");
    // ... commit_outputs ...

    dt::mark("stream");
    serial.send_stream_data(output_values, sig::MAX_OUTPUTS);

    dt::tick_end();

    watchdog_kick();
}
```

### 3.2 Events

```cpp
namespace dt {
    void event(const char* channel, const char* message,
               const char* detail = nullptr);
    void event(const char* channel, const char* message, int detail);
}
```

Push a timestamped event into a ring buffer. Events are for
**infrequent, significant state transitions** — not per-tick data.

Channel names are freeform strings. Recommended channels:

| Channel | Typical events |
|---------|---------------|
| `"eval"` | `"begin"`, `"tokenize"`, `"compile"`, `"done"`, `"error"` |
| `"recompile"` | cell name (detail), `"done"` with count (detail int) |
| `"flash"` | `"save"`, `"load"`, `"error"` |
| `"protocol"` | `"hello"`, `"disconnect"`, `"rx_overflow"` |

**Call sites** (in `cold_eval.cpp` and `serial_protocol.cpp`):

```cpp
// cold_eval.cpp
EvalResult eval_cold(const char* source, uint32_t length, SignalEngine& engine)
{
    dt::event("eval", "begin");
    // ... tokenize ...
    // ... on error:
    dt::event("eval", "error");
    return result;
}

void on_cell_changed(SymbolID cell_id, SignalEngine& engine)
{
    dt::event("recompile", "begin");
    // ... recompile loop ...
    dt::event("recompile", "done", outputs_rebuilt);
}
```

### 3.3 Counters

```cpp
namespace dt {
    void count(const char* name);
}
```

Increment a named counter. Counters are monotonic and never reset
(except on device reboot). Useful for protocol statistics.

| Counter | Where |
|---------|-------|
| `"msg_in"` | `serial_protocol.cpp` — message received |
| `"msg_out"` | `serial_protocol.cpp` — response sent |
| `"stream_drop"` | `serial_protocol.cpp` — stream frame dropped (TX full) |
| `"rx_overflow"` | `serial_protocol.cpp` — oversized JSON line rejected and RX resynchronised |
| `"eval_count"` | `cold_eval.cpp` — eval_cold called |

### 3.4 Gauges

```cpp
namespace dt {
    void gauge(const char* name, uint32_t value);
    void gauge(const char* name, uint32_t value, uint32_t capacity);
}
```

Record a point-in-time measurement. The two-argument form is for
utilization ratios (used / capacity). Gauges are overwritten on each
call — only the most recent value is retained.

| Gauge | Where |
|-------|-------|
| `"watchdog_reboot"` | `firmware.cpp` — once at initialization |

Heap, stack, and engine-capacity measurements are produced directly by the
`resources` channel rather than registered as general-purpose gauges.

### 3.5 Streaming and Protocol Integration

```cpp
namespace dt {
    bool handle_debug_message(const char* json, size_t len,
                              void (*write_fn)(const char*, size_t));
    void emit_streaming(void (*write_fn)(const char*, size_t));
}
```

- `handle_debug_message()` — called by `SerialProtocol::dispatch_message()`
  when `type` is `"debug"`. Parses the action and responds via the
  provided write function. Returns `true` if handled.
- `emit_streaming()` — called once per tick (after `dt::tick_end()`).
  Emits data for any channels configured in `stream` mode, subject to
  rate limiting and TX backpressure. Opportunistic — no-op if nothing
  is streaming or TX is full.

---

## 4. Internal State

4.1 All devtools state lives in a single static `DevToolsState` struct in
`devtools.cpp`. All arrays are fixed-size. No dynamic allocation. Safe
for embedded static initialization.

```cpp
struct DevToolsState {
    // ── Channel configuration ──────────────────────────────────────────
    enum Channel : uint8_t {
        TICK, GRAPH, STATE, EVAL, RESOURCES, IO, PROTOCOL, CH_COUNT
    };
    enum Mode : uint8_t { OFF, POLL, STREAM, EVENTS };

    Mode modes[CH_COUNT] = {};
    uint32_t stream_interval_us = 100000;  // 10 Hz default

    // ── Tick profiling ─────────────────────────────────────────────────
    struct {
        uint32_t tick_start_us;
        uint32_t phase_starts[8];
        uint32_t phase_durations[8];
        const char* phase_names[8];
        uint8_t phase_count;
        uint32_t tick_total_us;
        uint32_t tick_count;
        // Rolling window (last N ticks)
        uint32_t min_us, max_us, sum_us;
        uint32_t window_count;
        static constexpr uint32_t WINDOW_SIZE = 100;
    } tick;

    // ── Event ring buffer ──────────────────────────────────────────────
    struct Event {
        uint32_t timestamp_us;
        const char* channel;
        const char* message;
        const char* detail_str;
        int detail_int;
        bool has_int_detail;
    };
    Event events[32];
    uint8_t event_head = 0;
    uint8_t event_count = 0;

    // ── Counters ───────────────────────────────────────────────────────
    struct Counter {
        const char* name;
        uint32_t value;
    };
    Counter counters[16];
    uint8_t counter_count = 0;

    // ── Gauges ─────────────────────────────────────────────────────────
    struct Gauge {
        const char* name;
        uint32_t value;
        uint32_t capacity;  // 0 means no capacity (plain gauge)
    };
    Gauge gauges[16];
    uint8_t gauge_count = 0;

    // ── Stream timing ──────────────────────────────────────────────────
    uint32_t last_stream_us = 0;
};
```

4.2 **Buffer limits are hard.**

| Buffer | Capacity | Overflow behaviour |
|--------|----------|-------------------|
| Tick phases | 8 per tick | Additional `mark()` calls are silently dropped |
| Events | 32 ring | Oldest event overwritten |
| Counters | 16 named | New `count()` on a full table is dropped |
| Gauges | 16 named | New `gauge()` on a full table is dropped |

These limits are generous for the expected instrumentation density. If
they prove tight, increase the constants — the total struct size is
well under 2 KB.

---

## 5. Wire Protocol Extension

5.1 DevTools adds one new JSON message type to the wire protocol:
`"debug"`. It follows the standard request/response contract
([wire-protocol.md §5.1](wire-protocol.md)). The `action` field
selects the operation.

All `debug` messages are gated behind `USEQ_DEVTOOLS` at compile time.
A production device that receives a `debug` message MUST ignore it
per the forward-compatibility rule
([wire-protocol.md §3.4](wire-protocol.md)).

### 5.1 `debug` — `capabilities` (editor → device, request)

Self-describing channel catalog. An agent's first query after `hello`.

```json
{"type":"debug","action":"capabilities","requestId":"req-10"}
```

**Response:**

```json
{
  "type": "response",
  "requestId": "req-10",
  "success": true,
  "devtools": true,
  "channels": [
    {"name":"tick",      "modes":["off","poll","stream"], "current":"off",
     "description":"Per-phase tick timing (µs)"},
    {"name":"graph",     "modes":["off","poll"],          "current":"off",
     "description":"Signal graph topology, node types, connections"},
    {"name":"state",     "modes":["off","poll"],          "current":"off",
     "description":"Cells, outputs, health, LKG values"},
    {"name":"eval",      "modes":["off","events"],        "current":"off",
     "description":"Compilation events and recompile cascades"},
    {"name":"resources", "modes":["off","poll","stream"], "current":"off",
     "description":"Heap, node pool, arena utilization"},
    {"name":"io",        "modes":["off","poll","stream"], "current":"off",
     "description":"Hardware input and output snapshot"},
    {"name":"protocol",  "modes":["off","poll","stream"], "current":"off",
     "description":"Message counters, drops, backpressure"}
  ]
}
```

If `USEQ_DEVTOOLS` is not compiled in, the device never responds to
this message (forward-compatibility: the editor sees no response, which
is distinguishable from `devtools: true`). An agent MAY detect the
absence of devtools by checking for the `devtools` field in the
`capabilities` response — or by checking the `hello` response if a
`devtools` field is added there in future.

### 5.2 `debug` — `configure` (editor → device, request)

Set channel modes. Only listed channels are changed; omitted channels
retain their current mode.

```json
{
  "type": "debug",
  "action": "configure",
  "channels": {"tick": "stream", "eval": "events", "graph": "poll"},
  "streamRateHz": 10,
  "requestId": "req-11"
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | yes | Always `"debug"`. |
| `action` | string | yes | `"configure"`. |
| `channels` | object | yes | Map of channel name → desired mode string. |
| `streamRateHz` | number | no | Global stream emission rate for all streaming channels. Default 10. Range [1, 100]. |
| `requestId` | string | yes | Per wire-protocol §5.1. |

**Response:**

```json
{
  "type": "response",
  "requestId": "req-11",
  "success": true,
  "channels": {"tick": "stream", "eval": "events", "graph": "poll"}
}
```

Invalid channel names or modes are silently ignored (the response
reflects only what was actually applied).

**Mode semantics:**

| Mode | Meaning |
|------|---------|
| `"off"` | Channel disabled. No collection overhead beyond what's always-on (tick timing, counters). |
| `"poll"` | Data available via `query` only. |
| `"stream"` | Data emitted periodically as unsolicited `debug` frames. |
| `"events"` | Events emitted as they occur (eval channel only). |

### 5.3 `debug` — `query` (editor → device, request)

On-demand snapshot of a single channel.

```json
{"type":"debug","action":"query","channel":"tick","requestId":"req-12"}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `channel` | string | yes | Channel name to query. |
| `output` | string | no | For `graph` channel: specific output name (e.g. `"d1"`). If omitted, returns all active outputs. |

**Response shapes by channel:**

#### `tick`

```json
{
  "type": "response", "requestId": "req-12", "success": true,
  "channel": "tick",
  "data": {
    "tick_count": 482910,
    "last": {
      "total_us": 847,
      "phases": {"input": 12, "serial": 45, "execute": 680, "output": 35, "lkg": 5, "stream": 70}
    },
    "window": {
      "count": 100,
      "total_us": {"min": 420, "max": 12800, "avg": 847}
    }
  }
}
```

#### `graph`

Returns the node-level topology of one or all output signal graphs.
This is the **killer feature for agent-driven debugging** — the agent
can structurally reason about the signal graph.

```json
{
  "type": "response", "requestId": "req-13", "success": true,
  "channel": "graph",
  "outputs": [
    {
      "name": "d1",
      "health": "running",
      "root": 4,
      "node_count": 5,
      "nodes": [
        {"id": 0, "op": "Const", "imm": 440.0},
        {"id": 1, "op": "RawTimeLoad"},
        {"id": 2, "op": "Mul", "a": 0, "b": 1},
        {"id": 3, "op": "Sin", "a": 2},
        {"id": 4, "op": "Scale", "a": 3, "b": 5, "c": 6}
      ],
      "exec_order": [0, 1, 2, 3, 5, 6, 4],
      "source": "(scale (sin (* 440 t)) 0.0 1.0)",
      "deps": ["freq"]
    }
  ]
}
```

Node fields:

| Field | Type | Present | Description |
|-------|------|---------|-------------|
| `id` | int | always | Node index in the pool. |
| `op` | string | always | NodeOp name (see `node_pool.h`). |
| `imm` | number | when meaningful | Immediate value (Const value, cell ID, input index, etc.). |
| `a`, `b`, `c` | int | when connected | Input node indices. Absent when `NODE_NONE`. |

#### `state`

```json
{
  "type": "response", "requestId": "req-14", "success": true,
  "channel": "state",
  "data": {
    "is_playing": true,
    "time": 142.387,
    "cells": [
      {"name": "freq", "kind": "number", "value": 440.0},
      {"name": "scale", "kind": "data", "length": 7}
    ],
    "outputs": [
      {"name": "d1", "health": "running", "lkg": 0.73},
      {"name": "a1", "health": "idle", "lkg": 0.0}
    ],
    "state_slots": [
      {"id": 0, "value": 0.5, "update_root": 12}
    ]
  }
}
```

#### `eval`

Returns target-side eval timing counters and the event ring buffer contents
(most recent first). `last_us` measures only `eval_cold`; serial parsing and
response emission are outside the interval. The simulator runner polls this
channel after each workload form and computes its workload percentiles from
the individual `last_us` samples. A query with `"output":"timing"` returns
only `last_us`, `max_us`, `count`, and `error_count`; this bounded polling
shape avoids retransmitting the event ring after every form.

```json
{
  "type": "response", "requestId": "req-15", "success": true,
  "channel": "eval",
  "data": {
    "last_us": 202,
    "max_us": 202,
    "count": 1,
    "error_count": 0,
    "events": [
      {"ts_us": 12849102, "channel": "eval", "message": "done", "detail": 202},
      {"ts_us": 12848900, "channel": "eval", "message": "begin"}
    ]
  }
}
```

#### `resources`

```json
{
  "type": "response", "requestId": "req-16", "success": true,
  "channel": "resources",
  "data": {
    "heap_free": 42800,
    "heap_min_free": 39120,
    "core0_stack_margin_intact": true,
    "core0_stack":       {"initialized": true, "used": 912, "capacity": 2048},
    "nodes":             {"used": 47, "capacity": 1024},
    "arena":             {"used": 1280, "capacity": 16384},
    "cells":             {"used": 12, "capacity": 512},
    "data_entries":      {"used": 256, "capacity": 512},
    "state_slots":       {"used": 3, "capacity": 256},
    "live_slots":        {"used": 2, "capacity": 32},
    "watchdog_reboot": 0,
    "cpu_hz": 250000000
  }
}
```

The firmware profile does not report synth declarations or controls. The
synth compiler and its artefact graph are generated-WASM capabilities and are
not linked into RP2040 firmware.

`heap_free` and `heap_min_free` are bytes. The latter is the lowest value
observed at firmware-tick and compiler sampling points. It measures aggregate
free heap, not the largest contiguous allocation, so fragmentation remains a
separate failure mode.

The core-0 stack watermark is initialized after firmware construction and is
a conservative upper bound: the unpainted initialization margin is counted as
used. `initialized` is false if the linked bounds or current stack pointer did
not permit painting; acceptance must reject that state rather than interpret a
zero high-water value as evidence. `core0_stack_margin_intact` becomes false
when execution reaches the lowest sampled byte above the SDK's 32-byte guard
region. The current DSP engine does not execute work on core 1, so core-1
runtime stack usage is not reported. The linked core-1 reserve remains part of
the ELF memory report.

#### `io`

```json
{
  "type": "response", "requestId": "req-17", "success": true,
  "channel": "io",
  "data": {
    "inputs":  {"in1": 0.0, "in2": 0.5, "ain1": 0.33, "ain2": 0.0},
    "outputs": {"a1": 0.73, "a2": 0.0, "d1": 1.0, "d2": 0.0, "s1": 0.5}
  }
}
```

#### `protocol`

```json
{
  "type": "response", "requestId": "req-18", "success": true,
  "channel": "protocol",
  "data": {
    "msg_in": 1420,
    "msg_out": 1418,
    "eval_count": 87,
    "stream_drop": 3,
    "uptime_s": 3842
  }
}
```

### 5.4 `debug` — `status` (editor → device, request)

Returns current channel configuration (same shape as `configure`
response).

```json
{"type":"debug","action":"status","requestId":"req-19"}
```

### 5.5 `debug` (device → editor, unsolicited)

Streaming data for channels in `stream` or `events` mode. Same JSON
shape as query responses but without `requestId`.

```json
{"type":"debug","channel":"tick","data":{...}}
{"type":"debug","channel":"eval","data":{"event":{...}}}
```

**Delivery.** Unsolicited `debug` frames are **opportunistic** — the
device drops them if TX is full, matching stream-frame semantics
([wire-protocol.md §3.2](wire-protocol.md)). Debug telemetry must never
starve must-deliver messages (eval responses, hw-input events).

**Rate limiting.** Streaming channels emit at most once per
`stream_interval_us` (configurable via `streamRateHz` in the
`configure` message). Default: 10 Hz (100 ms). The `events` mode emits
immediately on each event (not rate-limited), but still subject to TX
backpressure.

---

## 6. Channels

6.1 The channel set is fixed at compile time. Adding a new channel
requires a code change (new enum value in `DevToolsState::Channel`,
new entry in `capabilities` response, new query handler).

| Channel | Modes | Collection cost | Emission cost |
|---------|-------|----------------|---------------|
| `tick` | off, poll, stream | ~6 `micros()` calls per tick | JSON serialization |
| `graph` | off, poll | Zero (reads existing `NodePool`) | JSON serialization of node arrays |
| `state` | off, poll | Zero (reads existing engine state) | JSON serialization |
| `eval` | off, events | 1 ring-buffer write per event | JSON serialization per event |
| `resources` | off, poll, stream | heap sample each tick and at compiler allocation boundaries; stack-canary scan on emission | Small JSON |
| `io` | off, poll, stream | Zero (reads existing buffers) | Small JSON |
| `protocol` | off, poll, stream | Counter increments | Small JSON |

6.2 **Graph and state channels are poll-only** because serializing the
full node pool or cell store is expensive (hundreds of bytes of JSON).
Streaming these at 10 Hz would dominate the serial bandwidth. An agent
queries them when needed, not continuously.

6.3 **The tick channel is the primary streaming channel.** Its data is
small (~100 bytes JSON) and provides the real-time health signal that
lets an agent detect problems (stuck ticks, runaway execution time,
backpressure).

---

## 7. Agent Interaction Model

7.1 A debugging agent connects to the device and follows this sequence:

1. **Handshake.** Send `hello` per [wire-protocol.md §5.1](wire-protocol.md).
2. **Discover.** Send `debug:capabilities`. Parse the channel list. If
   no response (or no `devtools` field), the firmware was built without
   devtools — the agent must fall back to eval-only debugging.
3. **Configure.** Send `debug:configure` to enable desired channels.
   Typical first pass: `tick: stream`, `eval: events`.
4. **Baseline.** Send `debug:query` for `state`, `graph`, `resources`
   to establish the current system state.
5. **Observe.** Read streaming `tick` data to assess system health. Read
   `eval` events to trace compilation activity.
6. **Hypothesize.** Send `eval` requests to test theories. After each
   eval, query `graph` to verify the signal graph changed as expected.
7. **Iterate.** Adjust channel configuration as the investigation
   narrows. Query specific outputs, check resource trends, trace
   recompile cascades.

7.2 The `capabilities` response is the agent's entry point — it provides
a machine-readable menu of everything the device can report, with
human-readable descriptions. An agent that has never seen this firmware
before can orient itself from this response alone.

---

## 8. Implementation Constraints

8.1 **Fixed collection state.** All persistent devtools state is in
fixed-size static arrays. Collection calls do not allocate. Query and stream
emission use the existing `JsonBuilder` (`uSEQ/src/utils/json_builder.h`) and
the Arduino `String` type, which may allocate transient heap storage. Runtime
memory acceptance therefore uses the minimum observed during the workload and
allows explicit measurement-image overhead; the production image is assessed
separately from `musicthing-observe`.

8.2 **No hot-path impact when disabled.** When `USEQ_DEVTOOLS` is not
defined, the inline stubs produce zero instructions. When defined, the
tick profiler calls `micros()` at phase boundaries, and the resource sampler
reads heap state each tick. The core-0 stack canary is scanned only when the
resources channel is queried or emitted. The resulting timing cost is an
empirical property of the measurement image and must be reported by the
capacity/endurance run rather than assumed.

8.3 **No interference with must-deliver messages.** All devtools
emission is opportunistic. The `emit_streaming()` call checks
`can_write()` before emitting and silently drops the frame if TX is
full. Eval responses, hw-input events, and calibration messages always
take priority.

8.4 **String literals only.** Phase names, event channels, counter names,
and gauge names are `const char*` pointers to string literals. The
devtools system never copies or allocates strings for names. This means
callers MUST pass string literals (or other static-lifetime strings).

8.5 **Platform `micros()`.** Tick timing uses the platform's `micros()`
function (RP2040 native on Arduino, `std::chrono` on desktop). On WASM,
devtools is not compiled in.

---

## 9. Relationship to Existing Systems

9.1 **`USEQ_DEBUG` / `DebugLogger`** — Printf-style logging with
mute/solo filtering. Remains available for human debugging. DevTools
does not replace it; they serve different audiences.

9.2 **`ENABLE_PROFILING` / `ENABLE_SERIAL_DEBUG`** — Existing build
flags in `platformio.ini` that are currently unwired. These may be
deprecated in favour of `USEQ_DEVTOOLS` once the devtools system is
stable. Until then, they are orthogonal.

9.3 **Diagnostics** ([diagnostics.md](diagnostics.md)) — Compiler
diagnostics (errors, warnings, suggestions) are user-facing and flow
through eval responses and standalone diagnostic frames. DevTools
telemetry is developer/agent-facing and flows through the `debug`
message type. They are separate systems that happen to share the same
serial link.

---

## 10. Open Questions

10.1 **Graph node value snapshots.** Should the `graph` query include the
current workspace value for each node (the result of the most recent
tick's evaluation)? This would let an agent see not just the topology
but the actual signal values flowing through the graph. Cost: reading
from the workspace array (which exists and is cheap). The open question
is whether the workspace values are meaningful outside the tick context
(they're overwritten each tick).

10.2 **Per-node tracing.** A future extension could allow the agent to
"trace" specific node indices — recording their value into a ring buffer
across ticks. This would provide per-node signal waveforms for
debugging. The cost is one branch per node per tick plus a ring buffer
write for traced nodes. This is deferred to a future version.

10.3 **WASM devtools.** The current spec excludes WASM. If browser-side
devtools are needed, a parallel API surface (`useq_devtools_query()` etc.)
could expose the same data through WASM exports instead of serial JSON.
Deferred.

10.4 **`hello` response `devtools` field.** Currently, an agent discovers
devtools by sending `debug:capabilities` and waiting for a response.
Adding a `"devtools": true` field to the `hello` response would let
agents know immediately whether devtools are available, without an extra
round trip. Deferred to a wire-protocol revision.
