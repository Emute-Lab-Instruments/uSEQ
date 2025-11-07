#!/bin/bash
# SessionStart hook for Claude Code on the web
# This script runs when a session starts and ensures meson is installed

set -e

echo "🔧 Setting up uSEQ development environment..."

# Check if meson is already installed
if command -v meson &> /dev/null; then
    MESON_VERSION=$(meson --version)
    echo "✓ Meson already installed (version $MESON_VERSION)"
else
    echo "📦 Installing meson..."

    # Try to install meson using pip
    if command -v pip3 &> /dev/null; then
        pip3 install --user meson ninja
        echo "✓ Meson installed successfully via pip3"
    elif command -v pip &> /dev/null; then
        pip install --user meson ninja
        echo "✓ Meson installed successfully via pip"
    else
        echo "⚠️  Warning: Could not install meson automatically (pip not found)"
        echo "   Please install meson manually: pip install meson"
        exit 1
    fi
fi

# Verify installation
if command -v meson &> /dev/null; then
    echo "✓ Environment setup complete!"
    meson --version
else
    echo "⚠️  Warning: Meson installation may require PATH update"
    echo "   You may need to add ~/.local/bin to your PATH"
fi
