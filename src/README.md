# uSEQ Refactored Core Architecture

This directory contains the **Phase 1 refactoring** of the uSEQ codebase, designed to simplify development, improve testability, and make it easier to support multiple hardware platforms.

## Philosophy

The refactored architecture follows these principles:

1. **Composition over Inheritance** - No class hierarchies, just data structures and functions
2. **Procedural/Functional over OOP** - Pure functions operating on explicit state
3. **Compile-time Polymorphism** - Templates instead of virtual functions (zero runtime overhead)
4. **Platform Abstraction** - Hardware differences isolated in config files, not scattered throughout code
5. **No Preprocessor in Core** - All `#ifdef ARDUINO` moved to platform directories
6. **Testability First** - Mock hardware for unit testing without real hardware

## Architecture Overview

```
┌─────────────────────────────────────┐
│   ModuLispInterpreter (unchanged)   │  ← Core LISP interpreter
└─────────────────────────────────────┘
              ↓ contained in
┌─────────────────────────────────────┐
│   USEQState (src/core/useq_state.h) │  ← All state in one struct
│   - interpreter                     │     - No inheritance
│   - environment                     │     - No virtual methods
│   - ASTs (expressions)              │     - Pure data
│   - cached values                   │
│   - flags and timing                │
└─────────────────────────────────────┘
              ↓ operated on by
┌─────────────────────────────────────┐
│   Core Functions                    │  ← Platform-agnostic logic
│   (src/core/useq_update.h)          │
│   - useq_tick()                     │     Template-based functions
│   - useq_update_signals()           │     No platform-specific code
│   - useq_update_outs()              │     Delegates to HardwareConfig
│   - useq_update_inputs()            │
└─────────────────────────────────────┘
              ↓ uses
┌─────────────────────────────────────┐
│   HardwareConfig Interface          │  ← Platform abstraction
│   (src/core/hardware_config.h)      │
│   - NUM_INPUTS (constexpr)          │     Compile-time traits
│   - NUM_OUTPUTS (constexpr)         │     I/O function pointers
│   - read_input()                    │     Timing functions
│   - write_output()                  │
│   - micros(), delay()               │
└─────────────────────────────────────┘
              ↑ implemented by
    ┌─────────┴─────────┬─────────────────┬──────────────┐
    │                   │                 │              │
┌───────────┐   ┌───────────────┐   ┌─────────┐   ┌──────────┐
│   Mock    │   │  HardwareV1_0 │   │ WASM    │   │   VST    │
│ (testing) │   │  (eurorack)   │   │ (web)   │   │ (plugin) │
└───────────┘   └───────────────┘   └─────────┘   └──────────┘
```

## Directory Structure

```
src/
├── core/                          # Platform-agnostic core
│   ├── useq_state.h              # Main state struct (no platform deps)
│   ├── useq_update.h             # Update loop logic (templates)
│   └── hardware_config.h         # Hardware interface definition
│
├── platforms/                     # Platform-specific implementations
│   ├── mock/
│   │   └── mock_hardware.h       # Testing/simulation platform
│   ├── hardware_v1_0/
│   │   └── hardware_v1_0_config.h # uSEQ v1.0 eurorack module
│   ├── musicthing/               # (TODO)
│   ├── wasm/                     # (TODO)
│   └── vst/                      # (TODO - future)
│
├── api/                           # (TODO - Phase 2)
│   ├── api_timing.cpp            # Split from modulisp_api.cpp
│   ├── api_lists.cpp
│   └── api_math.cpp
│
└── examples/
    └── simple_mock_test.cpp      # Example usage
```

## Key Components

### 1. USEQState (`core/useq_state.h`)

Pure data structure containing all uSEQ state:

```cpp
struct USEQState {
    ModuLispInterpreter interpreter;  // Composition, not inheritance!
    Environment environment;
    uLispParser parser;

    std::vector<Value> continuous_ASTs;  // Expressions to evaluate
    std::vector<float> continuous_vals;  // Cached results

    bool is_playing;
    int ts;  // Timestamp
    // ... etc
};
```

