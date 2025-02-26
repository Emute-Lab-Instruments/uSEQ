#ifndef __DSP_ENGINE_HPP
#define __DSP_ENGINE_HPP

#include "dspatch/include/DSPatch_Embedded.h"
#include "dsp-queues.hpp"
#include "uSeqGens/uSeqGen_SerialPrint.h"
#include "uSeqGens/uSeqGen_Counter.h"
#include "uSeqGens/uSeqGen_Mul.h"
#include "uSeqGens/uSeqGen_QueueOutput.h"
#include "uSeqGens/uSeqGen_QueueInput.h"
#include <array>
#include <unordered_map>

using componentPtr = std::shared_ptr<DSPatch::Component>;

class uSEQDSPEngine {
public:
    enum COMMANDS {START, STOP};
    struct command_info {
        COMMANDS command;
    };
    void setup() {
        circuit->AddComponent(testugen);
        circuit->AddComponent(counter);

        testOutput = std::make_shared<uSeqGen_QueueOutput>(&DSPQ::q_outputs[0]);
        circuit->AddComponent(testOutput);
    }

    bool FAST_FUNC(run)(float sampleRate) {
        size_t quantum = 1.0e6/sampleRate;
        add_repeating_timer_us(quantum, [](repeating_timer_t *rt) -> bool {
            return static_cast<uSEQDSPEngine*>(rt->user_data)->timer_callback();
        }, this, &timer);
        return true;
    }

    bool FAST_FUNC(timer_callback)() {
        circuit->Tick();
        return true;
    }    

private:
    std::shared_ptr<DSPatch::Circuit> circuit = std::make_shared<DSPatch::Circuit>();

    componentPtr testugen = std::make_shared<uSeqGen_SerialPrint>();
    componentPtr counter = std::make_shared<uSeqGen_Counter>();
    repeating_timer_t timer;

    std::unordered_map<size_t, componentPtr> components;

    componentPtr testOutput;

    size_t nextComponentKey = 0;
};

#endif