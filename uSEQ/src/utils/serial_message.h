#ifndef SERIAL_MESSAGE_H
#define SERIAL_MESSAGE_H

#include <cstdint>

namespace SerialMsg
{

constexpr uint8_t message_begin_marker = 31;

enum serial_message_types
{
    // TEXT (0x20) and MSG_TO_EDITOR (0x64) removed: replaced by
    // {type:"log",...} JSON envelope per wire-protocol spec §5.6.
    // message_end_marker (0x03) removed: never sent by firmware::Firmware.
    // execute_now_marker ('@') removed: wire is immediate-only per spec §1.1.
    JSON   = 101,
    STREAM = 0
};
constexpr unsigned long serial_message_rate_limit = 1000000 / 100 /*Hz*/;

}; // namespace SerialMsg

#endif
