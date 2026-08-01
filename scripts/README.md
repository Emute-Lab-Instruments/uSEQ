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
