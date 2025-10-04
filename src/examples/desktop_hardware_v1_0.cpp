/**
 * desktop_hardware_v1_0.cpp - Desktop simulation of Hardware v1.0
 *
 * This demonstrates running the Hardware v1.0 configuration on desktop
 * for testing and development without real hardware.
 *
 * Compile with:
 *   g++ -std=c++20 -I../.. -I../../uSEQ/src \
 *       desktop_hardware_v1_0.cpp \
 *       ../../uSEQ/src/modulisp/lisp/*.cpp \
 *       ../../uSEQ/src/modulisp/*.cpp \
 *       ../../uSEQ/src/utils/*.cpp \
 *       -o desktop_hardware_v1_0
 */

#include "../core/useq_state.h"
#include "../core/useq_update.h"
#include "../core/useq_init.h"
#include "../platforms/mock/mock_hardware.h"
#include <iostream>
#include <string>
#include <sstream>

// Simple REPL loop
void run_repl(USEQState& state, MockHardwareConfig& hw) {
    std::cout << "\n=== uSEQ REPL (type 'quit' to exit) ===\n";
    std::cout << "Enter LISP expressions to evaluate.\n";
    std::cout << "Examples:\n";
    std::cout << "  (+ 1 2)\n";
    std::cout << "  (tri 1.0)\n";
    std::cout << "  (set 'a1 (tri 1.0))\n";
    std::cout << "  (get 'bpm)\n\n";

    std::string line;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, line);

        if (line == "quit" || line == "exit") {
            break;
        }

        if (line.empty()) {
            continue;
        }

        try {
            String result = state.interpreter.eval(String(line.c_str()));
            std::cout << result.c_str() << "\n";
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << "\n";
        } catch (...) {
            std::cout << "Unknown error occurred\n";
        }
    }
}

int main(int argc, char* argv[]) {
    std::cout << "====================================\n";
    std::cout << "uSEQ Hardware v1.0 - Desktop Build\n";
    std::cout << "====================================\n\n";

    // Create state and hardware (using mock for desktop)
    USEQState state;
    MockHardwareConfig hw;

    // Configure mock hardware to match hardware v1.0 layout
    std::cout << "Initializing...\n";
    useq_init<MockHardwareConfig>(state);
    useq_setup_env_vars(state, hw);
    hw.init();

    std::cout << "\nConfiguration:\n";
    std::cout << "  Inputs: " << MockHardwareConfig::NUM_INPUTS << "\n";
    std::cout << "  Continuous outputs: " << MockHardwareConfig::NUM_CONTINUOUS_OUTS << "\n";
    std::cout << "  Binary outputs: " << MockHardwareConfig::NUM_BINARY_OUTS << "\n";
    std::cout << "  Serial outputs: " << MockHardwareConfig::NUM_SERIAL_OUTS << "\n\n";

    // Set some test expressions
    std::cout << "Setting test expressions:\n";

    // a1 = (tri 1.0) - triangle wave at 1 Hz
    std::cout << "  a1 = (tri 1.0)\n";
    state.continuous_ASTs[0] = state.parser.parse("(tri 1.0)");

    // a2 = (+ 0.3 0.2) - constant 0.5
    std::cout << "  a2 = (+ 0.3 0.2)\n";
    state.continuous_ASTs[1] = state.parser.parse("(+ 0.3 0.2)");

    // d1 = 1 - always high
    std::cout << "  d1 = 1\n";
    state.binary_ASTs[0] = Value(1.0);

    std::cout << "\nRunning 10 update cycles...\n\n";

    // Run a few update cycles to show it working
    for (int i = 0; i < 10; i++) {
        // Advance time
        hw.advance_time(10000); // 10ms

        // Run update
        useq_tick(state, hw);

        // Print outputs
        std::cout << "Cycle " << i + 1 << ":\n";
        std::cout << "  a1 = " << hw.get_continuous_output(0) << "\n";
        std::cout << "  a2 = " << hw.get_continuous_output(1) << "\n";
        std::cout << "  d1 = " << hw.get_binary_output(0) << "\n";
        std::cout << "  s0 (time) = " << hw.get_serial_output(0) << " seconds\n";
        std::cout << "\n";
    }

    // Interactive mode
    if (argc > 1 && std::string(argv[1]) == "--repl") {
        run_repl(state, hw);
    } else {
        std::cout << "Tip: Run with --repl for interactive mode\n";
    }

    std::cout << "\nDone!\n";
    return 0;
}
