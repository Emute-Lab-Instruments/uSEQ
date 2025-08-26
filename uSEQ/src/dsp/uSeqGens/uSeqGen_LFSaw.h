#ifndef USEQGEN_LFSAW_H
#define USEQGEN_LFSAW_H

#include "uSeqGen_Base.h"

class uSeqGen_LFSaw final : public uSeqGen_Base
{
public:
    uSeqGen_LFSaw(queue_t *q, size_t key) : uSeqGen_Base(q, key)
    {
        SetInputCount_(0);
        SetOutputCount_(1);

        addMessageHandler("freq", [this](command_data_message_data &data) {
            setFrequency(data.floatData.value);
        });    
        
        setFrequency(1.f);
    }

    void setFrequency(float freq) {
         inc = freq * uSeqGen_Base::sampleRateRcpr * 0.5f;
         frequency = freq;
    }



protected:
    void __force_inline Process_(DSPatch::SignalBus& inputs, DSPatch::SignalBus& outputs) override
    {
        outputs.SetValue(0, phase);
        phase += inc;
        if (phase > 1.f) {
            phase -= 2.f;
        }
    }
private:
    float count = 0;
    float frequency = 100.0;
    float phase=-1.f;
    float inc=0.02f;

};

#endif // USEQGEN_MUL_H
