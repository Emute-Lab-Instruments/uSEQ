#!/usr/bin/env bash

# Build script for the osc/sine NodeDef WASM artefact.
#
# VAL-DSP-005, VAL-DSP-015, VAL-DSP-016: this module is a SEPARATE build
# target from the ModuLisp interpreter (scripts/build_wasm.sh) and from the
# firmware image. It imports host-owned shared WebAssembly.Memory and defines
# no private memory. It exposes a small, named C ABI used by the source-
# agnostic host adapter in the AudioWorklet.
#
# Output:
#   wasm/osc_sine.wasm  — bare WASM binary (no JS wrapper required)
#   wasm/osc_sine.wat   — text-format disassembly, regenerated for inspection
#
# The build is intentionally bare: we do not use Emscripten's MODULARIZE JS
# wrapper because the worklet host compiles WebAssembly.Module off-thread and
# instantiates it directly against its own shared memory.

set -euo pipefail

# Change to project root (src-useq/).
cd "$(dirname "$0")/.."

OUTPUT_DIR="wasm"
SRC_CPP="nodedef/osc_sine.cpp"

if [ ! -f "$SRC_CPP" ]; then
    echo "error: $SRC_CPP not found (expected at project root)" >&2
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

echo "Building osc/sine NodeDef WASM artefact (imported shared memory)..."

# Exported functions list (bracketed, comma-separated, with leading
# underscores matching the Emscripten C symbol convention). The leading
# underscore is preserved in STRICT mode; the host adapter looks up
# `_osc_sine_*` symbols on the compiled module.
EXPORTED_FUNCS='[_osc_sine_registry_json,'
EXPORTED_FUNCS+='_osc_sine_state_bytes,'
EXPORTED_FUNCS+='_osc_sine_state_align,'
EXPORTED_FUNCS+='_osc_sine_control_stride_bytes,'
EXPORTED_FUNCS+='_osc_sine_output_stride_bytes,'
EXPORTED_FUNCS+='_osc_sine_min_quantum,'
EXPORTED_FUNCS+='_osc_sine_max_quantum,'
EXPORTED_FUNCS+='_osc_sine_sample_rate,'
EXPORTED_FUNCS+='_osc_sine_fade_in_ms,'
EXPORTED_FUNCS+='_osc_sine_fade_out_ms,'
EXPORTED_FUNCS+='_osc_sine_validate_layout,'
EXPORTED_FUNCS+='_osc_sine_init,'
EXPORTED_FUNCS+='_osc_sine_compute,'
EXPORTED_FUNCS+='_osc_sine_get_phase,'
EXPORTED_FUNCS+='_osc_sine_get_smoothed_amp,'
EXPORTED_FUNCS+='_osc_sine_reset_phase'
EXPORTED_FUNCS+=']'

# Build the WASM artefact.
#
# Emscripten 3.1.69 invocation notes:
#   * "-sX=Y" must arrive at emcc as TWO separate argv entries ("-s" and
#     "X=Y"). Bash arrays that pack them into a single quoted element
#     ("-s X=Y") make emcc forward the joined string to clang, which
#     rejects it. We therefore use the unquoted line-continuation form so
#     the shell splits on whitespace as intended.
#   * IMPORTED_MEMORY=1        — module imports memory under "env.memory"
#                                 instead of defining its own (VAL-DSP-005).
#   * ENVIRONMENT=web,worker   — loadable in main thread, Worker, and
#                                 AudioWorkletGlobalScope.
#   * ALLOW_MEMORY_GROWTH=0    — host owns memory; growth is host-side.
#   * STRICT=1                 — minimal runtime, no deprecated defaults.
#   * NO_FILESYSTEM=1          — no FS support needed in compute.
#   * This Emscripten version has no IMPORT_NAME setting; the memory
#     import lands under "env" by default, which matches the host adapter.
# shellcheck disable=SC2086
emcc nodedef/osc_sine.cpp \
    -std=c++17 -O3 -I./nodedef -DNODEDEF_BUILD \
    -s IMPORTED_MEMORY=1 \
    -s ENVIRONMENT=web,worker \
    -s ALLOW_MEMORY_GROWTH=0 \
    -s STRICT=1 \
    -s NO_FILESYSTEM=1 \
    -s EXPORTED_FUNCTIONS=${EXPORTED_FUNCS} \
    --no-entry \
    -o ${OUTPUT_DIR}/osc_sine.wasm

if [ ! -f "${OUTPUT_DIR}/osc_sine.wasm" ]; then
    echo "Build failed: osc_sine.wasm not produced" >&2
    exit 1
fi

echo "emcc build successful."

# Post-process with wasm-opt (Binaryen) if available.
if command -v wasm-opt >/dev/null 2>&1; then
    echo "Running wasm-opt -O3..."
    wasm-opt -O3 --all-features "${OUTPUT_DIR}/osc_sine.wasm" \
              -o "${OUTPUT_DIR}/osc_sine.wasm"
    echo "wasm-opt complete."
else
    echo "wasm-opt not found - skipping post-processing"
fi

# Regenerate a text-format disassembly for binary inspection tests
# (wasm-objdump / wasm-dis is the WABT/Binaryen text format that the
# inspection script greps for the memory import and export tables).
if command -v wasm-dis >/dev/null 2>&1; then
    echo "Generating ${OUTPUT_DIR}/osc_sine.wat (text format)..."
    wasm-dis "${OUTPUT_DIR}/osc_sine.wasm" -o "${OUTPUT_DIR}/osc_sine.wat" || {
        echo "warning: wasm-dis failed - text disassembly not regenerated" >&2
    }
else
    echo "wasm-dis not found - skipping .wat regeneration"
fi

echo
echo "osc/sine NodeDef WASM artefact:"
ls -lh "${OUTPUT_DIR}/osc_sine.wasm" "${OUTPUT_DIR}/osc_sine.wat" 2>/dev/null || true
