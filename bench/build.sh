#!/usr/bin/env bash
# Standalone build for bench/bench_probe.cpp — intentionally independent of
# test/meson.build. Output: bench/bench_probe
set -euo pipefail
cd "$(dirname "$0")/.."

DEFS=(-DUSE_OWN_ARDUINO_STR -DUSE_STD_IO -DNO_ETL
      '-D__not_in_flash(section)=' '-D__not_in_flash_func(x)='
      -DENABLE_SIGNAL_ENGINE)

SRCS=(bench/bench_probe.cpp
      uSEQ/src/signal_engine/diagnostics.cpp
      uSEQ/src/signal_engine/token.cpp
      uSEQ/src/signal_engine/cell_store.cpp
      uSEQ/src/signal_engine/node_pool.cpp
      uSEQ/src/signal_engine/executor.cpp
      uSEQ/src/signal_engine/graph_builder.cpp
      uSEQ/src/signal_engine/cold_eval.cpp
      uSEQ/src/signal_engine/state_registry.cpp
      uSEQ/src/utils/string.cpp
      uSEQ/src/utils/common.cpp
      uSEQ/src/utils/itoa.cpp
      uSEQ/src/utils/log.cpp)

g++ -O2 -std=c++17 -IuSEQ -IuSEQ/src -IuSEQ/src/devtools \
    "${DEFS[@]}" "${SRCS[@]}" -lm -o bench/bench_probe
echo "built bench/bench_probe"
