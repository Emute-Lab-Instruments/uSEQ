# Complete Migration Guide: Using the Refactored Architecture

This guide shows you how to use the refactored uSEQ architecture in your own projects, whether for Arduino/embedded hardware or desktop development.

## Overview

The refactored architecture provides:
- **Clean separation** between core logic and platform-specific code
- **Easy testing** with mock hardware
- **Zero overhead** through compile-time polymorphism
- **Flexible deployment** across Arduino, desktop, WASM, and future platforms

## Quick Start

### For Arduino/Embedded (Hardware v1.0 Example)

```cpp
#define USEQHARDWARE_1_0  // Define before includes

#include "core/useq_state.h"
#include "core/useq_update.h"
#include "core/useq_init.h"
#include "platforms/hardware_v1_0/hardware_v1_0_config.h"

USEQState g_state;
HardwareV1_0_Config g_hardware;

void setup() {
    // 1. Initialize hardware
    g_hardware.init();

    // 2. Initialize state and interpreter
    useq_init<HardwareV1_0_Config>(g_state);

    // 3. Set up environment variables
    useq_setup_env_vars(g_state, g_hardware);

    // 4. Optional: Set some expressions
    g_state.continuous_ASTs[0] = g_state.parser.parse("(tri 1.0)");
}

void loop() {
    // Run one update cycle
    useq_tick(g_state, g_hardware);
}
```

### For Desktop Development

```cpp
#include "core/useq_state.h"
#include "core/useq_update.h"
#include "core/useq_init.h"
#include "platforms/mock/mock_hardware.h"

int main() {
    USEQState state;
    MockHardwareConfig hw;

    // Initialize
    useq_init<MockHardwareConfig>(state);
    useq_setup_env_vars(state, hw);
    hw.init();

    // Set expressions
    state.continuous_ASTs[0] = state.parser.parse("(tri 1.0)");

    // Run update loop
    for (int i = 0; i < 100; i++) {
        hw.advance_time(10000);  // 10ms
        useq_tick(state, hw);

        // Check results
        float a1 = hw.get_continuous_output(0);
        std::cout << "a1 = " << a1 << "\n";
    }

    return 0;
}
```

## Core Components

### 1. USEQState - The State Container

All program state lives in `USEQState`:

```cpp
struct USEQState {
    ModuLispInterpreter interpreter;  // LISP engine
    Environment environment;           // Variables
    uLispParser parser;               // Code parser

    // Output signals
    std::vector<Value> continuous_ASTs;  // Expressions to evaluate
    std::vector<float> continuous_vals;  // Cached results
    std::vector<Value> binary_ASTs;
    std::vector<float> binary_vals;
    std::vector<Value> serial_ASTs;
    std::vector<std::optional<float>> serial_vals;

    // Input signals
    std::vector<float> input_vals;

    // Flags and timing
    bool is_playing;
    bool waiting_for_sync_trigger;
    int ts;
    int update_speed;
    // ... etc
};
```

**No hidden state. Everything is explicit.**

### 2. Hardware Configuration - Platform Abstraction

Each platform implements:

```cpp
struct MyPlatformConfig {
    // Compile-time configuration
    static constexpr size_t NUM_INPUTS = 8;
    static constexpr size_t NUM_CONTINUOUS_OUTS = 3;
    static constexpr size_t NUM_BINARY_OUTS = 3;
    static constexpr size_t NUM_SERIAL_OUTS = 9;
    static constexpr size_t NUM_SERIAL_INS = 32;

    static constexpr bool HAS_TEMPO_DETECTION = true;
    static constexpr bool HAS_I2C = true;
    static constexpr bool HAS_FLASH_STORAGE = true;
    static constexpr bool HAS_DSP_ENGINE = false;
    static constexpr bool SUPPORTS_DYNAMIC_IO = false;

    // I/O functions
    float read_input(size_t index) const;
    void write_continuous_output(size_t index, float value) const;
    void write_binary_output(size_t index, float value) const;
    void write_serial_output(size_t index, float value) const;

    // Timing
    unsigned long micros() const;
    void delay_microseconds(unsigned int us) const;

    // Initialization
    void init();
    void tick_maintenance();
};
```

### 3. Core Update Functions

Platform-agnostic logic:

```cpp
template<typename HW>
void useq_tick(USEQState& state, const HW& hw) {
    // Update timing
    // Read inputs
    // Update interpreter time
    // Evaluate expressions
    // Write outputs
}
```

**No preprocessor. No platform-specific code. Just pure logic.**

### 4. Initialization Helpers

```cpp
// Initialize state arrays
template<typename HW>
void useq_init_state(USEQState& state);

// Initialize interpreter
void useq_init_interpreter(USEQState& state);

// Do both
template<typename HW>
void useq_init(USEQState& state);

// Set up environment variables
template<typename HW>
void useq_setup_env_vars(USEQState& state, const HW& hw);
```

## Reserved Outputs

### Serial Output s0

**IMPORTANT:** Serial output 0 (s0) is RESERVED for time output on all platforms.

