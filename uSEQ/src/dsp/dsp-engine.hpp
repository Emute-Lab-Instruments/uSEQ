#ifndef __DSP_ENGINE_HPP
#define __DSP_ENGINE_HPP

#include "dspatch/include/DSPatch_Embedded.h"
#include "dsp-queues.hpp"
#include "uSeqGens/uSeqGen_SerialPrint.h"
#include "uSeqGens/uSeqGen_Counter.h"
#include "uSeqGens/uSeqGen_Mul.h"
#include "uSeqGens/uSeqGen_Phasor.h"
#include "uSeqGens/uSeqGen_QueueOutput.h"
#include "uSeqGens/uSeqGen_QueueInput.h"
#include "uSeqGens/uSeqGen_I2COut.h"
#include "uSeqGens/uSeqGen_Euclidean.h"
#include "uSeqGens/uSeqGen_NN.h"
#include "uSeqGens/uSeqGen_MT_DAC.h"
#include "uSeqGens/uSeqGen_LoopPlayer.h"
#include "uSeqGens/uSeqGen_Sampler.h"
#include <array>
#include <unordered_map>
#include "dsp-q-data.hpp"

using componentPtr = std::shared_ptr<uSeqGen_Base>;

class uSEQDSPEngine {
public:
    enum COMMANDS {SETUP, START, STOP, CREATE, DESTROY, RESET, CONNECT, DISCONNECT, GETUGENINFO, MESSAGE};
    static constexpr size_t MAX_MSG_KEY_LENGTH = 16;
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
    //todo: message should be a union of string/float/int etc and ugen should choose how to interpret it
    struct command_data_message {
        size_t ugen_key;
        char message[MAX_MSG_KEY_LENGTH];
        float value;
    };
    union command_data {
        command_data_start start;
        command_data_create create;
        command_data_destroy destroy;
        command_data_connect connect;
        command_data_message message;
    };

    struct command_info {
        COMMANDS command;
        command_data data;
    };

    uSEQDSPEngine() {
        circuit = std::make_shared<DSPatch::Circuit>();
        // //start to listen for commands
        add_repeating_timer_ms(-40, [](repeating_timer_t *rt) -> bool {
            return static_cast<uSEQDSPEngine*>(rt->user_data)->command_timer_callback();
        }, this, &command_timer);

    }

    void setup() {
        registerUGen<uSeqGen_SerialPrint>("serial-print");
        registerUGen<uSeqGen_Counter>("counter");
        registerUGen<uSeqGen_QueueOutput>("queue-output");
        registerUGen<uSeqGen_QueueInput>("queue-input");
        registerUGen<uSeqGen_Phasor>("phasor"); 
        registerUGen<uSeqGen_I2COut>("i2c-out"); 
        registerUGen<uSeqGen_Euclidean>("euclid"); 
        registerUGen<uSeqGen_NN>("nn"); 
        registerUGen<uSeqGen_MT_DAC>("dac");
        // registerUGen<uSeqGen_LoopPlayer>("loop");
        registerUGen<uSeqGen_Sampler>("sampler");
    }


    bool FAST_FUNC(timer_callback)() {
        circuit->Tick();
        return true;
    }    

    bool FAST_FUNC(command_timer_callback)() {
        command_info cmd;
        while(queue_try_remove(&DSPQ::q_engine_commands, &cmd)) {
            switch(cmd.command) {
                case SETUP:
                    setup();
                    break;
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
                case RESET:
                    reset();
                    break;
                case MESSAGE:
                    {
                        auto it = components.find(cmd.data.message.ugen_key);
                        if (it != components.end()) {
                            it->second->message(String(cmd.data.message.message), cmd.data.message.value);
                        } else {
                            println("Processor not found: " + String(cmd.data.message.ugen_key));
                        }
                    }
                    break;
                default:
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

    void FAST_FUNC(reset)() {
        if (isRunning) {
            stop();
        }
        circuit->RemoveAllComponents();
        components.clear();
        println("PPP reset");
    }

    void FAST_FUNC(create)(size_t processor, size_t key) {
        if (processor < uGenFactories.size()) {
            componentPtr newProcessor = uGenFactories[processor].create(key);
            circuit->AddComponent(newProcessor);
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
        uSeqGen_Base::setSampleRate(sampleRate);
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
            DSPQ::response_info resp;
            resp.response = DSPQ::RESPONSES::UGENINFO;
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
        std::function<componentPtr(size_t)> create;
    };

    std::vector<uGenFactory> uGenFactories;

    template <typename ugenType>
    void registerUGen(const String& ugenName) {
        uGenFactories.push_back({
            ugenName,
            [](size_t key) -> componentPtr {
                return std::make_shared<ugenType>(&DSPQ::q_engine_responses, key);
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