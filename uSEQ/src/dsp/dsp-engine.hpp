#ifndef __DSP_ENGINE_HPP
#define __DSP_ENGINE_HPP

#include "dspatch/include/DSPatch_Embedded.h"
#include "pico/util/queue.h"
#include <array>
#include <unordered_map>

using componentPtr = std::shared_ptr<DSPatch::Component>;

class uSeqGen_SerialPrint final : public DSPatch::Component
{
public:
    uSeqGen_SerialPrint()
        // the order in which buffers are Process_()'ed is not important
        : Component( ProcessOrder::OutOfOrder )
    {
        SetInputCount_(0);
        SetOutputCount_( 1 );
    }


protected:
    void __not_in_flash_func(Process_)( DSPatch::SignalBus&, DSPatch::SignalBus& outputs ) override
    {
      Serial.println("ugen");
      outputs.SetValue( 0, 0);
    }
private:
};

class uSeqGen_Counter final : public DSPatch::Component
{
public:
    uSeqGen_Counter()
        // the order in which buffers are Process_()'ed is not important
        : Component( ProcessOrder::OutOfOrder )
    {
        SetInputCount_(0);
        SetOutputCount_( 1 );
    }


protected:
    void __not_in_flash_func(Process_)( DSPatch::SignalBus&, DSPatch::SignalBus& outputs ) override
    {
        outputs.SetValue( 0, count);
        count++;
    }
private:
    float count=0;
};

class uSeqGen_QueueOutput final : public DSPatch::Component
{
public:
    uSeqGen_QueueOutput(queue_t *dest) 
        // the order in which buffers are Process_()'ed is not important
        : Component( ProcessOrder::OutOfOrder ), queue(dest)
    {
        SetInputCount_(1);
        SetOutputCount_( 0 );

    }
    
    ~uSeqGen_QueueOutput() {
    }


protected:
    void __not_in_flash_func(Process_)( DSPatch::SignalBus& inputs, DSPatch::SignalBus& ) override
    {
        const double sig0 = *inputs.GetValue<double>(0);        
        queue_try_add(queue, &tmp);
        tmp+= 0.11;
    }
private:
    queue_t *queue;
    double tmp=0;

};

class uSEQDSPEngine {
public:
    enum COMMANDS {START, STOP};
    struct command_info {
        COMMANDS command;
    };
    void setup() {
        circuit->AddComponent(testugen);
        circuit->AddComponent(counter);

        for(auto &v : q_inputs) {
            queue_init(&v, sizeof(double), 1);
        }

        for(auto &v : q_outputs) {
            queue_init(&v, sizeof(double), 1);
        }

        testOutput = std::make_shared<uSeqGen_QueueOutput>(&q_outputs[0]);
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

    inline queue_t* getOutputQueue(const size_t idx) {
        return &q_outputs[idx];
    }
    
private:
    std::shared_ptr<DSPatch::Circuit> circuit = std::make_shared<DSPatch::Circuit>();

    componentPtr testugen = std::make_shared<uSeqGen_SerialPrint>();
    componentPtr counter = std::make_shared<uSeqGen_Counter>();
    repeating_timer_t timer;

    queue_t q_engine_commands;
    queue_t q_engine_messages;

    std::array<queue_t, 8> q_inputs;
    std::array<queue_t, 8> q_outputs;

    std::unordered_map<size_t, componentPtr> components;

    componentPtr testOutput;

    size_t nextComponentKey=0;
};

#endif