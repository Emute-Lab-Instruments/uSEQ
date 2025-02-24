#ifndef __DSP_QUEUE_HPP
#define __DSP_QUEUE_HPP

#include "pico/util/queue.h"

#define N_INPUT_QUEUES 8
#define N_OUTPUT_QUEUES 8

namespace DSPQ {
    extern queue_t q_inputs[N_INPUT_QUEUES];
    extern queue_t q_outputs[N_OUTPUT_QUEUES];
    
    extern queue_t q_engine_commands;
    extern queue_t q_engine_messages;    
}

#endif