- `s0` always outputs `time_since_boot` in seconds
- User code cannot override `s0`
- User-assignable serial outputs start at `s1` (index 1)

This is enforced in `useq_update_serial_signals()`:

```cpp
// s0 is RESERVED: always outputs time in seconds
if (num_outs > useq::SERIAL_OUT_TIME_INDEX) {
    double time_seconds = time_manager->get_time_since_boot() / 1e6;
    state.serial_vals[useq::SERIAL_OUT_TIME_INDEX] = time_seconds;
}

// s1, s2, s3, ... are user-assignable
for (size_t i = useq::SERIAL_OUT_FIRST_USER; i < num_outs; i++) {
    // Evaluate user expressions
}
```

### Platform-Specific Reserved Outputs

Check your platform's documentation for additional reserved outputs.

**Example (MUSICTHING):**
- `a1`, `a2` - Reserved for DAC audio output
- `a3`, `a4` - User-assignable CV

**Example (Hardware v1.0):**
- All outputs user-assignable (no additional reservations)

## Creating a New Platform

### Step 1: Create Configuration Header

Create `platforms/my_platform/my_platform_config.h`:

```cpp
#ifndef USEQ_PLATFORMS_MY_PLATFORM_CONFIG_H
#define USEQ_PLATFORMS_MY_PLATFORM_CONFIG_H

#include "../../core/hardware_config.h"

struct MyPlatformConfig {
    // 1. Define compile-time constants
    static constexpr size_t NUM_INPUTS = 4;
    static constexpr size_t NUM_CONTINUOUS_OUTS = 2;
    static constexpr size_t NUM_BINARY_OUTS = 2;
    static constexpr size_t NUM_SERIAL_OUTS = 4;
    static constexpr size_t NUM_SERIAL_INS = 4;

    static constexpr bool HAS_TEMPO_DETECTION = false;
    static constexpr bool HAS_I2C = false;
    static constexpr bool HAS_FLASH_STORAGE = false;
    static constexpr bool HAS_DSP_ENGINE = false;
    static constexpr bool SUPPORTS_DYNAMIC_IO = false;

    // 2. Platform-specific constants
    static constexpr const char* HARDWARE_TYPE_ID = "MyPlatform";

    // 3. Declare I/O functions
    float read_input(size_t index) const;
    void write_continuous_output(size_t index, float value) const;
    void write_binary_output(size_t index, float value) const;
    void write_serial_output(size_t index, float value) const;

    unsigned long micros() const;
    void delay_microseconds(unsigned int us) const;

    void init();
    void tick_maintenance();
};

#endif
```

### Step 2: Implement I/O Functions

Create `platforms/my_platform/my_platform_impl.cpp`:

```cpp
#include "my_platform_config.h"

#ifdef MY_PLATFORM_BUILD

#include <my_platform_headers.h>

void MyPlatformConfig::init() {
    // Initialize pins, peripherals, etc.
}

float MyPlatformConfig::read_input(size_t index) const {
    // Read from hardware
    return 0.0f;
}

void MyPlatformConfig::write_continuous_output(size_t index, float value) const {
    // Write to hardware
}

// ... implement other functions ...

#endif // MY_PLATFORM_BUILD
```

### Step 3: Create Main Entry Point

Arduino sketch (`my_platform_main.ino`):

```cpp
#define MY_PLATFORM_BUILD

#include "core/useq_state.h"
#include "core/useq_update.h"
#include "core/useq_init.h"
#include "platforms/my_platform/my_platform_config.h"

USEQState g_state;
MyPlatformConfig g_hardware;

void setup() {
    g_hardware.init();
    useq_init<MyPlatformConfig>(g_state);
    useq_setup_env_vars(g_state, g_hardware);
}

void loop() {
    useq_tick(g_state, g_hardware);
}
```

### Step 4: Build and Test

Use Arduino IDE, PlatformIO, or your platform's build system.

## File Organization

```
src/
├── core/                         # Platform-agnostic core
│   ├── useq_state.h             # State structure
│   ├── useq_update.h            # Update loop
│   ├── useq_init.h              # Initialization helpers
│   ├── useq_config.h            # Constants
│   └── hardware_config.h        # Platform interface
│
├── platforms/                    # Platform implementations
│   ├── mock/
│   │   └── mock_hardware.h      # Testing platform
│   ├── hardware_v1_0/
│   │   ├── hardware_v1_0_config.h
│   │   ├── hardware_v1_0_impl.cpp
│   │   └── hardware_v1_0_main.ino
│   └── my_platform/             # Your platform here
│
└── examples/
    ├── simple_mock_test.cpp
    └── desktop_hardware_v1_0.cpp
```

## Testing Your Platform

### Unit Testing with Mock Hardware

```cpp
#include "core/useq_state.h"
#include "core/useq_update.h"
#include "core/useq_init.h"
#include "platforms/mock/mock_hardware.h"

TEST_CASE("Signal evaluation works") {
    USEQState state;
    MockHardwareConfig hw;

    useq_init<MockHardwareConfig>(state);

    // Set expression
    state.continuous_ASTs[0] = Value(0.75);

    // Run update
    useq_update_signals(state, hw);

    // Verify
    REQUIRE(state.continuous_vals[0] == 0.75f);
}

TEST_CASE("Output writing works") {
    USEQState state;
    MockHardwareConfig hw;

    useq_init<MockHardwareConfig>(state);

    state.continuous_vals[0] = 0.5f;

    // Write outputs
    useq_update_outs(state, hw);

    // Verify
    REQUIRE(hw.get_continuous_output(0) == 0.5f);
}
```

