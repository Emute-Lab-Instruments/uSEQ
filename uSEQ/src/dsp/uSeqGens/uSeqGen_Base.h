#ifndef USEQGEN_BASE_H
#define USEQGEN_BASE_H

#include "../dspatch/include/DSPatch_Embedded.h"
#include "pico/util/queue.h"
#include "../dsp-q-data.hpp"
#include <vector>
#include "Arduino.h"
#include <cstring>
#include <map>
#include <functional>
#include "../../utils/log.h"



#define GET_INPUT_SAFE(inputs, type, index, defaultVal) \
    (inputs.GetValue<type>(index) ? *inputs.GetValue<type>(index) : (defaultVal))


class uSeqGen_Base : public DSPatch::Component
{
public:

    using MessageHandlerFunction = std::function<void(float)>;

    uSeqGen_Base(queue_t *q, size_t key)
        : Component(ProcessOrder::OutOfOrder), q_message(q), key(key)
    {}

    size_t key=0;

    struct queue_spec {
        size_t nFloats;
        size_t key;
    };

    std::vector<uSeqGen_Base::queue_spec> outputQueues;
    std::vector<uSeqGen_Base::queue_spec> inputQueues;

    static void __not_in_flash_func(setSampleRate)(size_t sr) {
        uSeqGen_Base::sampleRate= sr;
        uSeqGen_Base::sampleRateRcpr = 1.f/sr;
    }

    void addMessageHandler(const String& name, MessageHandlerFunction func) {
        msgHandlerMap[name] = func;
    }

    void message(String s, float value) {
        auto it = msgHandlerMap.find(s);
        if (it != msgHandlerMap.end()) {
            it->second(value);
        } else {
           println("Message handler not found for: " + s);
        }
    }
    
    static float sampleRate;
    static float sampleRateRcpr;

protected:

    queue_t *q_message;

    void send_message(String s) {
        DSPQ::response_info resp;
        resp.response = DSPQ::RESPONSES::MESSAGE;
        resp.data.ugenMessage.key = key;
        std::strncpy(resp.data.ugenMessage.msg, s.c_str(), 63);
        resp.data.ugenMessage.msg[63] = '\0';
        queue_try_add(q_message, &resp);
    }


    std::map<String, MessageHandlerFunction> msgHandlerMap;

private:

};


#endif // USEQGEN_BASE_H