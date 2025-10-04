# Platform-Specific Implementations

This directory contains platform-specific implementations for the refactored uSEQ architecture.

## Architecture Overview

The refactored architecture separates core logic from platform-specific code:

```
/src/
├── core/               # Platform-agnostic core logic
│   ├── hardware_config.h     # Hardware abstraction interface
│   ├── useq_state.h          # State management
│   ├── useq_init.h           # Initialization routines
│   ├── useq_update.h         # Main update loop
│   └── useq_config.h         # Configuration helpers
└── platforms/          # Platform-specific implementations
    ├── musicthing/           # Music Thing Modular variant
    ├── hardware_v1_0/        # uSEQ Hardware v1.0
    └── mock/                 # Mock platform for testing
```

## Available Platforms

### Music Thing Modular (`musicthing/`)

Music Thing Modular variant with:
- 4 CV outputs (aL, aR via DSP + a3, a4 via external DAC)
- 2 gate outputs (d1, d2) with hardware inversion
- Audio inputs (L, R)
- Gate inputs (I1, I2)
- Multiplexed analog inputs (2 MUX, 4 channels each)
- Control inputs (Main knob, X, Y, Z switch)
- 16MB flash storage
- DSP engine with audio processing
- Tempo estimation on gate inputs
- I2C networking (host + client modes)
- RGB LED support
- Serial debugging and REPL

**Files:**
- `musicthing_config.h` - Hardware configuration and pin definitions
- `musicthing_impl.cpp` - Arduino-specific implementations
- `musicthing_main.ino` - Main sketch entry point

**Features:**
- `MUSICTHING` - Hardware variant flag
- `ENABLE_DSP_ENGINE` - Audio processing
- `ENABLE_TEMPO_ESTIMATOR` - Tempo detection
- `ENABLE_I2C_NETWORKING` - Multi-module networking
- `ENABLE_RGB_LED` - RGB LED control
- `DIGI_OUT_INVERTED`, `ANALOG_OUT_INVERTED`, `AUDIO_OUT_INVERTED` - Output inversion

### uSEQ Hardware v1.0 (`hardware_v1_0/`)

Original uSEQ hardware revision 1.0:
- 3 CV outputs (a1, a2, a3)
- 3 gate outputs (d1, d2, d3)
- 2 gate inputs (I1, I2)
- 2 analog inputs (AI1, AI2)
- Switches and controls
- Tempo detection
- I2C networking
- Flash storage

**Files:**
- `hardware_v1_0_config.h` - Hardware configuration
- `hardware_v1_0_impl.cpp` - Implementation with PDM analog outputs
- `hardware_v1_0_main.ino` - Main sketch

### Mock Platform (`mock/`)

Testing platform with no hardware dependencies:
- Configurable I/O counts
- No actual hardware access
- Used for desktop builds and testing

## Building for Specific Platforms

### Desktop/Testing Builds (Meson)

The desktop builds use the mock platform and build with Meson:

```bash
# Build desktop version
./scripts/build.sh

# Run standalone interpreter
./build/standalone

# Run tests
./scripts/test.sh
```

### Hardware Builds (PlatformIO)

**Note:** The PlatformIO build system currently uses the legacy architecture in `/uSEQ/`.
To use the refactored platform architecture, you have two options:

#### Option 1: Migrate PlatformIO to Refactored Architecture

Update `platformio.ini` to use platform-specific main files:

```ini
[env:musicthing-refactored]
src_dir = src/platforms/musicthing
; ... rest of configuration
```

#### Option 2: Use Legacy Build (Current Default)

The existing PlatformIO configuration in the project root builds the legacy
architecture from `/uSEQ/`:

```bash
# Build for Music Thing (legacy architecture)
pio run -e musicthing

# Or use convenience script
./scripts/build_pio.sh musicthing
```

## Creating a New Platform

To add support for new hardware:

