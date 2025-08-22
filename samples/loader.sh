#!/bin/bash
# Auto-generated picotool script for multi-audio binary
# Binary file: test.bin
# Files: 122, Sample Rate: 22050 Hz
# File size: 12,792,212 bytes (12.20 MB)
# Flash address: 0x10200000

echo "Loading multi-audio binary to flash..."
echo "File: test.bin"
echo "Size: 12,792,212 bytes (12.20 MB)"
echo "Address: 0x10200000"
echo "Files: 122"
echo ""
picotool load -x test.bin -o 0x10200000
echo "Multi-audio binary loaded successfully!"
