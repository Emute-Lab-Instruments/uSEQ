#!/usr/bin/env sh

# Script to run all uSEQ Tests (API, builtin functions, parser)
# Usage: ./scripts/test.sh [options]
#
# Options:
#   -v, --verbose     Show detailed test output
#   -f, --fast        Skip build step (run tests only)
#   -s, --single TEST Run only a specific test (value, environment, parser, interpreter, builtins, parser_unit)
#   -h, --help        Show this help message

set -e  # Exit on any error

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default options
VERBOSE=false
SKIP_BUILD=false
SINGLE_TEST=""
BUILD_DIR="build"

# Script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

print_error() {
    echo -e "${RED}[FAIL]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

# Function to show help
show_help() {
    cat << EOF
uSEQ Unified Test Runner

USAGE:
    ./scripts/test.sh [OPTIONS]

OPTIONS:
    -v, --verbose     Show detailed test output including individual assertions
    -f, --fast        Skip build step and run tests only (assumes tests are built)
    -s, --single TEST Run only a specific test suite:
                        value       - Value API tests only
                        environment - Environment API tests only  
                        parser      - Parser API tests only
                        interpreter - Interpreter API tests only
                        builtins    - Builtin functions tests only
                        parser_unit - Parser unit tests only
    -h, --help        Show this help message

EXAMPLES:
    ./scripts/test.sh                      # Run all tests with build
    ./scripts/test.sh -v                   # Run all tests with verbose output
    ./scripts/test.sh -f                   # Run tests without building (fast)
    ./scripts/test.sh -s value             # Run only Value API tests
    ./scripts/test.sh -s builtins -v       # Run builtin tests with verbose output

EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -f|--fast)
            SKIP_BUILD=true
            shift
            ;;
        -s|--single)
            SINGLE_TEST="$2"
            shift 2
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            echo "Use -h or --help for usage information."
            exit 1
            ;;
    esac
done

# Validate single test option
if [[ -n "$SINGLE_TEST" ]]; then
    case "$SINGLE_TEST" in
        value|environment|parser|interpreter|builtins|parser_unit)
            ;;
        *)
            print_error "Invalid test name: $SINGLE_TEST"
            print_error "Valid options: value, environment, parser, interpreter, builtins, parser_unit"
            exit 1
            ;;
    esac
fi

# Change to project root
cd "$PROJECT_ROOT"

print_status "uSEQ Unified Test Runner"
print_status "Project root: $PROJECT_ROOT"

# Check if build directory exists
if [[ ! -d "$BUILD_DIR" ]]; then
    print_warning "Build directory '$BUILD_DIR' not found. Creating it..."
    SKIP_BUILD=false
fi

# Build tests unless skipped
if [[ "$SKIP_BUILD" == "false" ]]; then
    print_status "Setting up build environment..."
    
    if [[ ! -f "$BUILD_DIR/build.ninja" ]]; then
        print_status "Configuring build with Meson..."
        meson setup "$BUILD_DIR" --reconfigure
    fi
    
    print_status "Building all tests..."
    if ninja -j4 -C "$BUILD_DIR"; then
        print_success "Build completed successfully"
    else
        print_error "Build failed"
        exit 1
    fi
else
    print_status "Skipping build step (--fast mode)"
fi

# Function to run a single test executable
run_test() {
    local test_name=$1
    local test_executable=""
    local test_description=""
    
    # Map test names to executables and descriptions
    case "$test_name" in
        value|environment|parser|interpreter)
            test_executable="$BUILD_DIR/test/test_${test_name}_api"
            test_description="${test_name} API tests"
            ;;
        builtins)
            test_executable="$BUILD_DIR/test/test_builtins"
            test_description="Builtin functions tests"
            ;;
        parser_unit)
            test_executable="$BUILD_DIR/test/test_parser"
            test_description="Parser unit tests"
            ;;
        *)
            print_error "Unknown test type: $test_name"
            return 1
            ;;
    esac
    
    if [[ ! -f "$test_executable" ]]; then
        print_error "Test executable not found: $test_executable"
        return 1
    fi
    
    print_status "Running ${test_description}..."
    
    if [[ "$VERBOSE" == "true" ]]; then
        # Run with full output
        if "$test_executable"; then
            print_success "${test_description} passed"
            return 0
        else
            print_error "${test_description} failed"
            return 1
        fi
    else
        # Capture output and show summary
        local output
        if output=$("$test_executable" 2>&1); then
            # Count test cases by looking for "passed!" messages
            local test_count=$(echo "$output" | grep -c "passed!" || echo "0")
            print_success "${test_description} passed ($test_count test cases)"
            return 0
        else
            print_error "${test_description} failed"
            echo "$output" | tail -10  # Show last 10 lines of output for debugging
            return 1
        fi
    fi
}

