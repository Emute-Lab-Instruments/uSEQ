# Wire Protocol

> Spec: the byte-level and message-level contract over USB CDC serial between
> a uSEQ module and a host editor. Counterpart to [MAIN.md](MAIN.md) and
> [firmware.md](firmware.md). This is the **single source of truth** for
> what crosses the wire — where any other doc, comment, or implementation
> disagrees with this file, this file wins and they are bugs.
>
> See also: [live-edit.md](live-edit.md) for the slot-write semantics that
> ride on top of `set-live-inputs`; [diagnostics.md](diagnostics.md) for the
> diagnostic shape; [failure-model.md](failure-model.md) for the
> must-deliver vs. opportunistic guarantees.
>
> Editor-side counterpart: `useq-perform/docs/specs/transport.md` (transport
> state machine, clock policy). Editor-side reference implementation:
> `useq-perform/src/transport/`.
>
> Firmware implementation map (non-normative): request handling and dispatch
> live in `uSEQ/src/firmware/serial_protocol.cpp`; protocol state and JSON
> response/log helpers live in `uSEQ/src/utils/log.cpp`; lightweight JSON
> construction lives in `uSEQ/src/utils/json_builder.h`; output routing,
> including serial stream frame emission, lives under `uSEQ/src/uSEQ/`.

### Source Files

Firmware side:

- `uSEQ/src/firmware/serial_protocol.{h,cpp}` — `SerialProtocol`: RX ring buffer, JSON message extraction, `dispatch_message()`, `handle_hello()`, `handle_ping()`, `handle_stream_config()`, `handle_set_live_inputs()`, `send_eval_response()`, `send_stream_data()`, `send_diagnostics()`, `send_log()`, `send_ready()`. Dispatches `debug` messages to the devtools subsystem.
- `uSEQ/src/devtools/devtools.{h,cpp}` — `debug` message handler: capability negotiation, channel configuration, query dispatch, streaming telemetry. Compile-gated behind `USEQ_DEVTOOLS`.
- `uSEQ/src/firmware/firmware.{h,cpp}` — `Firmware::tick()` (drains serial, dispatches eval, emits stream frames)
- `uSEQ/src/utils/serial_message.h` — wire-level constants: start marker (`0x1F`), message type bytes (`STREAM`, `INPUT_SET`), rate limits
- `uSEQ/src/utils/json_builder.h` — `JsonBuilder`: lightweight fluent JSON construction (no external library)
- `uSEQ/src/utils/log.{h,cpp}` — `Protocol` namespace: JSON mode toggle, request ID tracking, response helpers
- `uSEQ/src/firmware/flash_storage.{h,cpp}` — persistence surface referenced by boot sequence

WASM parity:

- `wasm/wasm_wrapper.cpp` — same JSON shapes as ABI calls (`useq_eval`, `useq_last_diagnostics`, etc.)

Tests:

- `test/firmware/test_wire_protocol_contract.cpp` — contract tests encoding the spec section by section
- `test/firmware/test_firmware_e2e.cpp` — end-to-end tests including serial protocol interactions

---

## 1. Frame

1.1 The protocol is **JSON-only for structured traffic** with a single binary
escape for high-rate output stream frames. There is no plain-text legacy
mode, no negotiated fallback, no `@`-prefix immediate-eval marker, and no
queued/immediate flag on the wire (per [firmware.md §6.2](firmware.md)).
The host (the *editor*, in app terminology) initiates all conversations
because Web Serial only enumerates host-side: the device has no signal
that a host is now listening.

1.2 The protocol is **versioned with the firmware**. The minimum supported
firmware version is the floor declared in `useq-perform/docs/specs/bootstrap.md`
and `useq-perform/docs/specs/MAIN.md` §4 (currently `1.2.0`).
Firmware below that floor is not supported by current editors; bringing it
in-spec requires a new firmware build, not a protocol fallback.

1.3 The protocol is **symmetric** in framing: editor → device and device →
editor share the same on-wire shape. Reserved/forward-compatible binary
frame types (§3.4) exist in both directions.

---

## 2. Physical Layer

| Parameter | Value |
|-----------|-------|
| Interface | USB CDC (virtual serial port) |
| Baud rate | 115,200 |
| Data bits | 8 |
| Stop bits | 1 |
| Parity | None |
| Flow control | None |

The baud rate `1200` is **not** a serial mode; opening the port at 1200 baud
and closing it immediately is the RP2040 BOOTSEL convention for entering
the DFU bootloader. The editor uses this for firmware updates. The device
itself does not handle 1200 baud as a runtime mode.

---

## 3. Wire-Level Framing

3.1 **Discrimination from the first byte.** (See `uSEQ/src/utils/serial_message.h` — start marker 0x1F, message type byte constants; `uSEQ/src/firmware/serial_protocol.cpp` — try_extract_message, byte-level discrimination.)

| First byte | Frame kind |
|-----------|-----------|
| `0x1F` (`31`, ASCII Unit Separator) | Binary frame (see §3.2). Second byte selects the binary subtype. |
| `{` | JSON message (see §3.3). Continues until terminator. |
| anything else | Garbage / out-of-sync; receiver advances one byte and re-discriminates. |

3.2 **Binary frame layout.** All binary frames begin with `[0x1F][type]`.
The type byte selects a fixed-shape payload. Binary frames are **not**
terminated by a delimiter — their length is determined by the type byte.

| Type byte | Name | Direction | Payload shape | Total length |
|-----------|------|-----------|---------------|--------------|
| `0x00` | `STREAM` | device → editor | `[channel:u8][value:f64-LE]` | 11 bytes |
| `0x01` | `INPUT_SET` (**implemented**) | editor → device | `[count:u16-LE][(slot_index:u16-LE, value:f64-LE) × count]` | 4 + 10·count bytes |

Type bytes outside this table are reserved. Receivers MUST advance one
byte (i.e. discard the leading `0x1F`) on encountering an unknown type
byte, then re-discriminate.

3.3 **JSON message layout.** A JSON message is a single JSON object (one
`{ … }` pair) followed by a line terminator. Implementations MUST emit
`\n` (LF) as terminator. Implementations MUST accept either `\n` or
`\r\n`, and MUST ignore intervening blank lines. There is **no** `0x1F`
marker or type byte preceding the `{`.

3.4 **Forward compatibility.** New binary type bytes will be added as
needed; new JSON `type` values will be added as needed. Receivers MUST
ignore unknown JSON `type` values (logging them at debug level is
acceptable; raising an error is not).

