# Migration Guide: Legacy to Refactored Architecture

This guide explains how to migrate code from the legacy `uSEQ/src/` architecture to the refactored `src/` architecture.

## Quick Reference

| Legacy Pattern | Refactored Pattern |
|----------------|-------------------|
| `class uSEQ : public ModuLisp` | `struct USEQState { ModuLispInterpreter interpreter; }` |
| `void uSEQ::tick()` | `template<typename HW> void useq_tick(USEQState&, const HW&)` |
| `#ifdef ARDUINO` | `if constexpr (HW::HAS_FEATURE)` or separate config file |
| `analogRead(pin)` | `hw.read_input(index)` |
| `analogWrite(pin, val)` | `hw.write_continuous_output(index, val)` |
| `m_continuous_vals[i]` | `state.continuous_vals[i]` |
| `get_environment()->get("x")` | `state.environment.get("x")` |

## Step-by-Step Migration

### 1. Converting a Simple Function

**Legacy (`uSEQ/src/uSEQ_update.cpp`):**
```cpp
void uSEQ::update_continuous_signals() {
    for (int i = 0; i < m_num_continuous_outs; i++) {
        Value expr = m_continuous_ASTs[i];
        if (!expr.is_nil()) {
            Value result = eval(expr);
            m_continuous_vals[i] = result.as_float();
        }
    }
}
```

**Refactored (`src/core/useq_update.h`):**
```cpp
template<typename HardwareConfig>
void useq_update_continuous_signals(USEQState& state, const HardwareConfig& hw) {
    constexpr size_t num_outs = HardwareConfig::NUM_CONTINUOUS_OUTS;

    for (size_t i = 0; i < num_outs; i++) {
        Value expr = state.continuous_ASTs[i];
        if (!expr.is_nil()) {
            Value result = state.interpreter.eval(expr);
            state.continuous_vals[i] = result.as_float();
        }
    }
}
```

**Key Changes:**
- Function is now a template (takes `HardwareConfig` type parameter)
- Takes `USEQState& state` as explicit parameter (not implicit `this`)
- Uses `state.` instead of `m_` member access
- Uses `constexpr` for compile-time configuration
- Uses `state.interpreter.eval()` instead of `eval()` (explicit composition)

### 2. Converting Platform-Specific Code

**Legacy:**
```cpp
void uSEQ::analog_write_with_led(int output, float val) {
#ifdef ARDUINO
    int pin = useq_output_pins[output];
    int led_pin = useq_output_led_pins[output];
    analogWrite(pin, val * 4095);
    analogWrite(led_pin, val * 255);
#else
    if (io) {
        io->analog_write(output, val);
    }
#endif
}
```

**Refactored:**

Platform config file (`platforms/hardware_v1_0/hardware_v1_0_config.h`):
```cpp
struct HardwareV1_0_Config {
    static constexpr int OUTPUT_PINS[6] = { 21, 20, 19, 18, 17, 16 };
    static constexpr int OUTPUT_LED_PINS[6] = { 3, 2, 11, 12, 13, 22 };

    void write_continuous_output(size_t index, float value) const {
#ifdef ARDUINO
        int pin = OUTPUT_PINS[index];
        int led_pin = OUTPUT_LED_PINS[index];
        analogWrite(pin, value * 4095);
        analogWrite(led_pin, value * 255);
#endif
    }
};
```

Core update function (`core/useq_update.h`):
```cpp
template<typename HardwareConfig>
void useq_update_continuous_outs(USEQState& state, const HardwareConfig& hw) {
    constexpr size_t num_outs = HardwareConfig::NUM_CONTINUOUS_OUTS;

    for (size_t i = 0; i < num_outs; i++) {
        hw.write_continuous_output(i, state.continuous_vals[i]);
    }
}
```

**Key Changes:**
- Platform-specific code moved to platform config file
- Core logic has **no preprocessor conditionals**
- Hardware differences abstracted through `hw` interface

### 3. Converting the Main Loop

**Legacy (`uSEQ/src/uSEQ.cpp`):**
```cpp
void uSEQ::tick() {
    get_update_speed() = micros() - get_ts();
    get_ts() = micros();

    if (m_waiting_for_sync_trigger) {
        delayMicroseconds(100);
        return;
    }

#if HAS_INPUTS
    update_inputs();
#endif

    update_time();
    run_scheduled_items();
    update_signals();

#if HAS_OUTPUTS
    update_outs();
#endif

    check_and_handle_user_input();
    delayMicroseconds(100);
}
```

**Refactored (`src/core/useq_update.h`):**
```cpp
template<typename HardwareConfig>
void useq_tick(USEQState& state, const HardwareConfig& hw) {
    unsigned long now = hw.micros();
    state.update_speed = now - state.ts;
    state.ts = now;

    if (state.waiting_for_sync_trigger) {
        hw.delay_microseconds(100);
        return;
    }

    if constexpr (HardwareConfig::NUM_INPUTS > 0) {
        useq_update_inputs(state, hw);
    }

    state.interpreter.update_time();
    state.interpreter.run_scheduled_items();
    useq_update_signals(state, hw);

    if constexpr (HardwareConfig::NUM_CONTINUOUS_OUTS > 0 ||
                  HardwareConfig::NUM_BINARY_OUTS > 0) {
        useq_update_outs(state, hw);
    }

    // check_and_handle_user_input(state, hw);  // TODO
    hw.delay_microseconds(100);
}
```

