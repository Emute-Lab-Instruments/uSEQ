# Quick Start Guide: Refactored uSEQ Architecture

New to the refactored architecture? This guide gets you up to speed in 10 minutes.

## Core Concepts (60 seconds)

**Old way (legacy):**
```cpp
class uSEQ : public ModuLisp {
    void tick();  // Hidden state in member variables
};
```

**New way (refactored):**
```cpp
struct USEQState { /* all state here */ };

template<typename HW>
void useq_tick(USEQState& state, const HW& hw);  // Explicit state
```

**Key difference:** State is explicit data, logic is pure functions, platform is a template parameter.

## Three Things You Need to Know

### 1. USEQState = All Your State

Everything goes in `USEQState`:
```cpp
struct USEQState {
    ModuLispInterpreter interpreter;  // The LISP interpreter
    Environment environment;           // Variables
    std::vector<Value> continuous_ASTs;  // Expressions to eval
    std::vector<float> continuous_vals;  // Cached results
    bool is_playing;                   // Flags
    int ts;                           // Timestamp
    // ... etc
};
```

No hidden state. Everything is here.

### 2. Core Functions = Platform-Agnostic Logic

All core logic is template functions:
```cpp
template<typename HardwareConfig>
void useq_tick(USEQState& state, const HardwareConfig& hw) {
    useq_update_inputs(state, hw);    // Read inputs
    state.interpreter.update_time();  // Update timing
    useq_update_signals(state, hw);   // Eval expressions
    useq_update_outs(state, hw);      // Write outputs
}
```

These work with **any** platform that implements `HardwareConfig`.

### 3. HardwareConfig = Platform Abstraction

Each platform (real hardware, mock, WASM, etc.) implements this:
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

## Example: Hello World

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
    state.interpreter.init();

    // 3. Set expression: a1 = 0.5
    state.continuous_ASTs[0] = Value(0.5);

    // 4. Run one update cycle
    useq_tick(state, hw);

    // 5. Check output
    float output = hw.get_continuous_output(0);  // Should be 0.5!

    return 0;
}
```

That's it! No inheritance, no virtual functions, no preprocessor magic.

## Common Tasks

### Task: Add a New Platform

1. Create `platforms/my_platform/my_platform_config.h`
2. Define traits and implement I/O functions
3. Use it:
   ```cpp
   USEQState state;
   MyPlatformConfig hw;
   useq_tick(state, hw);
   ```

### Task: Test Core Logic

Use `MockHardwareConfig`:
```cpp
TEST_CASE("Signals update correctly") {
    USEQState state;
    MockHardwareConfig hw;

    state.continuous_ASTs[0] = Value(0.75);
    useq_update_signals(state, hw);

    REQUIRE(state.continuous_vals[0] == 0.75f);
}
```

### Task: Access Interpreter State

Everything is explicit:
```cpp
// Get a variable
Value x = state.environment.get("x");

// Set a variable
state.environment.set("bpm", Value(120.0));

// Eval an expression
Value result = state.interpreter.eval(expr);
```

### Task: Platform-Specific Code

Put it in your config file:
```cpp
struct MyPlatformConfig {
    void write_continuous_output(size_t index, float value) const {
#ifdef ARDUINO
        // Arduino-specific code
        analogWrite(pins[index], value * 4095);
#else
        // Desktop code
        printf("Output[%zu] = %.3f\n", index, value);
#endif
    }
};
```

Core logic stays clean - **no #ifdef in core/**.

### Task: Conditional Features

Use compile-time checks:
```cpp
template<typename HW>
void useq_tick(USEQState& state, const HW& hw) {
    // Only read inputs if platform has them
    if constexpr (HW::NUM_INPUTS > 0) {
        useq_update_inputs(state, hw);
    }

    // Only do tempo detection if supported
    if constexpr (HW::HAS_TEMPO_DETECTION) {
        // Tempo detection code
    }
}
```

No preprocessor, but code still optimizes away!

## File Layout

```
src/
├── core/                    # Platform-agnostic
│   ├── useq_state.h         # State struct (read this first!)
│   ├── useq_update.h        # Update loop (read this second!)
│   └── hardware_config.h    # Interface docs (read this third!)
│
├── platforms/               # Platform-specific
│   ├── mock/                # For testing
│   ├── hardware_v1_0/       # Real hardware
│   ├── musicthing/          # (TODO)
│   └── wasm/                # (TODO)
│
└── examples/
    └── simple_mock_test.cpp # Complete working example
```

## Key Files to Read

1. **Start here:** `src/README.md` - Architecture overview
2. **Then:** `src/core/useq_state.h` - What's in the state?
3. **Then:** `src/core/useq_update.h` - How does the loop work?
4. **Then:** `src/platforms/mock/mock_hardware.h` - Example platform
5. **Finally:** `src/examples/simple_mock_test.cpp` - See it in action

Total reading time: ~20 minutes.

## Mental Model

Think of it like this:

```
┌─────────────────┐
│   USEQState     │  ← A box with all your stuff
│   (just data)   │
└─────────────────┘
         ↓
┌─────────────────┐
│  useq_tick()    │  ← Functions that operate on the box
│  (pure logic)   │
└─────────────────┘
         ↓
┌─────────────────┐
│  HardwareConfig │  ← How to talk to the outside world
│  (platform)     │
└─────────────────┘
```

No inheritance. No hidden state. Just data, logic, and I/O.

## Common Questions

### Q: Why templates instead of virtual functions?

**A:** Templates are compile-time (zero overhead). Virtual functions have runtime cost (vtable lookup). For embedded systems, every cycle counts.

### Q: Why explicit state instead of member variables?

**A:** Explicit state is easier to test, debug, and understand. No hidden surprises.

### Q: Where did the preprocessor go?

**A:** Platform-specific code moved to config files. Core logic uses `if constexpr` for feature checks.

### Q: How do I migrate legacy code?

**A:** See `MIGRATION.md` for detailed patterns and examples.

### Q: Is this slower than the old code?

**A:** No! Templates optimize to the same machine code. Often faster due to better inlining.

### Q: Can I still use the old code?

**A:** Yes! Old code in `uSEQ/src/` still works. This refactored code lives in `src/` alongside it.

## Next Steps

1. **Read the example:** `src/examples/simple_mock_test.cpp`
2. **Try compiling it** (once dependencies are set up)
3. **Read the architecture:** `src/README.md`
4. **Study a platform:** `src/platforms/mock/mock_hardware.h`
5. **Try adding a feature** to the mock platform

## Summary

**Old way:**
- Inheritance (`class uSEQ : public ModuLisp`)
- Hidden state (member variables)
- Virtual functions (runtime overhead)
- Preprocessor everywhere (`#ifdef ARDUINO`)

**New way:**
- Composition (`USEQState` contains interpreter)
- Explicit state (pass `state` parameter)
- Templates (compile-time, zero overhead)
- Platform configs (one file per platform)

**Result:**
- Simpler
- Clearer
- More testable
- Just as fast (or faster!)

Welcome to the refactored uSEQ architecture! 🎉
