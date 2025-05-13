#ifndef USEQGEN_MUL_H
#define USEQGEN_MUL_H

#include "uSeqGen_Base.h"

class uSeqGen_Mul final : public uSeqGen_Base
{
public:
    uSeqGen_Mul(queue_t *q, size_t key) : uSeqGen_Base(q, key)
    {
        SetInputCount_(2);
        SetOutputCount_(1);
    }

protected:
    void __force_inline Process_(DSPatch::SignalBus& inputs, DSPatch::SignalBus& outputs) override
    {
        const float sig0 = *inputs.GetValue<float>(0);        
        const float sig1 = *inputs.GetValue<float>(1);        
        outputs.SetValue(0, sig0 * sig1);
        count++;
    }
private:
    float count = 0;
};

#endif // USEQGEN_MUL_H
