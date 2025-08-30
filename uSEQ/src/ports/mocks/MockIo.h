// Simple recording I/O mock for tests
#pragma once

#include "../IIo.h"
#include <vector>

struct MockIo : public IIo {
    struct DigitalWrite { uint8_t pin; uint8_t value; };
    struct AnalogWrite { uint8_t pin; int value; };
    struct SerialWrite { uint8_t channel; SERIAL_OUTPUT_VALUE_TYPE value; };

    std::vector<DigitalWrite> digital_writes;
    std::vector<AnalogWrite> analog_writes;
    std::vector<SerialWrite> serial_writes;

    void pinMode(uint8_t /*pin*/, uint8_t /*mode*/) override {}

    void digitalWrite(uint8_t pin, uint8_t value) override {
        digital_writes.push_back({pin, value});
    }
    int digitalRead(uint8_t /*pin*/) override { return 0; }

    void analogWrite(uint8_t pin, int value) override {
        analog_writes.push_back({pin, value});
    }
    int analogRead(uint8_t /*pin*/) override { return 0; }

    void serialWrite(uint8_t channel, SERIAL_OUTPUT_VALUE_TYPE value) override {
        serial_writes.push_back({channel, value});
    }
};

