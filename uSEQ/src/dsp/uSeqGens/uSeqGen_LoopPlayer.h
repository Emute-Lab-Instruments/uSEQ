#ifndef USEQGEN_LOOPPLAYER_H
#define USEQGEN_LOOPPLAYER_H

#include "uSeqGen_Base.h"

#include <cstdint>
// Flash memory address where audio data is loaded
#define AUDIO_FLASH_ADDRESS    0x10200000U
#define AUDIO_SAMPLE_RATE      22050U
#define AUDIO_NUM_SAMPLES      91054U
#define AUDIO_DATA_SIZE        364224U

// Pointer to audio data in flash


class uSeqGen_LoopPlayer final : public uSeqGen_Base
{
public:
    uSeqGen_LoopPlayer(queue_t *q, size_t key) : uSeqGen_Base(q, key)
    {
        SetInputCount_(2);
        SetOutputCount_(1);

        addMessageHandler("rate", [this](float value) {
            rate = value;
        });        
        audio_samples = (float*)(AUDIO_FLASH_ADDRESS + 8);         
    }

    // void setFrequency(float freq) {
    //      inc = freq * uSeqGen_Base::sampleRateRcpr * 0.5f;
    //      frequency = freq;
    // }



protected:
    void __force_inline Process_(DSPatch::SignalBus& inputs, DSPatch::SignalBus& outputs) override
    {
        const float rateInput = GET_INPUT_SAFE(inputs, float, 0, rate);
        const float phaseInput = GET_INPUT_SAFE(inputs, float, 1, 0.0f);

        if (fabs(phaseInput - lastTrigValue) > changeThreshold) {
            // Jump to new phase if the input phase has changed significantly
            phase = phaseInput * AUDIO_NUM_SAMPLES;
            lastTrigValue = phaseInput;
        }

        //cast phase with rounding
        outputs.SetValue(0, audio_samples[static_cast<size_t>(phase+0.5f)]);


        phase += rateInput;
        if (rateInput >=0) {
            if (phase >= AUDIO_NUM_SAMPLES) {
                phase -= AUDIO_NUM_SAMPLES;
            }
        } else {
            if (phase < 0) {
                phase += AUDIO_NUM_SAMPLES;
            }
        }
    }
private:
    float count = 0;
    float rate = 1;
    float phase=0.f;
    float* audio_samples; 
    float lastTrigValue = 0.0f;
    const float changeThreshold = 1.f/128.f;
};

#endif // USEQGEN_MUL_H
