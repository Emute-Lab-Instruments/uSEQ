#ifndef USEQGEN_QUEUEINPUT_H
#define USEQGEN_QUEUEINPUT_H

#include "uSeqGen_Base.h"
#ifdef ARDUINO
#include "pico/util/queue.h"
#endif

class uSeqGen_QueueInput final : public uSeqGen_Base
{
public:
    uSeqGen_QueueInput(queue_t *q, size_t key) 
        : uSeqGen_Base(q, key)
    {
        SetInputCount_(0);
        SetOutputCount_(1);

        //create a queue
        // queue_init(&q_input, sizeof(float), 1);


        // //share it with the interpreter
        // DSPQ::response_info resp;
        // resp.response = DSPQ::RESPONSES::ADD_INPUT_QUEUE;
        // resp.data.queueInfo.key = key;
        // resp.data.queueInfo.queueptr = &q_input;
        // resp.data.queueInfo.index = 0;
        // resp.data.queueInfo.queueSize = 1;
        // queue_try_add(q_message, &resp);
        createInputQueue(0, q_input);
    }
    
    ~uSeqGen_QueueInput() {
        queue_free(&q_input);
    }

protected:
    void __not_in_flash_func(Process_)(DSPatch::SignalBus&, DSPatch::SignalBus& outputs) override
    {
        float tmp;
        // println("QueueInput");
        if (queue_try_remove(&q_input, &tmp)) {
            lastValue = tmp;
            // println("QueueInput: " + String(tmp));
        }
        outputs.SetValue(0, lastValue);
    }
private:
    // queue_t *queue;
    // double val = 0;
    queue_t q_input;
    float lastValue = 0.f;
};

#endif // USEQGEN_QUEUEINPUT_H
