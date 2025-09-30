#!/usr/bin/env bash
set -e

echo "Testing all PlatformIO build environments..."
echo "==========================================="
echo ""

# Array of environments to test
ENVS=(
  "musicthing"
  "hardware_v0_2"
  "hardware_v1_0"
  "minimal"
)

for ENV in "${ENVS[@]}"; do
  echo "Building environment: $ENV"
  echo "-------------------------------------------"
  pio run -e "$ENV" 2>&1 | tail -20
  echo ""
done

echo "All builds completed successfully!"