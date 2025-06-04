#!/usr/bin/env python3
"""
Audio to Binary Converter for Pi Pico
Converts WAV files to binary format suitable for embedding in flash memory
"""

import librosa
import numpy as np
import struct
import argparse
import os

def wav_to_binary(input_file, output_file, sample_rate=44100, normalize=True):
    """
    Convert WAV file to floating point binary format
    
    Args:
        input_file: Path to input WAV file
        output_file: Path to output binary file
        sample_rate: Target sample rate (Hz)
        normalize: Whether to normalize audio to [-1, 1] range
    """
    
    print(f"Loading audio file: {input_file}")
    
    # Load audio file with librosa
    audio_data, sr = librosa.load(input_file, sr=sample_rate, mono=True)
    
    print(f"Original sample rate: {sr} Hz")
    print(f"Audio length: {len(audio_data)} samples ({len(audio_data)/sr:.2f} seconds)")
    print(f"Audio range: [{np.min(audio_data):.3f}, {np.max(audio_data):.3f}]")
    
    # Normalize if requested
    if normalize:
        max_val = np.max(np.abs(audio_data))
        if max_val > 0:
            audio_data = audio_data / max_val
            print(f"Normalized to range: [{np.min(audio_data):.3f}, {np.max(audio_data):.3f}]")
    
    # Convert to 32-bit float and write binary file
    print(f"Writing binary file: {output_file}")
    
    with open(output_file, 'wb') as f:
        # Write header information (optional, for reference)
        f.write(struct.pack('<I', len(audio_data)))  # Number of samples
        f.write(struct.pack('<I', sample_rate))      # Sample rate
        
        # Write audio data as 32-bit floats (little-endian)
        for sample in audio_data:
            f.write(struct.pack('<f', sample))
    
    print(f"Successfully converted {len(audio_data)} samples")
    print(f"Output file size: {os.path.getsize(output_file)} bytes")

def generate_picotool_script(binary_file, output_file, flash_address=0x10200000):
    """
    Generate shell script for loading binary with picotool
    
    Args:
        binary_file: Path to binary file
        output_file: Path to output shell script
        flash_address: Flash address to load binary (default: 0x10100000)
    """
    
    print(f"Generating picotool script: {output_file}")
    
    # Read header info for the script comments
    with open(binary_file, 'rb') as f:
        num_samples = struct.unpack('<I', f.read(4))[0]
        sample_rate = struct.unpack('<I', f.read(4))[0]
        f.seek(0, 2)  # Seek to end
        file_size = f.tell()
    
    with open(output_file, 'w') as f:
        f.write(f"#!/bin/bash\n")
        f.write(f"# Auto-generated picotool script\n")
        f.write(f"# Audio file: {binary_file}\n")
        f.write(f"# Samples: {num_samples}, Sample Rate: {sample_rate} Hz\n")
        f.write(f"# File size: {file_size} bytes\n")
        f.write(f"# Flash address: 0x{flash_address:08x}\n\n")
        f.write(f"echo \"Loading audio data to flash...\"\n")
        f.write(f"picotool load -x {binary_file} -o 0x{flash_address:08x}\n")
        f.write(f"echo \"Audio data loaded at address 0x{flash_address:08x}\"\n")
    
    # Make script executable
    os.chmod(output_file, 0o755)
    print(f"Picotool script generated (address: 0x{flash_address:08x})")

def generate_c_defines(binary_file, output_file, flash_address=0x10200000):
    """
    Generate C header with defines for flash-loaded audio
    
    Args:
        binary_file: Path to binary file  
        output_file: Path to output C header file
        flash_address: Flash address where audio will be loaded
    """
    
    print(f"Generating C defines header: {output_file}")
    
    with open(binary_file, 'rb') as f:
        num_samples = struct.unpack('<I', f.read(4))[0]
        sample_rate = struct.unpack('<I', f.read(4))[0]
        f.seek(0, 2)  # Seek to end
        file_size = f.tell()
    
    with open(output_file, 'w') as f:
        f.write(f"// Auto-generated audio defines for flash-loaded data\n")
        f.write(f"// Samples: {num_samples}, Sample Rate: {sample_rate} Hz\n")
        f.write(f"// Load with: picotool load -x {os.path.basename(binary_file)} -o 0x{flash_address:08x}\n\n")
        f.write(f"#ifndef AUDIO_FLASH_H\n")
        f.write(f"#define AUDIO_FLASH_H\n\n")
        f.write(f"#include <stdint.h>\n\n")
        f.write(f"// Flash memory address where audio data is loaded\n")
        f.write(f"#define AUDIO_FLASH_ADDRESS    0x{flash_address:08x}U\n")
        f.write(f"#define AUDIO_SAMPLE_RATE      {sample_rate}U\n")
        f.write(f"#define AUDIO_NUM_SAMPLES      {num_samples}U\n")
        f.write(f"#define AUDIO_DATA_SIZE        {file_size}U\n\n")
        f.write(f"// Pointer to audio data in flash\n")
        f.write(f"#define AUDIO_DATA_PTR         ((const uint8_t*)AUDIO_FLASH_ADDRESS)\n\n")
        f.write(f"#endif // AUDIO_FLASH_H\n")
    
    print(f"C defines header generated")

def main():
    parser = argparse.ArgumentParser(description='Convert WAV files to binary for Pi Pico')
    parser.add_argument('input', help='Input WAV file')
    parser.add_argument('-o', '--output', help='Output binary file (default: input.bin)')
    parser.add_argument('-r', '--rate', type=int, default=44100, help='Sample rate (default: 44100)')
    parser.add_argument('-c', '--c-defines', help='Generate C defines header for flash-loaded data')
    parser.add_argument('-s', '--script', help='Generate picotool loading script')
    parser.add_argument('-a', '--address', default='0x10100000', help='Flash address for picotool (default: 0x10100000)')
    parser.add_argument('--no-normalize', action='store_true', help='Skip normalization')
    
    args = parser.parse_args()
    
    # Determine output filename
    if args.output:
        output_file = args.output
    else:
        base_name = os.path.splitext(args.input)[0]
        output_file = f"{base_name}.bin"
    
    # Convert flash address string to int (handle hex format)
    if args.address.startswith('0x'):
        flash_address = int(args.address, 16)
    else:
        flash_address = int(args.address)
    
    # Convert WAV to binary
    wav_to_binary(
        args.input, 
        output_file, 
        sample_rate=args.rate,
        normalize=not args.no_normalize
    )
    
    # Generate C defines header if requested
    if args.c_defines:
        generate_c_defines(output_file, args.c_defines, flash_address)
    
    # Generate picotool script if requested  
    if args.script:
        generate_picotool_script(output_file, args.script, flash_address)

if __name__ == "__main__":
    main()
