#!/usr/bin/env python3
"""
Multi-Audio Binary Generator for Pi Pico
Converts a folder of WAV files to a single binary with file system structure
"""

import librosa
import numpy as np
import struct
import argparse
import os
import glob
from pathlib import Path

# Binary format structures
MAGIC_HEADER = b'PICO'  # 4 bytes magic number
VERSION = 1             # 4 bytes version

def wav_to_float_array(input_file, sample_rate=44100, normalize=True, max_length_ms=None, force_mono=True):
    """
    Convert WAV file to float array
    
    Args:
        input_file: Path to WAV file
        sample_rate: Target sample rate
        normalize: Whether to normalize audio
        max_length_ms: Maximum length in milliseconds (None for no limit)
        force_mono: Force conversion to mono
    
    Returns:
        tuple: (audio_data, actual_sample_rate, was_truncated)
    """
    print(f"Processing: {input_file}")
    
    # Load audio file with librosa (mono=force_mono)
    audio_data, sr = librosa.load(input_file, sr=sample_rate, mono=force_mono)
    
    was_truncated = False
    
    # Check if we need to truncate based on max length
    if max_length_ms is not None:
        max_samples = int((max_length_ms / 1000.0) * sr)
        if len(audio_data) > max_samples:
            audio_data = audio_data[:max_samples]
            was_truncated = True
            print(f"  Truncated from {len(audio_data)/sr:.2f}s to {max_length_ms/1000.0:.2f}s")
    
    print(f"  Channels: {'Mono' if force_mono else 'Original'}")
    print(f"  Length: {len(audio_data)} samples ({len(audio_data)/sr:.2f}s)")
    if was_truncated:
        print(f"  ** TRUNCATED ** (was longer than {max_length_ms}ms)")
    print(f"  Range: [{np.min(audio_data):.3f}, {np.max(audio_data):.3f}]")
    
    # Normalize if requested
    if normalize:
        max_val = np.max(np.abs(audio_data))
        if max_val > 0:
            audio_data = audio_data / max_val
            print(f"  Normalized to: [{np.min(audio_data):.3f}, {np.max(audio_data):.3f}]")
    
    return audio_data, sr, was_truncated

