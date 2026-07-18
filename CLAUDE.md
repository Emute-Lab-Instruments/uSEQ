# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

uSEQ is a **livecodeable eurorack module** - a hardware music sequencer that can be programmed in real-time using a custom LISP-based language called ModuLisp. It combines embedded firmware (C++), hardware design (KiCad), and software interfaces (Python, Clojure) for live musical performance.

## Agent Workflow

### Issue Tracking

This project uses **`ergo`** for task tracking. `ergo` is the coding-work CLI
over the Holon EAV substrate and replaced Beads (`bd`) on 2026-06-15.

The `ergo` CLI is installed at `/home/w1n5t0n/.local/bin/ergo`. The required
environment (`HOLON_TOKEN`, `HOLON_CORE_URL`, optional `HOLON_PRINCIPAL`) is
loaded for every shell by `~/.zshenv` sourcing `~/.secrets/env`. The
authoritative workflow is `/home/w1n5t0n/agents/skills/ergo/SKILL.md`.

Use `ergo` for all task tracking. Do not create markdown TODO lists or track
work outside `ergo`. Beads (`bd`) and Dolt are frozen read-only historical
infrastructure.

```bash
ergo ready [--mine] [--json]
ergo show <id>
ergo claim <id>
ergo done <id> --reason "Done"
```

When new follow-up work is discovered, create a linked task:

```bash
ergo create "Issue title" --type bug|feature|task --priority 0-4 --body "Context" --discovered-from <parent-id>
```

### Non-Interactive Shell Commands

Always use non-interactive flags with shell commands that may prompt.

```bash
cp -f source dest
mv -f source dest
rm -f file
rm -rf directory
cp -rf source dest
scp -o BatchMode=yes ...
ssh -o BatchMode=yes ...
apt-get -y ...
HOMEBREW_NO_AUTO_UPDATE=1 brew ...
```

### WIP Discipline

This repository has active long-running epics. Agents must not be trigger-happy about declaring them complete.

- Treat closed subtasks as completed slices, not proof that the parent epic is done.
- If implementation is partial, first-pass, or knowingly missing semantic coverage, say so explicitly in code review notes, handoffs, and issue wording.
- Do not use language like "replaces the interpreter", "general VM complete", or "closure support done" unless the remaining semantic gaps and regression coverage actually support that claim.
- When you find a known gap that is not being fixed in the current change, file or update an `ergo` task instead of hand-waving it away.

### Session Completion

When ending a work session, finish the operational steps instead of leaving work stranded locally:

1. File `ergo` tasks for remaining work.
2. Run quality gates if code changed.
3. Update task state in `ergo` (`ergo done <id> --reason "..."`).
4. Push all changes:
   `git pull --rebase`
   `git push`
   `git status`
5. Verify the branch is up to date with origin before handing off.

(Beads/`bd` push steps that previously appeared here are retired; the
`.beads/dolt` server is frozen read-only historical infrastructure.)

## Build Commands

### Desktop/Standalone Build (Default for Development)
```bash
# Standard Meson build for desktop testing
./scripts/build.sh
# OR manually:
meson setup build
ninja -C build

# Run standalone interpreter
./build/standalone
```

### PlatformIO Firmware Build (RECOMMENDED for Hardware)
```bash
# Enter PlatformIO environment (NixOS)
nix-shell platformio.nix
# Or with direnv: edit .envrc to use platformio.nix

# Build firmware for Raspberry Pi Pico (default: Music Thing variant)
./scripts/build_pio.sh                  # Build for musicthing
./scripts/build_pio.sh hardware_v0_2    # Build for hardware v0.2
./scripts/build_pio.sh hardware_v1_0    # Build for hardware v1.0

# Flash to Pico
./scripts/flash_pio.sh                  # Flash default variant
./scripts/flash_pio.sh musicthing /dev/ttyACM0  # Flash to specific device

# Convenience wrapper (recommended)
./scripts/pio.sh build                  # Build
./scripts/pio.sh flash                  # Flash
./scripts/pio.sh clean                  # Clean builds
./scripts/pio.sh monitor                # Open serial monitor
./scripts/pio.sh size                   # Show firmware size
./scripts/pio.sh list                   # List all environments

# Direct PlatformIO commands
pio run -e musicthing                   # Build
pio run -e musicthing -t upload         # Flash
pio run -e musicthing -t clean          # Clean
pio device monitor                      # Serial monitor
```

### Arduino CLI Build (Legacy - Use PlatformIO Instead)
```bash
# Build firmware for Raspberry Pi Pico
./scripts/build_arduino_cli.sh

# Manual Arduino CLI build with specific settings:
arduino-cli compile --fqbn rp2040:rp2040:generic:boot2=boot2_w25q080_2_padded_checksum,flash=8388608_7340032,freq=250,opt=Optimize3 uSEQ/

# Flash to Pico
./scripts/flash_arduino_cli.sh
```

