#!/bin/bash
# Auto-generated picotool script
# Audio file: buttercup.bin
# Samples: 91054, Sample Rate: 22050 Hz
# File size: 364224 bytes
# Flash address: 0x10200000

echo "Loading audio data to flash..."
picotool load -x buttercup.bin -o 0x10200000
echo "Audio data loaded at address 0x10200000"