def create_multi_audio_binary(folder_path, output_file, sample_rate=44100, normalize=True, 
                             max_length_ms=None, force_mono=True):
    """
    Create binary file with multiple audio files and file system structure
    
    Args:
        folder_path: Folder containing WAV files
        output_file: Output binary file path
        sample_rate: Target sample rate
        normalize: Whether to normalize audio
        max_length_ms: Maximum length in milliseconds (None for no limit)
        force_mono: Force conversion to mono
    
    Binary format:
    - Header (16 bytes):
      - Magic: 'PICO' (4 bytes)
      - Version: uint32 (4 bytes) 
      - File count: uint32 (4 bytes)
      - Sample rate: uint32 (4 bytes)
    
    - File Table (32 bytes per file):
      - Name: char[16] (16 bytes, null-terminated)
      - Offset: uint32 (4 bytes) - offset to audio data
      - Sample count: uint32 (4 bytes)
      - Duration: float (4 bytes) - duration in seconds
      - Reserved: uint32 (4 bytes) - for future use
    
    - Audio Data:
      - Raw float samples for each file
    """
    
    # Find all WAV files in folder
    wav_files = []
    for ext in ['*.wav', '*.WAV']:
        wav_files.extend(glob.glob(os.path.join(folder_path, ext)))
    
    if not wav_files:
        print(f"No WAV files found in {folder_path}")
        return False
    
    wav_files.sort()  # Sort for consistent ordering
    print(f"Found {len(wav_files)} WAV files")
    
    if max_length_ms is not None:
        print(f"Maximum file length: {max_length_ms}ms ({max_length_ms/1000.0:.2f}s)")
    if force_mono:
        print(f"Converting all files to mono")
    
    # Process all audio files
    audio_data_list = []
    file_info_list = []
    skipped_files = []
    truncated_files = []
    
    for wav_file in wav_files:
        try:
            audio_data, sr, was_truncated = wav_to_float_array(
                wav_file, sample_rate, normalize, max_length_ms, force_mono
            )
            
            # Skip empty files
            if len(audio_data) == 0:
                print(f"  Warning: Skipping empty file")
                skipped_files.append(wav_file)
                continue
            
            # Extract filename (without extension, max 15 chars + null terminator)
            filename = Path(wav_file).stem[:15]
            
            file_info = {
                'name': filename,
                'samples': len(audio_data),
                'duration': len(audio_data) / sr,
                'data': audio_data,
                'was_truncated': was_truncated
            }
            
            audio_data_list.append(audio_data)
            file_info_list.append(file_info)
            
            if was_truncated:
                truncated_files.append(wav_file)
                
        except Exception as e:
            print(f"  Error processing {wav_file}: {e}")
            skipped_files.append(wav_file)
    
    if not file_info_list:
        print("No valid audio files to process!")
        return False
    
    # Calculate offsets
    header_size = 16  # Magic + version + file_count + sample_rate
    file_table_size = len(file_info_list) * 32  # 32 bytes per file entry
    data_start_offset = header_size + file_table_size
    
    current_offset = data_start_offset
    for i, file_info in enumerate(file_info_list):
        file_info['offset'] = current_offset
        current_offset += len(file_info['data']) * 4  # 4 bytes per float
    
    # Write binary file
    print(f"Writing binary file: {output_file}")
    
    with open(output_file, 'wb') as f:
        # Write header
        f.write(MAGIC_HEADER)                           # Magic: 'PICO'
        f.write(struct.pack('<I', VERSION))             # Version
        f.write(struct.pack('<I', len(file_info_list))) # File count
        f.write(struct.pack('<I', sample_rate))         # Sample rate
        
        # Write file table
        for file_info in file_info_list:
            # File name (16 bytes, null-terminated)
            name_bytes = file_info['name'].encode('utf-8')[:15]
            name_padded = name_bytes + b'\x00' * (16 - len(name_bytes))
            f.write(name_padded)
            
            # File data
            f.write(struct.pack('<I', file_info['offset']))     # Offset
            f.write(struct.pack('<I', file_info['samples']))    # Sample count
            f.write(struct.pack('<f', file_info['duration']))   # Duration
            f.write(struct.pack('<I', 0))                       # Reserved
        
        # Write audio data
        for file_info in file_info_list:
            for sample in file_info['data']:
                f.write(struct.pack('<f', sample))
    
    # Print summary
    total_size = os.path.getsize(output_file)
    total_duration = sum(info['duration'] for info in file_info_list)
    
    print(f"\nBinary file created successfully!")
    print(f"Output file: {output_file}")
    print(f"Total size: {total_size:,} bytes ({total_size/1024/1024:.2f} MB)")
    print(f"Files included: {len(file_info_list)}")
    print(f"Total duration: {total_duration:.2f} seconds")
    print(f"Sample rate: {sample_rate} Hz")
    print(f"Audio format: {'Mono' if force_mono else 'Original channels'}")
    
    if max_length_ms is not None:
        print(f"Length limit: {max_length_ms}ms")
    
    if skipped_files:
        print(f"\nSkipped files ({len(skipped_files)}):")
        for f in skipped_files:
            print(f"  - {os.path.basename(f)}")
    
    if truncated_files:
        print(f"\nTruncated files ({len(truncated_files)}):")
        for f in truncated_files:
            print(f"  - {os.path.basename(f)}")
    
    print(f"\nFile listing:")
    for i, info in enumerate(file_info_list):
        truncated_marker = " [TRUNCATED]" if info['was_truncated'] else ""
        print(f"  [{i}] {info['name']}: {info['samples']} samples, {info['duration']:.2f}s{truncated_marker}")
    
    return True