3.5 **No legacy types.** The legacy type bytes `0x20 TEXT` and `0x64
MSG_TO_EDITOR` are removed. Anything that was previously framed text from
the device is now a JSON `{type:"log",...}` envelope (§5.6). Anything
that was previously raw text from the editor (the `(useq-version)` probe,
the `@`-prefix, plain LISP code) is removed entirely.

---

## 4. Connection Lifecycle

### 4.1 Boot (device side)

On boot completion, the device emits **one** unsolicited `ready` frame
(§5.5) and then enters its tick loop. The frame may or may not be
delivered to a listening host — the device has no way to know if a host
is currently attached. Hosts MUST NOT depend on receiving the `ready`
frame.

### 4.2 Connect (editor side)

When the editor opens the port (whether via user gesture or
auto-reconnect to a saved port):

1. Editor opens the port at 115,200 baud.
2. Editor sends a `hello` request (§5.1) immediately.
3. Editor starts a retry timer (~700 ms per attempt, ≤ 8 attempts).
4. While waiting:
   - If the device replies with a `hello` response (§5.2): handshake
     complete. Editor proceeds to §4.3.
   - If the editor receives an unsolicited `ready` frame: editor
     immediately re-sends `hello` (a `ready` indicates the device just
     finished booting, possibly mid-handshake).
   - If the retry timer fires without either of the above: editor
     re-sends `hello` and increments attempt count.
5. If all 8 attempts time out, editor surfaces a connection error.

### 4.3 Steady state

Once the handshake is complete, the editor:

1. Sends a `stream-config` request (§5.3) using the io config from the
   `hello` response.
2. Starts a heartbeat: a `ping` request (§5.4) every 60 s, with a 10 s
   timeout. On heartbeat failure, the editor warns the user but does not
   forcibly disconnect.
3. Begins sending `eval` requests (§5.7) and `set-live-inputs` requests
   (§5.8) as the user works.
4. Begins consuming binary `STREAM` frames (§3.2 / §6) and any
   unsolicited JSON messages (§5.5, §5.6, §5.9, §5.10).

### 4.4 Disconnect

The Web Serial API fires a disconnect event when the USB device is
removed. On disconnect, the editor:

- Rejects all pending requests with `"Connection reset"`.
- Stops the heartbeat.
- Resets all protocol state.
- Posts a console notice.

The device has no observable disconnect event; it simply stops receiving
bytes. Outstanding TX writes that have not been drained are lost.

---

## 5. JSON Messages

5.1 **Common rules.**

- Every JSON message MUST be a single object on one line, terminated per
  §3.3.
- Every JSON message MUST have a top-level `"type"` string field.
- Every **request** (editor → device) MUST have a `"requestId"` string
  field unless that message's section explicitly defines a fire-and-forget
  form. Format is freeform (the reference editor uses `req-N`); receivers
  MUST treat it as opaque and echo it verbatim.
- Every **response** (reply to a request) MUST have `"type":"response"`,
  the echoed `"requestId"`, and a `"success":bool` field.
- Every **unsolicited** message (e.g. `ready`, `log`, standalone
  `diagnostics`) MUST have its own `type` value and MUST NOT have a
  `requestId`.

The full request → response → unsolicited message catalog follows.

### 5.1 `hello` (editor → device, request)

Sent by the editor immediately on port open. Retried per §4.2. (See `uSEQ/src/firmware/serial_protocol.cpp` — handle_hello parses hello request, sends hello response with config.)

