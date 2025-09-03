#include "default_logger.h"

#ifdef ARDUINO
#include "Arduino.h"
#else
#include <iostream>
#endif

void DefaultLogger::info(const String& msg) { write_message("[INFO]", msg); }

void DefaultLogger::warn(const String& msg) { write_message("[WARN]", msg); }

void DefaultLogger::error(const String& msg) { write_message("[ERROR]", msg); }

void DefaultLogger::debug(const String& msg) { write_message("[DEBUG]", msg); }

void DefaultLogger::write_message(const char* prefix, const String& msg)
{
#ifdef ARDUINO
    if (Serial.availableForWrite())
    {
        Serial.print(prefix);
        Serial.print(" ");
        Serial.println(msg);
    }
#else
    if (strcmp(prefix, "[ERROR]") == 0)
    {
        std::cerr << prefix << " " << msg.c_str() << std::endl;
    }
    else
    {
        std::cout << prefix << " " << msg.c_str() << std::endl;
    }
#endif
}