#!/usr/bin/env bash

# Flash uSEQ firmware using arduino-cli
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
DEVICE="${1:-/dev/ttyACM0}"

echo "Building and uploading firmware..."
arduino-cli compile --upload --fqbn rp2040:rp2040:generic:boot2=boot2_w25q080_2_padded_checksum,flash=8388608_7340032,freq=250,opt=Optimize3 --port "$DEVICE" "$PROJECT_DIR/uSEQ/" --verbose

if [ $? -eq 0 ]; then
    echo "Flash completed successfully!"
else
    echo "Flash failed. Make sure the device is connected at $DEVICE"
    echo "Try putting the Pico in bootloader mode by holding BOOTSEL while connecting USB"
    exit 1
fi