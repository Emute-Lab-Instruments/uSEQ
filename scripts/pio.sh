#!/usr/bin/env bash

# Convenience wrapper for PlatformIO commands
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

# Show usage
usage() {
    cat << EOF
Usage: $0 <command> [environment] [options]

Commands:
    build [env]         Build firmware for environment (default: musicthing)
    flash [env] [dev]   Flash firmware to device
    clean [env]         Clean build for environment (or all if not specified)
    monitor [env]       Open serial monitor
    size [env]          Show firmware size
    list                List all environments
    test                Run native tests

Environments:
    musicthing          Music Thing Modular (default)
    hardware_v0_2       uSEQ Hardware v0.2
    hardware_v1_0       uSEQ Hardware v1.0
    minimal             Minimal build
    musicthing-debug    Debug build with symbols
    musicthing-verbose  Verbose serial output

Examples:
    $0 build                        # Build for musicthing
    $0 build hardware_v0_2          # Build for hardware v0.2
    $0 flash musicthing /dev/ttyACM0  # Flash to specific device
    $0 clean                        # Clean all builds
    $0 monitor                      # Open serial monitor
    $0 size musicthing              # Show firmware size

EOF
    exit 1
}

# Parse command
COMMAND="${1:-}"
ENV="${2:-musicthing}"
OPT="${3:-}"

case "$COMMAND" in
    build)
        echo "Building for environment: $ENV"
        pio run -e "$ENV"
        ;;

    flash|upload)
        echo "Flashing environment: $ENV"
        if [ -n "$OPT" ]; then
            pio run -e "$ENV" -t upload --upload-port "$OPT"
        else
            pio run -e "$ENV" -t upload
        fi
        ;;

    clean)
        if [ "$ENV" = "musicthing" ] && [ -z "$2" ]; then
            echo "Cleaning all builds..."
            pio run -t clean
        else
            echo "Cleaning environment: $ENV"
            pio run -e "$ENV" -t clean
        fi
        ;;

    monitor|serial)
        echo "Opening serial monitor for: $ENV"
        pio device monitor -e "$ENV"
        ;;

    size)
        echo "Firmware size for: $ENV"
        pio run -e "$ENV" -t size
        ;;

    list|envs)
        echo "Available environments:"
        pio run --list-targets | grep "Environment: " || pio project config
        ;;

    test)
        echo "Running native tests..."
        pio test -e native
        ;;

    init)
        echo "Initializing PlatformIO project..."
        pio project init
        ;;

    help|--help|-h|"")
        usage
        ;;

    *)
        echo "Error: Unknown command '$COMMAND'"
        echo ""
        usage
        ;;
esac