**Key Changes:**
- `#if HAS_INPUTS` → `if constexpr (HW::NUM_INPUTS > 0)`
- Compile-time conditionals, not preprocessor
- Explicit state passing
- Hardware functions through `hw` interface

### 4. Creating a New Platform Config

To add a new platform, create a config file:

```cpp
// platforms/my_platform/my_platform_config.h

struct MyPlatformConfig {
    // 1. Define compile-time configuration
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

    // 2. Implement I/O functions
    float read_input(size_t index) const {
        // Your hardware-specific code here
        return 0.0f;
    }

    void write_continuous_output(size_t index, float value) const {
        // Your hardware-specific code here
    }

    void write_binary_output(size_t index, float value) const {
        // Your hardware-specific code here
    }

    void write_serial_output(size_t index, float value) const {
        // Your hardware-specific code here
    }

    // 3. Implement timing functions
    unsigned long micros() const {
        // Your timing code here
        return 0;
    }

    void delay_microseconds(unsigned int us) const {
        // Your delay code here
    }

    // 4. Initialize hardware
    void init() {
        // Setup pins, peripherals, etc.
    }

    void tick_maintenance() {
        // Any per-tick maintenance
    }
};
```

Then use it:

```cpp
#include "core/useq_state.h"
#include "core/useq_update.h"
#include "platforms/my_platform/my_platform_config.h"

USEQState state;
MyPlatformConfig hw;

void setup() {
    hw.init();
    state.interpreter.init();
    // Initialize arrays to match hardware config...
}

void loop() {
    useq_tick(state, hw);
}
```

## Common Patterns

### Pattern: Accessing Member Variables

**Legacy:**
```cpp
void uSEQ::some_function() {
    m_continuous_vals[0] = 0.5;
    get_environment()->set("x", Value(1.0));
    eval(expr);
}
```

**Refactored:**
```cpp
template<typename HW>
void some_function(USEQState& state, const HW& hw) {
    state.continuous_vals[0] = 0.5;
    state.environment.set("x", Value(1.0));
    state.interpreter.eval(expr);
}
```

### Pattern: Conditional Compilation

**Legacy:**
```cpp
#ifdef ENABLE_TEMPO_ESTIMATION
    tempoI1.estimateTempo(timestamp);
#endif
```

**Refactored:**
```cpp
if constexpr (HardwareConfig::HAS_TEMPO_DETECTION) {
    // Tempo detection code
}
```

Or move to platform config:
```cpp
struct MyPlatformConfig {
    void handle_tempo_input(double timestamp) const {
        if constexpr (HAS_TEMPO_DETECTION) {
            // Platform-specific tempo handling
        }
    }
};
```

### Pattern: Hardware Abstraction

**Legacy:**
```cpp
#ifdef ARDUINO
    int val = analogRead(PIN_I1);
#else
    int val = io ? io->analog_read(0) : 0;
#endif
```

**Refactored:**
```cpp
float val = hw.read_input(0);  // Platform handles the details
```

## Migration Checklist

When converting a file from legacy to refactored:

- [ ] Remove class definition, create free functions
- [ ] Add `USEQState&` parameter to functions
- [ ] Add `HardwareConfig` template parameter
- [ ] Replace `m_` member access with `state.`
- [ ] Replace `eval()` with `state.interpreter.eval()`
- [ ] Replace `#ifdef` with `if constexpr` or move to config
- [ ] Replace direct hardware calls with `hw.` calls
- [ ] Use `constexpr` for compile-time configuration
- [ ] Remove inheritance, use composition
- [ ] Make state explicit, not hidden in `this`

## Testing Your Migration

1. **Unit test with MockHardwareConfig:**
```cpp
TEST_CASE("Your converted function") {
    USEQState state;
    MockHardwareConfig hw;

    // Setup
    state.continuous_ASTs[0] = Value(0.5);

    // Execute
    your_converted_function(state, hw);

    // Verify
    REQUIRE(state.continuous_vals[0] == 0.5f);
}
```

2. **Compile for multiple platforms:**
```cpp
// Should compile with any config:
useq_tick(state, MockHardwareConfig{});
useq_tick(state, HardwareV1_0_Config{});
useq_tick(state, WasmConfig{});
```

3. **Verify no preprocessor in core:**
```bash
grep -r "#ifdef" src/core/
# Should return nothing (or only include guards)
```

## Advantages of the Refactored Approach

✅ **Simpler**: No inheritance hierarchies
✅ **Testable**: Mock any platform
✅ **Maintainable**: Platform code isolated
✅ **Performant**: Templates are zero-overhead
✅ **Extensible**: New platforms = new config file
✅ **Clear**: State is explicit, not hidden

## Need Help?

See:
- `src/README.md` - Architecture overview
- `src/examples/simple_mock_test.cpp` - Complete example
- `src/core/hardware_config.h` - Interface documentation
- `src/platforms/mock/mock_hardware.h` - Reference implementation
