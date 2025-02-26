#ifndef USEQGEN_SERIALPRINT_H
#define USEQGEN_SERIALPRINT_H

#include "uSeqGen_Base.h"

class uSeqGen_SerialPrint final : public uSeqGen_Base
{
public:
    uSeqGen_SerialPrint()
        : uSeqGen_Base()
    {
        SetInputCount_(0);
        SetOutputCount_(1);
    }

protected:
    void __not_in_flash_func(Process_)(DSPatch::SignalBus&, DSPatch::SignalBus& outputs) override
    {
        Serial.println("ugen");
        outputs.SetValue(0, 0);
    }
private:
};

#endif // USEQGEN_SERIALPRINT_H
