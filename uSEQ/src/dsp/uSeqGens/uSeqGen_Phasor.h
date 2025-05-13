#ifndef USEQGEN_PHASOR_H
#define USEQGEN_PHASOR_H

#include "uSeqGen_Base.h"

class uSeqGen_Phasor final : public uSeqGen_Base
{
public:
    uSeqGen_Phasor(queue_t *q, size_t key) : uSeqGen_Base(q, key)
    {
        SetInputCount_(0);
        SetOutputCount_(1);
    }

    void setFrequency(float freq) {
         inc = freq * uSeqGen_Base::sampleRateRcpr;
         frequency = freq;
    }

protected:
    void __force_inline Process_(DSPatch::SignalBus& inputs, DSPatch::SignalBus& outputs) override
    {
        outputs.SetValue(0, phase);
        phase += inc;
        if (phase > 1.f) {
            phase -= 1.f;
        }
    }
private:
    float count = 0;
    float frequency = 10;
    float phase=0.f;
    float inc=0.1f;

};

#endif // USEQGEN_MUL_H
