{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    arduino-cli
    systemd  # provides libudev
    eudev    # alternative udev implementation
    libusb1
  ];
  
  shellHook = ''
    # Add nix lib paths to LD_LIBRARY_PATH for picotool
    export LD_LIBRARY_PATH="${pkgs.systemd}/lib:${pkgs.eudev}/lib:${pkgs.libusb1}/lib:$LD_LIBRARY_PATH"
    
    echo "Arduino development environment loaded"
    echo "Use: arduino-cli compile --fqbn rp2040:rp2040:generic uSEQ/"
  '';
}