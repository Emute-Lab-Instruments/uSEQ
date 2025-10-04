#ifndef USEQ_PLATFORMS_MUSICTHING_CONFIG_H
#define USEQ_PLATFORMS_MUSICTHING_CONFIG_H

#include "../../core/hardware_config.h"

/**
 * MusicThingConfig - Configuration for Music Thing Modular variant
 *
 * This is the Music Thing Modular hardware configuration for the uSEQ eurorack module.
 * It features inverted outputs, RGB LED support, DSP engine, and tempo estimation.
 *
 * Hardware features:
 * - 4 analog (CV) outputs (aL, aR, a3, a4)
 * - 2 digital (gate) outputs (d1, d2)
 * - 4 serial outputs
 * - Audio inputs (L, R)
 * - 2 gate inputs (I1, I2)
 * - Multiplexed analog inputs via MUX
 * - RGB LED support
 * - Tempo detection on I1 and I2
 * - I2C networking support
 * - Flash storage (16MB)
 * - DSP engine for audio processing
 *
 * Pin mapping (from pinmap.h):
 * - Output pins: -1, -1, 23, 22, 8, 9 (aL, aR, a3, a4, d1, d2)
 *   Note: aL and aR are audio outputs handled by DSP, not direct GPIO
 * - Output LEDs: pins 10, 11, 12, 13, 14, 15
 * - Gate inputs: pins 2, 3 (I1, I2)
 * - Audio inputs: pins 26, 27 (L, R)
 * - MUX inputs: pins 28, 29
 * - MUX logic: pins 24, 25
 * - DAC (for CV outputs): SCK=18, SDI=19, CS=21
 */
struct MusicThingConfig {
    // === Compile-time Configuration ===
    static constexpr size_t NUM_INPUTS = 14;           // I1, I2, + MUX inputs + controls
    static constexpr size_t NUM_CONTINUOUS_OUTS = 4;   // aL, aR, a3, a4
    static constexpr size_t NUM_BINARY_OUTS = 2;       // d1, d2
    static constexpr size_t NUM_SERIAL_OUTS = 4;
    static constexpr size_t NUM_SERIAL_INS = 4;

    static constexpr bool HAS_TEMPO_DETECTION = true;
    static constexpr bool HAS_I2C = true;
    static constexpr bool HAS_FLASH_STORAGE = true;
    static constexpr bool HAS_DSP_ENGINE = true;
    static constexpr bool HAS_RGB_LED = true;
    static constexpr bool SUPPORTS_DYNAMIC_IO = false;

    // Output inversion flags (Music Thing hardware specifics)
    static constexpr bool INVERT_DIGITAL_OUTPUTS = true;
    static constexpr bool INVERT_ANALOG_OUTPUTS = true;
    static constexpr bool INVERT_AUDIO_OUTPUTS = true;

    // === Pin Definitions ===
    // Note: -1 indicates no direct GPIO pin (e.g., audio outputs via DSP)
    static constexpr int OUTPUT_PINS[6] = { -1, -1, 23, 22, 8, 9 };
    static constexpr int OUTPUT_LED_PINS[6] = { 10, 11, 12, 13, 14, 15 };

    static constexpr int PIN_I1 = 2;
    static constexpr int PIN_I2 = 3;

    static constexpr int PIN_AUDIO_IN_L = 26;
    static constexpr int PIN_AUDIO_IN_R = 27;

    static constexpr int PIN_MUX_IN_1 = 28;
    static constexpr int PIN_MUX_IN_2 = 29;
    static constexpr int PIN_MUX_LOGIC_A = 24;
    static constexpr int PIN_MUX_LOGIC_B = 25;

    // DAC pins for CV outputs
    static constexpr int PIN_DAC_SCK = 18;
    static constexpr int PIN_DAC_SDI = 19;
    static constexpr int PIN_DAC_CS = 21;

    // === I2C Configuration ===
    static constexpr int I2C_SDA_PIN = 0;
    static constexpr int I2C_SCL_PIN = 1;
    static constexpr int I2C_ADDRESS = 0x42;  // Default address

    // === Hardware Type ID ===
    static constexpr const char* HARDWARE_TYPE_ID = "MTTHING";

    // === I/O Functions ===

    /**
     * Read an input value
     *
     * Input mapping:
     *   0, 1: Gate inputs I1, I2
     *   2, 3: Audio inputs L, R
     *   4-7: Multiplexed inputs (4 channels via 2 MUX inputs)
     *   8-13: Control inputs (knobs, switches from MUX)
     */
    float read_input(size_t index) const {
#ifdef ARDUINO
        // TODO: Implement actual hardware reads
        // - Gates: digitalRead() for I1, I2
        // - Audio: analogRead() for L, R
        // - MUX: Set MUX_LOGIC_A/B, read MUX_IN_1/2
        return 0.0f;
#else
        // Desktop/test build - no hardware
        return 0.0f;
#endif
    }

    /**
     * Write a continuous (CV) output
     *
     * Note: aL and aR (indices 0, 1) are handled by the DSP engine,
     * not by this function. Only a3 and a4 (indices 2, 3) use the DAC.
     */
    void write_continuous_output(size_t index, float value) const {
#ifdef ARDUINO
        if (index >= NUM_CONTINUOUS_OUTS) return;

        // Audio outputs (aL, aR) are handled by DSP engine
        if (index < 2) {
            // Update LED to show output activity
            int led_pin = OUTPUT_LED_PINS[index];
            int led_value = static_cast<int>(value * 255.0f);
            // analogWrite(led_pin, led_value);
            return;
        }

        // CV outputs (a3, a4) use external DAC
        int pin = OUTPUT_PINS[index];
        int led_pin = OUTPUT_LED_PINS[index];

        // Apply inversion if configured
        float output_value = INVERT_ANALOG_OUTPUTS ? (1.0f - value) : value;

        // Convert [0, 1] to DAC range (typically 0-4095 for 12-bit)
        int dac_value = static_cast<int>(output_value * 4095.0f);

        // Write to DAC via SPI
        // writeDACChannel(index - 2, dac_value);

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

        // Apply inversion if configured
        if (INVERT_DIGITAL_OUTPUTS) {
            is_high = !is_high;
        }

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
        // Initialize output pins
        // for (size_t i = 0; i < 6; i++) {
        //     if (OUTPUT_PINS[i] >= 0) {
        //         pinMode(OUTPUT_PINS[i], OUTPUT);
        //     }
        //     pinMode(OUTPUT_LED_PINS[i], OUTPUT);
        // }
        //
        // Initialize input pins
        // pinMode(PIN_I1, INPUT);
        // pinMode(PIN_I2, INPUT);
        //
        // Initialize MUX pins
        // pinMode(PIN_MUX_LOGIC_A, OUTPUT);
        // pinMode(PIN_MUX_LOGIC_B, OUTPUT);
        //
        // Initialize DAC (SPI)
        // pinMode(PIN_DAC_CS, OUTPUT);
        // digitalWrite(PIN_DAC_CS, HIGH);
        // SPI.begin();
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
        // - Update RGB LED animations
        // - Update DSP engine state
        // - etc.
    }
};

#endif // USEQ_PLATFORMS_MUSICTHING_CONFIG_H