```json
{"type":"hello","client":"editor","version":"1.2.0","requestId":"req-1"}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | yes | Always `"hello"`. |
| `client` | string | yes | Identifier for the host implementation. The reference editor sends `"editor"`. Devices MAY log this but MUST NOT vary behaviour by client. |
| `version` | string | yes | Editor's protocol version (semver). |
| `requestId` | string | yes | Per §5.1. |

### 5.2 `hello` response (device → editor)

```json
{
  "type": "response",
  "requestId": "req-1",
  "success": true,
  "mode": "json",
  "fw": "1.2.0",
  "config": {
    "inputs":  [{"index": 1, "name": "ssin1"}, ...],
    "outputs": [{"index": 1, "name": "time"},
                {"index": 2, "name": "s1"}, ...]
  }
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | yes | Always `"response"`. |
| `requestId` | string | yes | Echoed from the request. |
| `success` | bool | yes | Always `true` if a hello response is sent at all. (Failure mode is "no response"; editor times out and retries.) |
| `mode` | string | yes | Always `"json"`. Reserved field; future protocol variants might use other values. |
| `fw` | string | yes | Device firmware version. Used by the editor for upgrade checks. |
| `config` | object | yes | I/O configuration; see below. |

**`config.inputs`** is an array of `{index, name}` describing externally
addressable hardware-input slots. Indices are 1-based. Names match the
ModuLisp builtins (`ssin1`, `ssin2`, …). Currently informational —
externally-driven inputs flow via `set-live-inputs` (§5.8), not via input
channels.

**`config.outputs`** is an array of `{index, name}` describing streamed
outputs. Indices are 1-based. The reserved name `"time"` denotes the
canonical time channel; other names follow the s-output convention
(`s1`, `s2`, …). Stream frames (§6) carry these indices in the channel
byte.

The hello response is the **single source of truth** for the firmware
version and the I/O configuration, replacing all prior probing
mechanisms.

### 5.3 `stream-config` (editor → device, request)

Sent by the editor after a successful hello, using the io config from the
hello response. (See `uSEQ/src/firmware/serial_protocol.cpp` — handle_stream_config parses channel configs, updates `stream_rate_limit_us` and the `stream_channels[]` array of `StreamChannel{enabled, source, source_idx, on_change_only}`.)

```json
{
  "type": "stream-config",
  "maxRateHz": 30,
  "channels": [
    {"id": 1, "name": "time", "direction": "output", "enabled": true,  "maxRateHz": 30},
    {"id": 2, "name": "s1",   "direction": "output", "enabled": true,  "maxRateHz": 30},
    ...
  ],
  "requestId": "req-2"
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `maxRateHz` | number | yes | Global maximum stream emission rate. Device MUST NOT emit stream frames faster than this rate, summed across channels. |
| `channels` | array | yes | Per-channel configuration. |
| `channels[].id` | number | yes | Channel index (matches `hello.config` index space). |
| `channels[].name` | string | yes | Channel name (informational; mirrors `hello.config`). |
| `channels[].direction` | string | yes | One of `"input"` / `"output"`. |
| `channels[].enabled` | bool | yes | If `false`, device MUST NOT emit stream frames for this channel. |
| `channels[].maxRateHz` | number | optional | Per-channel rate cap. Reserved for future use; current firmware ignores it. The global `maxRateHz` is authoritative. |
| `requestId` | string | yes | Per §5.1. |

**Response:** standard ack `{type:"response",requestId,success:true}`.

The reference editor enables all advertised channels at the default rate
(100 Hz). Editors MAY narrow this if they need to reduce bandwidth.

### 5.4 `ping` (editor → device, request)

```json
{"type":"ping","requestId":"req-3"}
```

**Response:** standard ack `{type:"response",requestId,success:true}`.

The editor sends a ping every **60 seconds** while the connection is
established. If no response arrives within **10 seconds**, the editor
warns the user but does not forcibly disconnect.

The ping carries no payload other than the type and request id. Devices
MUST respond promptly without doing meaningful work.

### 5.5 `ready` (device → editor, unsolicited)

Sent **once** by the device on boot completion (post-flash-load,
post-watchdog-arm). Advisory only — see §4.1.

```json
{"type":"ready","version":"1.2.0"}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | yes | Always `"ready"`. |
| `version` | string | yes | Device firmware version (matches the `fw` field of the hello response). |

The field name on this frame is `"version"` — not `"fw"` — matching the
historical naming in [firmware.md §3.1.6](firmware.md). The hello
response keeps `"fw"` for backwards compatibility with prior editors;
both name the same value.

Editors that see a `ready` after their handshake is already complete MUST
treat it as a no-op (the device may have rebooted; the editor SHOULD log
this but does not need to re-handshake unless it observes other failure).

### 5.6 `log` (device → editor, unsolicited)

Replaces the legacy `TEXT (0x20)` and `MSG_TO_EDITOR (0x64)` framed-text
type bytes. Used by builtins such as `(println …)` and
`(message-editor …)` to emit unsolicited text.

```json
{"type":"log","level":"info","text":"Hello from uSEQ"}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | yes | Always `"log"`. |
| `level` | string | yes | One of `"debug"`, `"info"`, `"notice"`, `"warn"`, `"error"`. |
| `text` | string | yes | The message text. May be multi-line (newlines escaped per JSON). |

`"notice"` is the level used by `(message-editor …)` — the editor SHOULD
display these prominently rather than mixing them into routine console
output.

### 5.7 `eval` (editor → device, request)

Evaluate a ModuLisp expression. (See `uSEQ/src/firmware/serial_protocol.cpp` — dispatch_message returns true for eval type, caller reads code; `uSEQ/src/firmware/firmware.cpp` — Firmware::tick calls eval_cold with the code buffer, then send_eval_response; `uSEQ/src/signal_engine/cold_eval.cpp` — eval_cold, the actual evaluation.) Defaults to immediate; the optional
`quant` flag opts the request into the runtime's quantised-eval queue.

```json
{"type":"eval","code":"(a1 0.5)","requestId":"req-4"}
{"type":"eval","code":"(a1 0.5)","quant":true,"requestId":"req-5"}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | yes | Always `"eval"`. |
| `code` | string | yes | The ModuLisp source to evaluate. Multi-form bodies are permitted. |
| `quant` | bool | optional | When `true`, the device queues the form and evaluates it on the next wrap of the global quant phasor (see Quantisation note below). When `false` or absent, the form evaluates immediately. |
| `requestId` | string | yes | Per §5.1. |

Maximum `code` length: 2048 bytes (firmware RX buffer cap). Editors SHOULD
keep evals comfortably under this; oversized inputs MAY be truncated by
the device.

**Response shape:**

```json
{
  "type": "response",
  "requestId": "req-4",
  "success": true,
  "text": "0.5",
  "console": "",
  "diagnostics": [
    {"severity":"warning","category":"…","start":3,"end":7,"message":"…","suggestion":"…"}
  ],
  "meta": null
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | yes | Always `"response"`. |
| `requestId` | string | yes | Echoed from the request. |
| `success` | bool | yes | `false` iff the eval produced an error diagnostic that aborted execution. Compile-time warnings do not flip success to false. |
| `text` | string | optional | The eval's return value rendered as a string. Empty string if the result is unprintable or `nil`. |
| `console` | string | optional | Captured stdout from `print`/`println` invocations during the eval, accumulated as a single string. Empty string if nothing was printed. |
| `diagnostics` | array | optional | Diagnostics produced during compile or evaluation; see [diagnostics.md](diagnostics.md). Per §5.9, these are also delivered embedded in this response (not just as standalone frames). |
| `meta` | object \| null | optional | Structured side-channel data (e.g. transport state changes resulting from the eval). When non-null, the editor dispatches this to interested subscribers. May be `null` (or absent) for evals that produce no side-effect metadata. |

Per [failure-model.md](failure-model.md) and [firmware.md §7.2](firmware.md),
eval responses are **must-deliver**: the device blocks briefly until TX
buffer space exists rather than dropping. Stream frames and standalone
diagnostics are opportunistic and may be dropped under load.

**Quantisation.** Eval requests are immediate by default. Setting
`quant: true` opts the request into the device's quantised-eval queue:
the device buffers the form and evaluates it on the next wrap of the
**global quant phasor** — a single runtime-side phasor that defaults to
`bar` and is changed via the ModuLisp builtin
`(set-quant-phasor expr)`. The wire only conveys the on/off bit; the
period itself is part of runtime state, not protocol state. See
[firmware.md §6](firmware.md) for queue ordering, drain semantics, and
phasor-update behaviour.

The eval response (§5.7 response shape) is sent **after the deferred
evaluation completes**, not at submission time, so a quantised eval's
`requestId` round-trip latency is bounded by the current quant period.
Editors SHOULD surface a "pending" affordance until the response
arrives.

### 5.8 `set-live-inputs` (editor → device, request)

Write to one or more `live-edit` slots in a single atomic message. (See `uSEQ/src/firmware/serial_protocol.cpp` — handle_set_live_inputs parses slot JSON, applies values.) See
[live-edit.md §5.3](live-edit.md) for the runtime semantics; this section
specifies only the wire shape.

```json
{
  "type":"set-live-inputs",
  "slots":{"knob1":0.5,"toggle1":true,"mode1":"forward"},
  "requestId":"req-5"
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | yes | Always `"set-live-inputs"`. |
| `slots` | object | yes | Map of `:id` (string) → new value. Numeric values are JSON numbers; boolean slots accept `true`/`false`; keyword slots accept the keyword name as a JSON string (without the leading colon). |
| `requestId` | string | optional | Editors MAY include a request id for tracing. If present, device responds with `{type:"response",requestId,success:true,"applied":N}` where `N` is the count of slots successfully applied (matching the `useq_set_live_inputs` ABI return — see [live-edit.md §5.10](live-edit.md)). If absent, the message is fire-and-forget; the device applies what it can and emits no response. |

Per [live-edit.md §5.4](live-edit.md), unknown ids are silently dropped
(the host may be racing the most recent eval). Type or range violations
emit a runtime warning via the standalone diagnostics frame (§5.9) but
do not flip `success` for the request.

The shape of `slots` exactly mirrors the WASM ABI
`useq_set_live_inputs(json_str)` argument (see [live-edit.md §5.10](live-edit.md))
so editors and tests can use one payload-builder for both transports.

### 5.9 `diagnostics` (device → editor, unsolicited)

Standalone diagnostics frame — sent for runtime-occurring diagnostics
that do not correspond to a single eval response (e.g. live-edit
slot-write type violations, fallback-to-LKG events, runtime non-finite
warnings).

```json
{"type":"diagnostics","diagnostics":[{"severity":"warning",...}]}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | yes | Always `"diagnostics"`. |
| `diagnostics` | array | yes | Same shape as the embedded form in eval responses (§5.7). See [diagnostics.md](diagnostics.md). |

These frames are **opportunistic** — the device MAY drop them if TX is
backed up. Eval responses are still the must-deliver path for
eval-correlated diagnostics; the standalone frame is for everything else.

### 5.10 `hw-input` (device → editor, unsolicited)

Pushed by the device when a physical control (button, toggle, encoder
click, gate input) changes state. The device emits one frame per edge
transition — rising and falling for momentary controls, state-change for
toggles.

See [hardware-bindings.md](../../useq-perform/docs/specs/hardware-bindings.md)
for how the editor routes these events to bound expressions.

```json
{"type":"hw-input","kind":"button","id":"sw1","state":"pressed","ts":123456}
{"type":"hw-input","kind":"toggle","id":"sw2","state":true}
{"type":"hw-input","kind":"encoder","id":"swr","state":"pressed"}
{"type":"hw-input","kind":"gate","id":"in1","state":"released"}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | yes | Always `"hw-input"`. |
| `kind` | string | yes | One of `"button"`, `"toggle"`, `"encoder"`, `"gate"`. Identifies the control type — see below. |
| `id` | string | yes | Physical-control keyword **without** the leading colon. E.g. `"sw1"`, `"sw2"`, `"swr"`, `"in1"`. Matches the `<input-id>` keywords in [hardware-bindings.md §2.4](../../useq-perform/docs/specs/hardware-bindings.md). |
| `state` | string \| bool | yes | For `button`/`encoder`/`gate`: `"pressed"` or `"released"` (string). For `toggle`: `true` (on) or `false` (off) (boolean). |
| `ts` | number | optional | Device-side timestamp in milliseconds since boot. Absent when the firmware does not yet emit timestamps. Editors MUST NOT depend on its presence. |

**Kind semantics:**

- `"button"` — momentary switch. Two edges: `"pressed"` then `"released"`.
  The device debounces; the editor receives clean edges.
- `"toggle"` — sticky toggle switch. One event per flip, carrying the
  post-flip boolean state (`true` = on, `false` = off).
- `"encoder"` — encoder click (the push-button on a rotary encoder).
  Treated as momentary (`"pressed"` / `"released"`). Encoder *rotation*
  is a continuous signal (`(rot)`), not an event — it has no `hw-input`
  frame.
- `"gate"` — external gate/trigger input (`:in1`, `:in2`). Carries
  `"pressed"` (gate high) and `"released"` (gate low).

**Delivery.** `hw-input` frames are **must-deliver**: hardware input
events are infrequent (human-rate, not audio-rate) and losing one would
silently break the performer's control flow. The device blocks briefly
if TX is full, matching eval-response delivery semantics.

**Forward compatibility.** New `kind` values may be added in future
firmware. Editors MUST ignore unknown kinds (logging at debug level is
acceptable).

### 5.11 `calibrate-begin` (editor → device, request)

Enter calibration takeover mode for one analog output. The device freezes
all other outputs at their current LKG values and takes exclusive control
of the named output. See
[calibration.md](../../docs/specs/calibration.md) for the editor-side UX
spec.

```json
{"type":"calibrate-begin","output":"a1","requestId":"req-20"}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | yes | Always `"calibrate-begin"`. |
| `output` | string | yes | Analog output id (e.g. `"a1"`, `"a2"`). Must match an output exposed by the connected variant. |
| `requestId` | string | yes | Per §5.1. |

**Response:**

```json
{
  "type": "response",
  "requestId": "req-20",
  "success": true,
  "status": {"kind": "uncalibrated"}
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | yes | Always `"response"`. |
| `requestId` | string | yes | Echoed from the request. |
| `success` | bool | yes | `false` if the output is unknown, already in takeover for a different output, or the variant does not support calibration. |
| `error` | string | on failure | Human-readable rejection reason. |
| `status` | object | on success | Per-output calibration status. One of `{"kind":"uncalibrated"}`, `{"kind":"calibrated","date":"<ISO>"}`, or `{"kind":"partial","savedOctaves":[0,1]}`. |

Only one output may be in takeover at a time. Sending `calibrate-begin`
for a second output while a takeover is active MUST fail with
`success: false`.

### 5.12 `calibrate-set-target` (editor → device, request)

Drive the takeover output to a specific target voltage. Sent on takeover
entry (to set the initial 0V target) and when advancing to the next
octave step.

```json
{"type":"calibrate-set-target","output":"a1","voltage":1.0,"requestId":"req-21"}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | yes | Always `"calibrate-set-target"`. |
| `output` | string | yes | Must match the output currently in takeover. |
| `voltage` | number | yes | Target voltage (typically 0, 1, 2, 3, or 4). The firmware drives the DAC to produce this voltage (before any calibration correction). |
| `requestId` | string | yes | Per §5.1. |

**Response:** standard ack `{type:"response",requestId,success:true}`.
On failure (e.g. output mismatch, no active takeover), `success: false`
with `error`.

### 5.13 `calibrate-adjust` (editor → device, request)

Apply a fine correction delta (in cents) to the current output. The
firmware accumulates deltas internally; the editor maintains a local
mirror for display (see
[calibration.md §4.5](../../docs/specs/calibration.md)).

```json
{"type":"calibrate-adjust","output":"a1","delta":0.5,"requestId":"req-22"}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | yes | Always `"calibrate-adjust"`. |
| `output` | string | yes | Must match the output currently in takeover. |
| `delta` | number | yes | Signed correction in cents. Positive = sharper / higher voltage; negative = flatter / lower voltage. |
| `requestId` | string | yes | Per §5.1. |

**Response:**

```json
{"type":"response","requestId":"req-22","success":true}
```

If the firmware clamps the accumulated offset (e.g. hardware limit), the
response includes `clampedOffset`:

```json
{"type":"response","requestId":"req-22","success":true,"clampedOffset":48.2}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `clampedOffset` | number | optional | The firmware's authoritative cumulative offset in cents after applying the delta. Present only when the value was clamped. The editor MUST snap its UI to this value. |

### 5.14 `calibrate-save-point` (editor → device, request)

Stage the current calibration point for a specific octave. The value is
held in RAM; it is not flushed to flash until `calibrate-end` with
`commit: true` (§5.15).

```json
{"type":"calibrate-save-point","output":"a1","octave":2,"requestId":"req-23"}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | yes | Always `"calibrate-save-point"`. |
| `output` | string | yes | Must match the output currently in takeover. |
| `octave` | number | yes | Octave index: `0` (0V), `1` (1V), `2` (2V), `3` (3V), or `4` (4V). |
| `requestId` | string | yes | Per §5.1. |

**Response:** standard ack `{type:"response",requestId,success:true}`.
On failure (e.g. flash write error), `success: false` with `error`.

### 5.15 `calibrate-end` (editor → device, request)

Exit calibration takeover and resume normal operation.

```json
{"type":"calibrate-end","commit":true,"requestId":"req-24"}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | yes | Always `"calibrate-end"`. |
| `commit` | bool | yes | `true` — flush all staged calibration points to flash and apply them. `false` — discard staged points and revert the output to its pre-takeover calibration state. |
| `requestId` | string | yes | Per §5.1. |

**Response:** standard ack `{type:"response",requestId,success:true}`.

After a successful `calibrate-end`, the device resumes normal output
behaviour. Stream frames resume if they were suspended during takeover.

### 5.16 `debug` (editor → device, request)

Dev-build-only telemetry and introspection. Gated behind `USEQ_DEVTOOLS` at compile time. Release-build devices ignore this message type per §3.4.

```json
{"type":"debug","action":"capabilities","requestId":"req-30"}
{"type":"debug","action":"configure","channels":{"tick":"stream"},"requestId":"req-31"}
{"type":"debug","action":"query","channel":"state","requestId":"req-32"}
{"type":"debug","action":"status","requestId":"req-33"}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | yes | Always `"debug"`. |
| `action` | string | yes | One of `"capabilities"`, `"configure"`, `"query"`, `"status"`. |
| `requestId` | string | yes | Per §5.1. |

Additional fields depend on `action`. See [devtools.md](devtools.md) for
the full channel catalog, response shapes, and streaming semantics.

**Delivery.** Unsolicited `debug` frames (streaming telemetry) are
**opportunistic** — dropped if TX is full, matching stream-frame
semantics (§3.2).

### 5.17 Calibration response conventions

All `calibrate-*` requests are **must-deliver** (the device blocks
briefly on TX backpressure rather than dropping). Response shapes follow
the standard `{type:"response",requestId,success}` envelope (§5.1).

Additional fields that MAY appear on calibration responses:

| Field | Type | Description |
|-------|------|-------------|
| `error` | string | Human-readable rejection reason (present when `success: false`). |
| `clampedOffset` | number | Firmware's authoritative cumulative offset in cents. Present on `calibrate-adjust` responses when the value was clamped. |
| `status` | object | Per-output calibration status. Present on `calibrate-begin` success responses. |

The pre-takeover calibration state is sacred: the firmware holds the
prior calibration in memory across the takeover and only commits new
points to flash on `calibrate-end { commit: true }`. Individual
`calibrate-save-point` messages stage values; they do not flush
per-step. This protects the user from data loss on disconnect / abort.
See [calibration.md §6.3](../../docs/specs/calibration.md).

### 5.18 `set-failure-mode` (editor → device, request)

Configure the runtime non-finite failure policy
([failure-model.md §3.2](failure-model.md)). (See
`uSEQ/src/firmware/serial_protocol.cpp` — `handle_set_failure_mode`.)

```json
{"type":"set-failure-mode","mode":"lkg","requestId":"req-9"}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | yes | Always `"set-failure-mode"`. |
| `mode` | string | yes | `"lkg"` (default — non-finite at an output root falls back to the last-known-good value and raises a runtime diagnostic) or `"zero"` (legacy — every non-finite node result is clamped to `0.0`, no fallback, no diagnostic). |
| `requestId` | string | optional | Standard response correlation. |

The device responds with the standard `{type:"response",success}`
envelope; `success: false` (with an explanatory `text`) for any `mode`
other than `"lkg"` / `"zero"`. The mode is engine-global (not
per-output), defaults to `"lkg"` at boot, and is not persisted — editors
that expose the setting re-send it on connect. The WASM runtime
equivalent is `useq_set_failure_mode(0|1)` (0 = lkg, 1 = zero), keeping
both transports behind one editor setting.

---

## 6. Binary Stream Frames

6.1 **Output stream frame** (`type 0x00`, device → editor). (See `uSEQ/src/firmware/serial_protocol.cpp` — send_stream_data writes 0x1F 0x00 channel value bytes; `uSEQ/src/utils/serial_message.h` — SerialMsg constants for frame layout.)

```
[0x1F] [0x00] [channel: u8] [value: 8 bytes, IEEE 754 double, little-endian]
  0      1        2                3..10
```

Total: **11 bytes**, fixed.

| Field | Width | Description |
|-------|-------|-------------|
| `0x1F` | 1 byte | Marker. |
| `0x00` | 1 byte | STREAM type byte. |
| `channel` | 1 byte | 1-based channel index, matching `hello.config.outputs[].index`. |
| `value` | 8 bytes | IEEE 754 double, little-endian. NaN and ±Inf are valid (and reserved for future health-state signalling). |

6.2 **Channel space.** Channels are 1-based and dense: 1, 2, 3, …, N where
N = `hello.config.outputs.length`. The reserved channel `1` carries
`time` by convention; channels `2..N` carry s-outputs (`s1..s(N-1)`).
This is **not** the same numbering as the firmware's internal output-slot
index (the 24-slot a/d/s space defined in [firmware.md §4.4](firmware.md));
the wire uses a stream-only channel namespace advertised in `hello`.

6.3 **Rate.** Limited by the most recent `stream-config.maxRateHz`
(default 100 Hz from the editor). The firmware's built-in default rate
cap is 100 Hz (`serial_message_rate_limit` in `serial_message.h`),
which applies before `stream-config` arrives. Devices MAY emit fewer
frames than the cap when a channel's value has not changed
(implementation-defined). Devices MUST NOT emit faster than the cap.

6.3.1 **Default streaming (before `stream-config`).** On entering JSON
mode (after hello), the device streams **only channel 1 (time)** at the
firmware default rate (100 Hz). All other channels are disabled until
the editor sends `stream-config` to enable them. This ensures the wire
is not saturated during the handshake window and that the editor
receives a time reference immediately for clock synchronization.

6.4 **Drop policy.** Stream frames are **opportunistic** — the device
drops them if its TX buffer is full ([failure-model.md](failure-model.md)).
Visualisation must tolerate gaps.

6.5 **Binary input frame** (`type 0x01`, `INPUT_SET`, editor → device):
**implemented**. This is the low-latency / high-throughput fast-path for
manual live control (knob/slider/joystick scrub, MIDI-learn CC) — it
carries high-rate value updates to live-edit slots that were already
**declared** via `set-live-inputs` (§5.8). Byte layout:

```
[0x1F] [0x01] [count: u16-LE] [(slot_index: u16-LE, value: f64-LE) × count]
  0      1        2..3              4..(4 + 10·count - 1)
```

Total: 4 + 10·count bytes. Multi-byte fields are little-endian. `value`
is an IEEE-754 `binary64` (f64-LE). A `count` of 0 is a valid (empty)
frame.

**`slot_index` semantics.** `slot_index` is a **direct zero-based index
into the device's live-slot table** — i.e. the same `live_slots[]` array
(`engine->pool.live_slots[0 .. live_slot_count)`) that `set-live-inputs`
(§5.8) writes after resolving its string `:id` keys. Slot indices are
assigned in **declaration order** by the most recent successful eval
(the order in which `live-edit` slots are first allocated during graph
build). A `set-live-inputs` write to `:knob1` and an `INPUT_SET` write to
that slot's index therefore target the **identical** slot and apply the
**identical** clamp/variant coercion (numeric clamp to `[min,max]`,
boolean → 0/1, keyword → option index; non-finite values rejected,
slot retains its prior value).

**Obtaining the index.** The editor learns each slot's index from the
`liveSlots` array returned by `get-state` (state-sync.md §2) and/or from
the `set-live-inputs` declaration order: entry *i* in the device's
live-slot table has `slot_index = i`. Because eval re-allocates the
table, the editor MUST re-sync its `id → slot_index` map after every
successful eval; until it has, it should fall back to JSON
`set-live-inputs` (which is keyed by string `:id` and is robust to
re-ordering).

**Bounds / garbage.** A `slot_index ≥ live_slot_count` is silently
dropped (consistent with §5.4 "unknown ids are dropped" and the
project's "garbage: skip" philosophy); other entries in the same frame
still apply. The frame is fire-and-forget — the device emits **no**
response or `applied` count. Range/type coercion is applied silently (no
per-frame diagnostics on the hot path).

Devices parse `INPUT_SET` allocation-free directly in the serial RX
demux (`SerialProtocol::try_consume_binary_frame` →
`NodePool::set_live_slot_value_by_index`). An incomplete frame (fewer
than `4 + 10·count` bytes buffered) is left in the RX buffer until the
remaining bytes arrive — **but only if the frame can ever fit**. A frame
whose declared length `4 + 10·count` exceeds the RX buffer capacity
(2048 bytes), or whose `count` exceeds the live-slot cap
(`MAX_LIVE_SLOTS`), can never be completed by waiting; the device MUST
treat it as a protocol error, drop the `0x1F` marker, and resync to the
next message start rather than parking on it forever. (A single
corrupted `count` byte must not be able to wedge the RX path.)

**RX resync (line/frame too long).** The RX ring buffer is fixed at 2048
bytes. If a single logical message — a JSON line with no newline, or a
malformed binary frame — fills the ring so that no further bytes can be
read, the device MUST NOT stall permanently. It drops the overflowing
message (advancing the read head to the next `{` or `0x1F` marker, or
clearing the ring if none) and emits a `{"type":"log","level":"error"}`
diagnostic ("message too long") so the editor learns its send was
dropped. Serial input remains live for subsequent messages.

> **NOTE (editor manual-control path).** `set-live-inputs` (§5.8) and the
> `0x01` `INPUT_SET` binary frame are **complementary**, not alternatives:
> `set-live-inputs` **declares** a live input (binds a string `:id` to a
> slot, with min/max/variant/options) and is the robust, self-describing
> path; the §6.5 `INPUT_SET` frame carries **high-rate value updates** to
> those already-declared slots, addressed by their integer table index,
> for the lowest-latency manual-control path (knob/slider/joystick scrub,
> MIDI-learn CC). An earlier editor build emitted a malformed 10-byte
> frame (`[0x1F][channel:u8][value:f64]`, with **no** type byte and no
> count); that shape is **not** conforming — the conforming binary write
> is the type-tagged, length-prefixed `INPUT_SET` frame above. Editors
> that have not yet synced their `id → slot_index` map after an eval MUST
> use `set-live-inputs` until the map is rebuilt.

---

## 7. Limits and Constants

| Constant | Value | Notes |
|----------|-------|-------|
| Baud rate | 115,200 | USB CDC; not negotiable (1200 is the BOOTSEL trigger, not a runtime mode). |
| Heartbeat interval | 60 s | Editor-driven. |
| Heartbeat timeout | 10 s | Editor warns user on miss; does not forcibly disconnect. |
| Default stream rate | 100 Hz | Editor's `stream-config` initial value (`DEFAULT_STREAM_MAX_RATE_HZ`). |
| Max JSON message length | 2048 bytes | Firmware RX buffer cap. Editors SHOULD stay comfortably below. Oversized messages MAY be truncated. |
| Hello retry attempts | 8 | Per §4.2. |
| Hello attempt timeout | ≈ 700 ms | Per §4.2. |
| Live-edit slot cap | 16 (firmware) / 256 (WASM) | Compile-time `MAX_LIVE_SLOTS` (`signal_engine/types.h`); see [live-edit.md §5.1](live-edit.md). `INPUT_SET` `slot_index` is bounds-checked against the live runtime `live_slot_count`, not this static cap. |
| `INPUT_SET` `slot_index` | u16 (0…65535) | Wire range; values `≥ live_slot_count` are silently dropped (§6.5). |
| `INPUT_SET` `count` | u16 (0…65535) | Entries per frame; frame is `4 + 10·count` bytes. `count = 0` is a valid no-op. |

---

## 8. Worked Examples

### 8.1 Cold start, healthy device

```
device boots:
  → 0x1F 0x65 ... NO. Device emits:
    {"type":"ready","version":"1.2.0"}\n

editor opens port:
  ← {"type":"hello","client":"editor","version":"1.2.0","requestId":"req-1"}\n

device replies:
  → {"type":"response","requestId":"req-1","success":true,"mode":"json",
     "fw":"1.2.0","config":{"inputs":[...],"outputs":[...]}}\n

editor sends stream-config:
  ← {"type":"stream-config","maxRateHz":30,"channels":[...],"requestId":"req-2"}\n

device acks:
  → {"type":"response","requestId":"req-2","success":true}\n

steady state — device emits stream frames:
  → 0x1F 0x00 0x01 [time-bytes]
  → 0x1F 0x00 0x02 [s1-bytes]
  ...
```

### 8.2 Boot race (editor connects mid-boot)

```
editor opens port (device still booting):
  ← {"type":"hello","client":"editor","version":"1.2.0","requestId":"req-1"}\n

(no response — device's RX buffer received the hello but hasn't reached
 the tick loop yet)

device finishes boot:
  → {"type":"ready","version":"1.2.0"}\n

editor sees ready, retries hello immediately:
  ← {"type":"hello","client":"editor","version":"1.2.0","requestId":"req-2"}\n

device tick loop reads RX, processes the latest hello:
  → {"type":"response","requestId":"req-2","success":true,...}\n

editor proceeds to stream-config + steady state.
```

### 8.3 Eval with diagnostics

```
editor:
  ← {"type":"eval","code":"(a1 (+ 1 nope))","requestId":"req-7"}\n

device:
  → {"type":"response","requestId":"req-7","success":false,
     "text":"","console":"",
     "diagnostics":[
       {"severity":"error","category":"unbound","start":7,"end":11,
        "message":"unknown name 'nope'",
        "suggestion":"did you mean 'note'?"}
     ],
     "meta":null}\n
```

### 8.4 Live-edit slot updates

```
editor (on every UI tick while user drags slider):
  ← {"type":"set-live-inputs","slots":{"knob1":0.42},"requestId":"req-12"}\n

device (if requestId present):
  → {"type":"response","requestId":"req-12","success":true,"applied":1}\n
```

### 8.5 Hardware button press (with binding)

```
user presses sw1 on the module:
  → {"type":"hw-input","kind":"button","id":"sw1","state":"pressed","ts":48230}\n

editor looks up binding for (on-press :sw1 …), dispatches eval:
  ← {"type":"eval","code":"(mute-toggle)","requestId":"req-14"}\n

device responds:
  → {"type":"response","requestId":"req-14","success":true,"text":"1","console":"","meta":null}\n

user releases sw1:
  → {"type":"hw-input","kind":"button","id":"sw1","state":"released","ts":48412}\n

editor looks up binding for (on-release :sw1 …) — none registered, no eval sent.
```

### 8.6 Toggle switch flip

```
user flips toggle sw2 to "on":
  → {"type":"hw-input","kind":"toggle","id":"sw2","state":true}\n

editor dispatches (on-toggle :sw2 (lambda (state) …)) with state = true.
```

### 8.7 CV calibration session (one output, two octaves then abort)

```
editor enters calibration for a1:
  ← {"type":"calibrate-begin","output":"a1","requestId":"req-20"}\n

device acks with status:
  → {"type":"response","requestId":"req-20","success":true,
     "status":{"kind":"uncalibrated"}}\n

editor sets initial target to 0V:
  ← {"type":"calibrate-set-target","output":"a1","voltage":0,"requestId":"req-21"}\n
  → {"type":"response","requestId":"req-21","success":true}\n

user adjusts slider (delta = +1.3 cents):
  ← {"type":"calibrate-adjust","output":"a1","delta":1.3,"requestId":"req-22"}\n
  → {"type":"response","requestId":"req-22","success":true}\n

user saves octave 0:
  ← {"type":"calibrate-save-point","output":"a1","octave":0,"requestId":"req-23"}\n
  → {"type":"response","requestId":"req-23","success":true}\n

editor advances to 1V:
  ← {"type":"calibrate-set-target","output":"a1","voltage":1,"requestId":"req-24"}\n
  → {"type":"response","requestId":"req-24","success":true}\n

user adjusts, firmware clamps:
  ← {"type":"calibrate-adjust","output":"a1","delta":52,"requestId":"req-25"}\n
  → {"type":"response","requestId":"req-25","success":true,"clampedOffset":50}\n
  (editor snaps slider to 50 cents)

user aborts — discard partial calibration:
  ← {"type":"calibrate-end","commit":false,"requestId":"req-26"}\n
  → {"type":"response","requestId":"req-26","success":true}\n

device reverts a1 to pre-takeover state, resumes normal operation.
```

---

## 9. Implementation Notes

This section is non-normative — pointers for implementers, not part of the
contract.

### 9.1 Device side

- `firmware::SerialProtocol` in `uSEQ/src/firmware/serial_protocol.{h,cpp}` is
  the canonical implementation surface.
- `firmware::Firmware::tick()` drains RX once per tick (non-blocking) and
  emits responses and stream frames synchronously within the tick.
- TX backpressure: eval responses use `Serial.availableForWrite()` and
  block briefly; stream frames and standalone diagnostics check the same
  flag and drop if it returns 0.
- The legacy `Protocol::` namespace in `uSEQ/src/utils/log.cpp` and the
  legacy `uSEQ/src/uSEQ_lisp.h` interpreter declarations are out of scope
  for this spec; they belong to the old interpreter path being phased
  out.

### 9.2 Editor side

- The reference implementation lives in `useq-perform/src/transport/`.
  - `connector.ts`: port lifecycle, Web Serial events.
  - `json-protocol.ts`: hello/eval/ping/set-live-inputs lifecycle.
  - `stream-parser.ts`: byte-level discrimination per §3.
  - `types.ts`: TypeScript shape definitions.
- `useq-perform/src/runtime/jsonProtocol.ts` is the schema/type layer
  shared between the wire driver and the WASM in-memory transport.

### 9.3 WASM parity

- The same JSON shapes that flow over the wire flow as ABI calls into the
  WASM build (`useq_eval`, `useq_set_live_inputs`, etc.). One shape, two
  transports.
- See [live-edit.md §5.10](live-edit.md) for the live-edit ABI mirror.

---

## 10. Migration Notes (from prior protocol versions)

This section is non-normative — context for upgrading editor and firmware
implementations against this spec.

### 10.1 Removed surfaces

- **Legacy text mode.** Editor no longer sends raw LISP text; firmware no
  longer interprets bytes outside JSON `{ … }` and binary frames.
- **Firmware-info text probe** (`@(useq-report-firmware-info)`). Replaced
  by the `hello` response's `fw` field.
- **`@`-prefix immediate-eval marker.** Wire is immediate-only.
- **`exec` field on eval requests.** The legacy string-enum `exec` field
  was removed. The current spec exposes a single boolean `quant` flag on
  the eval request (§5.7) — opting an eval into the runtime's
  quantised-eval queue, gated by the global quant phasor. This is a
  different surface from the legacy `exec` (no per-request "scheduled at
  absolute time" mode, no editor-side hold).
- **`pending_commands` ring buffer in `firmware::Firmware`.** The legacy
  ring buffer (drained on bar-phasor wrap, populated by `@`-prefix text)
  is gone. The new firmware-side quantised-eval queue (§5.7,
  [firmware.md §6](firmware.md)) is its replacement: drained on
  global-quant-phasor wrap, populated by JSON eval requests carrying
  `quant: true`.
- **`TEXT (0x20)` and `MSG_TO_EDITOR (0x64)` type bytes.** Replaced by
  `{type:"log",...}` JSON envelopes.
- **Inbound 10-byte stream frame** (`[0x1F][channel][value]` editor →
  device, no type byte). Malformed/non-conforming. Replaced by
  `set-live-inputs` (§5.8) for slot **declaration** and the
  type-tagged, length-prefixed binary `INPUT_SET` frame (§6.5,
  **implemented**) for high-rate value updates.
- **`0x65` JSON type-byte prefix.** Both directions now emit bare
  `{ … }\n` with no `0x1F`/`0x65` framing prefix.

### 10.2 Renamed / reshaped

- **Ready frame field.** Was `"fw"` in current firmware code, now
  `"version"` (matches [firmware.md §3.1.6](firmware.md)). The hello
  response keeps `"fw"`; both name the same value.
- **Diagnostics field on eval response.** Newly required: editors that
  ignored it now MUST parse it and route to inline annotations.
- **`set-live-inputs`.** Newly defined. Editors implementing live-edit
  use this; firmware implementing live-edit handles this.
- **Standalone `diagnostics` frame.** Newly defined for runtime-occurring
  diagnostics outside an eval.

### 10.3 Editor changes implied by this spec

- Drop the firmware-info text-probe path; send `hello` immediately on
  port open.
- Re-send `hello` on receiving an unsolicited `ready`.
- Drop the `0x65` JSON type-byte expectation when receiving (or accept
  both for a transition window).
- Add handlers for `{type:"ready"}`, `{type:"log"}`, and standalone
  `{type:"diagnostics"}`.
- Parse `diagnostics` from eval responses (currently read only from the
  WASM exports).
- Drop `exec: "immediate"` from `JsonEvalRequest` (legacy field).
- Send `quant: true` on the eval request when the user invokes the
  Quantised eval keybinding; omit (or send `false`) for Immediate eval.
- Surface a "pending" affordance for in-flight quantised evals — the
  response arrives only after the deferred evaluation completes.
- Implement `set-live-inputs` request (when live-edit ships).

### 10.4 Firmware changes implied by this spec

- Change `ready` frame field name from `"fw"` to `"version"`.
- Stop emitting the `0x65` JSON type-byte prefix; emit bare `{ … }\n`.
- Stop emitting `TEXT (0x20)` and `MSG_TO_EDITOR (0x64)` type bytes;
  migrate to `{type:"log",level,text}` envelopes.
- Implement `set-live-inputs` handler (when live-edit ships).
- Implement the binary `INPUT_SET` (§6.5) RX demux (done:
  `SerialProtocol::try_consume_binary_frame` →
  `NodePool::set_live_slot_value_by_index`) for the low-latency
  manual-control fast-path.
- Remove the legacy `pending_commands` ring buffer in
  `firmware::Firmware`. Add the new quantised-eval queue, drained on
  global-quant-phasor wrap, populated by eval requests with
  `quant: true` (§5.7, [firmware.md §6](firmware.md)).
- Implement the `(set-quant-phasor expr)` ModuLisp builtin (default
  phasor: `bar`).

---

## 11. Open / Deferred

11.1 **Slot-registration sync for binary `INPUT_SET` (§6.5).** The
firmware side is **implemented**: `slot_index` is a direct index into the
live-slot table in declaration order, and `get-state`'s `liveSlots` array
(state-sync.md §2) exposes that table for the editor to build its
`id → slot_index` map. Remaining open question (editor-side): whether to
add a lightweight dedicated `slot-table` query frame rather than reusing
the heavier `get-state` snapshot, and how the editor should atomically
swap its index map on eval to avoid a window where a stale index targets
the wrong slot (mitigation today: fall back to `set-live-inputs` until the
map is re-synced).

11.2 **Per-channel `maxRateHz` in `stream-config` (§5.3).** Currently
ignored by firmware. Either drop the field or implement honoring it
when a real use case appears.

11.3 **Structured error codes.** v1 surfaces protocol-level errors as
`{success:false, text:"…"}` with a human-readable message. A future
revision could add a structured `errorCode` enum for programmatic
handling.

11.4 **Wire-protocol version separate from firmware version.** Currently
the protocol version is implicit in the firmware semver. If the wire
shape needs to evolve independently, add a `protocol` field to the
`hello` response and `ready` frame.

(11.5 was the open `(at-next-bar …)` question — resolved in v1 by the
`quant: true` eval-request flag (§5.7) plus the `(set-quant-phasor expr)`
ModuLisp builtin; see [firmware.md §6](firmware.md).)
