# uSEQ WASM Build

This directory contains the WebAssembly build of the uSEQ LISP interpreter.

## Files

- `wasm_wrapper.cpp` - C++ wrapper providing a clean C API for JavaScript
- `index.html` - Browser-based REPL interface
- `useq.js` - Generated Emscripten host wrapper (created by build script)
- `useq.wasm` - Generated compiler/runtime WebAssembly module
- `useq-capabilities.json` - Generated deterministic capability/build manifest;
  binds the exact source revision and input digest, toolchain/profile, compiler
  hard limits, public ABI/export set, size gate, and SHA-256/size of both
  generated artifacts

## Building

From the project root, run:

```bash
./scripts/build_wasm.sh
```

This compiles the uSEQ interpreter, runs the real init/eval/synth smoke, enforces
the hard WASM size gate, generates `useq-capabilities.json`, and verifies the
manifest back against the current source, toolchain, build profile, JS export
bindings, and artifact bytes. Any mismatch fails the build.

This is content-addressed reproducibility evidence, not authenticated
provenance. The hashes cannot distinguish an authorised build from a malicious
replacement that supplies a self-consistent manifest; use a signed release or
another external trust anchor when publisher authenticity matters.

To verify an already-built bundle without changing it:

```bash
python3 scripts/wasm_manifest.py verify
```

## Running

To test the WASM build locally:

```bash
./scripts/serve.py
```

Then open http://localhost:8000/index.html in your browser.

## API

The full public function ABI is machine-readable in
`scripts/wasm_build_profile.json` and copied into the verified capability
manifest. Its core entry points include:

- `useq_init()` - Initialize the interpreter (must be called first)
- `useq_eval(string)` - Evaluate a LISP expression and return the result as a string

Example JavaScript usage:

```javascript
const module = await createModule();
const useq_init = module.cwrap('useq_init', null, []);
const useq_eval = module.cwrap('useq_eval', 'string', ['string']);

useq_init();
const result = useq_eval("(+ 1 2)");  // Returns "3"
```

## Examples

Try these expressions in the REPL:

- `(+ 1 2 3)` - Basic arithmetic
- `(define x 42)` - Define variables
- `(lambda (x) (* x x))` - Create functions
- `(list 1 2 3)` - Create lists
- `(if (> 5 3) "yes" "no")` - Conditionals
