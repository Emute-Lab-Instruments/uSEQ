# uSEQ Build Scripts

This directory contains build and utility scripts for the uSEQ project.

## Scripts

### `build_wasm.sh`
Builds the WebAssembly version of uSEQ using Emscripten.

```bash
./scripts/build_wasm.sh
```

Output: `wasm/useq.js`

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