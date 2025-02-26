#ifndef USEQGEN_COUNTER_H
#define USEQGEN_COUNTER_H

#include "../dspatch/include/DSPatch_Embedded.h"

class uSeqGen_Counter final : public DSPatch::Component
{
public:
    uSeqGen_Counter()
        : Component(ProcessOrder::OutOfOrder)
    {
        SetInputCount_(0);
        SetOutputCount_(1);
    }

protected:
    void __not_in_flash_func(Process_)(DSPatch::SignalBus&, DSPatch::SignalBus& outputs) override
    {
        outputs.SetValue(0, count);
        count++;
    }
private:
    float count = 0;
};

#endif // USEQGEN_COUNTER_H