### Integration Testing on Hardware

1. Load Arduino sketch
2. Open serial monitor
3. Send LISP code via serial
4. Verify outputs with multimeter/oscilloscope

Example test sequence:
```lisp
; Set a1 to constant 0.5
(set 'a1 0.5)

; Set a1 to triangle wave
(set 'a1 (tri 1.0))

; Set d1 high
(set 'd1 1)

; Read from environment
(get 'bpm)
(get 'num-inputs)
```

## Advanced Usage

### Custom Initialization

```cpp
void setup() {
    // Custom hardware init
    g_hardware.init();

    // Custom state init (don't use helper)
    g_state.continuous_ASTs.resize(3, Value::nil());
    g_state.continuous_vals.resize(3, 0.5f);
    // ... etc

    // Custom interpreter init
    g_state.interpreter.init();

    // Add custom environment variables
    g_state.environment.set("my-var", Value(42.0));

    // Load code from file/flash
    String code = load_code_from_somewhere();
    g_state.interpreter.eval(code);
}
```

### Accessing Internals

```cpp
// Get interpreter
ModuLispInterpreter& interp = state.interpreter;

// Get environment
Environment& env = state.environment;

// Get parser
uLispParser& parser = state.parser;

// Set/get variables
env.set("x", Value(10.0));
Value x = env.get("x");

// Parse and evaluate
Value expr = parser.parse("(+ 1 2)");
Value result = interp.eval(expr);

// Access timing
if (auto* time_mgr = interp.get_time_manager()) {
    double bpm = time_mgr->get_bpm();
    double beat = time_mgr->get_beat();
}
```

### Error Handling

```cpp
try {
    String result = state.interpreter.eval("(invalid syntax");
} catch (const std::exception& e) {
    Serial.print("Error: ");
    Serial.println(e.what());
}
```

## Performance Considerations

### Zero-Overhead Abstractions

The refactored architecture uses templates, which compile to the same machine code as the original:

```cpp
// This template function...
template<typename HW>
void useq_tick(USEQState& state, const HW& hw) {
    hw.write_continuous_output(0, 0.5f);
}

// ...compiles to the same code as:
void useq_tick_old() {
    analogWrite(pin, value);  // Direct call, no indirection
}
```

**No vtable lookups. No function pointer overhead. Pure speed.**

### Memory Usage

The refactored architecture uses the same memory as the original:
- State is explicit (no hidden overhead)
- Templates don't add runtime data
- Vectors replace fixed arrays (same size when used correctly)

## Migration Checklist

When porting existing code to the refactored architecture:

- [ ] Create platform config struct with all constants
- [ ] Implement I/O functions for your platform
- [ ] Create main entry point (Arduino sketch or desktop main)
- [ ] Initialize state with `useq_init()`
- [ ] Move platform-specific code to platform config
- [ ] Remove `#ifdef` from core logic
- [ ] Test with mock hardware first
- [ ] Test on real hardware
- [ ] Verify identical behavior to original

## Troubleshooting

### Compilation Errors

**Error:** "No matching function for useq_tick"
- **Fix:** Make sure you've included the hardware config header
- **Fix:** Pass both state and hardware: `useq_tick(state, hw)`

**Error:** "Cannot access private member"
- **Fix:** The refactored code uses public members. Check you're using USEQState, not the old uSEQ class.

**Error:** "Vector index out of range"
- **Fix:** Call `useq_init_state<YourPlatform>(state)` before using state

### Runtime Issues

**Issue:** Outputs don't work
- **Check:** Did you call `hw.init()`?
- **Check:** Are pin numbers correct in your config?
- **Check:** Did you call `useq_update_outs()` in your loop?

**Issue:** LISP evaluation fails
- **Check:** Did you call `useq_init_interpreter(state)`?
- **Check:** Are you calling `useq_update_signals()` to evaluate expressions?

**Issue:** s0 doesn't output time
- **Check:** Is `NUM_SERIAL_OUTS > 0` in your config?
- **Check:** Did you initialize `state.serial_vals`?

## Summary

The refactored architecture provides:

✅ **Clean separation** - Core vs platform
✅ **Easy testing** - Mock hardware for unit tests
✅ **Zero overhead** - Templates compile to direct calls
✅ **Flexibility** - Arduino, desktop, WASM, future platforms
✅ **Maintainability** - No preprocessor soup
✅ **Documentation** - This guide + code comments

**Get started:** See `examples/desktop_hardware_v1_0.cpp` or `platforms/hardware_v1_0/hardware_v1_0_main.ino`

**Questions?** See `README.md`, `QUICKSTART.md`, or `ARCHITECTURE.txt`

Happy coding! 🎵
