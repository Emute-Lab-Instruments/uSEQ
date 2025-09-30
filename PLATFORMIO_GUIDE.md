# PlatformIO Build System Guide

## Overview

The uSEQ firmware now uses PlatformIO as the recommended build system for hardware targets. PlatformIO provides better dependency management, multiple environment support, and easier configuration management compared to Arduino CLI.

## Quick Start

### Installation (NixOS)

```bash
# Enter PlatformIO development environment
nix-shell platformio.nix

# Or use direnv (edit .envrc first)
# Change: use nix
# To:     use nix platformio.nix
direnv allow
```

### Basic Usage

```bash
# Build for default environment (musicthing)
scripts/pio.sh build

# Build for specific hardware variant
scripts/pio.sh build hardware_v0_2
scripts/pio.sh build hardware_v1_0
scripts/pio.sh build minimal

# Flash to device
scripts/pio.sh flash

# Flash to specific device
scripts/pio.sh flash musicthing /dev/ttyACM0

# Monitor serial output
scripts/pio.sh monitor

# Clean builds
scripts/pio.sh clean

# Show firmware size
scripts/pio.sh size

# List all available environments
scripts/pio.sh list
```

## Hardware Environments

### Production Environments

#### musicthing (Default)
Music Thing Modular hardware variant with all features:
- DSP engine and tempo estimation
- I2C networking (host and client)
- Flash storage for persistent state
- RGB LED control
- Analog inputs and digital I/O
- Rotary encoder support

**Build:** `pio run -e musicthing`

#### hardware_v0_2
uSEQ Hardware v0.2 variant:
- DSP engine
- I2C networking
- Flash storage
- LED control
- Analog inputs
- Rotary encoder

**Build:** `pio run -e hardware_v0_2`

#### hardware_v1_0
uSEQ Hardware v1.0 variant:
- DSP engine and tempo estimation
- I2C networking
- Flash storage
- LED control
- Analog inputs
- No rotary encoder (hardware limitation)

**Build:** `pio run -e hardware_v1_0`

#### minimal
Minimal build with core features only:
- No DSP, no I2C, no flash storage
- Useful for testing and debugging
- Smallest binary size

**Build:** `pio run -e minimal`

### Development Environments

#### musicthing-debug
Debug build with symbols and reduced optimization:
- All musicthing features
- Debug symbols enabled
- USEQ_DEBUG=1 flag
- Better for GDB debugging

**Build:** `pio run -e musicthing-debug`

#### musicthing-verbose
Verbose serial output build:
- All musicthing features
- Extra verbose serial debugging
- VERBOSE_SERIAL=1 flag

**Build:** `pio run -e musicthing-verbose`

### Native Testing Environment

#### native
Desktop build for running tests on the host computer:
- No Arduino dependencies
- Uses standard C++ libraries
- For running unit tests without hardware

**Build:** `pio test -e native`

## Configuration

### platformio.ini Structure

The `platformio.ini` file defines all build configurations:

```ini
[platformio]
default_envs = musicthing    # Default environment

[env]
# Common settings shared by all environments
platform = ...
board = pico
build_flags = ...

[env:musicthing]
# Music Thing specific settings
build_flags = ${env.build_flags}
    -DMUSICTHING
    -DENABLE_DSP_ENGINE
    ...
```

### Adding a New Hardware Variant

1. **Create new environment in platformio.ini:**
   ```ini
   [env:my_hardware]
   build_flags =
       ${env.build_flags}
       -DMY_HARDWARE
       -DENABLE_DSP_ENGINE
       # Add other flags as needed
   ```

2. **Update build scripts** (if needed):
   - Add to `VALID_ENVS` in `scripts/build_pio.sh`
   - Add to `VALID_ENVS` in `scripts/flash_pio.sh`

3. **Test the build:**
   ```bash
   scripts/pio.sh build my_hardware
   ```

## Build System Comparison

### PlatformIO vs Arduino CLI

| Feature | PlatformIO | Arduino CLI |
|---------|------------|-------------|
| Multi-environment | ✅ Native | ❌ Manual |
| Dependency mgmt | ✅ Automatic | ⚠️ Manual |
| Build caching | ✅ Yes | ✅ Yes |
| Testing framework | ✅ Built-in | ❌ External |
| CI/CD support | ✅ Excellent | ⚠️ Manual |
| Configuration | ✅ Single file | ❌ CLI flags |

### PlatformIO vs Meson

