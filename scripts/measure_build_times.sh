#!/usr/bin/env bash

# Build Time Measurement Tool for uSEQ Project
# Measures and tracks compilation times across different build configurations

set -euo pipefail

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Default values
BUILD_DIR="build"
OUTPUT_FILE=""
VERBOSE=false
CONFIGURATIONS=("debug" "release" "minsize")
UNITY_BUILDS=("false" "true")
CCACHE_MODES=("false" "true")

# Function to print colored output
print_color() {
    local color=$1
    shift
    echo -e "${color}$*${NC}"
}

# Function to display usage
usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Measure and track uSEQ project build times across different configurations.

OPTIONS:
    -h, --help              Show this help message
    -o, --output FILE       Save results to file (JSON format)
    -b, --build-dir DIR     Build directory (default: build)
    -c, --config CONFIG     Test specific configuration (debug/release/minsize)
    -v, --verbose           Show detailed output
    --clean-only            Measure only clean build times
    --incremental-only      Measure only incremental build times
    --quick                 Quick test with default configuration only

EXAMPLES:
    $0                      # Run all measurements
    $0 -o results.json      # Save results to file
    $0 -c release           # Test only release configuration
    $0 --quick              # Quick test with defaults
    $0 --clean-only         # Measure only clean builds

EOF
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                usage
                exit 0
                ;;
            -o|--output)
                OUTPUT_FILE="$2"
                shift 2
                ;;
            -b|--build-dir)
                BUILD_DIR="$2"
                shift 2
                ;;
            -c|--config)
                CONFIGURATIONS=("$2")
                shift 2
                ;;
            -v|--verbose)
                VERBOSE=true
                shift
                ;;
            --clean-only)
                TEST_MODE="clean"
                shift
                ;;
            --incremental-only)
                TEST_MODE="incremental"
                shift
                ;;
            --quick)
                CONFIGURATIONS=("release")
                UNITY_BUILDS=("false")
                CCACHE_MODES=("true")
                shift
                ;;
            *)
                print_color "$RED" "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done
}

# Function to measure build time
measure_build_time() {
    local start_time end_time duration
    start_time=$(date +%s)
    
    if [[ "$VERBOSE" == "true" ]]; then
        "$@"
    else
        "$@" > /dev/null 2>&1
    fi
    
    local exit_code=$?
    end_time=$(date +%s)
    duration=$((end_time - start_time))
    
    echo "$duration"
    return $exit_code
}

# Function to get file modification time
get_file_mtime() {
    local file=$1
    if [[ -f "$file" ]]; then
        stat -c %Y "$file" 2>/dev/null || stat -f %m "$file" 2>/dev/null || echo "0"
    else
        echo "0"
    fi
}

# Function to perform clean build measurement
measure_clean_build() {
    local config=$1
    local unity=$2
    local ccache=$3
    local build_name="${config}_unity${unity}_ccache${ccache}"
    
    print_color "$CYAN" "Measuring clean build: $build_name"
    
    # Remove build directory
    rm -rf "$BUILD_DIR"
    
    # Configure build
    local meson_args="--buildtype=$config"
    [[ "$unity" == "true" ]] && meson_args="$meson_args -Dunity_build=true"
    [[ "$ccache" == "true" ]] && meson_args="$meson_args -Duse_ccache=true" || meson_args="$meson_args -Duse_ccache=false"
    
    # Measure configuration time
    local config_time
    config_time=$(measure_build_time meson setup "$BUILD_DIR" $meson_args)
    
    # Measure build time
    local build_time
    build_time=$(measure_build_time ninja -j4 -C "$BUILD_DIR")
    
    local total_time=$((config_time + build_time))
    
    echo "{\"type\":\"clean\",\"config\":\"$config\",\"unity\":$unity,\"ccache\":$ccache,\"config_time\":$config_time,\"build_time\":$build_time,\"total_time\":$total_time}"
}

