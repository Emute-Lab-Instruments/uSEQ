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
        double data;
    };
    void setup() {
        circuit->AddComponent(testugen);
        circuit->AddComponent(counter);

        testOutput = std::make_shared<uSeqGen_QueueOutput>(&DSPQ::q_outputs[0]);
        circuit->AddComponent(testOutput);

        //start to listen for commands
        add_repeating_timer_ms(50, [](repeating_timer_t *rt) -> bool {
            return static_cast<uSEQDSPEngine*>(rt->user_data)->command_timer_callback();
        }, this, &command_timer);
    }


    bool FAST_FUNC(timer_callback)() {
        circuit->Tick();
        return true;
    }    

    bool FAST_FUNC(command_timer_callback)() {
        command_info cmd;
        while(queue_try_remove(&DSPQ::q_engine_commands, &cmd)) {
            switch(cmd.command) {
                case START:
                    run(cmd.data);
                    break;
                case STOP:
                    stop();
                    break;
            }
        }
        return true;
    }    

private:
    bool FAST_FUNC(run)(double sampleRate) {
        if (isRunning) {
            stop();
        }
        size_t quantum = 1.0e6/sampleRate;
        add_repeating_timer_us(quantum, [](repeating_timer_t *rt) -> bool {
            return static_cast<uSEQDSPEngine*>(rt->user_data)->timer_callback();
        }, this, &timer);
        isRunning = true;
        return true;
    }

    void FAST_FUNC(stop)() {
        cancel_repeating_timer(&timer);
        isRunning = false;
    }

    std::shared_ptr<DSPatch::Circuit> circuit = std::make_shared<DSPatch::Circuit>();

    componentPtr testugen = std::make_shared<uSeqGen_SerialPrint>();
    componentPtr counter = std::make_shared<uSeqGen_Counter>();
    repeating_timer_t timer, command_timer;

    std::unordered_map<size_t, componentPtr> components;

    componentPtr testOutput;

    size_t nextComponentKey = 0;

    bool isRunning = false;
};

#endif