### Testing
```bash
# Run all tests with Meson
./scripts/test.sh

# Run specific test suite
./scripts/test.sh -s signal_engine       # Core signal engine tests
./scripts/test.sh -s signal_engine_golden # Golden semantic tests
./scripts/test.sh -s firmware_e2e        # Firmware end-to-end tests
./scripts/test.sh -s flash_storage       # Flash persistence round-trips
./scripts/test.sh -s ugens              # Unit generator tests

# Run with verbose output
./scripts/test.sh -v

# Skip build (run tests only)
./scripts/test.sh -f
```

### WASM Build
```bash
./scripts/build_wasm.sh
# Serve WASM version
python scripts/serve_wasm.py
```

## Core Architecture

### Project Structure
```
/uSEQ/src/
├── signal_engine/             # Signal compiler and runtime
│   ├── graph_builder.{h,cpp}  # Compiler: ModuLisp → node graph
│   ├── node_pool.{h,cpp}      # CSE, constant folding, node storage
│   ├── executor.{h,cpp}       # Per-sample hot-path evaluation
│   ├── cold_eval.{h,cpp}      # Cold eval: REPL, cell assign, output def
│   ├── cell_store.{h,cpp}     # Named cells and data tables
│   ├── token.{h,cpp}          # Tokenizer
│   ├── diagnostics.{h,cpp}    # Structured error reporting
│   ├── state_registry.{h,cpp} # State-identity resource registry
│   ├── signal_engine.h        # SignalEngine composition struct
│   ├── types.h                # Shared types (Node, Cell, Output, etc.)
│   └── symbols.def            # X-macro symbol definitions
├── firmware/                  # Hardware abstraction
│   ├── firmware.{h,cpp}       # Composition root, init(), tick() loop
│   ├── hardware_io.{h,cpp}    # All pin I/O, variant pin maps, LEDs
│   ├── serial_protocol.{h,cpp}# JSON wire protocol dispatch
│   ├── flash_storage.{h,cpp}  # LittleFS persistence
│   ├── i2c_network.{h,cpp}    # Multi-module I2C communication
│   ├── dsp_engine.{h,cpp}     # Core 1 stub (P3)
│   └── utils/                 # piopwm.h, ResponsiveAnalogRead
├── utils/                     # String, JSON builder, log, serial_message
├── modulisp/lisp/symbol_intern.h  # Symbol interning (shared)
└── ports/                     # IStorage.h, MockStorage.h (testability)

/wasm/wasm_wrapper.cpp         # WASM bindings (15 exports)
/test/                         # signal_engine/ and firmware/ test suites
/docs/specs/                   # Normative specs (MAIN.md is the index)
/scripts/                      # Build, test, flash scripts
```

### Key Components

1. **Signal Engine** (`signal_engine/`)
   - Compiles ModuLisp to a node DAG, evaluates per-sample via flat arrays
   - `graph_builder.cpp` — compiler with form dispatch table, X-macro symbols
   - `executor.cpp` — hot path: linear scan of topological order, zero allocation
   - `cold_eval.cpp` — cold path: parses + compiles on eval, manages cells/outputs
   - `node_pool.h` — hash-consed node pool with CSE + constant folding

2. **Firmware** (`firmware/`)
   - Thin wrapper: reads inputs → runs engine → writes outputs
   - `hardware_io.cpp` — all variant pin maps inline under `#ifdef` guards
   - `serial_protocol.cpp` — JSON wire protocol (hello, ping, eval, stream-config)
   - `flash_storage.cpp` — LittleFS persistence with CRC32 validation

3. **JSON Protocol** (`utils/`)
   - `log.cpp/h`: `Protocol` namespace — JSON mode toggle, request tracking
   - `json_builder.h`: Lightweight fluent JSON builder (no external libs)
   - `serial_message.h`: Wire-level constants (start marker `0x1F`)

## Error Handling and Diagnostics

The signal-engine compiler produces structured diagnostics that flow through to the browser editor as inline annotations. Specs: `docs/specs/diagnostics.md` (wire format), `docs/specs/failure-model.md` (LKG, health states).

### Key files

- `signal_engine/diagnostics.{h,cpp}` — `Diagnostic`, `DiagnosticSeverity`, `DiagnosticCategory`, `severity_to_cstr()`, `category_to_cstr()`, fuzzy matching
- `signal_engine/graph_builder.cpp` — compiler uses `report_error_at_cat()` (fatal) and `report_and_continue()` (non-fatal). Fuzzy matching suggests corrections for typos.
- `signal_engine/cold_eval.cpp` — `SignalEngine` owns diagnostics vector, accessed via `get_diagnostics()`, `clear_diagnostics()`
- `wasm/wasm_wrapper.cpp` — `useq_last_diagnostics()` (JSON array from last eval) and `useq_active_diagnostics()` (per-output health state)
- `firmware/serial_protocol.cpp` — firmware includes diagnostics in serial JSON eval responses

### Diagnostic flow

```
User code → Tokenizer (token.cpp) → GraphBuilder (diagnostics) → SignalEngine
  → WASM useq_last_diagnostics() → Frontend JSON parse → CodeMirror inline annotations
```

### Adding a new error