**No virtual methods, no inheritance, just data.**

### 2. Core Update Functions (`core/useq_update.h`)

Template-based functions that operate on state:

```cpp
template<typename HardwareConfig>
void useq_tick(USEQState& state, const HardwareConfig& hw) {
    useq_update_inputs(state, hw);
    state.interpreter.update_time();
    useq_update_signals(state, hw);
    useq_update_outs(state, hw);
}
```

**No preprocessor conditionals. Platform differences handled through `hw` parameter.**

### 3. HardwareConfig Interface (`core/hardware_config.h`)

Each platform implements this interface:

```cpp
struct MyPlatformConfig {
    // Compile-time configuration
    static constexpr size_t NUM_INPUTS = 8;
    static constexpr size_t NUM_CONTINUOUS_OUTS = 3;
    static constexpr bool HAS_TEMPO_DETECTION = true;

    // I/O functions
    float read_input(size_t index) const { /* ... */ }
    void write_continuous_output(size_t index, float value) const { /* ... */ }
    unsigned long micros() const { /* ... */ }
    void delay_microseconds(unsigned int us) const { /* ... */ }
};
```

### 4. Platform Implementations

#### MockHardwareConfig (`platforms/mock/mock_hardware.h`)

For testing and simulation:
- In-memory storage for all I/O
- Controllable simulated time
- Verbose logging option
- Perfect for unit tests

#### HardwareV1_0_Config (`platforms/hardware_v1_0/hardware_v1_0_config.h`)

For real uSEQ v1.0 hardware:
- Pin definitions
- ADC/DAC configuration
- I2C setup
- LED control

## How to Use

### Example: Simple Test with Mock Hardware

```cpp
#include "core/useq_state.h"
#include "core/useq_update.h"
#include "platforms/mock/mock_hardware.h"

int main() {
    // 1. Create state and hardware
    USEQState state;
    MockHardwareConfig hw;

    // 2. Initialize
    state.continuous_ASTs.resize(hw.NUM_CONTINUOUS_OUTS, Value::nil());
    state.continuous_vals.resize(hw.NUM_CONTINUOUS_OUTS, 0.0f);
    // ... (initialize other arrays)

    state.interpreter.init();

    // 3. Set up expressions
    state.continuous_ASTs[0] = Value(0.5);  // a1 = 0.5
    state.continuous_ASTs[1] = state.parser.parse("(+ 0.2 0.3)");  // a2 = 0.5

    // 4. Run update loop
    for (int i = 0; i < 100; i++) {
        hw.advance_time(1000);  // 1ms
        useq_tick(state, hw);

        // Check outputs
        float a1_out = hw.get_continuous_output(0);
        // ...
    }
}
```

See `examples/simple_mock_test.cpp` for a complete example.

### Example: Real Hardware (Hardware v1.0)

```cpp
#include "core/useq_state.h"
#include "core/useq_update.h"
#include "platforms/hardware_v1_0/hardware_v1_0_config.h"

USEQState state;
HardwareV1_0_Config hw;

void setup() {
    hw.init();  // Initialize pins, I2C, etc.
    state.interpreter.init();
    // ... initialize arrays
}

void loop() {
    useq_tick(state, hw);  // Run one update cycle
}
```

## Benefits vs Legacy Code

| Aspect | Legacy (`uSEQ/src/`) | Refactored (`src/`) |
|--------|---------------------|---------------------|
| **Architecture** | Inheritance (`uSEQ extends ModuLisp`) | Composition (`USEQState contains interpreter`) |
| **Platform code** | Scattered `#ifdef ARDUINO` everywhere | Isolated in `platforms/` directory |
| **Testability** | Requires mocking complex class hierarchy | Simple: pass `MockHardwareConfig` |
| **File size** | `uSEQ.cpp`: 1699 lines, `modulisp_api.cpp`: 2147 lines | Core functions: <500 lines per file |
| **Build matrix** | 1024 possible configurations | 5 explicit platform profiles |
| **Adding platforms** | Modify core files with more `#ifdef` | Create new config file in `platforms/` |
| **Overhead** | Virtual functions (vtable) | Templates (zero runtime cost) |

