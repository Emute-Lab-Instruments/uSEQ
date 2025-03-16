#!/usr/bin/env bash
# make the path relative to the position of the script
INO_PATH="$(dirname "$0")/../uSEQ/uSEQ.ino"
arduino-cli compile --fqbn rp2040:rp2040:rpipico "$INO_PATH"