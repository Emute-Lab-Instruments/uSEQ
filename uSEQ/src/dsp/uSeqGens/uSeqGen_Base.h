#ifndef USEQGEN_BASE_H
#define USEQGEN_BASE_H

#include "../dspatch/include/DSPatch_Embedded.h"

// Your code here
class uSeqGen_Base : public DSPatch::Component
{
public:
    uSeqGen_Base()
        : Component(ProcessOrder::OutOfOrder)
    {
    }

protected:
private:
};


#endif // USEQGEN_BASE_H