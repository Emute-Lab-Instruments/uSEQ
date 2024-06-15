#ifndef SERIAL_MESSAGE_H
#define SERIAL_MESSAGE_H

#include <cstdint>

namespace SerialMsg
{

constexpr uint8_t message_begin_marker = 31;
constexpr uint8_t message_end_marker   = 3; // End of text ASCII

constexpr char execute_now_marker = '@';

enum serial_message_types
{
    TEXT          = 32,
    MSG_TO_EDITOR = 100,
    STREAM        = 0
};
constexpr unsigned long serial_message_rate_limit = 1000000 / 100 /*Hz*/;

}; // namespace SerialMsg

#endif
