# ADR-0001: uSEQ Modular Refactor Boundary Contract

- Status: Accepted (baseline contract for `useq-tp1` epic)
- Date: 2026-02-11
- Epic: `useq-tp1`
- Primary task: `useq-tp1.1`

## Context

uSEQ currently concentrates runtime orchestration, transport handling, hardware I/O,
ModuLisp API wiring, and builtin registration across a small set of large files.
This ADR defines bounded contexts, ownership, and dependency direction so refactor
subtasks can proceed without changing behavior.

Scoped baseline hotspot files:

- `uSEQ/src/uSEQ.cpp`
- `uSEQ/src/uSEQ_api.cpp`
- `uSEQ/src/uSEQ_update.cpp`
- `uSEQ/src/modulisp/modulisp_api.cpp`
- `uSEQ/src/modulisp/lisp/builtins.cpp`
- `uSEQ/src/uSEQ/io_manager.cpp`

## Decision

### Bounded Context Map

1. Runtime Orchestration
- Owns: process lifecycle, init/run loop, request routing, update cadence.
- Current anchor files: `uSEQ/src/uSEQ.cpp`, `uSEQ/src/uSEQ_update.cpp`.

2. ModuLisp Core
- Owns: parser/environment/value/evaluator/scheduler primitives.
- Current anchor files: `uSEQ/src/modulisp/lisp/*`, `uSEQ/src/modulisp/modulisp_interpreter.*`.

3. Builtins and API Surface
- Owns: Lisp-callable functions and argument validation/registration.
- Current anchor files: `uSEQ/src/modulisp/lisp/builtins.cpp`, `uSEQ/src/modulisp/modulisp_api.cpp`, `uSEQ/src/uSEQ_api.cpp`.

4. Persistence
- Owns: save/load of expressions and state; storage adapter integration.
- Current anchor files: `uSEQ/src/uSEQ_flash.cpp` and any `IStorage` adapter call sites.

5. Transport
- Owns: serial/JSON/I2C transport decode/encode and command dispatch plumbing.
- Current anchor files: `uSEQ/src/uSEQ.cpp`, `uSEQ/src/uSEQ_i2c.cpp`, `uSEQ/src/uSEQ/i2cHost.cpp`, `uSEQ/src/uSEQ/i2cClient.cpp`.

6. Hardware I/O
- Owns: pin setup, ADC/GPIO/PWM interaction, board-profile-specific IO behavior.
- Current anchor files: `uSEQ/src/uSEQ/io_manager.cpp`, `uSEQ/src/uSEQ/hardware_output.cpp`, `uSEQ/src/uSEQ/board.h`.

### Scoped File Ownership Baseline

| File | Primary ownership now | Boundary notes |
|---|---|---|
| `uSEQ/src/uSEQ.cpp` | Runtime Orchestration + mixed Transport/API/DSP + some hardware coupling | Overloaded control plane; target extraction for command routing and service seams. |
| `uSEQ/src/uSEQ_api.cpp` | Builtins/API for hardware-facing `useq_*` wrappers | Mostly output/input wrapper layer; keep thin and delegate to IO/output services. |
| `uSEQ/src/uSEQ_update.cpp` | Runtime output update/evaluation cycle | Should depend on output-eval service, not direct mixed concerns. |
| `uSEQ/src/modulisp/modulisp_api.cpp` | ModuLisp API extensions + output evaluation semantics | Preserve as API layer; move policy-free core logic out over time where possible. |
| `uSEQ/src/modulisp/lisp/builtins.cpp` | Core builtin function implementations | Split by domain with shared validation helpers. |
| `uSEQ/src/uSEQ/io_manager.cpp` | Hardware I/O and board-conditional behavior | Should be adapter/service behind interface; currently reaches back to parent runtime. |

### Dependency Direction Contract

Allowed high-level direction:

- Runtime Orchestration -> Builtins/API
- Runtime Orchestration -> Transport
- Runtime Orchestration -> Persistence
- Runtime Orchestration -> Hardware I/O
- Builtins/API -> ModuLisp Core
- Transport -> codec/util + port interfaces
- Persistence -> serialization/util + `ports/IStorage`
- Hardware I/O -> board profile + `ports/IIo`

Forbidden direction:

- ModuLisp Core -> Runtime Orchestration
- ModuLisp Core -> Transport/Persistence/Hardware I/O
- Builtins/API -> concrete hardware headers or board-specific globals
- Transport/Persistence adapters -> direct evaluator internals (must call explicit runtime service APIs)
- Hardware I/O -> parser/evaluator internals

### Interface Ownership Rules

- Runtime owns orchestration interfaces and delegates into services; it does not own concrete hardware algorithm details.
- ModuLisp Core owns evaluation/data model contracts (`Value`, parser, environment) and is hardware-agnostic.
- Builtins/API owns function registration and arg validation entrypoints.
- Persistence owns save/load format and storage adapter boundaries.
- Transport owns framing and protocol adaptation only.
- Hardware I/O owns board-specific side effects and should expose them through `IIo`-style seams.

## Test Implications (Contractual)

- Any new boundary extraction must add or update desktop tests that pin behavior.
- Builtin/API refactors require table-driven arg-validation tests for both success and error cases.
- Runtime routing extractions require transport contract tests (serial/JSON/I2C paths) with mocked ports.
- Hardware I/O refactors require adapter-level tests for pin mapping and output write behavior.
- Persistence split requires deterministic round-trip tests via storage mocks.
- No subtask in this epic may merge with reduced test coverage over touched areas.

## Known Boundary Violations to Address in Follow-on Tasks

- `uSEQ/src/uSEQ/io_manager.cpp` includes `../uSEQ.h` and keeps a `uSEQ*` parent pointer, creating reverse coupling from hardware adapter to runtime orchestration.
- `uSEQ/src/uSEQ.cpp` still mixes command routing, transport framing, DSP commands, and hardware-trigger flows.
- Builtin and API implementation density remains high in `uSEQ/src/modulisp/lisp/builtins.cpp` and `uSEQ/src/modulisp/modulisp_api.cpp`.

## Consequences

- Follow-on subtasks can extract services safely as long as dependency direction above is preserved.
- Violations discovered during implementation should be documented against this ADR and either:
  - fixed in-scope, or
  - explicitly deferred with a tracked beads task linked to `useq-tp1`.

## References

- `docs/architecture/useq-tp1-checklist.md`
- `docs/architecture/useq-tp1-baseline-metrics.csv`
- `uSEQ/src/uSEQ.h`
- `uSEQ/src/ports/IIo.h`
- `uSEQ/src/ports/IStorage.h`
- `uSEQ/src/ports/II2CBus.h`
