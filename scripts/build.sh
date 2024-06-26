#!/usr/bin/env sh

# Change directory to the root of the project
cd "$(dirname "$0")/.."

# Run Meson setup and Ninja
meson setup build
ninja -C build
