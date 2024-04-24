#ifndef IO_H_
#define IO_H_

#include "utils/string.h"

// template <typename T>
// class Output
// {
// public:
//     // using OutputWriteRoutine = std::function<>
//     enum Type
//     {
//         ANALOG,
//         DIGITAL
//     };

//     Output(Type, num, ) int num;
//     Type type;
//     Value form;
//     virtual bool write() = 0;
//     T m_last_val;

// protected:
// };

// class Input
// {
//     Input() {}
// };

// BASE CLASS
class IO_INTERFACE
{
public:
    virtual bool init()                = 0;
    virtual bool is_new_code_waiting() = 0;
    virtual String get_latest_code() { return m_latest_code; };

protected:
    String m_latest_code = "";
};

#if defined(USE_ARDUINO_IO)

class ARDUINO_IO : public IO_INTERFACE
{
public:
    ARDUINO_IO() { init(); }
    bool init() override;
    bool is_new_code_waiting() override;
    String get_latest_code() override;

    ~STD_IO() {}

private:
};

typedef ARDUINO_IO IO;
#elif defined(USE_STD_IO)

#include <atomic>
#include <mutex>
#include <thread>

class STD_IO : public IO_INTERFACE
{
public:
    STD_IO() { init(); }
    bool init() override;
    bool is_new_code_waiting() override;
    String get_latest_code() override;

    // FIXME this may hang if thread doesn't move past
    // the blocking stdin call
    ~STD_IO()
    {
        if (m_stdin_thread.joinable())
        {
            m_stdin_thread.join();
        }
    }

#if defined(USE_ARDUINO_PIO)
    void analog_write_with_led(int pin, double val);
    void digital_write_with_led(int pin, int val);
#endif

private:
    std::mutex m_mtx;
    std::thread m_stdin_thread;
    std::atomic<bool> m_code_waiting_flag = ATOMIC_VAR_INIT(false);
    std::atomic<bool> m_thread_stop_flag  = ATOMIC_VAR_INIT(false);
    void m_input_checking_routine();
};

typedef STD_IO IO;
#endif // #if defined(...)

#endif // IO_H_
