#ifndef USEQGEN_I2COUT_H
#define USEQGEN_I2COUT_H

#include "uSeqGen_Base.h"
#include "Wire.h"

class uSeqGen_I2COut final : public uSeqGen_Base
{
public:
    uSeqGen_I2COut(queue_t *q, size_t key) : uSeqGen_Base(q, key)
    {
        SetInputCount_(8);
        SetOutputCount_(0);
        Wire1.setSDA(38);
        Wire1.setSCL(39);
        Wire1.begin();
        delay(10);
        for(size_t i=0; i<8; i++) {
            amps[i] = 1.f;
        }
        addMessageHandler("amp0", [this](float value) {
            amps[0] = value;
        });
        addMessageHandler("amp1", [this](float value) {
            amps[1] = value;
        }); 
        addMessageHandler("amp2", [this](float value) {
            amps[2] = value;
        });
        addMessageHandler("amp3", [this](float value) {
            amps[3] = value;
        });
        addMessageHandler("amp4", [this](float value) {
            amps[4] = value;
        });
        addMessageHandler("amp5", [this](float value) {
            amps[5] = value;
        });
        addMessageHandler("amp6", [this](float value) {
            amps[6] = value;
        });
        addMessageHandler("amp7", [this](float value) {
            amps[7] = value;
        });        

    }

    ~uSeqGen_I2COut() override
    {
        Wire1.end();
    }



protected:
    void __force_inline Process_(DSPatch::SignalBus& inputs, DSPatch::SignalBus& outputs) override
    {
        for(size_t i=0; i<8; i++) {
            values[i] = GET_INPUT_SAFE(inputs, float, i, 0.0);
        }
        Wire1.beginTransmission(1);
        Wire1.write((uint8_t*) &values, sizeof(values));
        int res = Wire1.endTransmission(true);      
    }

private:
    float values[8];
    float amps[8];

};

#endif // USEQGEN_MUL_H
