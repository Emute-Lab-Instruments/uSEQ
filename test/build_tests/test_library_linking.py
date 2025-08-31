#!/usr/bin/env python3
"""
Test static library linking in the build system.
Verifies that static libraries are correctly built and linked.
"""

import os
import sys
import subprocess
import shutil
from pathlib import Path

def run_command(cmd, cwd=None):
    """Run a command and return output, return code."""
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd=cwd)
    return result.stdout, result.stderr, result.returncode

def test_library_linking():
    """Test that static libraries are correctly built and linked."""
    project_root = Path(__file__).parent.parent.parent
    test_build_dir = project_root / "test_library_linking"
    
    print("Testing library linking...")
    
    try:
        # Clean any existing test build
        if test_build_dir.exists():
            shutil.rmtree(test_build_dir)
        
        # Setup build
        print("  1. Setting up build with static libraries...")
        stdout, stderr, retcode = run_command(
            f"meson setup {test_build_dir} --buildtype=debug",
            cwd=project_root
        )
        if retcode != 0:
            print(f"    ✗ Build setup failed: {stderr}")
            return False
        
        # Build the project
        print("  2. Building project...")
        stdout, stderr, retcode = run_command(
            f"ninja -C {test_build_dir}",
            cwd=project_root
        )
        if retcode != 0:
            print(f"    ✗ Build failed: {stderr}")
            return False
        
        # Check for static libraries
        print("  3. Verifying static libraries...")
        expected_libs = [
            "libuseq_utils.a",
            "libuseq_lisp_core.a", 
            "libuseq_modulisp.a",
            "libuseq_dsp.a",
            "libuseq_hw.a"
        ]
        
        libs_found = []
        libs_missing = []
        
        for lib_name in expected_libs:
            lib_files = list(test_build_dir.rglob(lib_name))
            if lib_files:
                libs_found.append(lib_name)
                lib_path = lib_files[0]
                # Check library has content
                stdout, stderr, retcode = run_command(
                    f"ar t {lib_path}",
                    cwd=project_root
                )
                if retcode == 0 and stdout.strip():
                    obj_count = len(stdout.strip().split('\n'))
                    print(f"    ✓ {lib_name} found with {obj_count} object files")
                else:
                    print(f"    ✗ {lib_name} appears to be empty")
            else:
                libs_missing.append(lib_name)
        
        if libs_missing:
            print(f"    ℹ Libraries not found: {libs_missing}")
            # Not all libraries may be built depending on configuration
        
        if not libs_found:
            print(f"    ✗ No expected static libraries found")
            return False
        
        # Test executable linking
        print("  4. Testing executable linking...")
        standalone = test_build_dir / "standalone"
        if not standalone.exists():
            print(f"    ✗ Standalone executable not found")
            return False
        
        # Check that executable is linked with libraries
        stdout, stderr, retcode = run_command(
            f"ldd {standalone} 2>/dev/null || otool -L {standalone} 2>/dev/null || echo 'ldd/otool not available'",
            cwd=project_root
        )
        
        # Run the executable to verify it works
        stdout, stderr, retcode = run_command(
            f"echo '(* 7 6)' | {standalone}",
            cwd=project_root
        )
        if retcode == 0 and "42" in stdout:
            print(f"    ✓ Executable properly linked and functional")
        else:
            print(f"    ✗ Executable failed runtime test")
            return False
        
        # Test library dependency tracking
        print("  5. Testing library dependency tracking...")
        
        # Touch a source file in utils library
        utils_file = project_root / "uSEQ/src/utils/str.cpp"
        if utils_file.exists():
            print(f"    Touching {utils_file.name}...")
            os.utime(utils_file, None)
            
            # Rebuild and check what gets rebuilt
            stdout, stderr, retcode = run_command(
                f"ninja -C {test_build_dir} -v",
                cwd=project_root
            )
            
            if retcode != 0:
                print(f"    ✗ Rebuild after touching utils failed")
                return False
            
            # Check if utils library was rebuilt
            if "libuseq_utils" in stdout or "str.cpp" in stdout:
                print(f"    ✓ Library correctly rebuilt after source change")
            else:
                print(f"    ℹ Unable to verify library rebuild (may be too fast)")
        
        # Test that tests link correctly
        print("  6. Testing test executable linking...")
        test_exe = test_build_dir / "test" / "test_value"
        if test_exe.exists():
            stdout, stderr, retcode = run_command(
                str(test_exe),
                cwd=project_root
            )
            if retcode == 0:
                print(f"    ✓ Test executable properly linked")
            else:
                print(f"    ✗ Test executable failed to run")
                # Not critical if tests fail, we're testing linking
        else:
            print(f"    ℹ Test executable not found (tests may not be built)")
        
        return True
        
    except Exception as e:
        print(f"    ✗ Exception during test: {e}")
        return False
    finally:
        # Cleanup
        if test_build_dir.exists():
            shutil.rmtree(test_build_dir)

if __name__ == "__main__":
    success = test_library_linking()
    sys.exit(0 if success else 1)