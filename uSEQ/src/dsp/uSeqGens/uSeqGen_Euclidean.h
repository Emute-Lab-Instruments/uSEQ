#ifndef USEQGEN_EUCLIDEAN_H
#define USEQGEN_EUCLIDEAN_H

#include "uSeqGen_Base.h"

class uSeqGen_Euclidean final : public uSeqGen_Base
{
public:
    uSeqGen_Euclidean(queue_t *q, size_t key) : uSeqGen_Base(q, key)
    {
        SetInputCount_(5);
        SetOutputCount_(1);
    }


protected:

    enum INPUTS {
        INPUT_PHASE = 0,
        INPUT_N = 1,
        INPUT_K = 2,
        INPUT_OFFSET = 3,
        INPUT_PULSE_WIDTH = 4,
    };

    bool euclidean(float phase, const size_t n, const size_t k, const size_t offset, const float pulseWidth)
    {
        // Euclidean function
        const float fi = phase * n;
        int i = static_cast<int>(fi);
        const float rem = fi - i;
        if (i == n)
        {
            i--;
        }
        const int idx = ((i + n - offset) * k) % n;
        return (idx < k && rem < pulseWidth) ? 1 : 0;
    }

    void __force_inline Process_(DSPatch::SignalBus& inputs, DSPatch::SignalBus& outputs) override
    {
        const float phase = GET_INPUT_SAFE(inputs, float, INPUT_PHASE, 0.0f);
        const bool output = euclidean(phase, 12, 5, 0, 0.5f);
        outputs.SetValue(0, output ? 1.0f : 0.0f);
    }

private:

};

#endif 
