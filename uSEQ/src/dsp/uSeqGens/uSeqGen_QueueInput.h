#ifndef USEQGEN_QUEUEINPUT_H
#define USEQGEN_QUEUEINPUT_H

#include "uSeqGen_Base.h"
#include "pico/util/queue.h"

class uSeqGen_QueueInput final : public uSeqGen_Base
{
public:
    uSeqGen_QueueInput(queue_t *src) 
        : uSeqGen_Base()
    {
        SetInputCount_(0);
        SetOutputCount_(1);
    }
    
    ~uSeqGen_QueueInput() {
    }

protected:
    void __not_in_flash_func(Process_)(DSPatch::SignalBus&, DSPatch::SignalBus& outputs) override
    {
        double tmp;
        if (queue_try_remove(queue, &tmp)) {
            val = tmp;
            outputs.SetValue(0, val);
        }
    }
private:
    queue_t *queue;
    double val = 0;
};

#endif // USEQGEN_QUEUEINPUT_H
