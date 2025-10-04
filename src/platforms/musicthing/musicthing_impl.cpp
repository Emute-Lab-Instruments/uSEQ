/**
 * musicthing_impl.cpp - Implementation of Music Thing Modular platform
 *
 * This file contains the actual Arduino-specific implementations for the
 * Music Thing Modular eurorack module.
 */

#include "musicthing_config.h"

#ifdef ARDUINO

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

// Music Thing specific includes
#ifdef MUSICTHING
#include <hardware/gpio.h>
#include <hardware/timer.h>
#endif

namespace {

// DAC control state
SPISettings dacSettings(20000000, MSBFIRST, SPI_MODE0);  // 20MHz SPI for DAC

// MUX state tracking
uint8_t current_mux_channel = 0;

/**
 * Write to external DAC via SPI
 * Music Thing uses an external DAC for CV outputs (a3, a4)
 */
inline void writeDACChannel(uint8_t channel, uint16_t value) {
    // Channel must be 0-3 for typical 4-channel DACs
    if (channel > 3) return;

    // Clamp value to 12-bit range
    if (value > 4095) value = 4095;

    // Prepare command word (varies by DAC chip, this is typical MCP4822/4)
    // Bit 15: Channel select (0=A, 1=B)
    // Bit 14: Unbuffered (1)
    // Bit 13: Gain (1 = 1x)
    // Bit 12: Shutdown (1 = active)
    // Bits 11-0: Data
    uint16_t command = 0x3000 | (value & 0x0FFF);
    if (channel & 1) command |= 0x8000;  // Select channel

    // Send via SPI
    SPI.beginTransaction(dacSettings);
    digitalWrite(MusicThingConfig::PIN_DAC_CS, LOW);
    SPI.transfer16(command);
    digitalWrite(MusicThingConfig::PIN_DAC_CS, HIGH);
    SPI.endTransaction();
}

/**
 * Set MUX channel and wait for settling
 */
inline void setMuxChannel(uint8_t channel) {
    // 2-bit channel select (4 channels per MUX)
    digitalWrite(MusicThingConfig::PIN_MUX_LOGIC_A, channel & 0x01);
    digitalWrite(MusicThingConfig::PIN_MUX_LOGIC_B, (channel >> 1) & 0x01);

    // Wait for MUX to settle (typical 100ns, but we use 1us to be safe)
    delayMicroseconds(1);

    current_mux_channel = channel;
}

/**
 * Read from MUX input
 */
inline float readMuxInput(uint8_t mux_pin, uint8_t channel) {
    setMuxChannel(channel);
    int raw = analogRead(mux_pin);
    return static_cast<float>(raw) / 4096.0f;  // 12-bit ADC
}

} // anonymous namespace

// === MusicThingConfig Implementation ===

void MusicThingConfig::init() {
#ifdef ARDUINO
    // Initialize CV output pins (a3, a4) - audio outputs handled by DSP
    if (OUTPUT_PINS[2] >= 0) pinMode(OUTPUT_PINS[2], OUTPUT);
    if (OUTPUT_PINS[3] >= 0) pinMode(OUTPUT_PINS[3], OUTPUT);

    // Initialize gate output pins (d1, d2)
    pinMode(OUTPUT_PINS[4], OUTPUT);
    pinMode(OUTPUT_PINS[5], OUTPUT);

    // Initialize all output LED pins
    for (int i = 0; i < 6; i++) {
        pinMode(OUTPUT_LED_PINS[i], OUTPUT);
    }

    // Initialize input pins
    pinMode(PIN_I1, INPUT);
    pinMode(PIN_I2, INPUT);
    pinMode(PIN_AUDIO_IN_L, INPUT);
    pinMode(PIN_AUDIO_IN_R, INPUT);
    pinMode(PIN_MUX_IN_1, INPUT);
    pinMode(PIN_MUX_IN_2, INPUT);

    // Initialize MUX control pins
    pinMode(PIN_MUX_LOGIC_A, OUTPUT);
    pinMode(PIN_MUX_LOGIC_B, OUTPUT);
    digitalWrite(PIN_MUX_LOGIC_A, LOW);
    digitalWrite(PIN_MUX_LOGIC_B, LOW);

    // Initialize DAC (SPI)
    pinMode(PIN_DAC_CS, OUTPUT);
    digitalWrite(PIN_DAC_CS, HIGH);
    SPI.begin();

    // Initialize both DAC channels to 0V
    writeDACChannel(0, 0);
    writeDACChannel(1, 0);

    // Initialize I2C
    Wire.setSDA(I2C_SDA_PIN);
    Wire.setSCL(I2C_SCL_PIN);
    Wire.begin(I2C_ADDRESS);

    // Set up ADC for faster reads (Music Thing uses overclocked ADC)
    analogReadResolution(12);  // 12-bit ADC resolution

#endif // ARDUINO
}

