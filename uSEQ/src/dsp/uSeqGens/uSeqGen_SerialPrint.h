#ifndef USEQGEN_SERIALPRINT_H
#define USEQGEN_SERIALPRINT_H

#include "uSeqGen_Base.h"
#include "../../utils/log.h"

class uSeqGen_SerialPrint final : public uSeqGen_Base
{
public:
    uSeqGen_SerialPrint(queue_t *q)
        : uSeqGen_Base(q)
    {
        SetInputCount_(1);
        SetOutputCount_(1);
    }

protected:
    void __not_in_flash_func(Process_)(DSPatch::SignalBus& inputs, DSPatch::SignalBus& outputs) override
    {
        println("ugen " + String(key));
        auto sig0 = inputs.GetValue<float>(0);
        if (sig0) {
            String s = "sig " + String(*sig0);
            println(s);
        }       else{
            println("sig0 is null");
        }     
        send_message("ugenspr " + String(key));
        // println(;
        // println("ugen");
        // outputs.SetValue(0, 0);
    }
private:
};

#endif // USEQGEN_SERIALPRINT_H