## Migration Path

This refactored code lives **alongside** the legacy code during Phase 1:

```
/root/repo/
├── uSEQ/                 # Legacy code (still builds and works)
│   └── src/
└── src/                  # New refactored code (this directory)
    └── core/
```

**Phase 1 (Current)**: Core extraction
- ✅ Extract USEQState struct
- ✅ Extract update loop functions
- ✅ Create HardwareConfig interface
- ✅ Implement MockHardwareConfig
- ✅ Create example HardwareV1_0_Config

**Phase 2 (Next)**: Split large files
- Split `modulisp_api.cpp` into domain modules
- Create unified function registry
- Convert one real platform (hardware_v1_0) to use new core

**Phase 3 (Future)**: Complete migration
- Convert all platforms
- Delete legacy code
- Reorganize file structure

## Testing

The refactored core can be tested without any hardware:

```bash
# Compile the example
cd src/examples
g++ -std=c++17 -I../.. -I../../uSEQ/src \
    simple_mock_test.cpp \
    ../../uSEQ/src/modulisp/lisp/*.cpp \
    ../../uSEQ/src/modulisp/*.cpp \
    -o simple_mock_test

# Run it
./simple_mock_test
```

Unit tests can use `MockHardwareConfig` to verify core logic:

```cpp
TEST_CASE("Update loop processes signals") {
    USEQState state;
    MockHardwareConfig hw;
    // ... setup

    state.continuous_ASTs[0] = Value(0.5);
    useq_update_signals(state, hw);

    REQUIRE(state.continuous_vals[0] == 0.5f);
}
```

## Design Decisions

### Why templates instead of virtual functions?

Templates provide **compile-time polymorphism** with zero runtime overhead. This is critical for embedded systems where every cycle counts. Virtual functions require a vtable lookup on every call.

```cpp
// Virtual (legacy approach):
class HardwareConfig {
    virtual float read_input(size_t) = 0;  // Vtable lookup at runtime
};

// Template (refactored approach):
template<typename HW>
void useq_tick(USEQState& state, const HW& hw) {
    hw.read_input(0);  // Direct function call (inlined)
}
```

### Why composition instead of inheritance?

Inheritance creates rigid hierarchies and tight coupling. Composition is flexible and explicit:

```cpp
// Inheritance (legacy):
class uSEQ : public ModuLisp {
    // Tightly coupled, hard to test
};

// Composition (refactored):
struct USEQState {
    ModuLispInterpreter interpreter;  // Explicit, easy to mock
};
```

### Why separate state from logic?

Separating data (USEQState) from operations (useq_tick) makes code:
- **Easier to test**: Pass different state, verify results
- **Easier to understand**: State is explicit, not hidden in class members
- **Easier to debug**: Inspect state directly
- **More functional**: Pure functions with no hidden side effects

## Future Platforms

Adding new platforms is straightforward:

1. Create `platforms/my_platform/my_platform_config.h`
2. Define compile-time traits (NUM_INPUTS, etc.)
3. Implement I/O functions (read_input, write_output, etc.)
4. Done! Core logic works unchanged.

Example platforms to add:
- **WASM**: Dynamic I/O, browser-based
- **VST**: Audio plugin interface
- **MUSICTHING**: Music Thing Modular variant with DSP
- **Desktop**: Full simulation with GUI

## Questions?

See also:
- `examples/simple_mock_test.cpp` - Complete working example
- `core/hardware_config.h` - Full interface documentation
- `platforms/mock/mock_hardware.h` - Reference implementation
- `/CLAUDE.md` - Project overview (legacy codebase)

## Summary

This refactoring provides:
✅ **Simplicity** - Core logic is just functions + data
✅ **Clarity** - Platform differences are explicit
✅ **Extensibility** - New platforms = new config file
✅ **Testability** - Mock any hardware
✅ **Performance** - Zero overhead (templates, not virtuals)
✅ **Maintainability** - No preprocessor soup in core
