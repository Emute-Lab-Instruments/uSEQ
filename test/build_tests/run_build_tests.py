#!/usr/bin/env python3
"""
Main test runner for build system regression tests.
Runs all build system tests and reports results.
"""

import os
import sys
import time
import subprocess
from pathlib import Path
from typing import List, Tuple

# ANSI color codes for output
class Colors:
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'

def run_test(test_script: Path) -> Tuple[bool, float]:
    """Run a single test script and return success status and time taken."""
    start_time = time.time()
    result = subprocess.run(
        [sys.executable, str(test_script)],
        capture_output=True,
        text=True
    )
    elapsed_time = time.time() - start_time
    success = result.returncode == 0
    
    if not success:
        print(result.stdout)
        if result.stderr:
            print(result.stderr)
    
    return success, elapsed_time

def main():
    """Run all build system regression tests."""
    test_dir = Path(__file__).parent
    project_root = test_dir.parent.parent
    
    # Find all test scripts
    test_scripts = sorted([
        f for f in test_dir.glob("test_*.py")
        if f != Path(__file__)
    ])
    
    if not test_scripts:
        print(f"{Colors.RED}✗ No test scripts found in {test_dir}{Colors.ENDC}")
        return 1
    
    print(f"{Colors.BOLD}Build System Regression Tests{Colors.ENDC}")
    print(f"Running {len(test_scripts)} tests from {test_dir.relative_to(project_root)}")
    print("=" * 60)
    
    results = []
    total_time = 0.0
    
    for test_script in test_scripts:
        test_name = test_script.stem.replace('test_', '').replace('_', ' ').title()
        print(f"\n{Colors.BLUE}Running:{Colors.ENDC} {test_name}")
        
        success, elapsed = run_test(test_script)
        results.append((test_name, success, elapsed))
        total_time += elapsed
        
        if success:
            print(f"{Colors.GREEN}✓ PASSED{Colors.ENDC} ({elapsed:.2f}s)")
        else:
            print(f"{Colors.RED}✗ FAILED{Colors.ENDC} ({elapsed:.2f}s)")
    
    # Print summary
    print("\n" + "=" * 60)
    print(f"{Colors.BOLD}Test Summary{Colors.ENDC}")
    print("=" * 60)
    
    passed = sum(1 for _, success, _ in results if success)
    failed = len(results) - passed
    
    for test_name, success, elapsed in results:
        status = f"{Colors.GREEN}✓ PASS{Colors.ENDC}" if success else f"{Colors.RED}✗ FAIL{Colors.ENDC}"
        print(f"  {status} {test_name:<40} ({elapsed:.2f}s)")
    
    print("-" * 60)
    
    if failed == 0:
        print(f"{Colors.GREEN}{Colors.BOLD}All tests passed!{Colors.ENDC}")
        status_color = Colors.GREEN
    else:
        print(f"{Colors.RED}{Colors.BOLD}{failed} test(s) failed{Colors.ENDC}")
        status_color = Colors.RED
    
    print(f"\nTotal: {status_color}{passed}/{len(results)}{Colors.ENDC} passed in {total_time:.2f}s")
    
    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main())