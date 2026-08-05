#!/usr/bin/env bash
# Standalone build for bench/bench_probe.cpp — intentionally independent of
# test/meson.build. Output: bench/bench_probe
set -euo pipefail
cd "$(dirname "$0")/.."

DEFS=(-DUSE_OWN_ARDUINO_STR -DUSE_STD_IO -DNO_ETL
      '-D__not_in_flash(section)=' '-D__not_in_flash_func(x)='
      -DENABLE_SIGNAL_ENGINE)

ENGINE_SRCS=(uSEQ/src/signal_engine/diagnostics.cpp
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

SYNTH_ENGINE_SRCS=(uSEQ/src/signal_engine/synth_graph.cpp
      uSEQ/src/signal_engine/synth_registry.cpp)

build_probe() {
    local output="$1"
    local main_source="$2"
    shift 2
    local sources=("${ENGINE_SRCS[@]}")
    local firmware_profile=false
    for arg in "$@"; do
        if [[ "$arg" == -DUSEQ_FIRMWARE_PROFILE=* ]]; then
            firmware_profile=true
        fi
    done
    if [[ "$firmware_profile" == false ]]; then
        sources+=("${SYNTH_ENGINE_SRCS[@]}")
    fi
    g++ -O2 -std=c++17 -IuSEQ -IuSEQ/src -IuSEQ/src/devtools \
        "${DEFS[@]}" "$@" "$main_source" "${sources[@]}" -lm -o "$output"
    echo "built $output"
}

TARGET="${1:-bench}"
case "$TARGET" in
    desktop)
        build_probe bench/bench_probe bench/bench_probe.cpp
        ;;
    firmware)
        build_probe bench/bench_probe_firmware bench/bench_probe.cpp \
            -DUSEQ_FIRMWARE_PROFILE=1
        ;;
    conformance)
        build_probe bench/signal_engine_probe_firmware \
            test/signal_engine/signal_engine_probe.cpp \
            -DUSEQ_FIRMWARE_PROFILE=1
        ;;
    endurance)
        build_probe bench/firmware_profile_endurance \
            bench/firmware_profile_endurance.cpp -DUSEQ_FIRMWARE_PROFILE=1
        build_probe bench/firmware_profile_endurance_asan \
            bench/firmware_profile_endurance.cpp -DUSEQ_FIRMWARE_PROFILE=1 \
            -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
        ;;
    bench)
        build_probe bench/bench_probe bench/bench_probe.cpp
        build_probe bench/bench_probe_firmware bench/bench_probe.cpp \
            -DUSEQ_FIRMWARE_PROFILE=1
        ;;
    all)
        "$0" bench
        "$0" conformance
        "$0" endurance
        ;;
    *)
        echo "usage: $0 [desktop|firmware|conformance|endurance|bench|all]" >&2
        exit 2
        ;;
esac
