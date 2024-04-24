#ifndef ARDUINO_IO_H_
#define ARDUINO_IO_H_

// FIXME include serial

namespace io
{
void setup() {}

bool new_code_waiting() { return Serial.available(); }
} // namespace io

#endif // ARDUINO_IO_H_
