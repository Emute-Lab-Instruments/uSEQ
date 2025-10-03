/**
 * simple_mock_test.cpp - Example showing how to use the refactored core
 *
 * This demonstrates:
 * 1. Creating a USEQState
 * 2. Configuring a hardware platform (MockHardwareConfig)
 * 3. Running the update loop
 * 4. Setting inputs and reading outputs
 *
 * Compile with:
 *   g++ -std=c++17 -I../.. -I../../uSEQ/src \
 *       simple_mock_test.cpp \
 *       ../../uSEQ/src/modulisp/lisp/*.cpp \
 *       ../../uSEQ/src/modulisp/*.cpp \
 *       -o simple_mock_test
 */

#include "../core/useq_state.h"
#include "../core/useq_update.h"
#include "../platforms/mock/mock_hardware.h"
#include <iostream>

int main() {
    std::cout << "=== uSEQ Refactored Core - Simple Mock Test ===\n\n";

    // 1. Create the core state
    USEQState state;

    // 2. Create the hardware configuration
    MockHardwareConfig hw;
    hw.set_verbose(true);  // Enable I/O logging

    // 3. Initialize arrays to match hardware config
    state.continuous_ASTs.resize(MockHardwareConfig::NUM_CONTINUOUS_OUTS, Value::nil());
    state.continuous_vals.resize(MockHardwareConfig::NUM_CONTINUOUS_OUTS, 0.0f);

    state.binary_ASTs.resize(MockHardwareConfig::NUM_BINARY_OUTS, Value::nil());
    state.binary_vals.resize(MockHardwareConfig::NUM_BINARY_OUTS, 0.0f);

    state.serial_ASTs.resize(MockHardwareConfig::NUM_SERIAL_OUTS, Value::nil());
    state.serial_vals.resize(MockHardwareConfig::NUM_SERIAL_OUTS, std::nullopt);

    state.input_vals.resize(MockHardwareConfig::NUM_INPUTS, 0.0f);

    // 4. Initialize the interpreter
    state.interpreter.init();
    state.is_initialized = true;

    std::cout << "State initialized with:\n";
    std::cout << "  - " << MockHardwareConfig::NUM_INPUTS << " inputs\n";
    std::cout << "  - " << MockHardwareConfig::NUM_CONTINUOUS_OUTS << " continuous outputs\n";
    std::cout << "  - " << MockHardwareConfig::NUM_BINARY_OUTS << " binary outputs\n";
    std::cout << "  - " << MockHardwareConfig::NUM_SERIAL_OUTS << " serial outputs\n\n";

    // 5. Set some expressions to evaluate
    std::cout << "Setting up test expressions...\n";

    // a1 = 0.5 (constant)
    state.continuous_ASTs[0] = Value(0.5);

    // a2 = (+ 0.2 0.3) (expression that evaluates to 0.5)
    state.continuous_ASTs[1] = state.parser.parse("(+ 0.2 0.3)");

    // d1 = 1 (high)
    state.binary_ASTs[0] = Value(1.0);

    std::cout << "  a1 = " << state.continuous_ASTs[0].display().c_str() << "\n";
    std::cout << "  a2 = " << state.continuous_ASTs[1].display().c_str() << "\n";
    std::cout << "  d1 = " << state.binary_ASTs[0].display().c_str() << "\n\n";

    // 6. Set some input values
    std::cout << "Setting input values...\n";
    hw.set_input(0, 0.75f);
    hw.set_input(1, 0.25f);
    std::cout << "\n";

    // 7. Run a few update cycles
    std::cout << "Running 3 update cycles...\n\n";
    for (int i = 0; i < 3; i++) {
        std::cout << "--- Cycle " << i + 1 << " ---\n";

        // Advance simulated time
        hw.advance_time(1000);  // 1ms

        // Run one tick of the update loop
        useq_tick(state, hw);

        // Check outputs
        std::cout << "After tick:\n";
        std::cout << "  continuous[0] = " << hw.get_continuous_output(0) << "\n";
        std::cout << "  continuous[1] = " << hw.get_continuous_output(1) << "\n";
        std::cout << "  binary[0] = " << hw.get_binary_output(0) << "\n";
        std::cout << "\n";
    }

    std::cout << "=== Test Complete ===\n";
    return 0;
}
