#!/usr/bin/env bash

# Build script for uSEQ WebAssembly module

# Change to project root directory
cd "$(dirname "$0")/.."

echo "Building uSEQ WASM module..."

# Source files from meson.build
SOURCES=(
    # Wrapper
    "wasm/wasm_wrapper.cpp"
    
    # Utils
    "uSEQ/src/utils/string.cpp"
    "uSEQ/src/utils/common.cpp"
    "uSEQ/src/utils/itoa.cpp"
    "uSEQ/src/utils/log.cpp"
    "uSEQ/src/utils/error_messages.cpp"
    "uSEQ/src/utils/flags.cpp"
    "uSEQ/src/utils.cpp"
    
    # LISP core
    "uSEQ/src/lisp/parser.cpp"
    "uSEQ/src/lisp/value.cpp"
    "uSEQ/src/lisp/environment.cpp"
    "uSEQ/src/lisp/interpreter.cpp"
    "uSEQ/src/modulisp/lisp/builtins.cpp"
    
    # uSEQ core
    "uSEQ/src/uSEQ.cpp"
    "uSEQ/src/uSEQ_eval.cpp"
    "uSEQ/src/uSEQ_io.cpp"
    "uSEQ/src/uSEQ_i2c.cpp"
    "uSEQ/src/uSEQ_led.cpp"
    "uSEQ/src/uSEQ_flash.cpp"
    "uSEQ/src/uSEQ_time.cpp"
    "uSEQ/src/uSEQ_api.cpp"
    "uSEQ/src/uSEQ_update.cpp"
    
    # External dependencies
    "uSEQ/src/dsp/tempoEstimator.cpp"
)

# Compiler flags
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
    "-s EXPORTED_FUNCTIONS=[\"_useq_init\",\"_useq_eval\",\"_free\"]"
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