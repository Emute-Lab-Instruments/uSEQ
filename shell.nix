{ pkgs ? import <nixpkgs> { } }:

let
  python = pkgs.python3.withPackages (pythonPackages: with pythonPackages; [
    pyyaml
  ]);
in
pkgs.mkShell {
  packages = with pkgs; [
    arduino-cli
    binaryen
    ccache
    coreutils
    curl
    emscripten
    eudev
    gawk
    gcc
    git
    gnumake
    libusb1
    meson
    ninja
    nodejs
    pkg-config
    platformio
    python
    systemd
    udisks2
    wabt
  ];

  shellHook = ''
    # picotool and the physical-device utilities resolve these dynamically.
    export LD_LIBRARY_PATH="${pkgs.systemd}/lib:${pkgs.eudev}/lib:${pkgs.libusb1}/lib:$LD_LIBRARY_PATH"

    echo "uSEQ RP2040 development environment loaded"
    echo "Full local acceptance: python3 scripts/run_rp2040_profile.py"
    echo "Complete candidate gate: python3 scripts/run_rp2040_goal_gate.py"
  '';
}
