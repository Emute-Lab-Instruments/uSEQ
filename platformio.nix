{ pkgs ? import <nixpkgs> {} }:

# Proper FHS environment for PlatformIO on NixOS
# This creates a chroot-like environment with standard Linux paths
# that dynamically linked ARM toolchain binaries expect

(pkgs.buildFHSEnv {
  name = "useq-platformio";

  targetPkgs = pkgs: (with pkgs; [
    # PlatformIO
    platformio-core
    python311
    python311Packages.pip
    python311Packages.setuptools

    # Build tools
    git
    gcc
    gnumake
    cmake

    # Libraries that ARM toolchain needs
    stdenv.cc.cc.lib
    zlib
    ncurses5
    libusb1
    udev

    # Additional libs that might be needed
    glibc
    expat
    libffi

    # Serial tools
    picocom
    screen
  ]);

  multiPkgs = pkgs: (with pkgs; [
    # 32-bit libraries in case toolchain needs them
    zlib
    ncurses5
  ]);

  runScript = "bash";

  profile = ''
    export PS1="\[\033[01;32m\](pio)\[\033[00m\] \w $ "

    # Make sure python packages are in path
    export PYTHONPATH="${pkgs.python311.pkgs.pip}/lib/python3.11/site-packages:$PYTHONPATH"

    echo ""
    echo "═══════════════════════════════════════════════════════"
    echo "  uSEQ PlatformIO Environment (FHS)"
    echo "═══════════════════════════════════════════════════════"
    echo ""
    echo "This provides a standard Linux environment for PlatformIO."
    echo ""
    echo "Quick Start:"
    echo "  pio run -e minimal              # Build minimal variant"
    echo "  pio run -e musicthing           # Build musicthing variant"
    echo "  pio run -e minimal -t upload    # Flash to device"
    echo ""
    echo "Convenience Scripts:"
    echo "  scripts/pio.sh build            # Build default"
    echo "  scripts/pio.sh flash            # Flash to device"
    echo "  scripts/pio.sh clean            # Clean build"
    echo ""
    echo "PlatformIO: $(pio --version 2>&1 | head -1)"
    echo ""
  '';
}).env