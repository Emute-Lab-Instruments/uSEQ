#!/usr/bin/env python3
"""
Test incremental build correctness for uSEQ build system.
Verifies that only affected files are rebuilt when sources change.
"""

import os
import sys
import time
import tempfile
import shutil
import subprocess
import hashlib
from pathlib import Path

def run_command(cmd, cwd=None):
    """Run a command and return output, return code."""
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd=cwd)
    return result.stdout, result.stderr, result.returncode

def get_file_hash(filepath):
    """Get hash of a file for change detection."""
    if not os.path.exists(filepath):
        return None
    with open(filepath, 'rb') as f:
        return hashlib.md5(f.read()).hexdigest()

def get_build_artifacts_hashes(build_dir):
    """Get hashes of all build artifacts."""
    hashes = {}
    build_path = Path(build_dir)
    if build_path.exists():
        for file in build_path.rglob('*.o'):
            hashes[str(file.relative_to(build_path))] = get_file_hash(file)
        for file in build_path.rglob('*.a'):
            hashes[str(file.relative_to(build_path))] = get_file_hash(file)
        for file in build_path.rglob('*.so'):
            hashes[str(file.relative_to(build_path))] = get_file_hash(file)
    return hashes

def test_incremental_build():
    """Test that incremental builds work correctly."""
    project_root = Path(__file__).parent.parent.parent
    test_build_dir = project_root / "test_incremental_build"
    
    print("Testing incremental build correctness...")
    
    try:
        # Clean any existing test build
        if test_build_dir.exists():
            shutil.rmtree(test_build_dir)
        
        # Initial build
        print("  1. Performing initial build...")
        stdout, stderr, retcode = run_command(
            f"meson setup {test_build_dir} --buildtype=debug",
            cwd=project_root
        )
        if retcode != 0:
            print(f"    ✗ Initial setup failed: {stderr}")
            return False
        
        stdout, stderr, retcode = run_command(
            f"ninja -C {test_build_dir}",
            cwd=project_root
        )
        if retcode != 0:
            print(f"    ✗ Initial build failed: {stderr}")
            return False
        
        # Get initial build artifacts
        initial_hashes = get_build_artifacts_hashes(test_build_dir)
        initial_time = time.time()
        
        # Sleep to ensure timestamp difference
        time.sleep(1)
        
        # Touch a header file that should trigger rebuild of dependent files
        print("  2. Modifying header file (value.h)...")
        value_h = project_root / "uSEQ/src/modulisp/lisp/value.h"
        os.utime(value_h, None)  # Touch the file
        
        # Rebuild
        print("  3. Performing incremental build...")
        rebuild_start = time.time()
        stdout, stderr, retcode = run_command(
            f"ninja -C {test_build_dir}",
            cwd=project_root
        )
        rebuild_time = time.time() - rebuild_start
        
        if retcode != 0:
            print(f"    ✗ Incremental build failed: {stderr}")
            return False
        
        # Get rebuilt artifacts
        rebuilt_hashes = get_build_artifacts_hashes(test_build_dir)
        
        # Check that some files were rebuilt (value.o should change)
        changed_files = []
        unchanged_files = []
        for file, hash_val in rebuilt_hashes.items():
            if file in initial_hashes:
                if initial_hashes[file] != hash_val:
                    changed_files.append(file)
                else:
                    unchanged_files.append(file)
        
        # Verify that value.o was rebuilt but not all files
        value_objects = [f for f in changed_files if 'value' in f.lower()]
        if not value_objects:
            print(f"    ✗ Expected value.o to be rebuilt but it wasn't")
            return False
        
        if len(unchanged_files) == 0:
            print(f"    ✗ All files were rebuilt (expected some to remain unchanged)")
            return False
        
        print(f"    ✓ Incremental build working: {len(changed_files)} files rebuilt, {len(unchanged_files)} unchanged")
        print(f"      Rebuild time: {rebuild_time:.2f}s")
        
        # Test that no-op rebuild is fast
        print("  4. Testing no-op rebuild...")
        noop_start = time.time()
        stdout, stderr, retcode = run_command(
            f"ninja -C {test_build_dir}",
            cwd=project_root
        )
        noop_time = time.time() - noop_start
        
        if retcode != 0:
            print(f"    ✗ No-op rebuild failed: {stderr}")
            return False
        
        if "ninja: no work to do" not in stdout:
            print(f"    ✗ Expected no work to do, but ninja did work")
            return False
        
        print(f"    ✓ No-op rebuild successful: {noop_time:.2f}s")
        
        return True
        
    except Exception as e:
        print(f"    ✗ Exception during test: {e}")
        return False
    finally:
        # Cleanup
        if test_build_dir.exists():
            shutil.rmtree(test_build_dir)

if __name__ == "__main__":
    success = test_incremental_build()
    sys.exit(0 if success else 1)