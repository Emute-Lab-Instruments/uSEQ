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

using componentPtr = std::shared_ptr<uSeqGen_Base>;

class uSEQDSPEngine {
public:
    enum UGENS {QUEUE_OUTPUT=0, QUEUE_INPUT, COUNTER, TEST_UGEN};
    enum COMMANDS {START, STOP, CREATE, DESTROY, CONNECT, DISCONNECT};
    struct command_data_start {
        double sampleRate;
    };
    struct command_data_create {
        UGENS processor;
        size_t key;
    };
    struct command_data_destroy {
        size_t key;
    };
    struct command_data_connect {
        size_t srcKey;
        size_t channelSrc;
        size_t destKey;
        size_t channelDest;
    };
    union command_data {
        command_data_start start;
        command_data_create create;
        command_data_destroy destroy;
        command_data_connect connect;
    };

    struct command_info {
        COMMANDS command;
        command_data data;
    };
    void setup() {
        // circuit->AddComponent(testugen);
        // circuit->AddComponent(counter);

        // testOutput = std::make_shared<uSeqGen_QueueOutput>(&DSPQ::q_outputs[0]);
        // circuit->AddComponent(testOutput);

        // //start to listen for commands
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
                    run(cmd.data.start.sampleRate);
                    break;
                case STOP:
                    stop();
                    break;
                case DESTROY:
                    destroy(cmd.data.destroy.key);
                    break;
                case CREATE:
                    create(cmd.data.create.processor, cmd.data.create.key);
                    break;
                case CONNECT:   
                    connect(cmd.data.connect.srcKey, cmd.data.connect.channelSrc, cmd.data.connect.destKey, cmd.data.connect.channelDest);
                    break;
            }
        }
        return true;
    }    

    void FAST_FUNC(connect)(size_t srcKey, size_t channelSrc, size_t destKey, size_t channelDest) {
        auto src = components[srcKey];
        auto dest = components[destKey];
        if (src && dest) {
            circuit->ConnectOutToIn(src, channelSrc, dest, channelDest);
        }else{
            println("Processor(s) not found: " + String(srcKey) + " or " + String(destKey));
        }
    }

    void FAST_FUNC(destroy)(size_t key) {
        auto processor = components[key];
        if (processor) {
            circuit->RemoveComponent(processor);
            components.erase(key);
        }else{
            println("Processor not found: " + String(key));
        }
    }

    void FAST_FUNC(create)(UGENS processor, size_t key) {
        componentPtr newProcessor;
        switch(processor) {
            case QUEUE_OUTPUT:
                newProcessor = std::make_shared<uSeqGen_QueueOutput>(&DSPQ::q_outputs[0]);
                break;
            case QUEUE_INPUT:
                newProcessor = std::make_shared<uSeqGen_QueueInput>(&DSPQ::q_inputs[0]);
                break;
            case COUNTER:
                newProcessor = std::make_shared<uSeqGen_Counter>();
                break;
            case TEST_UGEN:
                newProcessor = std::make_shared<uSeqGen_SerialPrint>();
                break;
        }
        circuit->AddComponent(newProcessor);
        newProcessor->key = key;
        components[key] = newProcessor;
    
    }
    
    bool FAST_FUNC(run)(double sampleRate) {
        Serial.printf("Run:\n");

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

private:
    std::shared_ptr<DSPatch::Circuit> circuit = std::make_shared<DSPatch::Circuit>();

    // componentPtr testugen = std::make_shared<uSeqGen_SerialPrint>();
    // componentPtr counter = std::make_shared<uSeqGen_Counter>();
    repeating_timer_t timer, command_timer;

    std::unordered_map<size_t, componentPtr> components;

    componentPtr testOutput;

    bool isRunning = false;
};

#endif