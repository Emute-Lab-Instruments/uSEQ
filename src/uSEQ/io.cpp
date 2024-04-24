#include "io.h"
#include "utils/log.h"
#include <atomic>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#if defined(USE_ARDUINO_IO)
inline int digital_out_pin(int out)
{
    int res    = -1;
    int pindex = PWM_OUTS + out;
    if (pindex <= 6)
        res = useq_output_pins[pindex - 1];
    return res;
}

inline int digital_out_LED_pin(int out)
{
    int res    = -1;
    int pindex = PWM_OUTS + out;
    if (pindex <= 6)
        res = useq_output_led_pins[pindex - 1];
    return res;
}

inline int analog_out_pin(int out)
{
    int res = -1;
    if (out <= PWM_OUTS)
        res = useq_output_pins[out - 1];
    return res;
}

inline int analog_out_LED_pin(int out)
{
    int res = -1;
    if (out <= PWM_OUTS)
        res = useq_output_led_pins[out - 1];
    return res;
}

// Write `level` to TX FIFO. State machine will copy this into X.
void pio_pwm_set_level(PIO pio, uint sm, uint32_t level) { pio_sm_put_blocking(pio, sm, level); }

void pio_pwm_set_period(PIO pio, uint sm, uint32_t period)
{
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_put_blocking(pio, sm, period);
    pio_sm_exec(pio, sm, pio_encode_pull(false, false));
    pio_sm_exec(pio, sm, pio_encode_out(pio_isr, 32));
    pio_sm_set_enabled(pio, sm, true);
}
bool ARD_IO::analog_write_with_led(int i, double sigval)
{
    pio_pwm_set_level(i < 4 ? pio0 : pio1, i % 4, sigval);

    // led out
    int led_pin   = analog_out_LED_pin(i + 1);
    int ledsigval = sigval >> 2; // shift to 11 bit range for the LED
    ledsigval     = (ledsigval * ledsigval) >> 11; // cheap way to square and get a exp curve
    analogWrite(led_pin, ledsigval);
    return true;
}

#elif defined(USE_STD_IO)
bool STD_IO::init()
{
    dbg("Hi from init STD_IO");
    m_stdin_thread = std::thread(&STD_IO::m_input_checking_routine, this);
    return true;
}

void STD_IO::m_input_checking_routine()
{
    std::string input;
    while (!m_thread_stop_flag)
    {
        std::getline(std::cin, input); // Get input from stdin
        m_mtx.lock();
        m_latest_code       = String(input.c_str()); // Safely update the command
        m_code_waiting_flag = true; // Set the new command flag
        m_mtx.unlock();
    }
}

bool STD_IO::is_new_code_waiting()
{
    m_mtx.lock();
    bool result = m_code_waiting_flag;
    m_mtx.unlock();
    return result;
}

String STD_IO::get_latest_code()
{
    m_mtx.lock();
    m_code_waiting_flag = false; // Set the new command flag
    m_mtx.unlock();
    return m_latest_code;
}

// // ASYNC checking for new code
// //
// std::atomic<bool> code_waiting_flag(false);

// String code("");

// std::mutex mtx;

// void input_checking_thread()
// {
//     std::string input;
//     while (true)
//     {
//         dbg("Waiting for stdin");
//         std::getline(std::cin, input); // Get input from stdin
//         dbg("Received stdin");
//         // if (input == "exit")
//         //     break; // Exit command to stop the program

//         mtx.lock();
//         code              = String(input.c_str()); // Safely update the command
//         code_waiting_flag = true; // Set the new command flag
//         mtx.unlock();
//     }
// }

// bool is_new_code_waiting()
// {
//     bool result;
//     mtx.lock();
//     result = code_waiting_flag;
//     mtx.unlock();
//     return result;
// }

// String last_received_code()
// {
//     mtx.lock();
//     code_waiting_flag = false; // Set the new command flag
//     mtx.unlock();
//     return code;
// }

// } // namespace io
#endif //
