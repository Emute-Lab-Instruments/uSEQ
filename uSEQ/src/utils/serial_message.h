#ifndef SERIAL_MESSAGE_H
#define SERIAL_MESSAGE_H

namespace SerialMsg {

constexpr u_int8_t message_begin_marker = 31;
constexpr char execute_now_marker  = '@';
enum serial_message_types {TEXT=32, STREAM=0};
constexpr unsigned long serial_message_rate_limit = 1000000 / 100/*Hz*/; 

};

#endif