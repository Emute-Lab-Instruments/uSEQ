#!/usr/bin/env bash

# Build script for uSEQ WASM module (signal engine)

set -euo pipefail

# Change to project root directory
cd "$(dirname "$0")/.."

echo "Building uSEQ WASM module..."

# The tracked build profile is the single source of truth for inputs, flags,
# exports, stack size, and artifact-size gate. The generated capability
# manifest embeds and independently verifies that same profile.
mapfile -t SOURCES < <(python3 scripts/wasm_manifest.py profile-lines sources)
mapfile -t FLAGS < <(python3 scripts/wasm_manifest.py profile-lines compile_flags)
EMCC_EXPORTS="$(python3 scripts/wasm_manifest.py emcc-exports)"
WASM_SIZE_LIMIT="$(python3 scripts/wasm_manifest.py size-limit)"
WASM_STACK_BYTES="$(python3 scripts/wasm_manifest.py profile-value stack_bytes)"

# Emscripten-specific flags
EM_FLAGS=(
    "-s EXPORTED_FUNCTIONS=${EMCC_EXPORTS}"
    "-s EXPORTED_RUNTIME_METHODS=[\"ccall\",\"cwrap\",\"UTF8ToString\"]"
    "-s ALLOW_MEMORY_GROWTH=1"
    "-s STACK_SIZE=${WASM_STACK_BYTES}"
    "-s MODULARIZE=1"
    "-s EXPORT_NAME='createModule'"
    "-s ENVIRONMENT='web'"
    "-s SINGLE_FILE=0"
    "--post-js=wasm/emscripten-post.js"
    "--no-entry"
)

# Build command — output to wasm directory
emcc "${SOURCES[@]}" "${FLAGS[@]}" ${EM_FLAGS[@]} -o wasm/useq.js

echo "emcc build successful."

# Post-process with wasm-opt if available (Binaryen)
POSTPROCESS="none"
if command -v wasm-opt &> /dev/null; then
    echo "Running wasm-opt -Oz..."
    wasm-opt -Oz --all-features wasm/useq.wasm -o wasm/useq.wasm
    echo "wasm-opt complete."
    POSTPROCESS="wasm-opt"
else
    echo "wasm-opt not found — skipping post-processing (install binaryen for smaller/faster WASM)"
fi

node scripts/wasm_init_smoke.mjs wasm

WASM_SIZE="$(stat -c '%s' wasm/useq.wasm)"
if [ "$WASM_SIZE" -gt "$WASM_SIZE_LIMIT" ]; then
    echo "Build failed: wasm/useq.wasm is ${WASM_SIZE} bytes; hard limit is ${WASM_SIZE_LIMIT}" >&2
    exit 1
fi

python3 scripts/wasm_manifest.py generate --postprocess "$POSTPROCESS"
python3 scripts/wasm_manifest.py verify

# Report output sizes
echo "Output:"
ls -lh wasm/useq.js wasm/useq.wasm wasm/useq-capabilities.json 2>/dev/null
