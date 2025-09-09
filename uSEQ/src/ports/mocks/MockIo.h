// Enhanced recording I/O mock for comprehensive testing
#pragma once

#include "../IIo.h"
#include <vector>
#include <unordered_map>

struct MockIo : public IIo {
    struct DigitalWrite { uint8_t pin; uint8_t value; };
    struct AnalogWrite { uint8_t pin; int value; };
    struct SerialWrite { uint8_t channel; SERIAL_OUTPUT_VALUE_TYPE value; };
    struct PinModeCall { uint8_t pin; uint8_t mode; };

    // Recording vectors
    std::vector<DigitalWrite> digital_writes;
    std::vector<AnalogWrite> analog_writes;
    std::vector<SerialWrite> serial_writes;
    std::vector<PinModeCall> pinmode_calls;

    // State tracking
    std::unordered_map<uint8_t, uint8_t> digital_pin_states;
    std::unordered_map<uint8_t, int> analog_pin_states;
    std::unordered_map<uint8_t, int> analog_read_values;
    std::unordered_map<uint8_t, int> digital_read_values;

    void pinMode(uint8_t pin, uint8_t mode) override {
        pinmode_calls.push_back({pin, mode});
    }

    void digitalWrite(uint8_t pin, uint8_t value) override {
        digital_writes.push_back({pin, value});
        digital_pin_states[pin] = value;
    }
    
    int digitalRead(uint8_t pin) override { 
        return digital_read_values.count(pin) ? digital_read_values[pin] : 0;
    }

    void analogWrite(uint8_t pin, int value) override {
        analog_writes.push_back({pin, value});
        analog_pin_states[pin] = value;
    }
    
    int analogRead(uint8_t pin) override { 
        return analog_read_values.count(pin) ? analog_read_values[pin] : 0;
    }

    void serialWrite(uint8_t channel, SERIAL_OUTPUT_VALUE_TYPE value) override {
        serial_writes.push_back({channel, value});
    }

    // Testing helpers
    void clear_all() {
        digital_writes.clear();
        analog_writes.clear();
        serial_writes.clear();
        pinmode_calls.clear();
        digital_pin_states.clear();
        analog_pin_states.clear();
    }

    void set_digital_read_value(uint8_t pin, int value) {
        digital_read_values[pin] = value;
    }

    void set_analog_read_value(uint8_t pin, int value) {
        analog_read_values[pin] = value;
    }

    // Query the last write to a specific pin
    bool has_digital_write_for_pin(uint8_t pin) const {
        for (const auto& write : digital_writes) {
            if (write.pin == pin) return true;
        }
        return false;
    }

    uint8_t last_digital_value_for_pin(uint8_t pin) const {
        for (auto it = digital_writes.rbegin(); it != digital_writes.rend(); ++it) {
            if (it->pin == pin) return it->value;
        }
        return 0;
    }

    bool has_analog_write_for_pin(uint8_t pin) const {
        for (const auto& write : analog_writes) {
            if (write.pin == pin) return true;
        }
        return false;
    }

    int last_analog_value_for_pin(uint8_t pin) const {
        for (auto it = analog_writes.rbegin(); it != analog_writes.rend(); ++it) {
            if (it->pin == pin) return it->value;
        }
        return 0;
    }

    // Count operations on specific pins
    size_t digital_write_count_for_pin(uint8_t pin) const {
        size_t count = 0;
        for (const auto& write : digital_writes) {
            if (write.pin == pin) count++;
        }
        return count;
    }

    size_t analog_write_count_for_pin(uint8_t pin) const {
        size_t count = 0;
        for (const auto& write : analog_writes) {
            if (write.pin == pin) count++;
        }
        return count;
    }
};

