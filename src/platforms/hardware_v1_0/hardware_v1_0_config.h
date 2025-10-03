#ifndef USEQ_PLATFORMS_HARDWARE_V1_0_CONFIG_H
#define USEQ_PLATFORMS_HARDWARE_V1_0_CONFIG_H

#include "../../core/hardware_config.h"

/**
 * HardwareV1_0_Config - Configuration for uSEQ Hardware v1.0
 *
 * This is the real hardware configuration for the uSEQ v1.0 eurorack module.
 * It demonstrates how to adapt the core logic to specific hardware.
 *
 * Hardware features:
 * - 3 analog (CV) outputs (a1, a2, a3)
 * - 3 digital (gate) outputs (d1, d2, d3)
 * - 4 serial outputs
 * - 8 inputs (2 digital gates + 2 analog CV + 4 from multiplexer)
 * - Tempo detection on I1 and I2
 * - I2C networking support
 * - Flash storage
 * - No DSP engine (unlike MUSICTHING variant)
 *
 * Pin mapping (from pinmap.h):
 * - Outputs: pins 21, 20, 19, 18, 17, 16 (a1, a2, a3, d1, d2, d3)
 * - Output LEDs: pins 3, 2, 11, 12, 13, 22
 * - Digital inputs: pins 8, 9 (I1, I2)
 * - Analog inputs: pins 26, 27 (AI1, AI2)
 * - Input LEDs: pins 5, 4, 25, 24
 */
struct HardwareV1_0_Config {
    // === Compile-time Configuration ===
    static constexpr size_t NUM_INPUTS = 8;
    static constexpr size_t NUM_CONTINUOUS_OUTS = 3;  // a1, a2, a3
    static constexpr size_t NUM_BINARY_OUTS = 3;      // d1, d2, d3
    static constexpr size_t NUM_SERIAL_OUTS = 4;
    static constexpr size_t NUM_SERIAL_INS = 4;

    static constexpr bool HAS_TEMPO_DETECTION = true;
    static constexpr bool HAS_I2C = true;
    static constexpr bool HAS_FLASH_STORAGE = true;
    static constexpr bool HAS_DSP_ENGINE = false;
    static constexpr bool SUPPORTS_DYNAMIC_IO = false;

    // === Pin Definitions ===
    static constexpr int OUTPUT_PINS[6] = { 21, 20, 19, 18, 17, 16 };
    static constexpr int OUTPUT_LED_PINS[6] = { 3, 2, 11, 12, 13, 22 };

    static constexpr int PIN_I1 = 8;
    static constexpr int PIN_I2 = 9;
    static constexpr int PIN_AI1 = 26;
    static constexpr int PIN_AI2 = 27;

    static constexpr int PIN_LED_I1 = 5;
    static constexpr int PIN_LED_I2 = 4;
    static constexpr int PIN_LED_AI1 = 25;
    static constexpr int PIN_LED_AI2 = 24;

    static constexpr int PIN_SWITCH_M1 = 10;
    static constexpr int PIN_SWITCH_T1 = 14;
    static constexpr int PIN_SWITCH_T2 = 23;

    static constexpr int LED_BOARD = 6;

    // === I2C Configuration ===
    static constexpr int I2C_SDA_PIN = 0;
    static constexpr int I2C_SCL_PIN = 1;
    static constexpr int I2C_ADDRESS = 0x42;  // Default address

    // === Hardware Type ID ===
    static constexpr const char* HARDWARE_TYPE_ID = "uSEQ10";

    // === I/O Functions ===

    /**
     * Read an input value
     *
     * Input mapping:
     *   0, 1: Digital gate inputs I1, I2
     *   2, 3: Analog CV inputs AI1, AI2
     *   4-7: Multiplexed inputs
     */
    float read_input(size_t index) const {
#ifdef ARDUINO
        // TODO: Implement actual hardware reads
        // This would use analogRead() for analog inputs,
        // digitalRead() for gates, and multiplexer logic for mux inputs
        return 0.0f;
#else
        // Desktop/test build - no hardware
        return 0.0f;
#endif
    }

    /**
     * Write a continuous (CV) output
     */
    void write_continuous_output(size_t index, float value) const {
#ifdef ARDUINO
        if (index >= NUM_CONTINUOUS_OUTS) return;

        // Get the actual pin number
        int pin = OUTPUT_PINS[index];
        int led_pin = OUTPUT_LED_PINS[index];

        // Convert [0, 1] to DAC range (typically 0-4095 for 12-bit)
        int dac_value = static_cast<int>(value * 4095.0f);

        // Write to output (would use analogWrite or hardware-specific DAC code)
        // analogWrite(pin, dac_value);

        // Update LED to show output activity
        int led_value = static_cast<int>(value * 255.0f);
        // analogWrite(led_pin, led_value);
#endif
    }

    /**
     * Write a binary (gate) output
     */
    void write_binary_output(size_t index, float value) const {
#ifdef ARDUINO
        if (index >= NUM_BINARY_OUTS) return;

        // Binary outputs start after continuous outputs
        size_t pin_index = NUM_CONTINUOUS_OUTS + index;
        int pin = OUTPUT_PINS[pin_index];
        int led_pin = OUTPUT_LED_PINS[pin_index];

        // Convert to digital high/low
        bool is_high = value > 0.5f;

        // Write to output
        // digitalWrite(pin, is_high ? HIGH : LOW);
        // digitalWrite(led_pin, is_high ? HIGH : LOW);
#endif
    }

    /**
     * Write a serial output value
     */
    void write_serial_output(size_t index, float value) const {
#ifdef ARDUINO
        // Serial output via Serial.print()
        // Format: "[index] value\n"
        // Serial.print("[");
        // Serial.print(index);
        // Serial.print("] ");
        // Serial.println(value, 6);
#else
        // Desktop: could log to stdout or file
#endif
    }

    // === Timing Functions ===

    unsigned long micros() const {
#ifdef ARDUINO
        return ::micros();
#else
        // Desktop stub - would use high-resolution timer
        return 0;
#endif
    }

    void delay_microseconds(unsigned int us) const {
#ifdef ARDUINO
        ::delayMicroseconds(us);
#else
        // Desktop stub
#endif
    }

    // === Initialization ===

    void init() {
#ifdef ARDUINO
        // Initialize pins
        // for (int pin : OUTPUT_PINS) {
        //     pinMode(pin, OUTPUT);
        // }
        // for (int pin : OUTPUT_LED_PINS) {
        //     pinMode(pin, OUTPUT);
        // }
        //
        // pinMode(PIN_I1, INPUT);
        // pinMode(PIN_I2, INPUT);
        // // ... etc
        //
        // Initialize I2C
        // Wire.setSDA(I2C_SDA_PIN);
        // Wire.setSCL(I2C_SCL_PIN);
        // Wire.begin(I2C_ADDRESS);
#endif
    }

    void tick_maintenance() {
        // Any per-tick maintenance
        // - Check for I2C messages
        // - Update LED animations
        // - etc.
    }
};

#endif // USEQ_PLATFORMS_HARDWARE_V1_0_CONFIG_H
