/**
 * hardware_v1_0_impl.cpp - Implementation of uSEQ Hardware v1.0 platform
 *
 * This file contains the actual Arduino-specific implementations for the
 * hardware v1.0 eurorack module.
 */

#include "hardware_v1_0_config.h"

#ifdef ARDUINO

#include <Arduino.h>
#include <Wire.h>

// Hardware v1.0 specific includes
#ifdef USEQHARDWARE_1_0
#include <hardware/gpio.h>
#include <hardware/timer.h>
#endif

namespace {

// PDM (Pulse Density Modulation) state for analog outputs
float pdm_y = 0;
float pdm_err = 0;
float pdm_w = 0;

// Conversion constants
constexpr double RECP_4096 = 1.0 / 4096.0;  // 12-bit ADC reciprocal
constexpr int MAX_PWM = 2047;  // 11-bit PWM max value
constexpr double MAX_PWM_F = static_cast<double>(MAX_PWM);

/**
 * PDM-based analog output for hardware v1.0
 *
 * Uses Pulse Density Modulation to create smooth analog outputs
 * from digital pins. This is a delta-sigma implementation.
 */
inline void pdm_analog_write(int pin, int value) {
    // Clamp value
    if (value > MAX_PWM) value = MAX_PWM;
    if (value < 0) value = 0;

    // Delta-sigma modulation
    pdm_w = static_cast<float>(value) / MAX_PWM_F;
    pdm_err = pdm_w - pdm_y;

    if (pdm_err > 0.0f) {
        digitalWrite(pin, HIGH);
        pdm_y += 0.1f;
    } else {
        digitalWrite(pin, LOW);
        pdm_y -= 0.1f;
    }

    // Clamp accumulator
    if (pdm_y > 1.0f) pdm_y = 1.0f;
    if (pdm_y < 0.0f) pdm_y = 0.0f;
}

} // anonymous namespace

// === HardwareV1_0_Config Implementation ===

void HardwareV1_0_Config::init() {
#ifdef ARDUINO
    // Initialize all output pins
    for (int i = 0; i < 6; i++) {  // 3 continuous + 3 binary = 6 total
        pinMode(OUTPUT_PINS[i], OUTPUT_2MA);
        pinMode(OUTPUT_LED_PINS[i], OUTPUT);
    }

    // Initialize input pins
    pinMode(PIN_I1, INPUT);
    pinMode(PIN_I2, INPUT);
    pinMode(PIN_AI1, INPUT);
    pinMode(PIN_AI2, INPUT);

    // Initialize input LED pins
    pinMode(PIN_LED_I1, OUTPUT);
    pinMode(PIN_LED_I2, OUTPUT);
    pinMode(PIN_LED_AI1, OUTPUT);
    pinMode(PIN_LED_AI2, OUTPUT);

    // Initialize switch pins
    pinMode(PIN_SWITCH_M1, INPUT_PULLUP);
    pinMode(PIN_SWITCH_T1, INPUT_PULLUP);
    pinMode(PIN_SWITCH_T2, INPUT_PULLUP);

    // Initialize board LED
    pinMode(LED_BOARD, OUTPUT);

    // Initialize I2C
    Wire.setSDA(I2C_SDA_PIN);
    Wire.setSCL(I2C_SCL_PIN);
    Wire.begin(I2C_ADDRESS);

    // PWM setup for continuous outputs
    analogWriteFreq(100000);   // 100kHz - out of hearing range
    analogWriteResolution(11);  // 11-bit resolution (0-2047)

#endif // ARDUINO
}

float HardwareV1_0_Config::read_input(size_t index) const {
#ifdef ARDUINO
    switch(index) {
        case 0: // I1 - Digital gate input
            return digitalRead(PIN_I1) ? 1.0f : 0.0f;

        case 1: // I2 - Digital gate input
            return digitalRead(PIN_I2) ? 1.0f : 0.0f;

        case 2: // AI1 - Analog CV input
            return analogRead(PIN_AI1) * RECP_4096;

        case 3: // AI2 - Analog CV input
            return analogRead(PIN_AI2) * RECP_4096;

        case 4: // Switch M1
            return (1 - digitalRead(PIN_SWITCH_M1)) ? 1.0f : 0.0f;

        case 5: // Switch T1
            return (1 - digitalRead(PIN_SWITCH_T1)) ? 1.0f : 0.0f;

        case 6: // Switch T2
            return (1 - digitalRead(PIN_SWITCH_T2)) ? 1.0f : 0.0f;

        case 7: // Board LED state (read-only, always 0)
            return 0.0f;

        default:
            return 0.0f;
    }
#else
    // Desktop build - no hardware
    (void)index;
    return 0.0f;
#endif
}

void HardwareV1_0_Config::write_continuous_output(size_t index, float value) const {
#ifdef ARDUINO
    if (index >= NUM_CONTINUOUS_OUTS) return;

    // Get pins
    int output_pin = OUTPUT_PINS[index];
    int led_pin = OUTPUT_LED_PINS[index];

    // Convert [0, 1] to PWM range [0, 2047]
    int pwm_value = static_cast<int>(value * MAX_PWM_F);

    // Clamp
    if (pwm_value > MAX_PWM) pwm_value = MAX_PWM;
    if (pwm_value < 0) pwm_value = 0;

    // Write to output (using PDM for hardware v1.0)
    pdm_analog_write(output_pin, pwm_value);

    // Write to LED (with exponential curve for better visual feedback)
    int led_value = (pwm_value * pwm_value) >> 11;  // Square and scale
    analogWrite(led_pin, led_value);
#else
    (void)index;
    (void)value;
#endif
}

void HardwareV1_0_Config::write_binary_output(size_t index, float value) const {
#ifdef ARDUINO
    if (index >= NUM_BINARY_OUTS) return;

    // Binary outputs start after continuous outputs
    size_t pin_index = NUM_CONTINUOUS_OUTS + index;
    int output_pin = OUTPUT_PINS[pin_index];
    int led_pin = OUTPUT_LED_PINS[pin_index];

    // Convert to digital
    bool is_high = value > 0.5f;

    // Write to output and LED
    digitalWrite(output_pin, is_high ? HIGH : LOW);
    digitalWrite(led_pin, is_high ? HIGH : LOW);
#else
    (void)index;
    (void)value;
#endif
}

void HardwareV1_0_Config::write_serial_output(size_t index, float value) const {
#ifdef ARDUINO
    // Serial output: format as "[index] value\n"
    Serial.print("[s");
    Serial.print(index);
    Serial.print("] ");
    Serial.println(value, 6);  // 6 decimal places
#else
    // Desktop: could write to stdout or log file
    (void)index;
    (void)value;
#endif
}

unsigned long HardwareV1_0_Config::micros() const {
#ifdef ARDUINO
    return ::micros();
#else
    // Desktop stub - would use high-resolution timer
    return 0;
#endif
}

void HardwareV1_0_Config::delay_microseconds(unsigned int us) const {
#ifdef ARDUINO
    ::delayMicroseconds(us);
#else
    (void)us;
#endif
}

void HardwareV1_0_Config::tick_maintenance() {
    // Any per-tick maintenance tasks
    // Could include:
    // - Checking I2C messages
    // - Updating LEDs
    // - Reading additional sensors
    // - etc.
}

#endif // ARDUINO