float MusicThingConfig::read_input(size_t index) const {
#ifdef ARDUINO
    switch(index) {
        case 0: // I1 - Gate input
            return digitalRead(PIN_I1) ? 1.0f : 0.0f;

        case 1: // I2 - Gate input
            return digitalRead(PIN_I2) ? 1.0f : 0.0f;

        case 2: // Audio Input L
            return static_cast<float>(analogRead(PIN_AUDIO_IN_L)) / 4096.0f;

        case 3: // Audio Input R
            return static_cast<float>(analogRead(PIN_AUDIO_IN_R)) / 4096.0f;

        // MUX inputs from MUX_IN_1 (channels 0-3)
        case 4:
            return readMuxInput(PIN_MUX_IN_1, 0);
        case 5:
            return readMuxInput(PIN_MUX_IN_1, 1);
        case 6:
            return readMuxInput(PIN_MUX_IN_1, 2);
        case 7:
            return readMuxInput(PIN_MUX_IN_1, 3);

        // MUX inputs from MUX_IN_2 (channels 0-3)
        // These are typically control inputs (Main knob, X, Y, Z switch)
        case 8:  // MTMAINKNOB
            return readMuxInput(PIN_MUX_IN_2, 0);
        case 9:  // MTXKNOB
            return readMuxInput(PIN_MUX_IN_2, 1);
        case 10: // MTYKNOB
            return readMuxInput(PIN_MUX_IN_2, 2);
        case 11: // MTZSWITCH
            return readMuxInput(PIN_MUX_IN_2, 3);

        default:
            return 0.0f;
    }
#else
    // Desktop build - no hardware
    (void)index;
    return 0.0f;
#endif
}

void MusicThingConfig::write_continuous_output(size_t index, float value) const {
#ifdef ARDUINO
    if (index >= NUM_CONTINUOUS_OUTS) return;

    int led_pin = OUTPUT_LED_PINS[index];

    // Audio outputs (aL, aR) are handled by DSP engine, not GPIO
    if (index < 2) {
        // Just update LED to show output activity
        float display_value = INVERT_AUDIO_OUTPUTS ? (1.0f - value) : value;
        int led_value = static_cast<int>(display_value * 255.0f);
        analogWrite(led_pin, led_value);
        return;
    }

    // CV outputs (a3, a4) use external DAC
    int pin = OUTPUT_PINS[index];

    // Apply inversion if configured
    float output_value = INVERT_ANALOG_OUTPUTS ? (1.0f - value) : value;

    // Clamp to [0, 1]
    if (output_value > 1.0f) output_value = 1.0f;
    if (output_value < 0.0f) output_value = 0.0f;

    // Convert [0, 1] to DAC range [0, 4095] (12-bit)
    uint16_t dac_value = static_cast<uint16_t>(output_value * 4095.0f);

    // Write to DAC (channel 0 = a3, channel 1 = a4)
    writeDACChannel(index - 2, dac_value);

    // Update LED to show output activity
    int led_value = static_cast<int>(value * 255.0f);
    analogWrite(led_pin, led_value);

#else
    (void)index;
    (void)value;
#endif
}

void MusicThingConfig::write_binary_output(size_t index, float value) const {
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

    // Write to output and LED
    digitalWrite(pin, is_high ? HIGH : LOW);
    digitalWrite(led_pin, is_high ? HIGH : LOW);

#else
    (void)index;
    (void)value;
#endif
}

void MusicThingConfig::write_serial_output(size_t index, float value) const {
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

unsigned long MusicThingConfig::micros() const {
#ifdef ARDUINO
    return ::micros();
#else
    // Desktop stub - would use high-resolution timer
    return 0;
#endif
}

void MusicThingConfig::delay_microseconds(unsigned int us) const {
#ifdef ARDUINO
    ::delayMicroseconds(us);
#else
    (void)us;
#endif
}

void MusicThingConfig::tick_maintenance() {
    // Any per-tick maintenance tasks
    // Could include:
    // - Checking I2C messages
    // - Updating RGB LED animations
    // - Updating DSP engine state
    // - Tempo estimation updates
    // - etc.
}

#endif // ARDUINO