| Feature | PlatformIO | Meson |
|---------|------------|-------|
| Embedded targets | ✅ Excellent | ⚠️ Complex |
| Desktop targets | ⚠️ Limited | ✅ Excellent |
| Multi-platform | ✅ Yes | ✅ Yes |
| Test framework | ✅ Built-in | ✅ Built-in |
| Build speed | ✅ Fast | ✅ Fast |

**Recommendation:**
- Use **PlatformIO** for hardware/embedded builds
- Use **Meson** for desktop/development builds
- Both systems coexist in this project

## Advanced Usage

### Custom Build Flags

Add temporary build flags:
```bash
pio run -e musicthing -D CUSTOM_FLAG=1
```

### Upload via BOOTSEL Mode

1. Hold BOOTSEL button on Pico
2. Connect USB cable
3. Release BOOTSEL button
4. Run flash command:
   ```bash
   scripts/pio.sh flash
   ```

### Debugging with Serial Monitor

```bash
# Build with verbose output
pio run -e musicthing-verbose

# Flash and immediately monitor
pio run -e musicthing-verbose -t upload && pio device monitor
```

### Building Multiple Environments

```bash
# Build all production environments
for env in musicthing hardware_v0_2 hardware_v1_0; do
    pio run -e $env
done
```

## Troubleshooting

### Build Fails: Platform Not Found

**Problem:** First build fails because platform needs to be downloaded

**Solution:** PlatformIO will automatically download the platform on first build. Be patient.

### Build Fails: Permission Denied

**Problem:** PlatformIO cache directory has wrong permissions

**Solution:**
```bash
rm -rf ~/.platformio
pio run -e musicthing  # Will recreate cache
```

### Upload Fails: Device Not Found

**Problem:** Pico not in bootloader mode or wrong device path

**Solutions:**
1. Put Pico in BOOTSEL mode (hold button while connecting USB)
2. Check device path: `ls /dev/tty*`
3. Use correct device: `scripts/pio.sh flash musicthing /dev/ttyACM0`

### Build Warnings About Conversions

**Problem:** Type conversion warnings in embedded code

**Solution:** These are expected and disabled with `-Wno-conversion` flag in platformio.ini

## File Locations

### Build Output
- Firmware binary: `.pio/build/{env}/firmware.elf`
- UF2 file: `.pio/build/{env}/firmware.uf2`
- Size report: `.pio/build/{env}/firmware.elf.size`

### PlatformIO Cache
- Platform packages: `~/.platformio/platforms/`
- Build cache: `~/.platformio/.cache/`
- Installed tools: `~/.platformio/packages/`

## Migration Notes

### From Arduino CLI

The PlatformIO configuration replicates all Arduino CLI settings:
- FQBN: `rp2040:rp2040:generic` → `board = pico` + platform settings
- Build options: `freq=250,opt=Optimize3` → build_flags in platformio.ini
- Flash layout: `flash=8388608_7340032` → `board_build.filesystem_size = 7M`

Old Arduino CLI builds will continue to work, but PlatformIO is recommended going forward.

### Configuration Migration

Hardware-specific defines previously in `configure.h` are now in `platformio.ini`:

**Before (configure.h):**
```cpp
#define MUSICTHING
#define ENABLE_DSP_ENGINE
```

**After (platformio.ini):**
```ini
[env:musicthing]
build_flags =
    -DMUSICTHING
    -DENABLE_DSP_ENGINE
```

This allows better separation of concerns and easier multi-variant builds.

## Integration with Development Workflow

### With Meson (Desktop Testing)

```bash
# Desktop development cycle
./scripts/build.sh          # Build with Meson
./scripts/test.sh           # Run tests
./build/standalone          # Test interpreter

# Hardware flash cycle
scripts/pio.sh build        # Build firmware
scripts/pio.sh flash        # Flash to device
```

### With direnv

```bash
# .envrc for PlatformIO
use nix platformio.nix

# Then all pio commands work automatically
pio run -e musicthing
```

### With CI/CD

PlatformIO has excellent CI/CD support:
```yaml
# Example GitHub Actions
- name: Install PlatformIO
  run: pip install platformio

- name: Build all variants
  run: |
    pio run -e musicthing
    pio run -e hardware_v0_2
    pio run -e hardware_v1_0
```

## Future Enhancements

Planned improvements to the PlatformIO setup:

1. **Unified testing** - Run Meson tests through PlatformIO native environment
2. **Hardware-in-loop tests** - Automated testing with connected hardware
3. **CI/CD pipeline** - Automated builds and releases
4. **Library management** - Track external dependencies in platformio.ini
5. **Custom scripts** - Pre/post build actions for code generation

---

*Last updated: 2025-09-30*