#!/usr/bin/env sh

# Script to run all uSEQ Tests (signal engine, firmware, golden semantics)
# Usage: ./scripts/test.sh [options]
#
# Options:
#   -v, --verbose     Show detailed test output
#   -f, --fast        Skip build step (run tests only)
#   -s, --single TEST Run only a specific test suite
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
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
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

# Available test suites (must match meson test names without the 'uSEQ:' prefix)
AVAILABLE_TESTS="signal_engine firmware_e2e firmware_e2e_part2 firmware_fuzz signal_engine_golden signal_engine_phase4 signal_engine_robustness signal_engine_probe_smoke flash_storage wire_protocol_contract devtools_contract live_edit output_classification ugen state_identity"

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
                        signal_engine          - Core signal engine tests
                        signal_engine_golden   - Golden semantic tests (data-driven)
                        signal_engine_phase4   - Advanced feature tests
                        signal_engine_robustness - Fuzz and stress tests
                        firmware_e2e           - Firmware end-to-end tests (part 1)
                        firmware_e2e_part2     - Firmware end-to-end tests (part 2)
                        firmware_fuzz          - Firmware fuzz tests
                        flash_storage          - Flash persistence round-trips
                        wire_protocol_contract - Wire protocol contract tests
                        devtools_contract      - Devtools debug protocol tests
                        live_edit              - Live-edit slot tests
                        output_classification  - Output type classification
                        ugen                   - Unit generator tests
                        state_identity         - State identity tests
    -h, --help        Show this help message

EXAMPLES:
    ./scripts/test.sh                              # Run all tests with build
    ./scripts/test.sh -v                           # Run all tests with verbose output
    ./scripts/test.sh -f                           # Run tests without building (fast)
    ./scripts/test.sh -s signal_engine             # Run only signal engine tests
    ./scripts/test.sh -s signal_engine_golden -v   # Run golden tests with verbose output

EOF
}

# Parse command line arguments
while [ $# -gt 0 ]; do
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
if [ -n "$SINGLE_TEST" ]; then
    valid=false
    for t in $AVAILABLE_TESTS; do
        if [ "$t" = "$SINGLE_TEST" ]; then
            valid=true
            break
        fi
    done
    if [ "$valid" = "false" ]; then
        print_error "Invalid test name: $SINGLE_TEST"
        print_error "Valid options: $AVAILABLE_TESTS"
        exit 1
    fi
fi

# Change to project root
cd "$PROJECT_ROOT"

print_status "uSEQ Unified Test Runner"
print_status "Project root: $PROJECT_ROOT"

# Build tests unless skipped
if [ "$SKIP_BUILD" = "false" ]; then
    print_status "Setting up build environment..."

    if [ ! -f "$BUILD_DIR/build.ninja" ]; then
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
    if [ ! -d "$BUILD_DIR" ]; then
        print_error "Build directory '$BUILD_DIR' not found. Run without --fast first."
        exit 1
    fi
fi

# Run tests via Meson
print_status "Running tests via Meson..."

meson_args=""
if [ "$VERBOSE" = "true" ]; then
    meson_args="-v"
fi

if [ -n "$SINGLE_TEST" ]; then
    # Map short name to meson test name
    meson_test_name="${SINGLE_TEST}_test"
    # Special cases where the meson name doesn't follow the pattern
    case "$SINGLE_TEST" in
        signal_engine)        meson_test_name="signal_engine_test" ;;
        signal_engine_golden) meson_test_name="signal_engine_golden_test" ;;
        signal_engine_phase4) meson_test_name="signal_engine_phase4_test" ;;
        signal_engine_robustness) meson_test_name="signal_engine_robustness_test" ;;
        signal_engine_probe_smoke) meson_test_name="signal_engine_probe_smoke" ;;
        firmware_e2e)         meson_test_name="firmware_e2e_test" ;;
        firmware_e2e_part2)   meson_test_name="firmware_e2e_test_part2" ;;
        firmware_fuzz)        meson_test_name="firmware_fuzz_test" ;;
        flash_storage)        meson_test_name="flash_storage_test" ;;
        wire_protocol_contract) meson_test_name="wire_protocol_contract_test" ;;
        devtools_contract)    meson_test_name="devtools_contract_test" ;;
        live_edit)            meson_test_name="live_edit_test" ;;
        output_classification) meson_test_name="output_classification_test" ;;
        ugen)                 meson_test_name="ugen_test" ;;
        state_identity)       meson_test_name="state_identity_test" ;;
    esac

    print_status "Running test: $meson_test_name"
    if meson test -C "$BUILD_DIR" $meson_args "$meson_test_name"; then
        print_success "Test '$SINGLE_TEST' passed"
    else
        print_error "Test '$SINGLE_TEST' failed"
        print_status "Check detailed log: $BUILD_DIR/meson-logs/testlog.txt"
        exit 1
    fi
else
    # Run all tests
    if meson test -C "$BUILD_DIR" $meson_args; then
        echo
        print_success "All tests passed!"
        echo
        print_status "Test Coverage:"
        echo "  - Signal Engine        - Core compiler + executor"
        echo "  - Signal Engine Golden - Data-driven semantic tests"
        echo "  - Signal Engine Phase4 - Advanced features"
        echo "  - Signal Engine Robustness - Fuzz and stress tests"
        echo "  - Firmware E2E         - Full tick-loop integration"
        echo "  - Firmware Fuzz        - Firmware fuzz tests"
        echo "  - Flash Storage        - Persistence round-trips"
        echo "  - Wire Protocol        - Serial protocol contract"
        echo "  - Devtools Contract    - Debug protocol tests"
        echo "  - Live Edit            - Live-edit slot tests"
        echo "  - Output Classification - Output type classification"
        echo "  - UGen                 - Unit generator tests"
        echo "  - State Identity       - State identity tests"
    else
        echo
        print_error "Some tests failed"
        print_status "Check detailed log: $BUILD_DIR/meson-logs/testlog.txt"
        print_status "To debug: ./scripts/test.sh -s <test_name> -v"
        exit 1
    fi
fi
