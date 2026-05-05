#!/usr/bin/env bash

# Build uSEQ firmware using arduino-cli and create UF2 file
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="${1:-$(pwd)}"

echo "Project dir: $PROJECT_DIR"
echo "Output dir: $OUTPUT_DIR"

# Compile the firmware
arduino-cli compile --fqbn rp2040:rp2040:generic:boot2=boot2_w25q080_2_padded_checksum,flash=8388608_7340032,freq=250,opt=Optimize3 "$PROJECT_DIR/uSEQ/"

# Find the generated UF2 file (arduino-cli creates it automatically)
CACHE_UF2=$(find ~/.cache/arduino/sketches -name "uSEQ.ino.uf2" -type f | head -1)

if [ ! -f "$CACHE_UF2" ]; then
    echo "Error: UF2 file not found in arduino cache"
    exit 1
fi

# Copy UF2 to output directory
UF2_FILE="$OUTPUT_DIR/uSEQ.uf2"
cp "$CACHE_UF2" "$UF2_FILE"

if [ -f "$UF2_FILE" ]; then
    echo "Success: UF2 created at $UF2_FILE"
else
    echo "Error: Failed to copy UF2 file"
    exit 1
fi
