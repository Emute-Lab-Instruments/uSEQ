# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

uSEQ is a **livecodeable eurorack module** - a hardware music sequencer that can be programmed in real-time using a custom LISP-based language called ModuLisp. It combines embedded firmware (C++), hardware design (KiCad), and software interfaces (Python, Clojure) for live musical performance.

## Agent Workflow

### Issue Tracking

This project uses **bd** (beads) for issue tracking. Run `bd onboard` to get started.

Use `bd` for all task tracking. Do not create markdown TODO lists or track work outside beads.

```bash
bd ready --json
bd show <id> --json
bd update <id> --claim --json
bd close <id> --reason "Done" --json
bd dolt push
```

When new follow-up work is discovered, create a linked issue:

```bash
bd create "Issue title" --description="Context" -t bug|feature|task -p 0-4 --deps discovered-from:<parent-id> --json
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
- When you find a known gap that is not being fixed in the current change, file or update a `bd` issue instead of hand-waving it away.

### Session Completion

When ending a work session, finish the operational steps instead of leaving work stranded locally:

1. File issues for remaining work.
2. Run quality gates if code changed.
3. Update issue state.
4. Push all changes:
   `git pull --rebase`
   `bd dolt push`
   `git push`
   `git status`
5. Verify the branch is up to date with origin before handing off.

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
./scripts/test.sh -s value      # Value API tests
./scripts/test.sh -s environment # Environment API tests  
./scripts/test.sh -s parser     # Parser API tests
./scripts/test.sh -s interpreter # Interpreter API tests
./scripts/test.sh -s builtins   # Builtin functions tests

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
/uSEQ/                          # Main firmware (Arduino project)
├── uSEQ.ino                   # Arduino entry point
├── src/
│   ├── modulisp/              # ModuLisp interpreter core
│   │   ├── modulisp.h/cpp     # Main ModuLisp class
│   │   ├── modulisp_api.cpp   # API implementation
│   │   ├── modulisp_eval.cpp  # Evaluation logic
│   │   ├── modulisp_time.cpp  # Timing functions
│   │   └── lisp/              # Core LISP interpreter
│   │       ├── parser.cpp/h        # S-expression parsing
│   │       ├── value.cpp/h         # Value types and operations
│   │       ├── environment.cpp/h   # Variable scoping
│   │       ├── interpreter.cpp/h   # Core evaluation engine
│   │       └── generated_builtins.cpp/h # Auto-generated functions
│   ├── uSEQ.h/cpp             # Main uSEQ class (inherits from ModuLisp)
│   ├── uSEQ_*.cpp             # Hardware-specific implementations
│   ├── utils/                 # Utility functions
│   └── dsp/                   # DSP processing (tempo estimation, filters)

/interfaces/useqedit/           # Python-based live coding editor
/hardware/                      # KiCad PCB designs (v0.1, v0.2, v1.0)
/test/                         # Comprehensive test suite
/scripts/                      # Build and utility scripts
/docs/                         # Documentation
```

### Key Classes and Components

1. **ModuLisp** (`/uSEQ/src/modulisp/`)
   - Main interpreter class that extends base `Interpreter`
   - Handles timing, API functions, and module-specific features
   - Key files: `modulisp.h/cpp`, `modulisp_api.cpp`, `modulisp_eval.cpp`

2. **uSEQ** (`/uSEQ/src/`)
   - Hardware interface class that extends `ModuLisp`
   - Manages I/O, I2C communication, LED control, flash storage
   - Handles JSON serial protocol (hello, ping, stream-config, eval dispatch)
   - Key files: `uSEQ.h/cpp`, `uSEQ_api.cpp`, `uSEQ_io.cpp`, `uSEQ_i2c.cpp`, `uSEQ_update.cpp`

3. **JSON Protocol** (`/uSEQ/src/utils/`)
   - `log.cpp` / `log.h`: `Protocol` namespace — JSON mode toggle, request tracking, response sending
   - `json_builder.h`: Lightweight fluent `JsonBuilder` for constructing JSON without external libraries
   - `serial_message.h`: Wire-level constants (start marker `0x1F`, message type bytes)
   - Protocol is negotiated via `hello` handshake; supports `ping`, `stream-config`, and `eval` requests
   - Transport builtins push state changes via the `meta` field in eval responses

4. **LISP Interpreter** (`/uSEQ/src/modulisp/lisp/`)
   - Complete LISP implementation with parser, evaluator, environment
   - Supports lists, symbols, numbers, strings, lambdas, macros
   - Generated builtins from EDN specifications

