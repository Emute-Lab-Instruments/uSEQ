#ifndef USEQ_CORE_UPDATE_H
#define USEQ_CORE_UPDATE_H

#include "useq_state.h"
#include "../../uSEQ/src/utils/log.h"
#include <cstddef>

/**
 * Core update loop functions - platform-agnostic
 *
 * These functions implement the uSEQ update logic without any platform-specific code.
 * They operate purely on USEQState and use the HardwareConfig interface for I/O.
 *
 * Philosophy:
 * - Pure functions (operate on state, don't hide it)
 * - No preprocessor conditionals
 * - Platform differences handled through HardwareConfig parameter
 * - Template-based for compile-time polymorphism (zero runtime overhead)
 */

/**
 * Update continuous output signals (analog outputs: a1, a2, a3, ...)
 *
 * Evaluates each continuous_AST and caches the result in continuous_vals.
 * If an expression doesn't evaluate to a number, it's cleared and replaced with default.
 */
template<typename HardwareConfig>
void useq_update_continuous_signals(USEQState& state, const HardwareConfig& hw) {
    constexpr size_t num_outs = HardwareConfig::NUM_CONTINUOUS_OUTS;

    for (size_t i = 0; i < num_outs; i++) {
        // Clear error queue
        error_msg_q.clear();

        // Set context for error reporting
        String expr_name = String("a") + String(i + 1);
        ModuLispInterpreter::set_atom_currently_being_evaluated(expr_name);

        Value expr = state.continuous_ASTs[i];

        if (expr.is_nil()) {
            state.continuous_vals[i] = 0.5f;  // Default middle value
        } else {
            dbg("Evalling: " + expr.display());

            Value result = state.interpreter.eval(expr);

            if (!result.is_number()) {
                println("**Warning**: Clearing the expression for **a" +
                       String(i + 1) +
                       "** because it doesn't evaluate to a number:\n    " +
                       expr.display());

                // Print first error if any
                if (error_msg_q.size() > 0) {
                    println(error_msg_q[0]);
                }

                state.continuous_ASTs[i] = state.default_continuous_expr;
                state.continuous_vals[i] = 0.5f;
            } else {
                state.continuous_vals[i] = result.as_float();
            }
        }
    }
}

/**
 * Update binary output signals (digital/gate outputs: d1, d2, d3, ...)
 *
 * Evaluates each binary_AST and caches the result in binary_vals.
 */
template<typename HardwareConfig>
void useq_update_binary_signals(USEQState& state, const HardwareConfig& hw) {
    constexpr size_t num_outs = HardwareConfig::NUM_BINARY_OUTS;

    for (size_t i = 0; i < num_outs; i++) {
        error_msg_q.clear();

        String expr_name = String("d") + String(i + 1);
        ModuLispInterpreter::set_atom_currently_being_evaluated(expr_name);

        Value expr = state.binary_ASTs[i];

        if (expr.is_nil()) {
            state.binary_vals[i] = 0.0f;
        } else {
            dbg("Evalling: " + expr.display());
            Value result = state.interpreter.eval(expr);

            if (!result.is_number()) {
                println("**Warning**: Clearing the expression for **d" +
                       String(i + 1) +
                       "** because it doesn't evaluate to a number:\n    " +
                       expr.display());

                if (error_msg_q.size() > 0) {
                    println(error_msg_q[0]);
                }

                state.binary_ASTs[i] = state.default_binary_expr;
                state.binary_vals[i] = 0.0f;
            } else {
                state.binary_vals[i] = result.as_float();
            }
        }
    }
}

/**
 * Update serial output signals (s0, s1, s2, ...)
 *
 * s0 is special - it outputs time_since_boot
 * Other serial outputs evaluate their ASTs
 */
template<typename HardwareConfig>
void useq_update_serial_signals(USEQState& state, const HardwareConfig& hw) {
    constexpr size_t num_outs = HardwareConfig::NUM_SERIAL_OUTS;

    // s0 is always time
    if (!state.serial_vals.empty()) {
        double time_seconds = 0.0;
        if (auto* time_manager = state.interpreter.get_time_manager()) {
            time_seconds = time_manager->get_time_since_boot() / 1e6;
        }
        state.serial_vals[0] = time_seconds;
    }

    // s1, s2, ... are user-defined
    for (size_t i = 1; i < num_outs; i++) {
        error_msg_q.clear();

        String expr_name = String("s") + String(i);
        ModuLispInterpreter::set_atom_currently_being_evaluated(expr_name);

        Value expr = state.serial_ASTs[i];

        if (expr.is_nil()) {
            // Signal that there's no value to write
            state.serial_vals[i] = std::nullopt;
        } else {
            dbg("Expr: " + expr.display());
            Value result = state.interpreter.eval(expr);

            if (!result.is_number()) {
                println("**Warning**: Clearing the expression for **s" +
                       String(i) +
                       "** because it doesn't evaluate to a number:\n    " +
                       expr.display());

                if (error_msg_q.size() > 0) {
                    println(error_msg_q[0]);
                }

                state.serial_ASTs[i] = state.default_serial_expr;
                state.serial_vals[i] = std::nullopt;
            } else {
                state.serial_vals[i] = result.as_float();
            }
        }
    }
}

/**
 * Update all output signals
 *
 * Main signal processing function that updates all cached output values.
 * Sets evaluation flags before/after to control interpreter behavior.
 */
