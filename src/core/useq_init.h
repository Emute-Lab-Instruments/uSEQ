#ifndef USEQ_CORE_INIT_H
#define USEQ_CORE_INIT_H

#include "useq_state.h"
#include "hardware_config.h"

/**
 * Initialization helpers for USEQState
 *
 * These functions help set up a USEQState instance for a specific hardware platform.
 * They handle the boilerplate of resizing arrays, setting defaults, etc.
 */

/**
 * Initialize USEQState for a specific hardware configuration
 *
 * This sets up all the arrays to the correct sizes and initializes defaults.
 * Call this once at startup before using useq_tick().
 *
 * Example:
 *   USEQState state;
 *   HardwareV1_0_Config hw;
 *   useq_init_state<HardwareV1_0_Config>(state);
 *   state.interpreter.init();
 */
template<typename HardwareConfig>
void useq_init_state(USEQState& state) {
    // Resize output arrays
    state.continuous_ASTs.resize(HardwareConfig::NUM_CONTINUOUS_OUTS, Value::nil());
    state.continuous_vals.resize(HardwareConfig::NUM_CONTINUOUS_OUTS, useq::DEFAULT_CONTINUOUS_VALUE);

    state.binary_ASTs.resize(HardwareConfig::NUM_BINARY_OUTS, Value::nil());
    state.binary_vals.resize(HardwareConfig::NUM_BINARY_OUTS, useq::DEFAULT_BINARY_VALUE);

    state.serial_ASTs.resize(HardwareConfig::NUM_SERIAL_OUTS, Value::nil());
    state.serial_vals.resize(HardwareConfig::NUM_SERIAL_OUTS, std::nullopt);

    // Resize input arrays
    state.input_vals.resize(HardwareConfig::NUM_INPUTS, 0.0f);
    state.serial_input_vals.resize(HardwareConfig::NUM_SERIAL_INS, 0.0f);

    // Set default expressions
    state.default_continuous_expr = Value::nil();
    state.default_binary_expr = Value::nil();
    state.default_serial_expr = Value::nil();

    // Initialize state flags
    state.is_initialized = false;  // Set to true after interpreter init
    state.should_quit = false;
    state.is_playing = true;
    state.waiting_for_sync_trigger = false;
    state.current_expr_sound = true;

    // Initialize timing
    state.ts = 0;
    state.update_speed = 0;
    state.serial_out_timestamp = 0;

    // Initialize clock source
    state.clock_source = USEQState::ClockSource::INTERNAL;
    state.reset_ext_clock_tracking();
}

/**
 * Initialize the interpreter and mark state as ready
 *
 * This must be called after useq_init_state() and before useq_tick().
 * It initializes the LISP interpreter and registers all builtin functions.
 */
inline void useq_init_interpreter(USEQState& state) {
    state.interpreter.init();
    state.is_initialized = true;
}

/**
 * Complete initialization - both state and interpreter
 *
 * Convenience function that calls both useq_init_state and useq_init_interpreter.
 */
template<typename HardwareConfig>
void useq_init(USEQState& state) {
    useq_init_state<HardwareConfig>(state);
    useq_init_interpreter(state);
}

/**
 * Set up environment variables based on hardware configuration
 *
 * This registers platform-specific constants in the LISP environment,
 * such as the number of inputs/outputs available.
 */
template<typename HardwareConfig>
void useq_setup_env_vars(USEQState& state, const HardwareConfig& hw) {
    // Register hardware capabilities
    state.environment.set("num-inputs", Value(static_cast<double>(HardwareConfig::NUM_INPUTS)));
    state.environment.set("num-continuous-outs", Value(static_cast<double>(HardwareConfig::NUM_CONTINUOUS_OUTS)));
    state.environment.set("num-binary-outs", Value(static_cast<double>(HardwareConfig::NUM_BINARY_OUTS)));
    state.environment.set("num-serial-outs", Value(static_cast<double>(HardwareConfig::NUM_SERIAL_OUTS)));
    state.environment.set("num-serial-ins", Value(static_cast<double>(HardwareConfig::NUM_SERIAL_INS)));

    // Register feature flags
    state.environment.set("has-tempo-detection", Value(HardwareConfig::HAS_TEMPO_DETECTION ? 1.0 : 0.0));
    state.environment.set("has-i2c", Value(HardwareConfig::HAS_I2C ? 1.0 : 0.0));
    state.environment.set("has-flash", Value(HardwareConfig::HAS_FLASH_STORAGE ? 1.0 : 0.0));
    state.environment.set("has-dsp", Value(HardwareConfig::HAS_DSP_ENGINE ? 1.0 : 0.0));
    state.environment.set("supports-dynamic-io", Value(HardwareConfig::SUPPORTS_DYNAMIC_IO ? 1.0 : 0.0));

    // Register hardware type ID (if available)
    if constexpr (requires { HardwareConfig::HARDWARE_TYPE_ID; }) {
        state.environment.set("hardware-id", Value::string(HardwareConfig::HARDWARE_TYPE_ID));
    }
}

#endif // USEQ_CORE_INIT_H
