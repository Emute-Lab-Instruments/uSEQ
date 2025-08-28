uSEQ Architecture Overview

This document sketches the current codebase structure and the direction for making the firmware and DSL read more like English, while keeping embedded constraints in mind.

Core Layers
- Firmware: `uSEQ/src` holding the `uSEQ` class, timekeeping, I/O, evaluation, scheduling, serial, and DSP integration.
- Lisp runtime: `uSEQ/src/lisp` (`Parser`, `Interpreter`, `Environment`, `Value`, generated builtins, library).
- Utilities: `uSEQ/src/utils` for logging, error messages, flags, portable `String` and stdio.
- Hardware glue: `uSEQ/src/uSEQ/*` plus `hardware_includes.h` for Arduino vs desktop stubs.
- Tests: Meson-based unit tests under `test/` (parser and builtins).

Immediate Improvements Implemented
- English aliases for common operations:
  - Scheduling: `at` is an alias of `eval-at-time`.
  - Tempo & meter: `bpm`, `time-signature`, `set-time-signature` map to existing functionality.
  - Outputs: `cv-1..cv-8` and `gate-1..gate-8` mirror `a1..a8` and `d1..d8`.
  - Getters: `get-cv-1..8`, `get-gate-1..8` mirror `get-a*`, `get-d*`.
  - Convenience setters: `(cv idx expr)` and `(gate idx expr)` set CV/gate by index.
- Clearer error phrasing: atom-not-defined and argument type messages read more naturally.
- Fixed off-by-index copy/paste bugs in output setter builtins (`a8`, `d7`, `d8`, and `s4` return).

Next Steps (Incremental Refactor Targets)
- Subsystems with English-like APIs (to be introduced gradually):
  - Transport (time/BPM/meter): `advance()`, `reset()`, `set_bpm()`, `phase_beat()`, `beat_num()`.
  - Inputs: `poll()`, `get_analog(i)`, `get_gate(i)`.
  - Outputs: `flush_continuous()`, `flush_binary()`.
  - UserCode: `eval_now(expr)`, `schedule(id, period, expr)`, `run_due(transport)`.
  - SerialInterface: `read()`, `write_text()`, `write_stream(ch, val)`, `report_error()`.
- HAL consolidation: replace scattered `#ifdef ARDUINO` with a thin hardware abstraction implemented per platform.
- Message bus: unify `println`/`message_editor`/error queue into typed categories (user-info, runtime-error, etc.).

Build & Tests
- Desktop build via Meson (see `docs/dev.md`).
- Tests cover parser and builtins; extend to cover new English aliases and transport math in future changes.