template<typename HardwareConfig>
void useq_update_signals(USEQState& state, const HardwareConfig& hw) {
    DBG("useq_update_signals");

    // Enable expression evaluation mode
    ModuLispInterpreter::set_attempt_expr_eval_first(true);
    ModuLispInterpreter::set_update_loop_evaluation(true);

    // Update all signal types
    useq_update_continuous_signals(state, hw);
    useq_update_binary_signals(state, hw);
    useq_update_serial_signals(state, hw);

    // Restore evaluation flags
    ModuLispInterpreter::set_attempt_expr_eval_first(false);
    ModuLispInterpreter::set_update_loop_evaluation(false);
}

/**
 * Write continuous outputs to hardware
 *
 * Takes cached continuous_vals and writes them to hardware outputs.
 * Platform-specific behavior is handled through HardwareConfig.
 */
template<typename HardwareConfig>
void useq_update_continuous_outs(USEQState& state, const HardwareConfig& hw) {
    DBG("useq_update_continuous_outs");

    constexpr size_t num_outs = HardwareConfig::NUM_CONTINUOUS_OUTS;

    for (size_t i = 0; i < num_outs; i++) {
        float val = state.continuous_vals[i];
        dbg(String(i));

        // Delegate to hardware-specific write function
        hw.write_continuous_output(i, val);
    }
}

/**
 * Write binary outputs to hardware
 */
template<typename HardwareConfig>
void useq_update_binary_outs(USEQState& state, const HardwareConfig& hw) {
    DBG("useq_update_binary_outs");

    constexpr size_t num_outs = HardwareConfig::NUM_BINARY_OUTS;

    for (size_t i = 0; i < num_outs; i++) {
        dbg(String(i));
        hw.write_binary_output(i, state.binary_vals[i]);
    }
}

/**
 * Write serial outputs to hardware (with rate limiting)
 */
template<typename HardwareConfig>
void useq_update_serial_outs(USEQState& state, const HardwareConfig& hw) {
    DBG("useq_update_serial_outs");

    unsigned long serial_now = hw.micros();
    unsigned long serial_time_elapsed = serial_now - state.serial_out_timestamp;

    // Rate limiting (from SerialMsg::serial_message_rate_limit)
    constexpr unsigned long RATE_LIMIT = 100000; // 100ms in microseconds

    if (serial_time_elapsed > RATE_LIMIT) {
        constexpr size_t num_outs = HardwareConfig::NUM_SERIAL_OUTS;

        for (size_t i = 0; i < num_outs; i++) {
            dbg(String(i));
            std::optional<float> v = state.serial_vals[i];

            // Only write if there is a value
            if (v) {
                dbg("writing value: " + String(*v));
                hw.write_serial_output(i, *v);
            }
        }

        state.serial_out_timestamp = serial_now - (serial_time_elapsed - RATE_LIMIT);
    }
}

/**
 * Update all hardware outputs
 */
template<typename HardwareConfig>
void useq_update_outs(USEQState& state, const HardwareConfig& hw) {
    DBG("useq_update_outs");

    // Order matters: binary before continuous to avoid LED issues
    useq_update_binary_outs(state, hw);
    useq_update_continuous_outs(state, hw);
    useq_update_serial_outs(state, hw);
}

/**
 * Read inputs from hardware
 */
template<typename HardwareConfig>
void useq_update_inputs(USEQState& state, const HardwareConfig& hw) {
    DBG("useq_update_inputs");

    constexpr size_t num_inputs = HardwareConfig::NUM_INPUTS;

    for (size_t i = 0; i < num_inputs; i++) {
        state.input_vals[i] = hw.read_input(i);
    }
}

/**
 * Main tick function - the core update loop
 *
 * This is the heart of uSEQ: read inputs, process signals, write outputs.
 * Platform differences are abstracted through the HardwareConfig interface.
 */
template<typename HardwareConfig>
void useq_tick(USEQState& state, const HardwareConfig& hw) {
    DBG("useq_tick");

    // Update frame timing
    unsigned long now = hw.micros();
    state.update_speed = now - state.ts;
    state.environment.set("fps", Value(1000000.0 / state.update_speed));
    state.environment.set("qt", Value(state.update_speed * 0.001));
    state.ts = now;

    // Early exit if waiting for sync trigger
    if (state.waiting_for_sync_trigger) {
        hw.delay_microseconds(100);
        return;
    }

    // Early exit if paused
    if (!state.is_playing) {
        // check_and_handle_user_input(state, hw);  // TODO: implement REPL
        hw.delay_microseconds(100);
        return;
    }

    // Read hardware inputs
    if constexpr (HardwareConfig::NUM_INPUTS > 0) {
        useq_update_inputs(state, hw);
    }

    // Update time and run scheduler
    state.interpreter.update_time();
    state.interpreter.check_code_quant_phasor();
    state.interpreter.run_scheduled_items();
    state.interpreter.update_Q0();

    // Re-evaluate and cache output signal expressions
    useq_update_signals(state, hw);

    // Write cached values to hardware
    if constexpr (HardwareConfig::NUM_CONTINUOUS_OUTS > 0 ||
                  HardwareConfig::NUM_BINARY_OUTS > 0 ||
                  HardwareConfig::NUM_SERIAL_OUTS > 0) {
        useq_update_outs(state, hw);
    }

    // Check for new code from REPL
    // check_and_handle_user_input(state, hw);  // TODO: implement

    // Small delay for interrupts
    hw.delay_microseconds(100);
}

#endif // USEQ_CORE_UPDATE_H