1. In the compiler (`graph_builder.cpp`), call `report_error_at_cat()` for fatal or `report_and_continue()` for non-fatal:
   ```cpp
   return report_error_at_cat(DiagnosticCategory::Arity,
          span_start, span_len, "fn needs N values", "Try: (fn arg1 arg2)");
   ```
2. Use plain language — no jargon in user-facing messages
3. Always include a `suggestion` with working example code
4. The diagnostic propagates automatically through the WASM ABI to the editor

### Important constraints

- `ninja -j4` — never compile with full parallelism (bricks the machine)
- Diagnostics are compile-time only — never allocated on the per-sample hot path

## Development Workflows

### Hardware Configuration
- Pin maps: defined inline in `firmware/hardware_io.cpp` under per-variant `#ifdef` guards
- Variant selection: PlatformIO environments in `platformio.ini` pass `-D MUSICTHING` etc.
- No separate configure.h — feature flags are PIO build flags

### Live Coding Interface
```bash
# Run the Python editor
python interfaces/useqedit/useqedit.py

# Send code to module: Ctrl+L
# Immediate execution: prefix with @
# Queue for next bar: no prefix
```

## Important Configuration

### Hardware Variants

The firmware supports multiple hardware configurations:

- **musicthing** (default) - Music Thing Modular variant
  - All features enabled (DSP, I2C, tempo estimation, RGB LED)
  - Inverted outputs for Music Thing hardware

- **hardware_v0_2** - uSEQ Hardware v0.2
  - DSP, I2C networking, rotary encoder

- **hardware_v1_0** - uSEQ Hardware v1.0
  - DSP, I2C, tempo estimation, no encoder

- **minimal** - Minimal build for testing
  - Core features only, reduced binary size

- **musicthing-debug** - Debug build with symbols
  - Reduced optimization, debug output

- **musicthing-verbose** - Extra verbose serial output

### Arduino/PlatformIO Build Settings
- **Board**: Generic RP2040 (Earle Philhower Pico core)
- **CPU Speed**: 250MHz (Overclock)
- **Flash**: 8MB (Sketch 1MB, FS: 7MB)
- **Optimization**: -O3 (Optimize Even More)
- **Boot Stage 2**: W25Q080 QSPI /2

### Compiler Flags
- Desktop: `-DUSE_OWN_ARDUINO_STR`, `-DUSE_STD_IO`, `-DNO_ETL`
- Arduino: Hardware-specific defines for Pico

### Meson Configuration
- Warning flags: `-Wall`, `-Wextra`, `-Wpedantic`, `-Wshadow`
- Disabled: `-Wno-conversion`, `-Wno-unused-parameter`
- Test builds: Located in `test_build/` directory

## Real-time Constraints

- **Dual-core Architecture**: Signal engine on core 0, core 1 reserved (P3 DSP stub)
- **Timing System**: Functional rendering with phasors (beat, bar, phrase, section)
- **Update Rate**: Runs as fast as possible, not fixed quantum
- **Hot path (executor.cpp)**: Zero allocation, no virtual dispatch, linear scan
- **I2C Networking**: Multi-module communication for synchronized performance

## Testing Strategy

Test suites in `test/signal_engine/` and `test/firmware/`:
- **signal_engine**: Core compiler + executor (136 test cases)
- **signal_engine_golden**: Golden semantic tests (data-driven, 1000+ assertions)
- **signal_engine_phase4**: Advanced features (robustness, edge cases)
- **signal_engine_robustness**: Fuzz and stress tests
- **ugens**: State-bearing unit generators (slew, env-follow, etc.)
- **firmware_e2e**: Full tick-loop integration tests
- **wire_protocol_contract**: Serial protocol contract tests
- **flash_storage**: Persistence round-trip tests (via MockStorage)
- **output_classification**: Output type classification
- **live_edit**: Live-edit state identity tests

Run tests frequently during development to ensure stability.

## Documentation References

- **Canonical Specs**: `/docs/specs/MAIN.md` - ModuLisp semantics index
- **Wire Protocol**: `/docs/specs/wire-protocol.md` - USB CDC protocol contract
- **Firmware Runtime**: `/docs/specs/firmware.md` - firmware runtime invariants
- **Changelog**: `/docs/changelog.md` - Version history

## Language Stack

1. **C++17/20** - Core firmware and interpreter
2. **Python** - Live coding editor and utilities
3. **Clojure** - Code generation scripts (if needed)
4. **LISP** - Domain-specific language for sequencing

## Critical Files to Understand

1. `uSEQ/src/signal_engine/signal_engine.h` — composition struct (owns CellStore, NodePool, SourceArena)
2. `uSEQ/src/signal_engine/graph_builder.cpp` — the compiler (ModuLisp → node DAG)
3. `uSEQ/src/signal_engine/executor.cpp` — the hot path (per-sample node evaluation)
4. `uSEQ/src/signal_engine/cold_eval.cpp` — cold path (REPL eval, cell/output management)
5. `uSEQ/src/firmware/firmware.cpp` — tick loop (the main runtime loop)
6. `wasm/wasm_wrapper.cpp` — WASM bindings (15 exports for the web editor)
7. `test/signal_engine/test_signal_engine_golden.cpp` — golden semantic tests
