#ifndef USEQ_PLATFORMS_MOCK_HARDWARE_H
#define USEQ_PLATFORMS_MOCK_HARDWARE_H

#include "../../core/hardware_config.h"
#include <vector>
#include <cstddef>
#include <cstdio>

/**
 * MockHardwareConfig - A testing/simulation hardware config
 *
 * This provides a simple in-memory implementation of the hardware interface
 * that can be used for:
 * - Unit testing core logic without real hardware
 * - Desktop simulation
 * - Debugging and development
 *
 * Features:
 * - All I/O stored in vectors
 * - Configurable number of inputs/outputs
 * - Simulated time (controllable for testing)
 * - Optional logging of all I/O operations
 */
class MockHardwareConfig {
public:
    // === Compile-time Configuration ===
    static constexpr size_t NUM_INPUTS = 8;
    static constexpr size_t NUM_CONTINUOUS_OUTS = 3;
    static constexpr size_t NUM_BINARY_OUTS = 3;
    static constexpr size_t NUM_SERIAL_OUTS = 4;
    static constexpr size_t NUM_SERIAL_INS = 4;

    static constexpr bool HAS_TEMPO_DETECTION = false;
    static constexpr bool HAS_I2C = false;
    static constexpr bool HAS_FLASH_STORAGE = false;
    static constexpr bool HAS_DSP_ENGINE = false;
    static constexpr bool SUPPORTS_DYNAMIC_IO = false;

    // === Storage for I/O ===
    mutable std::vector<float> inputs;
    mutable std::vector<float> continuous_outputs;
    mutable std::vector<float> binary_outputs;
    mutable std::vector<float> serial_outputs;

    // === Simulated Time ===
    mutable unsigned long simulated_time_us = 0;

    // === Options ===
    bool verbose = false;  // Log all I/O operations

    // === Constructor ===
    MockHardwareConfig()
        : inputs(NUM_INPUTS, 0.0f),
          continuous_outputs(NUM_CONTINUOUS_OUTS, 0.0f),
          binary_outputs(NUM_BINARY_OUTS, 0.0f),
          serial_outputs(NUM_SERIAL_OUTS, 0.0f)
    {
    }

    // === I/O Functions ===

    float read_input(size_t index) const {
        if (index >= NUM_INPUTS) {
            return 0.0f;
        }
        float value = inputs[index];
        if (verbose) {
            printf("[MockHW] Read input[%zu] = %.3f\n", index, value);
        }
        return value;
    }

    void write_continuous_output(size_t index, float value) const {
        if (index >= NUM_CONTINUOUS_OUTS) {
            return;
        }
        continuous_outputs[index] = value;
        if (verbose) {
            printf("[MockHW] Write continuous[%zu] = %.3f\n", index, value);
        }
    }

    void write_binary_output(size_t index, float value) const {
        if (index >= NUM_BINARY_OUTS) {
            return;
        }
        binary_outputs[index] = value;
        if (verbose) {
            printf("[MockHW] Write binary[%zu] = %.3f\n", index, value);
        }
    }

    void write_serial_output(size_t index, float value) const {
        if (index >= NUM_SERIAL_OUTS) {
            return;
        }
        serial_outputs[index] = value;
        if (verbose) {
            printf("[MockHW] Write serial[%zu] = %.3f\n", index, value);
        }
    }

    // === Timing Functions ===

    unsigned long micros() const {
        return simulated_time_us;
    }

    void delay_microseconds(unsigned int us) const {
        simulated_time_us += us;
    }

    // === Initialization ===

    void init() {
        if (verbose) {
            printf("[MockHW] Initialized with %zu inputs, %zu continuous outs, "
                   "%zu binary outs, %zu serial outs\n",
                   NUM_INPUTS, NUM_CONTINUOUS_OUTS, NUM_BINARY_OUTS, NUM_SERIAL_OUTS);
        }
    }

    void tick_maintenance() {
        // Nothing to do for mock hardware
    }

    // === Testing Helpers ===

    void set_input(size_t index, float value) {
        if (index < NUM_INPUTS) {
            inputs[index] = value;
        }
    }

    float get_continuous_output(size_t index) const {
        return index < NUM_CONTINUOUS_OUTS ? continuous_outputs[index] : 0.0f;
    }

    float get_binary_output(size_t index) const {
        return index < NUM_BINARY_OUTS ? binary_outputs[index] : 0.0f;
    }

    float get_serial_output(size_t index) const {
        return index < NUM_SERIAL_OUTS ? serial_outputs[index] : 0.0f;
    }

    void advance_time(unsigned long us) {
        simulated_time_us += us;
    }

    void reset_time() {
        simulated_time_us = 0;
    }

    void clear_outputs() {
        for (auto& v : continuous_outputs) v = 0.0f;
        for (auto& v : binary_outputs) v = 0.0f;
        for (auto& v : serial_outputs) v = 0.0f;
    }

    void set_verbose(bool v) {
        verbose = v;
    }
};

#endif // USEQ_PLATFORMS_MOCK_HARDWARE_H
