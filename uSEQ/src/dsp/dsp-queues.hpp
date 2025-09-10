#ifndef __DSP_QUEUE_HPP
#define __DSP_QUEUE_HPP

#ifdef ARDUINO
#include "pico/util/queue.h"
#endif

#define N_INPUT_QUEUES 8
#define N_OUTPUT_QUEUES 8

namespace DSPQ
{
#ifdef ARDUINO
// extern queue_t q_inputs[N_INPUT_QUEUES];
// extern queue_t q_outputs[N_OUTPUT_QUEUES];

extern queue_t q_engine_commands;
extern queue_t q_engine_responses;
#endif
} // namespace DSPQ

#endif