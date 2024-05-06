#ifndef IO_H_
#define IO_H_

#include "utils/string.h"

namespace io
{
void setup();
bool new_code_waiting;
String last_received_code = "";

bool is_new_input_waiting();

String get_line();
} // namespace io

#if defined(USE_STD_IO)

// #include "utils/io.h"
#include "utils/string.h"
#include <atomic>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

std::atomic<bool> new_code_waiting(false);
std::mutex mtx; // Mutex for shared data protection

namespace std_io
{
void input_checking_thread()
{
    std::string input;
    while (true)
    {
        std::getline(std::cin, input); // Get input from stdin
        // if (input == "exit")
        //     break; // Exit command to stop the program

        mtx.lock();
        io::last_received_code = String(input.c_str()); // Safely update the command
        io::new_code_waiting   = true; // Set the new command flag
        mtx.unlock();
    }
}

} // namespace std_io

namespace io
{
void setup() {}
bool is_new_input_waiting() { return new_code_waiting; }
} // namespace io
#elif defined(USE_ARDUINO_IO)
#include "utils/arduino_io.h"
#endif // USE_STDIO

/* int main() */
/* { */
/*     std::thread t1(input_thread); // Start the input thread */
/*     update_loop(); // Start the update loop in the main thread */

/*     t1.join(); // Wait for the input thread to finish */
/*     return 0; */
/* } */

/* bool read_user_input(String* out) { return static_cast<bool>(std::getline(std::cin, *out)); } */

#endif // IO_H_