## Error Handling and Diagnostics

The signal-engine compiler produces structured diagnostics that flow through to the browser editor as inline annotations. The on-wire data shapes and WASM ABI live in `docs/specs/diagnostics.md`; the failure semantics (LKG, health states, REPL-vs-output channels) live in `docs/specs/failure-model.md`.

### Key files

- `uSEQ/src/modulisp/diagnostic.h` — `SourceSpan`, `Diagnostic`, `DiagnosticSeverity`, `DiagnosticCategory` types, plus `severity_to_cstr()`/`category_to_cstr()` helpers
- `uSEQ/src/modulisp/bytecode_vm.cpp` — compiler uses `report()` (fatal) and `report_and_continue()` (non-fatal, emits placeholder and continues). Checkpoint/rollback truncates speculative diagnostics. `warn_if_non_numeric()` catches type errors at compile time. `find_fuzzy_match()` suggests corrections for typos.
- `uSEQ/src/modulisp/modulisp_interpreter.h` — interpreter owns `std::vector<Diagnostic> m_diagnostics`, accessed via `get_diagnostics()`, `clear_diagnostics()`, `has_diagnostics_error()`
- `uSEQ/src/modulisp/lisp/parser.cpp` — populates `SourceSpan` on every `Value` node during parsing; reports syntax errors as `Diagnostic` objects
- `uSEQ/src/modulisp/lisp/value.h` — `Value` has a `SourceSpan span` field (in the padding gap, zero size overhead)
- `wasm/wasm_wrapper.cpp` — `useq_last_diagnostics()` (JSON array from last eval) and `useq_active_diagnostics()` (per-output health state)
- `uSEQ/src/uSEQ.cpp` — firmware includes diagnostics in serial JSON eval responses

### Diagnostic flow

```
User code → Parser (spans) → VM Compiler (diagnostics) → Interpreter (m_diagnostics)
  → WASM useq_last_diagnostics() → Frontend JSON parse → CodeMirror inline annotations
```

### Adding a new error

1. In the compiler (`bytecode_vm.cpp`), call `report_and_continue()` for non-fatal or `report()` for fatal:
   ```cpp
   return report_and_continue(DiagnosticCategory::Arity,
          expr.span, "fn needs N values", "Try: (fn arg1 arg2)");
   ```
2. Use plain language — no "Numeric VM", no "arity", no jargon
3. Always include a `suggestion` with working example code
4. The diagnostic will automatically propagate through the WASM ABI to the editor

### Important constraints

- `ninja -j4` — never compile with full parallelism (bricks the machine)
- Diagnostics are compile-time only — never allocated on the per-sample hot path
- `Value::error()` still exists as an internal sentinel; the `is_error()` check at the `CALL_INTRINSIC` boundary converts it to a structured diagnostic

## Development Workflows

### Adding New LISP Functions
1. For built-in functions: Add to `/scripts/builtins.edn` and regenerate
2. For ModuLisp API: Add to `modulisp_api.cpp` using `MODULISP_FUNC` macro
3. For uSEQ-specific: Add to `uSEQ_api.cpp` using `USEQ_FUNC` macro

### Hardware Configuration
- Pin mappings: `/uSEQ/src/uSEQ/pinmap.h`
- Module settings: `/uSEQ/src/uSEQ/configure.h`
- LISP settings: `/uSEQ/src/modulisp/lisp/configure.h`

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

- **Dual-core Architecture**: LISP on core 0, DSP on core 1
- **Timing System**: Functional rendering with phasors (beat, bar, phrase, section)
- **Update Rate**: Runs as fast as possible, not fixed quantum
- **I2C Networking**: Multi-module communication for synchronized performance

## Testing Strategy

The project includes comprehensive API tests covering:
- **Value API**: Construction, type checking, conversions, operators
- **Environment API**: Variable storage, scoping, inheritance
- **Parser API**: String parsing, structure recognition
- **Interpreter API**: Expression evaluation, function application
- **Builtin Functions**: Generated function implementations
- **ModuLisp API**: Module-specific functionality

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

1. `/uSEQ/src/modulisp/modulisp.h` - Main interpreter interface
2. `/uSEQ/src/uSEQ.h` - Hardware abstraction layer
3. `/uSEQ/src/modulisp/lisp/interpreter.cpp` - Core evaluation logic
4. `/test/test_*.cpp` - Test suites showing API usage
5. `/interfaces/useqedit/useqedit.py` - Live coding interface
