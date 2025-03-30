#ifndef USEQGEN_BASE_H
#define USEQGEN_BASE_H

#include "../dspatch/include/DSPatch_Embedded.h"

class uSeqGen_Base : public DSPatch::Component
{
public:
    uSeqGen_Base()
        : Component(ProcessOrder::OutOfOrder)
    {
    }

    size_t key=0;

protected:
private:
};


#endif // USEQGEN_BASE_H