def generate_c_defines(binary_file, output_file, flash_address=0x10200000):
    """
    Generate C header with defines and structures for multi-audio binary
    """
    
    print(f"Generating C defines header: {output_file}")
    
    # Read header to get file count and sample rate
    with open(binary_file, 'rb') as f:
        magic = f.read(4)
        if magic != MAGIC_HEADER:
            print(f"Error: Invalid magic header in {binary_file}")
            return False
        
        version = struct.unpack('<I', f.read(4))[0]
        file_count = struct.unpack('<I', f.read(4))[0]
        sample_rate = struct.unpack('<I', f.read(4))[0]
        
        # Read file table for names
        file_names = []
        for i in range(file_count):
            name_bytes = f.read(16)
            name = name_bytes.rstrip(b'\x00').decode('utf-8')
            file_names.append(name)
            f.read(16)  # Skip rest of file entry
        
        f.seek(0, 2)  # Seek to end
        file_size = f.tell()
    
    with open(output_file, 'w') as f:
        f.write(f"// Auto-generated multi-audio defines for flash-loaded data\n")
        f.write(f"// Files: {file_count}, Sample Rate: {sample_rate} Hz\n")
        f.write(f"// Load with: picotool load -x {os.path.basename(binary_file)} -o 0x{flash_address:08x}\n\n")
        f.write(f"#ifndef MULTI_AUDIO_FLASH_H\n")
        f.write(f"#define MULTI_AUDIO_FLASH_H\n\n")
        f.write(f"#include <stdint.h>\n\n")
        
        # Constants
        f.write(f"// Flash memory configuration\n")
        f.write(f"#define AUDIO_FLASH_ADDRESS    0x{flash_address:08x}U\n")
        f.write(f"#define AUDIO_MAGIC            0x4F434950U  // 'PICO'\n")
        f.write(f"#define AUDIO_VERSION          {version}U\n")
        f.write(f"#define AUDIO_FILE_COUNT       {file_count}U\n")
        f.write(f"#define AUDIO_SAMPLE_RATE      {sample_rate}U\n")
        f.write(f"#define AUDIO_BINARY_SIZE      {file_size}U\n\n")
        
        # Structures
        f.write(f"// Binary format structures\n")
        f.write(f"typedef struct {{\n")
        f.write(f"    uint32_t magic;        // 'PICO' magic number\n")
        f.write(f"    uint32_t version;      // Format version\n")
        f.write(f"    uint32_t file_count;   // Number of audio files\n")
        f.write(f"    uint32_t sample_rate;  // Sample rate in Hz\n")
        f.write(f"}} audio_header_t;\n\n")
        
        f.write(f"typedef struct {{\n")
        f.write(f"    char name[16];         // Null-terminated filename\n")
        f.write(f"    uint32_t offset;       // Offset to audio data\n")
        f.write(f"    uint32_t sample_count; // Number of samples\n")
        f.write(f"    float duration;        // Duration in seconds\n")
        f.write(f"    uint32_t reserved;     // Reserved for future use\n")
        f.write(f"}} audio_file_entry_t;\n\n")
        
        # File indices
        f.write(f"// File indices (for easy access)\n")
        for i, name in enumerate(file_names):
            safe_name = name.upper().replace(' ', '_').replace('-', '_')
            f.write(f"#define AUDIO_{safe_name}_INDEX {i}U\n")
        f.write(f"\n")
        
        # Pointers (now just base address - pointers calculated at runtime)
        f.write(f"// Base flash address (pointers calculated at runtime)\n")
        f.write(f"#define AUDIO_BINARY_BASE      ((const uint8_t*)AUDIO_FLASH_ADDRESS)\n\n")
        
        f.write(f"#endif // MULTI_AUDIO_FLASH_H\n")
    
    print(f"C defines header generated with {file_count} files")
    return True

