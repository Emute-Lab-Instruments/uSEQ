#include "std.h"
#include "utils/log.h"
#include <iostream>
#include <string>

bool STD_IO::init()
{

    dbg("Hi from init STD_IO");
    m_stdin_thread = std::thread(m_input_checking_thread);
    return true;
}

bool STD_IO::is_new_code_waiting() { return true; }

String STD_IO::get_latest_code() { return ""; }

void STD_IO::m_input_checking_thread()
{
    std::string input;
    while (true)
    {
        dbg("Waiting for stdin");
        std::getline(std::cin, input); // Get input from stdin
        dbg("Received stdin");
        // if (input == "exit")
        //     break; // Exit command to stop the program

        m_mtx.lock();
        m_latest_code       = String(input.c_str()); // Safely update the command
        m_code_waiting_flag = true; // Set the new command flag
        m_mtx.unlock();
    }
}

bool is_new_code_waiting()
{
    bool result;
    mtx.lock();
    result = code_waiting_flag;
    mtx.unlock();
    return result;
}

String last_received_code()
{
    mtx.lock();
    code_waiting_flag = false; // Set the new command flag
    mtx.unlock();
    return code;
}

} // namespace io
