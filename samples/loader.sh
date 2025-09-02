#!/bin/bash
# Auto-generated picotool script for multi-audio binary
# Binary file: bettyloops.bin
# Files: 13, Sample Rate: 24000 Hz
# File size: 2,995,632 bytes (2.86 MB)
# Flash address: 0x10200000

echo "Loading multi-audio binary to flash..."
echo "File: bettyloops.bin"
echo "Size: 2,995,632 bytes (2.86 MB)"
echo "Address: 0x10200000"
echo "Files: 13"
echo ""
picotool load -x bettyloops.bin -o 0x10200000
echo "Multi-audio binary loaded successfully!"
