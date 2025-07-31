#include "dsp-queues.hpp"



namespace DSPQ {
#ifdef ARDUINO
    // queue_t __not_in_flash("DSP") q_inputs[N_INPUT_QUEUES];
    // queue_t __not_in_flash("DSP") q_outputs[N_OUTPUT_QUEUES];
    
    queue_t __not_in_flash("DSP")  q_engine_commands;
    queue_t __not_in_flash("DSP")  q_engine_responses;
#endif
}