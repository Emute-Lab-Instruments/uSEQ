#include "log.h"

#include <iostream>

// TODO
// Definitions
void print(String s)
{
    Serial.print(s);
    // #if defined(USE_STD_STR) && defined(USE_STD_IO)
    //     std::cout << s;
    // #elif defined(USE_ARDUINO_STR) && defined(USE_STD_IO)
    //     std::cout << s.c_str();
    // #elif defined(USE_ARDUINO_STR) && defined(USE_SERIAL_IO)
    //     Serial.print(s);
    // #endif
}

void println(String s)
{
    Serial.println(s);
    // #if defined(USE_STD_STR) && defined(USE_STD_IO)
    //     std::cout << s << std::endl;
    // #elif defined(USE_ARDUINO_STR) && defined(USE_STD_IO)
    //     std::cout << s.c_str() << std::endl;
    // #elif defined(USE_ARDUINO_STR) && defined(USE_SERIAL_IO)
    //     Serial.println(s);
    // #endif
}

void error(String s)
{
    print("ERROR: ");
    print(s);
    print("\\n");
}

int free_heap() { return rp2040.getFreeHeap() / 1024; }

// DEBUGGER

#if USEQ_DEBUG

bool DEBUGGER::print_free_heap = false;

int DEBUGGER::m_level = -1;

String DEBUGGER::m_spaces = "";

void DEBUGGER::inc_level()
{
    m_level += 1;
    update_spaces();
}

void DEBUGGER::dec_level()
{
    m_level -= 1;
    update_spaces();
}

DEBUGGER::DEBUGGER(String s) : name(s)
{
    start_heap = free_heap();
    inc_level();

    pr("[DBG] " + name + " START...");
    inc_level();
    if (print_free_heap)
    {
        pr("free heap (start): " + String(start_heap) + "KB");
    }
}

DEBUGGER::~DEBUGGER()
{
    int end_heap = free_heap();

    dec_level();

    pr("[DBG] " + name + " END.");
    if (print_free_heap)
    {
        pr(m_spaces + "free heap (end): " + String(end_heap) + "KB");
        pr(m_spaces + "heap difference: " + String(end_heap - start_heap) + "KB");
    }

    dec_level();
}

void DEBUGGER::log(const String& message)
{
    // pr("[" + name + "] " + addIndentAfterNewline(message));
    pr("[" + name + "] " + addIndentAfterNewline(message));
}

void DEBUGGER::operator()(const String& message) { log(message); }

void DEBUGGER::pr(const String& s)
{
    print(m_spaces + s);
    print("\n");
}

String DEBUGGER::addIndentAfterNewline(const String& input)
{
    String result;
    int lastIndex    = 0;
    int currentIndex = 0;

    // Search for newline characters and modify the string
    while ((currentIndex = input.indexOf('\n', lastIndex)) != -1)
    {
        // Add the current segment plus newline
        result += input.substring(lastIndex, currentIndex + 1);
        // Add spaces after the newline
        result += m_spaces;
        // Move past the newline character
        lastIndex = currentIndex + 1;
    }

    // Add the last part of the string if any
    result += input.substring(lastIndex);

    return result;
}

void DEBUGGER::update_spaces()
{
    String s = "";

    if (m_level > 0)
    {
        for (int i = 0; i < m_level; i++)
        {
            s += "  ";
        }

        m_spaces = s;
    }
    else
    {
        m_spaces = "";
    }
}

#endif // USEQ_DEBUG
