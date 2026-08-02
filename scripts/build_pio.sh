#!/usr/bin/env bash

# Build uSEQ firmware using PlatformIO
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Default environment is musicthing
ENV="${1:-musicthing}"

# Valid environments
VALID_ENVS="musicthing musicthing-observe hardware_v0_2 hardware_v1_0 minimal musicthing-debug musicthing-verbose"

# Check if environment is valid
if ! echo "$VALID_ENVS" | grep -wq "$ENV"; then
    echo "Error: Invalid environment '$ENV'"
    echo "Valid environments: $VALID_ENVS"
    exit 1
fi

echo "Building uSEQ firmware for environment: $ENV"
echo "Project directory: $PROJECT_DIR"

cd "$PROJECT_DIR"

# Build with PlatformIO
pio run -e "$ENV"

if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo "Firmware location: $PROJECT_DIR/.pio/build/$ENV/firmware.uf2"

    # Show firmware size
    if [ -f ".pio/build/$ENV/firmware.elf" ]; then
        echo ""
        echo "Firmware size:"
        pio run -e "$ENV" -t size
    fi
else
    echo "Build failed!"
    exit 1
fi
