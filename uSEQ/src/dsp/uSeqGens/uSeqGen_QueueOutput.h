#ifndef USEQGEN_QUEUEOUTPUT_H
#define USEQGEN_QUEUEOUTPUT_H

#include "uSeqGen_Base.h"
#include "pico/util/queue.h"

class uSeqGen_QueueOutput final : public uSeqGen_Base //uSeqGen_Base
{
public:
    uSeqGen_QueueOutput(queue_t *q) 
        : uSeqGen_Base(q)
    {
        SetInputCount_(1);
        SetOutputCount_(1);

        //create an output queue
        queue_init(&q_output, sizeof(float), 1);


        //share it with the interpreter
        DSPQ::response_info resp;
        resp.response = DSPQ::RESPONSES::ADD_OUTPUT_QUEUE;
        resp.data.queueInfo.key = key;
        resp.data.queueInfo.queueptr = &q_output;
        queue_try_add(q_message, &resp);
    }
    
    ~uSeqGen_QueueOutput() {
        queue_free(&q_output);
    }

protected:
    void __not_in_flash_func(Process_)(DSPatch::SignalBus& inputs, DSPatch::SignalBus& outputs) override
    {
        const float sig0 = *inputs.GetValue<float>(0);    
        queue_try_add(&q_output, &sig0);
        outputs.SetValue(0, sig0);
    }
private:
    queue_t q_output;
};

#endif // USEQGEN_QUEUEOUTPUT_H
