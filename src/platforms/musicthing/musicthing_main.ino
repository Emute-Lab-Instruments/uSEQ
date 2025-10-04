/**
 * musicthing_main.ino - Arduino sketch for Music Thing Modular variant
 *
 * This is the main Arduino entry point for the refactored uSEQ architecture
 * running on the Music Thing Modular hardware.
 *
 * Features:
 * - 4 CV outputs (aL, aR via DSP + a3, a4 via DAC)
 * - 2 gate outputs (d1, d2)
 * - Audio inputs (L, R)
 * - Gate inputs (I1, I2)
 * - Multiplexed control inputs (knobs, switches)
 * - RGB LED support
 * - DSP engine for audio processing
 * - Tempo estimation
 * - I2C networking
 * - 16MB flash storage
 *
 * To build this:
 * 1. Use PlatformIO (recommended): pio run -e musicthing
 * 2. Or Arduino CLI with Music Thing settings
 * 3. Or use ./scripts/build_pio.sh musicthing
 *
 * To upload:
 * - PlatformIO: pio run -e musicthing -t upload
 * - Or: ./scripts/flash_pio.sh musicthing
 */

// Define hardware variant before including anything
#define MUSICTHING
#define ENABLE_DSP_ENGINE
#define ENABLE_TEMPO_ESTIMATOR
#define ENABLE_I2C_NETWORKING
#define ENABLE_RGB_LED

#include "../../core/useq_state.h"
#include "../../core/useq_update.h"
#include "../../core/useq_init.h"
#include "../../core/useq_config.h"
#include "musicthing_config.h"

// Global state and hardware config
USEQState g_state;
MusicThingConfig g_hardware;

// Performance monitoring
unsigned long g_last_print = 0;
constexpr unsigned long PRINT_INTERVAL = 5000000; // 5 seconds in microseconds

// RGB LED state (if available)
#ifdef ENABLE_RGB_LED
uint8_t g_led_r = 0, g_led_g = 0, g_led_b = 0;
#endif

void setup() {
    // Initialize serial for debugging and REPL
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        // Wait up to 3 seconds for serial connection (for debugging)
    }

    Serial.println();
    Serial.println("========================================");
    Serial.println("uSEQ Music Thing Modular - Refactored");
    Serial.println("========================================");
    Serial.println();

    // Initialize hardware
    Serial.println("Initializing hardware...");
    g_hardware.init();
    Serial.println("  Hardware initialized!");

    // Initialize state arrays for this platform
    Serial.println("Initializing state...");
    useq_init_state<MusicThingConfig>(g_state);
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
    Serial.print("  Hardware: ");
    Serial.println(MusicThingConfig::HARDWARE_TYPE_ID);
    Serial.print("  Inputs: ");
    Serial.println(MusicThingConfig::NUM_INPUTS);
    Serial.print("  CV outputs: ");
    Serial.println(MusicThingConfig::NUM_CONTINUOUS_OUTS);
    Serial.print("  Gate outputs: ");
    Serial.println(MusicThingConfig::NUM_BINARY_OUTS);
    Serial.print("  Serial outputs: ");
    Serial.println(MusicThingConfig::NUM_SERIAL_OUTS);
    Serial.println();
    Serial.println("Features:");
    Serial.print("  DSP engine: ");
    Serial.println(MusicThingConfig::HAS_DSP_ENGINE ? "Yes" : "No");
    Serial.print("  Tempo detection: ");
    Serial.println(MusicThingConfig::HAS_TEMPO_DETECTION ? "Yes" : "No");
    Serial.print("  I2C networking: ");
    Serial.println(MusicThingConfig::HAS_I2C ? "Yes" : "No");
    Serial.print("  RGB LED: ");
    Serial.println(MusicThingConfig::HAS_RGB_LED ? "Yes" : "No");
    Serial.print("  Flash storage: ");
    Serial.println(MusicThingConfig::HAS_FLASH_STORAGE ? "Yes" : "No");
    Serial.println();

    // Set a simple test expression for CV output a3
    Serial.println("Setting test expression: a3 = (tri 1.0)");
    // a3 is index 2 (aL=0, aR=1, a3=2, a4=3)
    if (g_state.continuous_ASTs.size() > 2) {
        g_state.continuous_ASTs[2] = g_state.parser.parse("(tri 1.0)");
    }
    Serial.println();

    // Set test expression for gate output d1
    Serial.println("Setting test expression: d1 = (sqr 2.0)");
    // d1 is index 0 in binary outputs
    if (g_state.binary_ASTs.size() > 0) {
        g_state.binary_ASTs[0] = g_state.parser.parse("(sqr 2.0)");
    }
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
        Serial.print("  Update rate: ");
        Serial.print(1000000.0 / g_state.update_speed, 2);
        Serial.println(" Hz");
        Serial.print("  BPM: ");
        if (auto* time_mgr = g_state.interpreter.get_time_manager()) {
            Serial.println(time_mgr->get_bpm(), 2);
        } else {
            Serial.println("N/A");
        }

        // Print output values
        if (g_state.continuous_vals.size() > 2) {
            Serial.print("  a3 value: ");
            Serial.println(g_state.continuous_vals[2], 3);
        }
        if (g_state.binary_vals.size() > 0) {
            Serial.print("  d1 value: ");
            Serial.println(g_state.binary_vals[0], 3);
        }

        // Print input samples
        if (g_state.input_vals.size() > 0) {
            Serial.print("  I1: ");
            Serial.print(g_state.input_vals[0], 3);
            Serial.print("  I2: ");
            Serial.println(g_state.input_vals[1], 3);
        }

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

            // Handle special commands
            if (line.startsWith("@")) {
                // Immediate execution (prefix with @)
                line = line.substring(1);
                String result = g_state.interpreter.eval(line);
                Serial.println(result);
            } else if (line.startsWith("help")) {
                // Print help message
                Serial.println("Music Thing Modular REPL");
                Serial.println("Commands:");
                Serial.println("  @<expr>       - Immediate evaluation");
                Serial.println("  <expr>        - Queue for next bar");
                Serial.println("  help          - This help message");
                Serial.println("  info          - System information");
                Serial.println();
            } else if (line.startsWith("info")) {
                // Print system info
                Serial.println("System Information:");
                Serial.print("  Free heap: ");
                Serial.print(rp2040.getFreeHeap());
                Serial.println(" bytes");
                Serial.print("  CPU freq: ");
                Serial.print(rp2040.f_cpu() / 1000000);
                Serial.println(" MHz");
                Serial.println();
            } else {
                // Queue for next bar
                String result = g_state.interpreter.eval(line);
                Serial.println(result);
            }

            Serial.println();
        }
    }
}
