#ifndef USEQGEN_QUEUEOUTPUT_H
#define USEQGEN_QUEUEOUTPUT_H

#include "uSeqGen_Base.h"
#include "pico/util/queue.h"

class uSeqGen_QueueOutput final : public uSeqGen_Base
{
public:
    uSeqGen_QueueOutput(queue_t *dest) 
        : uSeqGen_Base()
    {
        SetInputCount_(1);
        SetOutputCount_(0);
    }
    
    ~uSeqGen_QueueOutput() {
    }

protected:
    void __not_in_flash_func(Process_)(DSPatch::SignalBus& inputs, DSPatch::SignalBus&) override
    {
        const double sig0 = *inputs.GetValue<double>(0);        
        queue_try_add(queue, &tmp);
        tmp += 0.11;
    }
private:
    queue_t *queue;
    double tmp = 0;
};

#endif // USEQGEN_QUEUEOUTPUT_H
