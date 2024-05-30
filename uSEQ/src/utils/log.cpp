#include "log.h"

#include <iostream>

// TODO
// Definitions
void print(String s)
{
    if (Serial.availableForWrite())
    {
        Serial.print(s);
    }
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
    if (Serial.availableForWrite())
    {
        Serial.println(s);
    }
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
    // print("ERROR: ");
    if (Serial.availableForWrite())
    {
        println("**Error**: " + s);
    }
    // print("\n");
}

void runtime_error(String s)
{
    if (Serial.availableForWrite())
    {
        println("**Runtime Error**: " + s);
    }
}

void user_warning(const String& s)
{
    // TODO
    if (Serial.availableForWrite())
    {
        println("**Warning**: " + s);
    }
}

// void error_num_args_incorrect(String function_name, String expected,
//                               int num_received)
// {
//     print("ERROR: ");
//     print("(" + function_name + ") Expected " + String(num_expected));
//     print("\\n");
// }

String comp_to_string(NumArgsComparison comp)
{
    switch (comp)
    {
    case NumArgsComparison::EqualTo:
    {
        return "exactly";
    }
    case NumArgsComparison::AtLeast:
    {
        return "at least";
    }
    case NumArgsComparison::AtMost:
    {
        return "at most";
    }
    case NumArgsComparison::Between:
    {
        return "between";
    }
    default:
        return "<comparison not found>";
    }

    return "";
}

void error_wrong_num_args(const String& function_name, int num_received,
                          NumArgsComparison expected_comp, int num, int num2 = 0)
{
    String expected_comparison_str = comp_to_string(expected_comp);
    String received_num_str        = String(num_received);

    expected_comparison_str += " " + String(num);

    if (expected_comp == NumArgsComparison::Between)
    {
        expected_comparison_str += " and " + String(num2);
    }

    error("(`" + function_name + "`) Wrong number of arguments: expected " +
          expected_comparison_str + " but received " + received_num_str +
          " instead.");
}

void error_arg_is_error(const String& function_name, int num,
                        const String& received_val_str)
{
    error("(`" + function_name + "`) Argument #" + String(num) +
          " evaluates to an error:");
    println("- " + received_val_str);
}

void error_wrong_all_pred(const String& function_name, int num,
                          const String& expected_str, const String& received_val_str)
{
    error("(`" + function_name + "`) All arguments should evaluate to " +
          expected_str + ", but argument #" + String(num) + " does not:");
    println("- " + received_val_str);
}

void error_wrong_specific_pred(const String& function_name, int num,
                               const String& expected_str,
                               const String& received_val_str)
{
    error("(`" + function_name + "`) Argument #" + String(num) +
          " should evaluate to " + expected_str + ", but instead it is:");
    println("- " + received_val_str);
}

void error_atom_not_defined(const String& atom)
{
    error("Atom **" + atom + "** not defined.");
}

int free_heap() { return rp2040.getFreeHeap() / 1024; }

// DebugLogger

#if USEQ_DEBUG

std::unordered_set<String> DebugLogger::mutes = {};
std::unordered_set<String> DebugLogger::solos = { "uSEQ::eval_at_time" };

bool DebugLogger::print_free_heap = false;

int DebugLogger::m_level = -1;

String DebugLogger::m_spaces = "";

void DebugLogger::inc_level()
{
    m_level += 1;
    update_spaces();
}

void DebugLogger::dec_level()
{
    m_level -= 1;
    update_spaces();
}

DebugLogger::DebugLogger(String s) : m_name(s)
{
    start_heap = free_heap();
    inc_level();

    debug_print("[DBG] " + m_name + " START...");
    inc_level();
    if (print_free_heap)
    {
        debug_print("free heap (start): " + String(start_heap) + "KB");
    }
}

DebugLogger::~DebugLogger()
{
    int end_heap = free_heap();

    dec_level();

    debug_print("[DBG] " + m_name + " END.");
    if (print_free_heap)
    {
        debug_print(m_spaces + "free heap (end): " + String(end_heap) + "KB");
        debug_print(m_spaces + "heap difference: " + String(end_heap - start_heap) +
                    "KB");
    }

    dec_level();
}

void DebugLogger::filtered_log(const String& message)
{
    bool should_print = true;

    // If solos have been specified and this isn't in them, don't print
    if (solos.size() > 0 && !solos.count(m_name))
    {
        should_print = false;
    }
    // Otherwise, print as long as it's not in the mutes
    else if (mutes.count(m_name))
    {
        should_print = false;
    }

    if (should_print)
    {
        debug_print("[" + m_name + "] " + addIndentAfterNewlines(message));
    }
}

void DebugLogger::operator()(const String& message) { filtered_log(message); }

void DebugLogger::debug_print(const String& s)
{
    print(m_spaces + s);
    print("\n");
}

String DebugLogger::addIndentAfterNewlines(const String& input)
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

void DebugLogger::update_spaces()
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
