#ifndef USEQGEN_TRIGRANG_H
#define USEQGEN_TRIGRAND_H

#include "uSeqGen_Base.h"

class uSeqGen_TrigRand final : public uSeqGen_Base
{
public:
    uSeqGen_TrigRand(queue_t* q, size_t key)
        : uSeqGen_Base(q, key), generator(std::random_device{}()),
          distribution(0.0f, 1.0f)
    {
        SetInputCount_(1);
        SetOutputCount_(1);
        randValue = distribution(generator);
    }

protected:
    void __force_inline Process_(DSPatch::SignalBus& inputs,
                                 DSPatch::SignalBus& outputs) override
    {
        const float trigInput = GET_INPUT_SAFE(inputs, float, 0, lastValue);
        if (trigInput > 0.f && lastValue <= 0.f)
        {
            // Rising edge detected
            randValue = distribution(generator);
        }

        lastValue = trigInput;

        outputs.SetValue(0, randValue);
    }

private:
    float lastValue = 0.f;
    float randValue = 0.f;
    std::mt19937 generator; // Mersenne Twister generator
    std::uniform_real_distribution<float> distribution;
};

#endif // USEQGEN_MUL_H
