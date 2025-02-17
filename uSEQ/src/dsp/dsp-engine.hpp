#ifndef __DSP_ENGINE_HPP
#define __DSP_ENGINE_HPP

#include "dspatch/include/DSPatch_Embedded.h"


class uSEQDSPEngine {
public:
    void setup() {

    }

    bool run() {
        return true;
    }
    
private:
    std::shared_ptr<DSPatch::Circuit> circuit = std::make_shared<DSPatch::Circuit>();
};

#endif