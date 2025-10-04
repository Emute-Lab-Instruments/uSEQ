/**
 * hardware_v1_0_main.ino - Arduino sketch for uSEQ Hardware v1.0
 *
 * This is the main Arduino entry point for the refactored uSEQ architecture.
 * It uses the new core system with platform-specific configuration.
 *
 * To build this:
 * 1. Open in Arduino IDE or use arduino-cli
 * 2. Select Board: "Raspberry Pi Pico" (Earle Philhower core)
 * 3. Set CPU Speed: 250MHz
 * 4. Set Flash: 8MB (Sketch: 1MB, FS: 7MB)
 * 5. Set Optimization: -O3 (Optimize Even More)
 * 6. Upload
 *
 * Or use PlatformIO (see platformio.ini in project root).
 */

// Define hardware variant before including anything
#define USEQHARDWARE_1_0

#include "../../core/useq_state.h"
#include "../../core/useq_update.h"
#include "../../core/useq_init.h"
#include "../../core/useq_config.h"
#include "hardware_v1_0_config.h"

// Global state and hardware config
USEQState g_state;
HardwareV1_0_Config g_hardware;

// Performance monitoring
unsigned long g_last_print = 0;
constexpr unsigned long PRINT_INTERVAL = 5000000; // 5 seconds in microseconds

void setup() {
    // Initialize serial for debugging and REPL
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        // Wait up to 3 seconds for serial connection (for debugging)
    }

    Serial.println();
    Serial.println("========================================");
    Serial.println("uSEQ Hardware v1.0 - Refactored Core");
    Serial.println("========================================");
    Serial.println();

    // Initialize hardware
    Serial.println("Initializing hardware...");
    g_hardware.init();
    Serial.println("  Hardware initialized!");

    // Initialize state arrays for this platform
    Serial.println("Initializing state...");
    useq_init_state<HardwareV1_0_Config>(g_state);
    Serial.println("  State arrays initialized!");

    // Initialize interpreter
    Serial.println("Initializing interpreter...");
    useq_init_interpreter(g_state);
    Serial.println("  Interpreter ready!");

    // Set up environment variables
    Serial.println("Setting up environment...");
    useq_setup_env_vars(g_state, g_hardware);
    Serial.println("  Environment configured!");

    // Print configuration
    Serial.println();
    Serial.println("Configuration:");
    Serial.print("  Inputs: ");
    Serial.println(HardwareV1_0_Config::NUM_INPUTS);
    Serial.print("  Continuous outputs: ");
    Serial.println(HardwareV1_0_Config::NUM_CONTINUOUS_OUTS);
    Serial.print("  Binary outputs: ");
    Serial.println(HardwareV1_0_Config::NUM_BINARY_OUTS);
    Serial.print("  Serial outputs: ");
    Serial.println(HardwareV1_0_Config::NUM_SERIAL_OUTS);
    Serial.print("  Tempo detection: ");
    Serial.println(HardwareV1_0_Config::HAS_TEMPO_DETECTION ? "Yes" : "No");
    Serial.print("  I2C networking: ");
    Serial.println(HardwareV1_0_Config::HAS_I2C ? "Yes" : "No");
    Serial.println();

    // Set a simple test expression for output a1
    Serial.println("Setting test expression: a1 = (tri 1.0)");
    g_state.continuous_ASTs[0] = g_state.parser.parse("(tri 1.0)");
    Serial.println();

    Serial.println("Entering main loop...");
    Serial.println("========================================");
    Serial.println();

    g_last_print = micros();
}

void loop() {
    // Run one update cycle
    useq_tick(g_state, g_hardware);

    // Optional: Print status periodically
    unsigned long now = micros();
    if (now - g_last_print > PRINT_INTERVAL) {
        Serial.println("Status:");
        Serial.print("  FPS: ");
        Serial.println(1000000.0 / g_state.update_speed, 2);
        Serial.print("  BPM: ");
        if (auto* time_mgr = g_state.interpreter.get_time_manager()) {
            Serial.println(time_mgr->get_bpm(), 2);
        } else {
            Serial.println("N/A");
        }
        Serial.print("  a1 value: ");
        Serial.println(g_state.continuous_vals[0], 3);
        Serial.println();

        g_last_print = now;
    }

    // Check for serial input (REPL)
    if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();

        if (line.length() > 0) {
            Serial.print("> ");
            Serial.println(line);

            // Evaluate the input
            String result = g_state.interpreter.eval(line);
            Serial.println(result);
            Serial.println();
        }
    }
}
