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

        addMessageHandler("n", [this](float value) {
            n = static_cast<size_t>(value);
        });        
        addMessageHandler("k", [this](float value) {
            k = static_cast<size_t>(value);
        });
        addMessageHandler("offset", [this](float value) {
            offset = static_cast<size_t>(value);
        });
        addMessageHandler("pulseWidth", [this](float value) {
            pulseWidth = value;
        });
    }

protected:

    enum INPUTS {
        INPUT_PHASE = 0,
        INPUT_N = 1,
        INPUT_K = 2,
        INPUT_OFFSET = 3,
        INPUT_PULSE_WIDTH = 4,
    };

    bool __force_inline euclidean(float phase, const size_t n, const size_t k, const size_t offset, const float pulseWidth)
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
        const float _phase = GET_INPUT_SAFE(inputs, float, INPUT_PHASE, 0.0f);
        const size_t _n = GET_INPUT_SAFE(inputs, float, INPUT_N, n);
        const size_t _k = GET_INPUT_SAFE(inputs, float, INPUT_K, k);
        // const size_t _offset = GET_INPUT_SAFE(inputs, float, INPUT_OFFSET, offset);
        // const float _pw = GET_INPUT_SAFE(inputs, float, INPUT_PULSE_WIDTH, pulseWidth);
        const bool output = euclidean(_phase, _n, _k, 0, 0.5f);
        outputs.SetValue(0, output ? 1.0f : 0.0f);
    }

private:
    size_t n = 12;
    size_t k = 5;
    size_t offset = 0;
    float pulseWidth = 0.5f;

};

#endif 