# Function to perform incremental build measurement
measure_incremental_build() {
    local config=$1
    local unity=$2
    local ccache=$3
    local build_name="${config}_unity${unity}_ccache${ccache}"
    
    print_color "$CYAN" "Measuring incremental build: $build_name"
    
    # Ensure we have a clean build first
    if [[ ! -d "$BUILD_DIR" ]]; then
        rm -rf "$BUILD_DIR"
        local meson_args="--buildtype=$config"
        [[ "$unity" == "true" ]] && meson_args="$meson_args -Dunity_build=true"
        [[ "$ccache" == "true" ]] && meson_args="$meson_args -Duse_ccache=true" || meson_args="$meson_args -Duse_ccache=false"
        
        meson setup "$BUILD_DIR" $meson_args > /dev/null 2>&1
        ninja -j4 -C "$BUILD_DIR" > /dev/null 2>&1
    fi
    
    # Touch a source file to trigger rebuild
    local test_file="uSEQ/src/modulisp/lisp/value.cpp"
    if [[ -f "$test_file" ]]; then
        touch "$test_file"
    else
        print_color "$YELLOW" "Warning: Test file not found, using first .cpp file"
        test_file=$(find uSEQ/src -name "*.cpp" -type f | head -n 1)
        touch "$test_file"
    fi
    
    # Measure incremental build time
    local build_time
    build_time=$(measure_build_time ninja -j4 -C "$BUILD_DIR")
    
    echo "{\"type\":\"incremental\",\"config\":\"$config\",\"unity\":$unity,\"ccache\":$ccache,\"build_time\":$build_time,\"modified_file\":\"$test_file\"}"
}

# Function to measure per-file compilation times
measure_per_file_times() {
    local config=$1
    
    print_color "$CYAN" "Measuring per-file compilation times for $config"
    
    # Clean build with timing
    rm -rf "$BUILD_DIR"
    meson setup "$BUILD_DIR" --buildtype="$config" > /dev/null 2>&1
    
    # Use ninja with -t commands to get per-file times
    local timing_output
    timing_output=$(ninja -C "$BUILD_DIR" -t commands | head -20)
    
    echo "{\"type\":\"per_file\",\"config\":\"$config\",\"sample_commands\":\"$(echo "$timing_output" | head -5 | tr '\n' ' ')\"}"
}

# Function to generate summary statistics
generate_summary() {
    local results=$1
    
    print_color "$GREEN" "\n=== Build Time Measurement Summary ==="
    
    # Parse and display results
    if [[ -n "$results" ]]; then
        echo "$results" | jq -r '
            select(.type == "clean") |
            "Clean Build (\(.config), unity=\(.unity), ccache=\(.ccache)): \(.total_time)s (config: \(.config_time)s, build: \(.build_time)s)"
        ' 2>/dev/null || echo "$results"
        
        echo ""
        
        echo "$results" | jq -r '
            select(.type == "incremental") |
            "Incremental Build (\(.config), unity=\(.unity), ccache=\(.ccache)): \(.build_time)s"
        ' 2>/dev/null || echo ""
    fi
}

# Main execution
main() {
    parse_args "$@"
    
    print_color "$GREEN" "=== uSEQ Build Time Measurement Tool ==="
    print_color "$BLUE" "Starting measurements at $(date)"
    echo ""
    
    # Check for required tools
    for tool in meson ninja; do
        if ! command -v "$tool" &> /dev/null; then
            print_color "$RED" "Error: Required tool '$tool' not found"
            exit 1
        fi
    done
    
    # Results array
    local all_results=""
    
    # Run measurements based on mode
    for config in "${CONFIGURATIONS[@]}"; do
        for unity in "${UNITY_BUILDS[@]}"; do
            for ccache in "${CCACHE_MODES[@]}"; do
                if [[ "${TEST_MODE:-both}" == "clean" ]] || [[ "${TEST_MODE:-both}" == "both" ]]; then
                    result=$(measure_clean_build "$config" "$unity" "$ccache")
                    all_results="${all_results}${result}\n"
                    [[ "$VERBOSE" == "true" ]] && echo "$result"
                fi
                
                if [[ "${TEST_MODE:-both}" == "incremental" ]] || [[ "${TEST_MODE:-both}" == "both" ]]; then
                    result=$(measure_incremental_build "$config" "$unity" "$ccache")
                    all_results="${all_results}${result}\n"
                    [[ "$VERBOSE" == "true" ]] && echo "$result"
                fi
            done
        done
    done
    
    # Measure per-file times for default configuration
    if [[ "${TEST_MODE:-both}" == "both" ]]; then
        result=$(measure_per_file_times "${CONFIGURATIONS[0]}")
        all_results="${all_results}${result}\n"
    fi
    
    # Save results if output file specified
    if [[ -n "$OUTPUT_FILE" ]]; then
        echo -e "$all_results" | grep -v '^$' > "$OUTPUT_FILE"
        print_color "$GREEN" "Results saved to: $OUTPUT_FILE"
    fi
    
    # Generate and display summary
    generate_summary "$(echo -e "$all_results")"
    
    print_color "$GREEN" "\nMeasurements completed at $(date)"
}

# Run main function
main "$@"