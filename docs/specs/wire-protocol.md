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
firmware version is the floor declared in `useq-perform/docs/STABLE_CORE.md`.
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

3.1 **Discrimination from the first byte.**

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
| `0x01` | `INPUT_SET` (reserved, v2+) | editor → device | `[count:u16-LE][(slot_index:u16-LE, value:f64-LE) × count]` | 4 + 10·count bytes |

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
   unsolicited JSON messages (§5.5, §5.6, §5.9).

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
  field. Format is freeform (the reference editor uses `req-N`); receivers
  MUST treat it as opaque and echo it verbatim.
- Every **response** (reply to a request) MUST have `"type":"response"`,
  the echoed `"requestId"`, and a `"success":bool` field.
- Every **unsolicited** message (e.g. `ready`, `log`, standalone
  `diagnostics`) MUST have its own `type` value and MUST NOT have a
  `requestId`.

The full request → response → unsolicited message catalog follows.

### 5.1 `hello` (editor → device, request)

Sent by the editor immediately on port open. Retried per §4.2.

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
hello response.

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
(30 Hz). Editors MAY narrow this if they need to reduce bandwidth.

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

Evaluate a ModuLisp expression. Always immediate; the wire carries no
queued/immediate flag.

```json
{"type":"eval","code":"(a1 0.5)","requestId":"req-4"}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | yes | Always `"eval"`. |
| `code` | string | yes | The ModuLisp source to evaluate. Multi-form bodies are permitted. |
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

**Quantisation.** The wire-level eval is always immediate. If the user
wants their code to run at a musical boundary (next bar / next beat /
absolute time), the editor holds the code locally and sends it as an
ordinary immediate eval at the boundary, OR the user wraps their code in
an in-language quantising form (e.g. a future `(at-next-bar …)`
builtin — see [firmware.md §6.3](firmware.md)). The protocol takes no
position; all timing semantics live in the language or the editor.

### 5.8 `set-live-inputs` (editor → device, request)

Write to one or more `live-edit` slots in a single atomic message. See
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

---

## 6. Binary Stream Frames

6.1 **Output stream frame** (`type 0x00`, device → editor):

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
(default 30 Hz). Devices MAY emit fewer frames than the cap when a
channel's value has not changed (implementation-defined). Devices MUST
NOT emit faster than the cap.

6.4 **Drop policy.** Stream frames are **opportunistic** — the device
drops them if its TX buffer is full ([failure-model.md](failure-model.md)).
Visualisation must tolerate gaps.

6.5 **Reserved binary input frame** (`type 0x01`, editor → device):
**not implemented in v1**. The byte layout is reserved here for future
high-rate batched live-edit slot writes:

```
[0x1F] [0x01] [count: u16-LE] [(slot_index: u16-LE, value: f64-LE) × count]
  0      1        2..3              4..(4 + 10·count - 1)
```

Total: 4 + 10·count bytes.

Using this frame requires a slot-registration step where the editor
queries the current slot table after each successful eval to obtain a
stable id → integer-index mapping; the query mechanism is **TBD** and
will be specified alongside the first implementation. Until then, all
live-edit slot writes use the JSON `set-live-inputs` request (§5.8).

Devices receiving an `INPUT_SET` frame in v1 MUST advance past it (per
§3.2 / §3.4) without effect.

---

## 7. Limits and Constants

| Constant | Value | Notes |
|----------|-------|-------|
| Baud rate | 115,200 | USB CDC; not negotiable (1200 is the BOOTSEL trigger, not a runtime mode). |
| Heartbeat interval | 60 s | Editor-driven. |
| Heartbeat timeout | 10 s | Editor warns user on miss; does not forcibly disconnect. |
| Default stream rate | 30 Hz | Editor's `stream-config` initial value. |
| Max JSON message length | 2048 bytes | Firmware RX buffer cap. Editors SHOULD stay comfortably below. Oversized messages MAY be truncated. |
| Hello retry attempts | 8 | Per §4.2. |
| Hello attempt timeout | ≈ 700 ms | Per §4.2. |
| Live-edit slot cap | 256 | Firmware compile-time cap; see [live-edit.md §5.1](live-edit.md). |

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
- **`exec` field on eval requests.** Removed — wire carries no
  queued/immediate flag.
- **`pending_commands` ring buffer in `firmware::Firmware`.** Dead code;
  may be removed.
- **`TEXT (0x20)` and `MSG_TO_EDITOR (0x64)` type bytes.** Replaced by
  `{type:"log",...}` JSON envelopes.
- **Inbound 10-byte stream frame** (`[0x1F][channel][value]` editor →
  device). Replaced semantically by `set-live-inputs` (§5.8); reserved
  binary INPUT_SET frame (§6.5) covers the future high-rate case.
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
- Drop `exec: "immediate"` from `JsonEvalRequest`.
- Implement `set-live-inputs` request (when live-edit ships).

### 10.4 Firmware changes implied by this spec

- Change `ready` frame field name from `"fw"` to `"version"`.
- Stop emitting the `0x65` JSON type-byte prefix; emit bare `{ … }\n`.
- Stop emitting `TEXT (0x20)` and `MSG_TO_EDITOR (0x64)` type bytes;
  migrate to `{type:"log",level,text}` envelopes.
- Implement `set-live-inputs` handler (when live-edit ships).
- Remove the `pending_commands` ring buffer in `firmware::Firmware`.

---

## 11. Open / Deferred

11.1 **Slot-registration mechanism for binary `INPUT_SET` (§6.5).** The
query that returns the post-eval slot table (id → integer index) is
unspecified pending the first implementation.

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

11.5 **In-language quantising builtin.** Whether the firmware grows a
`(at-next-bar …)` (or similar) form to allow user-authored quantisation
without editor-side hold is a language-spec question (see
[firmware.md §6.3](firmware.md)) — no protocol implication either way.
