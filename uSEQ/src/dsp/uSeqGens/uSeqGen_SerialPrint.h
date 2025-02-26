#ifndef USEQGEN_SERIALPRINT_H
#define USEQGEN_SERIALPRINT_H

#include "../dspatch/include/DSPatch_Embedded.h"

class uSeqGen_SerialPrint final : public DSPatch::Component
{
public:
    uSeqGen_SerialPrint()
        : Component(ProcessOrder::OutOfOrder)
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
