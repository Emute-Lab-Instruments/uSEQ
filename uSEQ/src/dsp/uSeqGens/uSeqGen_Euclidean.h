#ifndef USEQGEN_EUCLIDEAN_H
#define USEQGEN_EUCLIDEAN_H

#include "uSeqGen_Base.h"

class uSeqGen_Euclidean final : public uSeqGen_Base
{
public:
    uSeqGen_Euclidean(queue_t* q, size_t key) : uSeqGen_Base(q, key)
    {
        SetInputCount_(5);
        SetOutputCount_(1);

        addMessageHandler("n", [this](command_data_message_data& data)
                          { n = static_cast<size_t>(data.floatData.value); });
        addMessageHandler("k", [this](command_data_message_data& data)
                          { k = static_cast<size_t>(data.floatData.value); });
        addMessageHandler("offset", [this](command_data_message_data& data)
                          { offset = static_cast<size_t>(data.floatData.value); });
        addMessageHandler("pulseWidth", [this](command_data_message_data& data)
                          { pulseWidth = data.floatData.value; });
    }

protected:
    enum INPUTS
    {
        INPUT_PHASE       = 0,
        INPUT_N           = 1,
        INPUT_K           = 2,
        INPUT_OFFSET      = 3,
        INPUT_PULSE_WIDTH = 4,
    };

    bool __force_inline euclidean(float _phase, const size_t _n, const size_t _k,
                                  const size_t _offset, const float _pulseWidth)
    {
        // Euclidean function
        const float fi  = _phase * _n;
        int i           = static_cast<int>(fi);
        const float rem = fi - i;
        if (i == _n)
        {
            i--;
        }
        const int idx = ((i + _n - _offset) * _k) % _n;
        return (idx < _k && rem < _pulseWidth) ? 1 : 0;
    }

    void __force_inline Process_(DSPatch::SignalBus& inputs,
                                 DSPatch::SignalBus& outputs) override
    {
        const float _phase = GET_INPUT_SAFE(inputs, float, INPUT_PHASE, 0.0f);
        const size_t _n    = GET_INPUT_SAFE(inputs, float, INPUT_N, n);
        // const size_t _k = GET_INPUT_SAFE(inputs, float, INPUT_K, k);
        float knorm          = GET_INPUT_SAFE(inputs, float, INPUT_K, 1.0);
        const size_t _k      = 1 + (knorm * (_n - 1));
        const size_t _offset = GET_INPUT_SAFE(inputs, float, INPUT_OFFSET, offset);
        const float _pw =
            GET_INPUT_SAFE(inputs, float, INPUT_PULSE_WIDTH, pulseWidth);
        const bool output = euclidean(_phase, _n, _k, 0, 0.5f);
        outputs.SetValue(0, output ? 1.0f : 0.0f);
    }

private:
    size_t n         = 14;
    size_t k         = 5;
    size_t offset    = 0;
    float pulseWidth = 0.5f;
};

#endif
