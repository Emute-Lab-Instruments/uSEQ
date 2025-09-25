#!/usr/bin/env bash

# Build script for uSEQ WebAssembly module

# Change to project root directory
cd "$(dirname "$0")/.."

echo "Building uSEQ WASM module..."

# Source files for WASM - minimal set for basic LISP interpreter
SOURCES=(
    # Wrapper
    "wasm/wasm_wrapper.cpp"
    
    # Utils library (essential)
    "uSEQ/src/utils/string.cpp"
    "uSEQ/src/utils/common.cpp"
    "uSEQ/src/utils/itoa.cpp"
    "uSEQ/src/utils/log.cpp"
    "uSEQ/src/utils/error_messages.cpp"
    "uSEQ/src/utils/flags.cpp"
    "uSEQ/src/utils/logger_bridge.cpp"
    "uSEQ/src/utils/default_logger.cpp"
    "uSEQ/src/utils.cpp"
    
    # LISP core library (essential)
    "uSEQ/src/modulisp/lisp/parser.cpp"
    "uSEQ/src/modulisp/lisp/value.cpp"
    "uSEQ/src/modulisp/lisp/environment.cpp"
    "uSEQ/src/modulisp/lisp/signal_metadata.cpp"
    "uSEQ/src/modulisp/lisp/error_context.cpp"
    "uSEQ/src/modulisp/lisp/builtins.cpp"
    "uSEQ/src/template_instantiations.cpp"

    # ModuLisp library (essential for timing and interpreter)
    "uSEQ/src/modulisp/modulisp.cpp"
    "uSEQ/src/modulisp/modulisp_time.cpp"
    "uSEQ/src/modulisp/modulisp_api.cpp"
    "uSEQ/src/modulisp/modulisp_eval.cpp"
    "uSEQ/src/modulisp/modulisp_interpreter.cpp"
    "uSEQ/src/modulisp/modulisp_interpreter_core.cpp"
    "uSEQ/src/modulisp/function_registry.cpp"
    "uSEQ/src/modulisp/phasor_manager.cpp"
    "uSEQ/src/modulisp/random_generator.cpp"
    "uSEQ/src/modulisp/scheduler.cpp"
    "uSEQ/src/modulisp/time_manager.cpp"
    
    # Skip uSEQ.cpp entirely - use ModuLisp directly for WASM
    # "uSEQ/src/uSEQ.cpp"
    
    # Skip hardware-specific files for WASM:
    # - uSEQ/src/uSEQ_io.cpp (hardware I/O)
    # - uSEQ/src/uSEQ_i2c.cpp (I2C networking) 
    # - uSEQ/src/uSEQ_led.cpp (LED control)
    # - uSEQ/src/uSEQ_flash.cpp (flash storage)
    # - uSEQ/src/uSEQ/output_manager.cpp (hardware outputs)
    # - uSEQ/src/uSEQ/io_manager.cpp (hardware I/O management)
    # - uSEQ/src/uSEQ_update.cpp (firmware updates)
    # - uSEQ/src/dsp/tempoEstimator.cpp (DSP processing)
    # - uSEQ/src/uSEQ_api.cpp (hardware-specific APIs)
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
    "-O2"
)

# Emscripten-specific flags
EM_FLAGS=(
    "-s EXPORTED_FUNCTIONS=[\"_useq_init\",\"_useq_eval\",\"_useq_update_time\",\"_useq_eval_output\",\"_free\"]"
    "-s EXPORTED_RUNTIME_METHODS=[\"ccall\",\"cwrap\",\"UTF8ToString\"]"
    "-s ALLOW_MEMORY_GROWTH=1"
    "-s MODULARIZE=1"
    "-s EXPORT_NAME='createModule'"
    "-s ENVIRONMENT='web'"
    "-s SINGLE_FILE=1"
    "--no-entry"
)

# Build command - output to wasm directory
emcc "${SOURCES[@]}" "${FLAGS[@]}" ${EM_FLAGS[@]} -o wasm/useq.js

if [ $? -eq 0 ]; then
    echo "Build successful! Generated wasm/useq.js"
else
    echo "Build failed!"
    exit 1
fi
