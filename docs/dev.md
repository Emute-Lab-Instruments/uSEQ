# Developer Info

## Code Structure
- `uSEQ/uSEQ.ino`: Entry point for the Arduino IDE, which just includes `src/uSEQ.h`, creates a `uSEQ` object, and calls `uSEQ::tick()` on it forever in a loop. This is where you can specify which hardware version you have, or configure other parameters such as number of ins/outs.
- `uSEQ/src/uSEQ.h`: Main uSEQ class, which inherits from `Interpreter` and adds some uSEQ-specific Lisp functionality on top.
- `uSEQ/src/uSEQ`: Various module-specific configuration and functionality, unrelated to the Lisp interpreter.
  - `uSEQ/src/uSEQ/configure.h`: `#define`s to configure various aspects of the module, e.g. number of continuous/binary ins/outs etc.
- `uSEQ/src/lisp`: All classes related to the base Lisp interpreter functionality - `Parser, Value, Environment, Interpreter`.
  - `uSEQ/src/lisp/configure.h`: `#define`s for various aspects of the Lisp interpreter's configuration.
  - `uSEQ/src/lisp/macros.h`: Various convenience macros used throughout the codebas.
  - `uSEQ/src/lisp/LispLibrary.lisp`: uSEQ-style-Lisp library of functions that will be loaded on startup. 
  
    NOTE: whenever this is changed, the `scripts/lisplibrary.py` script needs to be re-run with the input and output files: 
    ```
    cd uSEQ/src/lisp && python3 ../../../scripts/lisplibrary.py LispLibrary.lisp LispLibrary.h
    ```
- `uSEQ/src/utils` & `utils.h`: Various utilities for logging, debugging etc.
  - `uSEQ/src/utils/json_builder.h`: Lightweight fluent `JsonBuilder` class for constructing JSON strings without an external library.
  - `uSEQ/src/utils/log.cpp`: Serial output routing and the `Protocol` namespace (JSON mode, request tracking, response sending).
- `uSEQ/src/dsp`: DSP (e.g. sampling and some basic synthesis) to run on the second core.
- `uSEQ/src/io`: Functionality relating to hardware and/or software IO (NOTE: not currently used).
- `uSEQ/src/ml`: Functionality for Machine Learning, e.g. input analysis or pattern generation.

## JSON Serial Protocol

When connected to the web editor (firmware v1.2.0+), the module can operate in a structured JSON protocol mode instead of the legacy text-based protocol.

### Protocol Negotiation

1. Editor sends `@(useq-report-firmware-info)` to get the firmware version.
2. If the version is >= 1.2.0, the editor sends a `hello` JSON request.
3. Firmware enables JSON mode and responds with device configuration (ioConfig).
4. All subsequent communication uses JSON framing.

### Message Framing

All serial messages start with a **message start marker** byte (`0x1F` / 31), followed by a **type byte**:
- `0x00` = STREAM (binary sensor data, 11 bytes total)
- `0x65` (101) = JSON (text JSON terminated by CR+LF)
- Any other value = TEXT (legacy text message terminated by CR+LF)

### JSON Request Types

Requests are JSON objects sent from the editor, terminated by newline. Each includes a `requestId` field for correlating responses.

| Type | Purpose | Key Fields |
|------|---------|------------|
| `hello` | Negotiate JSON mode | `client`, `version` |
| `ping` | Heartbeat keep-alive | (none) |
| `stream-config` | Configure input streaming rates | `channels`, `maxRateHz` |
| `eval` | Evaluate a LISP expression | `code`, optional `exec` |

### JSON Responses

Responses include `requestId`, `success` (bool), `console` and `text` (both contain output text), and an optional `meta` object. The `console` field is preferred by the editor over `text`.

### Transport State via Meta

When transport builtins (`useq-play`, `useq-pause`, `useq-stop`, `useq-rewind`) are evaluated via JSON `eval`, the response includes a `meta` field with the new transport state:

```json
{"meta": {"transport": "playing"}}
```

This allows the editor UI to update transport controls without polling.

### Implementation Files

- **Request handling**: `uSEQ.cpp` — `handle_json_serial_request()`, `try_handle_json_message()`
- **Response sending**: `utils/log.cpp` — `Protocol::send_json_response()`, `Protocol::send_raw_json()`
- **JSON construction**: `utils/json_builder.h` — `JsonBuilder` class
- **Protocol state**: `utils/log.cpp` — `Protocol` namespace (enable/disable, request tracking)
