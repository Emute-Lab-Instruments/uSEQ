#!/usr/bin/env sh

# Target device
DEVICE="/dev/ttyACM0"

# Wait for the device to appear
while [ ! -e "$DEVICE" ]
do
    echo "Waiting for device $DEVICE to be connected..."
    sleep 1
done

echo "Device $DEVICE connected. Starting capture..."

# Capture the serial output to a file
cat $DEVICE | tee "$(date +%Y%m%d_%H%M%S)_pico_serial_output.log"