1. Create a new directory under `platforms/`: `platforms/my_platform/`

2. Create the configuration header: `my_platform_config.h`
   ```cpp
   struct MyPlatformConfig {
       static constexpr size_t NUM_INPUTS = ...;
       static constexpr size_t NUM_CONTINUOUS_OUTS = ...;
       static constexpr size_t NUM_BINARY_OUTS = ...;
       // ... feature flags, pin definitions

       float read_input(size_t index) const;
       void write_continuous_output(size_t index, float value) const;
       void write_binary_output(size_t index, float value) const;
       void init();
       unsigned long micros() const;
   };
   ```

3. Create the implementation: `my_platform_impl.cpp`
   - Implement all methods from config struct
   - Add Arduino-specific code in `#ifdef ARDUINO` blocks
   - Provide desktop stubs for testing

4. Create the main sketch: `my_platform_main.ino`
   ```cpp
   #define MY_PLATFORM
   #include "../../core/useq_state.h"
   #include "../../core/useq_update.h"
   #include "../../core/useq_init.h"
   #include "my_platform_config.h"

   USEQState g_state;
   MyPlatformConfig g_hardware;

   void setup() {
       g_hardware.init();
       useq_init_state<MyPlatformConfig>(g_state);
       useq_init_interpreter(g_state);
       useq_setup_env_vars(g_state, g_hardware);
   }

   void loop() {
       useq_tick(g_state, g_hardware);
   }
   ```

5. Add PlatformIO environment to `platformio.ini`:
   ```ini
   [env:my_platform]
   build_flags =
       ${env.build_flags}
       -DMY_PLATFORM
       ; ... feature flags
   ```

## Platform Interface Reference

All platform configurations must implement:

### Required Constants
- `NUM_INPUTS` - Number of input channels
- `NUM_CONTINUOUS_OUTS` - Number of CV/analog outputs
- `NUM_BINARY_OUTS` - Number of gate/digital outputs
- `NUM_SERIAL_OUTS` - Number of serial output channels
- `NUM_SERIAL_INS` - Number of serial input channels

### Feature Flags
- `HAS_TEMPO_DETECTION` - Supports tempo estimation
- `HAS_I2C` - Supports I2C communication
- `HAS_FLASH_STORAGE` - Has persistent storage
- `HAS_DSP_ENGINE` - Has audio DSP capabilities
- `HAS_RGB_LED` - Has RGB LED support
- `SUPPORTS_DYNAMIC_IO` - Can reconfigure I/O at runtime

### Required Methods
- `void init()` - Initialize hardware
- `float read_input(size_t index)` - Read input value [0, 1]
- `void write_continuous_output(size_t index, float value)` - Write CV output
- `void write_binary_output(size_t index, float value)` - Write gate output
- `void write_serial_output(size_t index, float value)` - Write serial output
- `unsigned long micros()` - Get current time in microseconds
- `void delay_microseconds(unsigned int us)` - Delay execution
- `void tick_maintenance()` - Per-tick maintenance tasks

## Testing

Desktop builds automatically use the mock platform. To test a specific platform
configuration without hardware:

1. Build the desktop version
2. The mock platform provides stubbed I/O
3. Run the test suite to verify core logic
4. Use the standalone interpreter for REPL testing

## Migration Status

- ✅ Core architecture refactored
- ✅ Mock platform for testing
- ✅ Hardware v1.0 platform implemented
- ✅ Music Thing platform implemented
- ⏳ PlatformIO integration for refactored platforms (pending)
- ⏳ Hardware v0.2 platform (to be implemented)
- ⏳ Expander modules (to be implemented)

## Notes

- The legacy uSEQ class-based architecture remains in `/uSEQ/` for compatibility
- Desktop builds use the refactored architecture
- PlatformIO builds currently use legacy architecture
- Both architectures share the core ModuLisp interpreter
- Future work will fully migrate PlatformIO to refactored platforms
