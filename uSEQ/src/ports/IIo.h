// Hardware I/O port abstraction for outputs and inputs
#pragma once

#include "../uSEQ/configure.h"
#include <cstdint>

struct IIo
{
    virtual ~IIo() = default;

    // Pin configuration
    virtual void pinMode(uint8_t pin, uint8_t mode) = 0;

    // Digital I/O
    virtual void digitalWrite(uint8_t pin, uint8_t value) = 0;
    virtual int digitalRead(uint8_t pin)                  = 0;

    // Analog I/O
    virtual void analogWrite(uint8_t pin, int value) = 0;
    virtual int analogRead(uint8_t pin)              = 0;

    // Serial-like stream for output channels (logical, not UART)
    virtual void serialWrite(uint8_t channel, SERIAL_OUTPUT_VALUE_TYPE value) = 0;
};
