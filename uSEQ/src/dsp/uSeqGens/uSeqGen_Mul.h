#ifndef USEQGEN_MUL_H
#define USEQGEN_MUL_H

#include "../dspatch/include/DSPatch_Embedded.h"

class uSeqGen_Base : public DSPatch::Component
{
public:
    uSeqGen_Base()
        : Component(ProcessOrder::OutOfOrder)
    {
    }

protected:
private:
};

class uSeqGen_Mul final : public uSeqGen_Base
{
public:
    uSeqGen_Mul() : uSeqGen_Base()
    {
        SetInputCount_(2);
        SetOutputCount_(1);
    }

protected:
    void __not_in_flash_func(Process_)(DSPatch::SignalBus& inputs, DSPatch::SignalBus& outputs) override
    {
        const double sig0 = *inputs.GetValue<double>(0);        
        const double sig1 = *inputs.GetValue<double>(1);        
        outputs.SetValue(0, sig0 * sig1);
        count++;
    }
private:
    float count = 0;
};

#endif // USEQGEN_MUL_H
