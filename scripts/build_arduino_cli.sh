#!/usr/bin/env bash

# Build uSEQ firmware using arduino-cli
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
echo "Project dir: $PROJECT_DIR"
arduino-cli compile --fqbn rp2040:rp2040:generic "$PROJECT_DIR/uSEQ/"
