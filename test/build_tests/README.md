# Build System Regression Tests

This directory contains automated regression tests for the uSEQ build system. These tests ensure that build optimizations and changes don't break core functionality.

## Running Tests

### Run all tests:
```bash
python test/build_tests/run_build_tests.py
```

### Run individual tests:
```bash
python test/build_tests/test_incremental_build.py
python test/build_tests/test_pch_functionality.py
python test/build_tests/test_library_linking.py
```

## Test Coverage

### 1. Incremental Build Correctness (`test_incremental_build.py`)
- Verifies that only affected files are rebuilt when sources change
- Tests that header changes trigger appropriate rebuilds
- Ensures no-op builds are fast

### 2. PCH Functionality (`test_pch_functionality.py`)
- Tests precompiled header generation and usage
- Compares build times with and without PCH
- Verifies that executables work correctly with PCH

### 3. Library Linking (`test_library_linking.py`)
- Verifies static libraries are correctly built
- Tests that executables link properly with libraries
- Checks library dependency tracking

## Requirements

- Python 3.6+
- Meson build system
- Ninja build tool
- C++ compiler (g++ or clang++)

## Test Output

Tests use color-coded output:
- ✓ Green: Test passed
- ✗ Red: Test failed
- ℹ Blue: Informational message

## Adding New Tests

1. Create a new test file: `test_<feature>.py`
2. Implement the test function that returns `True` on success
3. Add proper cleanup in a `finally` block
4. The test will automatically be discovered by `run_build_tests.py`

## Continuous Integration

These tests should be run:
- Before merging build system changes
- After major refactoring
- As part of CI/CD pipeline
- When debugging build issues