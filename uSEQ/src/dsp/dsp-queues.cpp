#include "dsp-queues.hpp"



namespace DSPQ {
    queue_t __not_in_flash("DSP") q_inputs[N_INPUT_QUEUES];
    queue_t __not_in_flash("DSP") q_outputs[N_OUTPUT_QUEUES];
    
    queue_t __not_in_flash("DSP")  q_engine_commands;
    queue_t __not_in_flash("DSP")  q_engine_messages;    

}