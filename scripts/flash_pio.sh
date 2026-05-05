#!/usr/bin/env bash

# Flash uSEQ firmware using PlatformIO
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Default environment is musicthing
ENV="${1:-musicthing}"
# Default device (optional for RP2040 - can use BOOTSEL mode)
DEVICE="${2:-}"

# Valid environments
VALID_ENVS="musicthing hardware_v0_2 hardware_v1_0 minimal musicthing-debug musicthing-verbose"

# Check if environment is valid
if ! echo "$VALID_ENVS" | grep -wq "$ENV"; then
    echo "Error: Invalid environment '$ENV'"
    echo "Valid environments: $VALID_ENVS"
    exit 1
fi

echo "Flashing uSEQ firmware for environment: $ENV"
echo "Project directory: $PROJECT_DIR"

cd "$PROJECT_DIR"

# Upload with PlatformIO
if [ -n "$DEVICE" ]; then
    echo "Using device: $DEVICE"
    pio run -e "$ENV" -t upload --upload-port "$DEVICE"
else
    echo "Auto-detecting device..."
    echo "Put Pico in BOOTSEL mode (hold BOOTSEL button while connecting USB) if needed"
    pio run -e "$ENV" -t upload
fi

if [ $? -eq 0 ]; then
    echo "Flash completed successfully!"
else
    echo "Flash failed!"
    echo ""
    echo "Troubleshooting tips:"
    echo "1. Hold BOOTSEL button on Pico while connecting USB"
    echo "2. Check that the device appears as a USB mass storage device"
    echo "3. Try specifying the device explicitly: $0 $ENV /dev/ttyACM0"
    exit 1
fi