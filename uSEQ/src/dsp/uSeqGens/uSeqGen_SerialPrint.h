#ifndef USEQGEN_SERIALPRINT_H
#define USEQGEN_SERIALPRINT_H

#include "uSeqGen_Base.h"
#include "../../utils/log.h"

class uSeqGen_SerialPrint final : public uSeqGen_Base
{
public:
    uSeqGen_SerialPrint()
        : uSeqGen_Base()
    {
        SetInputCount_(1);
        SetOutputCount_(0);
    }

protected:
    void __not_in_flash_func(Process_)(DSPatch::SignalBus& inputs, DSPatch::SignalBus& outputs) override
    {
        println("ugen " + String(key));
        if (auto sig0 = inputs.GetValue<double>(0)) {
            String s = "sig " + String(*sig0);
        }            
        // println(;
        // println("ugen");
        // outputs.SetValue(0, 0);
    }
private:
};

#endif // USEQGEN_SERIALPRINT_H
