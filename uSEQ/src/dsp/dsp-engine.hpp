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
    // enum UGENS {QUEUE_OUTPUT=0, QUEUE_INPUT, COUNTER, TEST_UGEN, ENUM_END};
    enum COMMANDS {START, STOP, CREATE, DESTROY, CONNECT, DISCONNECT, GETUGENINFO};
    enum RESPONSES {UGENINFO};

    struct command_data_start {
        double sampleRate;
    };
    struct command_data_create {
        size_t processor;
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

    struct response_data_ugeninfo {
        size_t key;
        char name[64];
    };
    union response_data {
        response_data_ugeninfo ugenInfo;
    };
    struct response_info {
        RESPONSES response;
        response_data data;
    };

    void setup() {

        circuit = std::make_shared<DSPatch::Circuit>();
        registerUGen<uSeqGen_SerialPrint>("SerialPrint");
        registerUGen<uSeqGen_Counter>("Counter");
        // registerUGen<uSeqGen_QueueOutput>("QueueOutput");
        // registerUGen<uSeqGen_QueueInput>("QueueInput");
        // registerUGen<uSeqGen_Mul>("Mul");

        // testugen = std::make_shared<uSeqGen_SerialPrint>();
        // counter = std::make_shared<uSeqGen_Counter>();
    
        // circuit->AddComponent(testugen);
        // circuit->AddComponent(counter);
        // circuit->ConnectOutToIn(counter,0, testugen, 0);

        // testOutput = std::make_shared<uSeqGen_QueueOutput>(&DSPQ::q_outputs[0]);
        // circuit->AddComponent(testOutput);

        // //start to listen for commands
        add_repeating_timer_ms(-50, [](repeating_timer_t *rt) -> bool {
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
                case GETUGENINFO:
                    request_ugen_info();
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

    void FAST_FUNC(create)(size_t processor, size_t key) {
        if (processor < uGenFactories.size()) {
            componentPtr newProcessor = uGenFactories[processor].create();
            circuit->AddComponent(newProcessor);
            newProcessor->key = key;
            components[key] = newProcessor;
            println("Created processor: " + String(key) + " " + uGenFactories[processor].name);
        }else{
            println("Processor not found: " + String(processor));
        }
    }
    
    bool FAST_FUNC(run)(double sampleRate) {
        Serial.printf("Run:\n");

        if (isRunning) {
            stop();
        }
        int quantum = static_cast<int>(1.0e6/sampleRate);
        add_repeating_timer_us(-quantum, [](repeating_timer_t *rt) -> bool {
            return static_cast<uSEQDSPEngine*>(rt->user_data)->timer_callback();
        }, this, &timer);
        isRunning = true;
        return true;
    }

    void FAST_FUNC(stop)() {
        cancel_repeating_timer(&timer);
        isRunning = false;
    }

    void FAST_FUNC(request_ugen_info)() {
        for(size_t i=0; i< uGenFactories.size() ; i++) {
            auto factory = uGenFactories[i];
            response_info resp;
            resp.response = RESPONSES::UGENINFO;
            resp.data.ugenInfo.key = i;
            std::strncpy(resp.data.ugenInfo.name, factory.name.c_str(), 63);
            resp.data.ugenInfo.name[63] = '\0';
            queue_try_add(&DSPQ::q_engine_responses, &resp);
            println("UGENINFO: " + String(resp.data.ugenInfo.key) + " " + factory.name);
        }
    }

private:
    std::shared_ptr<DSPatch::Circuit> circuit;

    struct uGenFactory {
        String name;
        std::function<componentPtr()> create;
    };

    std::vector<uGenFactory> uGenFactories;

    template <typename ugenType>
    void registerUGen(const String& ugenName) {
        uGenFactories.push_back({
            ugenName,
            []() -> componentPtr {
                return std::make_shared<ugenType>();
            }
        });
    };

    componentPtr testugen; 
    componentPtr counter;
    repeating_timer_t timer, command_timer;

    std::unordered_map<size_t, componentPtr> components;

    componentPtr testOutput;

    bool isRunning = false;
};

#endif