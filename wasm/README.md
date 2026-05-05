# uSEQ WASM Build

This directory contains the WebAssembly build of the uSEQ LISP interpreter.

## Files

- `wasm_wrapper.cpp` - C++ wrapper providing a clean C API for JavaScript
- `index.html` - Browser-based REPL interface
- `useq.js` - Generated WebAssembly module (created by build script)

## Building

From the project root, run:

```bash
./scripts/build_wasm.sh
```

This will compile the uSEQ interpreter to WebAssembly and generate `useq.js`.

## Running

To test the WASM build locally:

```bash
./scripts/serve.py
```

Then open http://localhost:8000/index.html in your browser.

## API

The WASM module exports two functions:

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