def generate_picotool_script(binary_file, output_file, flash_address=0x10200000):
    """
    Generate shell script for loading binary with picotool
    """
    
    print(f"Generating picotool script: {output_file}")
    
    # Read basic info
    with open(binary_file, 'rb') as f:
        magic = f.read(4)
        if magic != MAGIC_HEADER:
            print(f"Error: Invalid magic header")
            return False
        
        version = struct.unpack('<I', f.read(4))[0]
        file_count = struct.unpack('<I', f.read(4))[0]
        sample_rate = struct.unpack('<I', f.read(4))[0]
        f.seek(0, 2)
        file_size = f.tell()
    
    with open(output_file, 'w') as f:
        f.write(f"#!/bin/bash\n")
        f.write(f"# Auto-generated picotool script for multi-audio binary\n")
        f.write(f"# Binary file: {binary_file}\n")
        f.write(f"# Files: {file_count}, Sample Rate: {sample_rate} Hz\n")
        f.write(f"# File size: {file_size:,} bytes ({file_size/1024/1024:.2f} MB)\n")
        f.write(f"# Flash address: 0x{flash_address:08x}\n\n")
        f.write(f"echo \"Loading multi-audio binary to flash...\"\n")
        f.write(f"echo \"File: {binary_file}\"\n")
        f.write(f"echo \"Size: {file_size:,} bytes ({file_size/1024/1024:.2f} MB)\"\n")
        f.write(f"echo \"Address: 0x{flash_address:08x}\"\n")
        f.write(f"echo \"Files: {file_count}\"\n")
        f.write(f"echo \"\"\n")
        f.write(f"picotool load -x {binary_file} -o 0x{flash_address:08x}\n")
        f.write(f"echo \"Multi-audio binary loaded successfully!\"\n")
    
    # Make script executable
    os.chmod(output_file, 0o755)
    print(f"Picotool script generated")
    return True

def main():
    parser = argparse.ArgumentParser(description='Create multi-audio binary for Pi Pico')
    parser.add_argument('folder', help='Folder containing WAV files')
    parser.add_argument('-o', '--output', help='Output binary file (default: audio_bundle.bin)')
    parser.add_argument('-r', '--rate', type=int, default=44100, help='Sample rate (default: 44100)')
    parser.add_argument('-c', '--c-defines', help='Generate C defines header')
    parser.add_argument('-s', '--script', help='Generate picotool loading script')
    parser.add_argument('-a', '--address', default='0x10200000', help='Flash address (default: 0x10200000)')
    parser.add_argument('--no-normalize', action='store_true', help='Skip normalization')
    parser.add_argument('--max-length', type=int, help='Maximum file length in milliseconds (e.g., 30000 for 30 seconds)')
    parser.add_argument('--stereo', action='store_true', help='Keep stereo files (default: convert to mono)')
    
    args = parser.parse_args()
    
    # Check if folder exists
    if not os.path.isdir(args.folder):
        print(f"Error: Folder '{args.folder}' does not exist")
        return 1
    
    # Determine output filename
    if args.output:
        output_file = args.output
    else:
        folder_name = os.path.basename(args.folder.rstrip(os.sep))
        output_file = f"{folder_name}_audio.bin"
    
    # Convert flash address
    if args.address.startswith('0x'):
        flash_address = int(args.address, 16)
    else:
        flash_address = int(args.address)
    
    # Create multi-audio binary
    if not create_multi_audio_binary(
        args.folder, 
        output_file, 
        sample_rate=args.rate,
        normalize=not args.no_normalize,
        max_length_ms=args.max_length,
        force_mono=not args.stereo
    ):
        return 1
    
    # Generate C defines header if requested
    if args.c_defines:
        if not generate_c_defines(output_file, args.c_defines, flash_address):
            return 1
    
    # Generate picotool script if requested  
    if args.script:
        if not generate_picotool_script(output_file, args.script, flash_address):
            return 1
    
    print(f"\nSuccess! Use the binary with your Pi Pico audio player.")
    return 0

if __name__ == "__main__":
    exit(main())
