#include "log.h"

#include <iostream>

// TODO
// Definitions
void print(String s)
{
#if defined(USE_STD_STR) && defined(USE_STD_IO)
    std::cout << s;
#elif defined(USE_ARDUINO_STR) && defined(USE_STD_IO)
    std::cout << s.c_str();
#elif defined(USE_ARDUINO_STR) && defined(USE_SERIAL_IO)
    Serial.print(s);
#endif
}

void println(String s)
{
#if defined(USE_STD_STR) && defined(USE_STD_IO)
    std::cout << s << std::endl;
#elif defined(USE_ARDUINO_STR) && defined(USE_STD_IO)
    std::cout << s.c_str() << std::endl;
#elif defined(USE_ARDUINO_STR) && defined(USE_SERIAL_IO)
    Serial.println(s);
#endif
}

void dbg(String s)
{
    print("DEBUG: ");
    print(s);
    println("");
}

void error(String s)
{
    print("ERROR: ");
    print(s);
    print("\\n");
}

// template <typename T>
// void print(T str)
// {
//     std::cout << str;
// }

// template <typename T>
// void println(T str)
// {
//     std::cout << str << std::endl;
// }
