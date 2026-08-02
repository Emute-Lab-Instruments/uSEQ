# uSEQ Build Scripts

This directory contains build and utility scripts for the uSEQ project.

## Scripts

### `build_wasm.sh`
Builds the WebAssembly version of uSEQ using Emscripten. It consumes the
tracked `wasm_build_profile.json`, runs the real compiler smoke, enforces the
byte ceiling, then generates and independently verifies the ignored
`wasm/useq-capabilities.json` reproducibility record with `wasm_manifest.py`.

```bash
./scripts/build_wasm.sh
```

Outputs: `wasm/useq.js`, `wasm/useq.wasm`, and
`wasm/useq-capabilities.json`. The verifier fails closed if the source
revision/input digest, toolchain, build profile, hard limits, JS export
bindings, size gate, or artifact digests differ. This is WASM build evidence,
not RP2040 target or HIL evidence.

The Git ID, source digest, and artifact hashes are content-addressed
provenance: they detect mismatch and support reproduction, but do not
authenticate a publisher against malicious substitution. Authentication needs
an external trust anchor such as a signed release or trusted distribution
channel for the manifest and the exact artifacts it names.

### `run_rp2040_profile.py`

Runs the default local acceptance surface for RP2040 optimization:

```bash
python3 scripts/run_rp2040_profile.py
```

It runs the native suite and conformance corpus, rebuilds native probes with
the exact firmware capacities, enforces the declared firmware workload bands,
runs optimized and ASan/UBSan endurance checks, links `musicthing` and
`musicthing-observe`, and applies exact ELF memory gates. Evidence is written
under `build/rp2040-profile/<revision>/`. Native timing remains comparative;
simulator and physical-device observations have separate result grades.

### `run_wokwi_rp2040.py`

Builds or reuses `musicthing-observe`, lints `wokwi/diagram.json`, generates a
bounded automation scenario from the moderate and combined-high firmware
corpora, runs the exact ELF, and evaluates the serial log and logic-analyzer
VCD against `rp2040_budget.json`.

```bash
python3 scripts/run_wokwi_rp2040.py
```

The command requires the official `wokwi-cli` and `WOKWI_CLI_TOKEN`. The token
is read only from the environment and is never written to evidence. Use
`--generate-only` to validate scenario generation without a token or hosted
simulation quota.

### `run_rp2040_goal_gate.py`

Runs one complete optimization candidate through `run_rp2040_profile.py` and
then Wokwi, reusing the exact observation ELF. Each invocation creates a new
directory under `build/rp2040-goal/`; an existing label is never overwritten.

```bash
python3 scripts/run_rp2040_goal_gate.py
```

The combined summary links the two subordinate summaries by SHA-256. Run
candidate comparisons with the same toolchain and Wokwi CLI version, and
retain only candidates for which the complete gate passes.

### `serve.py`
Simple Python web server for testing the WASM build locally.

```bash
./scripts/serve.py
```

Serves files from the `wasm/` directory on http://localhost:8000/

### Other Scripts (existing)

- `arduino_build.sh` - Build firmware for Arduino/Pico
- `build.sh` - Build desktop version using Meson
- `lisplibrary.py` - Convert LISP library to C++ header

## Requirements

- **WASM Build**: Emscripten (emcc)
- **Server**: Python 3
- **Arduino Build**: Arduino CLI
- **Desktop Build**: Meson, Ninja, C++ compiler
