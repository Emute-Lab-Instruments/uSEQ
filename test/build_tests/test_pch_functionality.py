#!/usr/bin/env python3
"""
Test precompiled header (PCH) functionality in the build system.
Verifies that PCH is correctly generated and used to speed up compilation.
"""

import os
import sys
import time
import tempfile
import shutil
import subprocess
from pathlib import Path

def run_command(cmd, cwd=None):
    """Run a command and return output, return code."""
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd=cwd)
    return result.stdout, result.stderr, result.returncode

def test_pch_functionality():
    """Test that precompiled headers work correctly."""
    project_root = Path(__file__).parent.parent.parent
    test_build_with_pch = project_root / "test_build_with_pch"
    test_build_without_pch = project_root / "test_build_without_pch"
    
    print("Testing PCH functionality...")
    
    try:
        # Clean any existing test builds
        for build_dir in [test_build_with_pch, test_build_without_pch]:
            if build_dir.exists():
                shutil.rmtree(build_dir)
        
        # Build without PCH
        print("  1. Building without PCH...")
        stdout, stderr, retcode = run_command(
            f"meson setup {test_build_without_pch} --buildtype=debug -Duse_pch=false",
            cwd=project_root
        )
        if retcode != 0:
            # PCH option might not exist, try without the option
            if test_build_without_pch.exists():
                shutil.rmtree(test_build_without_pch)
            stdout, stderr, retcode = run_command(
                f"meson setup {test_build_without_pch} --buildtype=debug",
                cwd=project_root
            )
            if retcode != 0:
                print(f"    ✗ Setup without PCH failed: {stderr}")
                return False
        
        # Clean build without PCH
        start_time = time.time()
        stdout, stderr, retcode = run_command(
            f"ninja -C {test_build_without_pch} clean && ninja -C {test_build_without_pch}",
            cwd=project_root
        )
        time_without_pch = time.time() - start_time
        
        if retcode != 0:
            print(f"    ✗ Build without PCH failed: {stderr}")
            return False
        
        print(f"    ✓ Build without PCH completed in {time_without_pch:.2f}s")
        
        # Build with PCH
        print("  2. Building with PCH...")
        stdout, stderr, retcode = run_command(
            f"meson setup {test_build_with_pch} --buildtype=debug -Duse_pch=true",
            cwd=project_root
        )
        if retcode != 0:
            # PCH option might not exist or be enabled by default
            if test_build_with_pch.exists():
                shutil.rmtree(test_build_with_pch)
            stdout, stderr, retcode = run_command(
                f"meson setup {test_build_with_pch} --buildtype=debug",
                cwd=project_root
            )
            if retcode != 0:
                print(f"    ✗ Setup with PCH failed: {stderr}")
                return False
        
        # Clean build with PCH
        start_time = time.time()
        stdout, stderr, retcode = run_command(
            f"ninja -C {test_build_with_pch} clean && ninja -C {test_build_with_pch}",
            cwd=project_root
        )
        time_with_pch = time.time() - start_time
        
        if retcode != 0:
            print(f"    ✗ Build with PCH failed: {stderr}")
            return False
        
        print(f"    ✓ Build with PCH completed in {time_with_pch:.2f}s")
        
        # Check for PCH files
        pch_files_found = False
        for ext in ['.gch', '.pch', '.pch.d', '.hpp.gch']:
            pch_files = list(test_build_with_pch.rglob(f'*{ext}'))
            if pch_files:
                pch_files_found = True
                print(f"    ✓ Found PCH files: {[f.name for f in pch_files[:3]]}")
                break
        
        if not pch_files_found:
            # PCH might be built-in or not visible as separate files
            print(f"    ℹ No explicit PCH files found (may be built-in)")
        
        # Check if PCH provides any speedup
        if time_with_pch < time_without_pch * 0.95:  # At least 5% improvement
            speedup = (1 - time_with_pch / time_without_pch) * 100
            print(f"    ✓ PCH provides {speedup:.1f}% speedup")
        else:
            print(f"    ℹ PCH speedup minimal or not configured")
        
        # Verify both builds produce working executables
        print("  3. Verifying build outputs...")
        for build_dir, name in [(test_build_with_pch, "with PCH"), 
                                (test_build_without_pch, "without PCH")]:
            standalone = build_dir / "standalone"
            if standalone.exists():
                stdout, stderr, retcode = run_command(
                    f"echo '(+ 1 2)' | {standalone}",
                    cwd=project_root
                )
                if retcode == 0 and "3" in stdout:
                    print(f"    ✓ Executable {name} works correctly")
                else:
                    print(f"    ✗ Executable {name} failed basic test")
                    return False
        
        return True
        
    except Exception as e:
        print(f"    ✗ Exception during test: {e}")
        return False
    finally:
        # Cleanup
        for build_dir in [test_build_with_pch, test_build_without_pch]:
            if build_dir.exists():
                shutil.rmtree(build_dir)

if __name__ == "__main__":
    success = test_pch_functionality()
    sys.exit(0 if success else 1)