# Changelog

## Unreleased

- Fixed browser-local evaluation on Chromium releases that expose
  `WebAssembly.Memory.toResizableBuffer()`. The WASM build now keeps classic
  fixed-length heap views across memory growth, and artifact verification
  rejects generated glue that re-enables growable ArrayBuffer views.

## 1.2.0-beta.1

- Firmware identity now uses canonical SemVer prereleases and one build-info
  source rather than hard-coded protocol strings.
- `hello` and `ready` advertise independent protocol version, exact hardware
  target, and additive capability names so an editor can select safe behavior
  and the correct UF2 without inferring from firmware semver.
- I2C output expanders advertise a CRC-protected factory identity, exact
  firmware target/version, hardware revision, batch, and serial to the main
  module. `hello.modules` exposes startup discovery and `rescan-modules`
  repeats it explicitly without inventing an I2C firmware-relay path.

## 1.2.0 (Protocol Enhancements)

### Additions
- JSON protocol handlers: `hello` (negotiation with ioConfig), `ping` (heartbeat), `stream-config` (channel configuration)
- `useq-get-transport-state` builtin — returns `"playing"`, `"paused"`, or `"stopped"`
- Transport state push via `meta` field in JSON eval responses
- `console` field in JSON responses (editor prefers this over `text`)
- `JsonBuilder` utility class (`utils/json_builder.h`) for structured JSON construction
- `Protocol::send_raw_json()` for sending pre-built JSON payloads

### Changes
- `handle_json_serial_request` now dispatches on request type instead of rejecting non-eval requests
- Transport builtins (`useq-play`, `useq-pause`, `useq-stop`, `useq-rewind`) set `m_pending_transport_meta` for automatic state reporting

---

## 1.0 -> 1.0.1
bb280b0..cc04555

# Fixes
- Fixed `pulse`'s width parameter being inverted.
- Fixed `pow` so that base is on the right
- Fixed `scale` to work with floats and take either 3 or 5 args
- added `lerp`
- added `random`
- added vector call syntax


- added `toggle_select` (?)

- fix `dm` args error checking
- fix `gatesw`, phasor on the right
- fix (re-introduce) `trigs`

- change IO analogWriteFreq from 80000 to 100000

- change `euclid`, remove some args and phasor on the right
- add `eu` TODO
- remove `looph`

- `len` now works with sequentials (currently, that means List & Vector) and not just with Lists.

# Additions
# Removals
# Changes
- Changed println to accept any object, not just strings
