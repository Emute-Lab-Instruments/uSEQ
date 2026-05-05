#!/bin/bash
# Build script with ccache support for faster repeated builds

# Set colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if ccache is available
if ! command -v ccache &> /dev/null; then
    echo -e "${YELLOW}Warning: ccache not found. Install it for 80-90% speedup on repeated builds.${NC}"
    echo "Falling back to regular build..."
    exec ./scripts/build.sh "$@"
fi

# Print ccache statistics before build
echo -e "${GREEN}=== ccache statistics before build ===${NC}"
ccache -s | grep -E "cache hit|cache miss|called for link|cache size"

# Export ccache compiler wrappers
export CXX="ccache g++"
export CC="ccache gcc"

# Clean build directory if requested
if [[ "$1" == "clean" ]]; then
    echo -e "${GREEN}Cleaning build directory...${NC}"
    rm -rf build
    shift
fi

# Setup build with ccache enabled
if [ ! -d "build" ]; then
    echo -e "${GREEN}Setting up Meson build with ccache...${NC}"
    meson setup build -Duse_ccache=true "$@"
else
    echo -e "${GREEN}Using existing build directory with ccache...${NC}"
fi

# Build the project
echo -e "${GREEN}Building with ccache...${NC}"
ninja -j4 -C build

# Print ccache statistics after build
echo -e "${GREEN}=== ccache statistics after build ===${NC}"
ccache -s | grep -E "cache hit|cache miss|called for link|cache size"

echo -e "${GREEN}Build complete! Use 'ccache -s' to see full statistics.${NC}"