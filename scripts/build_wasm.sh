#!/usr/bin/env bash

# Build script for uSEQ WASM module (signal engine)

# Change to project root directory
cd "$(dirname "$0")/.."

echo "Building uSEQ WASM module..."

# Source files for WASM — signal engine + utils (symbol_intern is header-only)
SOURCES=(
    # Wrapper
    "wasm/wasm_wrapper.cpp"

    # Utils library (essential — symbol_intern.h depends on String)
    "uSEQ/src/utils/string.cpp"
    "uSEQ/src/utils/common.cpp"
    "uSEQ/src/utils/itoa.cpp"
    "uSEQ/src/utils/log.cpp"
    "uSEQ/src/utils/error_messages.cpp"
    "uSEQ/src/utils/flags.cpp"

    # Signal engine
    "uSEQ/src/signal_engine/diagnostics.cpp"
    "uSEQ/src/signal_engine/token.cpp"
    "uSEQ/src/signal_engine/cell_store.cpp"
    "uSEQ/src/signal_engine/node_pool.cpp"
    "uSEQ/src/signal_engine/executor.cpp"
    "uSEQ/src/signal_engine/graph_builder.cpp"
    "uSEQ/src/signal_engine/cold_eval.cpp"
    "uSEQ/src/signal_engine/state_registry.cpp"
    "uSEQ/src/signal_engine/synth_registry.cpp"
    "uSEQ/src/signal_engine/synth_graph.cpp"
)

# Compiler flags (matching meson.build standalone_args)
FLAGS=(
    "-I./uSEQ"
    "-DUSE_OWN_ARDUINO_STR"
    "-DUSE_STD_IO"
    "-DNO_ETL"
    "-D__not_in_flash(section)="
    "-D__not_in_flash_func(x)="
    "-DWASM_BUILD"
    "-std=c++17"
    "-O3"
    "-flto"
)

# Emscripten-specific flags
EM_FLAGS=(
    "-s EXPORTED_FUNCTIONS=[\"_useq_init\",\"_useq_eval\",\"_useq_update_time\",\"_useq_set_input_value\",\"_useq_eval_output\",\"_useq_eval_outputs_time_window\",\"_useq_eval_outputs_time_window_into\",\"_useq_tick_and_project\",\"_useq_last_error\",\"_useq_last_diagnostics\",\"_useq_active_diagnostics\",\"_useq_synth_artifacts\",\"_useq_set_live_inputs\",\"_useq_set_failure_mode\",\"_useq_get_failure_mode\",\"_useq_get_live_slots\",\"_useq_apply_state_snapshot\",\"_useq_output_classifications\",\"_useq_output_dependencies\",\"_useq_probe_set\",\"_useq_probe_sample\",\"_useq_probe_free\",\"_malloc\",\"_free\"]"
    "-s EXPORTED_RUNTIME_METHODS=[\"ccall\",\"cwrap\",\"UTF8ToString\"]"
    "-s ALLOW_MEMORY_GROWTH=1"
    "-s MODULARIZE=1"
    "-s EXPORT_NAME='createModule'"
    "-s ENVIRONMENT='web'"
    "-s SINGLE_FILE=0"
    "--post-js=wasm/emscripten-post.js"
    "--no-entry"
)

# Build command — output to wasm directory
emcc "${SOURCES[@]}" "${FLAGS[@]}" ${EM_FLAGS[@]} -o wasm/useq.js

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo "emcc build successful."

# Post-process with wasm-opt if available (Binaryen)
if command -v wasm-opt &> /dev/null; then
    echo "Running wasm-opt -O3..."
    wasm-opt -O3 --all-features wasm/useq.wasm -o wasm/useq.wasm
    echo "wasm-opt complete."
else
    echo "wasm-opt not found — skipping post-processing (install binaryen for smaller/faster WASM)"
fi

# Report output sizes
echo "Output:"
ls -lh wasm/useq.js wasm/useq.wasm 2>/dev/null
