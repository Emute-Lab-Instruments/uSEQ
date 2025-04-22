#ifndef DSP_Q_DATA_HPP
#define DSP_Q_DATA_HPP

#include <stddef.h>

// Your code goes here
namespace DSPQ {

    enum RESPONSES {UGENINFO, MESSAGE, ADD_OUTPUT_QUEUE, ADD_INPUT_QUEUE};

    struct response_data_ugeninfo {
        size_t key;
        char name[64];
    };

    struct response_data_message {
        size_t key;
        char msg[64];
    };

    struct response_data_queue {
        size_t key;
        //pointer to queue
        queue_t *queueptr;
        size_t index;
        size_t queueSize;
    };

    union response_data {
        response_data_ugeninfo ugenInfo;
        response_data_queue queueInfo;
        response_data_message ugenMessage;
    };

    struct response_info {
        RESPONSES response;
        response_data data;
    };
}


#endif // DSP_Q_DATA_HPP