# Function to run all tests using Meson
run_all_tests_meson() {
    print_status "Running all tests via Meson..."
    
    local meson_cmd="meson test -C $BUILD_DIR"
    
    # Add verbose flag if requested
    if [[ "$VERBOSE" == "true" ]]; then
        meson_cmd="$meson_cmd -v"
    fi
    
    # All available tests (comprehensive test suite)
    local all_tests="value_api_test environment_api_test parser_api_test interpreter_api_test builtin_functions_test parser_test modulisp_api_test time_injection_test io_bridge_test logging_bridge_test i2c_bus_test storage_env_test output_manager_test test_with_helpers"
    
    if $meson_cmd $all_tests; then
        print_success "All tests passed"
        return 0
    else
        print_error "Some tests failed"
        print_status "Check detailed log: $BUILD_DIR/meson-logs/testlog.txt"
        return 1
    fi
}

# Main test execution
failed_tests=()
total_tests=0

if [[ -n "$SINGLE_TEST" ]]; then
    # Run single test
    total_tests=1
    if ! run_test "$SINGLE_TEST"; then
        failed_tests+=("$SINGLE_TEST")
    fi
else
    # Run all tests
    if [[ "$VERBOSE" == "true" ]]; then
        # Run individually for verbose output
        test_suites=("value" "environment" "parser" "interpreter" "builtins" "parser_unit")
        total_tests=${#test_suites[@]}
        
        for test_suite in "${test_suites[@]}"; do
            if ! run_test "$test_suite"; then
                failed_tests+=("$test_suite")
            fi
        done
    else
        # Use Meson for efficient batch execution
        total_tests=15
        if ! run_all_tests_meson; then
            # If Meson fails, mark that we have failures (we can't easily identify which specific ones)
            # by adding a placeholder to failed_tests array
            failed_tests+=("meson_batch_tests")
            
            # Optional: try individual tests to identify which basic ones failed
            print_status "Identifying failed tests..."
            test_suites=("value" "environment" "parser" "interpreter" "builtins" "parser_unit")
            for test_suite in "${test_suites[@]}"; do
                if ! run_test "$test_suite" >/dev/null 2>&1; then
                    failed_tests+=("$test_suite")
                fi
            done
        fi
    fi
fi

# Print summary
echo
print_status "=== Test Summary ==="
echo "Total test suites: $total_tests"
echo "Passed: $((total_tests - ${#failed_tests[@]}))"
echo "Failed: ${#failed_tests[@]}"

if [[ ${#failed_tests[@]} -eq 0 ]]; then
    print_success "All tests passed! ✓"
    echo
    print_status "Test Coverage:"
    echo "  ✓ Value API        - Construction, type checking, conversions, operators"
    echo "  ✓ Environment API  - Variable storage, scoping, inheritance"  
    echo "  ✓ Parser API       - String parsing, structure recognition, utilities"
    echo "  ✓ Interpreter API  - Expression evaluation, function application"
    echo "  ✓ Builtins         - Builtin function implementations"
    echo "  ✓ Parser Unit      - Core parser functionality and edge cases"
    echo "  ✓ ModuLisp API     - Module-specific LISP functionality"
    echo "  ✓ I/O Bridge       - Hardware I/O abstraction layer"
    echo "  ✓ I2C Bus          - Inter-module communication"
    echo "  ✓ Storage Env      - Persistent environment storage"
    echo "  ✓ Output Manager   - Output signal management"
    echo "  ✓ Logging Bridge   - Logging system integration"
    echo "  ✓ Time Injection   - Time-dependent functionality"
    echo "  ✓ Test Helpers     - Minimal interface testing"
    exit 0
else
    print_error "Failed test suites: ${failed_tests[*]}"
    echo
    print_status "To debug failures:"
    echo "  1. Run individual test: ./scripts/test.sh -s <test_name> -v"
    echo "  2. Run test directly: ./$BUILD_DIR/test/test_<executable>"
    echo "  3. Check build logs: $BUILD_DIR/meson-logs/"
    exit 1
fi
