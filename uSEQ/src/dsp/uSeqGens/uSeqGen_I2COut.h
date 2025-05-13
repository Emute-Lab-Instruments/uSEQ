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

};

#endif // USEQGEN_MUL_H
