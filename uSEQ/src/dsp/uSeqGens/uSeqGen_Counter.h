#ifndef USEQGEN_COUNTER_H
#define USEQGEN_COUNTER_H

#include "uSeqGen_Base.h"

class uSeqGen_Counter final : public uSeqGen_Base
{
public:
    uSeqGen_Counter()
        : uSeqGen_Base()
    {
        SetInputCount_(0);
        SetOutputCount_(1);
    }

protected:
    void __not_in_flash_func(Process_)(DSPatch::SignalBus&, DSPatch::SignalBus& outputs) override
    {
        println("counter " + String(count));
        outputs.SetValue(0, count);
        count++;
    }
private:
    float count = 0;
};

#endif // USEQGEN_COUNTER_H
