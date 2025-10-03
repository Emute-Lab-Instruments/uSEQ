#ifndef USEQ_CORE_HARDWARE_CONFIG_H
#define USEQ_CORE_HARDWARE_CONFIG_H

#include <cstddef>

/**
 * HardwareConfig Interface
 *
 * This defines the interface that all hardware platforms must implement.
 * Each platform provides:
 * 1. Compile-time constants (NUM_INPUTS, NUM_OUTPUTS, etc.)
 * 2. I/O functions (read_input, write_output, etc.)
 * 3. Timing functions (micros, delay_microseconds, etc.)
 *
 * Philosophy:
 * - Use constexpr for compile-time configuration (zero overhead)
 * - Use function pointers or member functions for runtime behavior
 * - No virtual functions (no vtable overhead for embedded systems)
 * - All platform-specific code lives in platform directories
 *
 * Usage:
 *   1. Define a struct with static constexpr members for configuration
 *   2. Implement I/O and timing functions as members or free functions
 *   3. Pass an instance to useq_tick() and other core functions
 *
 * Example:
 *   struct MyPlatformConfig {
 *       static constexpr size_t NUM_INPUTS = 8;
 *       static constexpr size_t NUM_CONTINUOUS_OUTS = 3;
 *       // ... etc
 *
 *       float read_input(size_t index) const { ... }
 *       void write_continuous_output(size_t index, float value) const { ... }
 *       // ... etc
 *   };
 */

/**
 * Example configuration struct (for documentation purposes)
 *
 * Real platforms should copy this pattern and provide actual implementations.
 */
struct ExampleHardwareConfig {
    // === Compile-time Configuration ===

    // Number of inputs/outputs (fixed at compile time)
    static constexpr size_t NUM_INPUTS = 8;
    static constexpr size_t NUM_CONTINUOUS_OUTS = 3;  // Analog CV outputs
    static constexpr size_t NUM_BINARY_OUTS = 3;      // Digital gate outputs
    static constexpr size_t NUM_SERIAL_OUTS = 4;      // Serial outputs
    static constexpr size_t NUM_SERIAL_INS = 4;       // Serial inputs

    // Feature flags (enable/disable features at compile time)
    static constexpr bool HAS_TEMPO_DETECTION = true;
    static constexpr bool HAS_I2C = true;
    static constexpr bool HAS_FLASH_STORAGE = true;
    static constexpr bool HAS_DSP_ENGINE = true;
    static constexpr bool SUPPORTS_DYNAMIC_IO = false;  // Can add/remove I/O at runtime?

    // === I/O Functions (must be implemented) ===

    /**
     * Read an input value
     * @param index Input index (0 to NUM_INPUTS-1)
     * @return Normalized value [0.0, 1.0]
     */
    float read_input(size_t index) const;

    /**
     * Write a continuous (analog/CV) output
     * @param index Output index (0 to NUM_CONTINUOUS_OUTS-1)
     * @param value Normalized value [0.0, 1.0]
     */
    void write_continuous_output(size_t index, float value) const;

    /**
     * Write a binary (digital/gate) output
     * @param index Output index (0 to NUM_BINARY_OUTS-1)
     * @param value 0.0 (low) or non-zero (high)
     */
    void write_binary_output(size_t index, float value) const;

    /**
     * Write a serial output value
     * @param index Output index (0 to NUM_SERIAL_OUTS-1)
     * @param value Value to send
     */
    void write_serial_output(size_t index, float value) const;

    // === Timing Functions ===

    /**
     * Get microseconds since boot
     * @return Time in microseconds
     */
    unsigned long micros() const;

    /**
     * Delay execution for microseconds
     * @param us Microseconds to delay
     */
    void delay_microseconds(unsigned int us) const;

    // === Optional Platform-Specific Functions ===

    /**
     * Initialize hardware (called once at startup)
     */
    void init();

    /**
     * Perform any per-tick maintenance (e.g., LED updates, DSP checks)
     */
    void tick_maintenance();
};

/**
 * Minimal config for platforms with no I/O (testing, etc.)
 */
struct MinimalConfig {
    static constexpr size_t NUM_INPUTS = 0;
    static constexpr size_t NUM_CONTINUOUS_OUTS = 0;
    static constexpr size_t NUM_BINARY_OUTS = 0;
    static constexpr size_t NUM_SERIAL_OUTS = 0;
    static constexpr size_t NUM_SERIAL_INS = 0;

    static constexpr bool HAS_TEMPO_DETECTION = false;
    static constexpr bool HAS_I2C = false;
    static constexpr bool HAS_FLASH_STORAGE = false;
    static constexpr bool HAS_DSP_ENGINE = false;
    static constexpr bool SUPPORTS_DYNAMIC_IO = false;

    float read_input(size_t) const { return 0.0f; }
    void write_continuous_output(size_t, float) const {}
    void write_binary_output(size_t, float) const {}
    void write_serial_output(size_t, float) const {}
    unsigned long micros() const { return 0; }
    void delay_microseconds(unsigned int) const {}
    void init() {}
    void tick_maintenance() {}
};

#endif // USEQ_CORE_HARDWARE_CONFIG_H
