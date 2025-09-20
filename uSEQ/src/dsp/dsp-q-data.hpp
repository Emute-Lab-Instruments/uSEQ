#ifndef DSP_Q_DATA_HPP
#define DSP_Q_DATA_HPP

#include <stddef.h>

#ifdef ARDUINO
#include "pico/util/queue.h"
#endif

// Your code goes here
namespace DSPQ
{

enum RESPONSES
{
    UGENINFO,
    MESSAGE,
    ADD_OUTPUT_QUEUE,
    ADD_INPUT_QUEUE,
    LISTQUEUES
};

struct response_data_ugeninfo
{
    size_t key;
    char name[64];
};

struct response_data_listqueues
{
    size_t key;
};

struct response_data_message
{
    size_t key;
    char msg[64];
};

enum class UGEN_TYPE
{
    DAC,
    OTHER
};

struct response_data_queue
{
    size_t key = 0;
    // pointer to queue
#ifdef ARDUINO
    queue_t* queueptr = nullptr;
#else
    void* queueptr = nullptr; // Generic pointer for desktop builds
#endif
    size_t index     = 0;
    size_t queueSize = 0;
    UGEN_TYPE type   = UGEN_TYPE::OTHER;
};

union response_data
{
    response_data_ugeninfo ugenInfo;
    response_data_queue queueInfo;
    response_data_message ugenMessage;
    response_data_listqueues listqueues;
};

struct response_info
{
    RESPONSES response;
    response_data data;
};
} // namespace DSPQ

#endif // DSP_Q_DATA_HPP