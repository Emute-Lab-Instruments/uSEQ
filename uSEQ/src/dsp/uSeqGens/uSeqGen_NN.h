#ifndef USEQGEN_NN_H
#define USEQGEN_NN_H

#include "uSeqGen_Base.h"

class uSeqGen_NN final : public uSeqGen_Base
{
public:
uSeqGen_NN(queue_t *q, size_t key) : uSeqGen_Base(q, key)
    {
        SetInputCount_(5);
        SetOutputCount_(1);

        // addMessageHandler("n", [this](float value) {
        //     n = static_cast<size_t>(value);
        // });        
    }

protected:


    void __force_inline Process_(DSPatch::SignalBus& inputs, DSPatch::SignalBus& outputs) override
    {
        outputs.SetValue(0, 0.49f);
    }

private:
};

#endif 
