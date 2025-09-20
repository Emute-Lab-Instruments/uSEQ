#ifndef USEQGEN_BASE_H
#define USEQGEN_BASE_H

#ifdef ARDUINO
#include "../dsp-q-data.hpp"
#include "../dspatch/include/DSPatch_Embedded.h"
#include "Arduino.h"
#include "pico/util/queue.h"
#endif
#include "../../utils/log.h"
#include <cstring>
#include <functional>
#include <map>
#include <vector>

#define GET_INPUT_SAFE(inputs, type, index, defaultVal)                             \
    (inputs.GetValue<type>(index) ? *inputs.GetValue<type>(index) : (defaultVal))

class uSeqGen_Base : public DSPatch::Component
{
public:
    static constexpr size_t MAX_MSG_VALUE_LENGTH = 32;

    struct command_data_message_float
    {
        float value;
    };
    struct command_data_message_int
    {
        int32_t value;
    };
    struct command_data_message_string
    {
        char value[MAX_MSG_VALUE_LENGTH];
    };

    union command_data_message_data
    {
        command_data_message_float floatData;
        // command_data_message_int intData;
        command_data_message_string stringData;
    };

    using MessageHandlerFunction = std::function<void(command_data_message_data&)>;

    uSeqGen_Base(queue_t* q, size_t key)
        : Component(ProcessOrder::OutOfOrder), q_message(q), key(key)
    {
    }

    size_t key = 0;

    struct queue_spec
    {
        size_t nFloats;
        size_t key;
        size_t index = 0;
    };

    std::vector<uSeqGen_Base::queue_spec> outputQueues;
    std::vector<uSeqGen_Base::queue_spec> inputQueues;

    void createInputQueue(size_t index, queue_t& q_input,
                          DSPQ::UGEN_TYPE type = DSPQ::UGEN_TYPE::OTHER)
    {
        queue_init(&q_input, sizeof(float), 1);
        queue_spec q;
        q.nFloats = 1;
        // q.key = key;
        q.index = index;
        DSPQ::response_info resp;
        resp.response                 = DSPQ::RESPONSES::ADD_INPUT_QUEUE;
        resp.data.queueInfo.key       = key;
        resp.data.queueInfo.queueptr  = &q_input;
        resp.data.queueInfo.index     = 0;
        resp.data.queueInfo.queueSize = 1;
        resp.data.queueInfo.type      = type;
        queue_try_add(q_message, &resp);
        inputQueues.push_back(q);
    }

    void createOutputQueue(size_t index, queue_t& q_output,
                           DSPQ::UGEN_TYPE type = DSPQ::UGEN_TYPE::OTHER)
    {
        queue_init(&q_output, sizeof(float), 1);
        queue_spec q;
        q.nFloats = 1;
        // q.key = key;
        q.index = index;
        DSPQ::response_info resp;
        resp.response                 = DSPQ::RESPONSES::ADD_OUTPUT_QUEUE;
        resp.data.queueInfo.key       = key;
        resp.data.queueInfo.queueptr  = &q_output;
        resp.data.queueInfo.index     = 0;
        resp.data.queueInfo.queueSize = 1;
        resp.data.queueInfo.type      = type;
        queue_try_add(q_message, &resp);
        outputQueues.push_back(q);
    }

    static void __not_in_flash_func(setSampleRate)(size_t sr)
    {
        uSeqGen_Base::sampleRate     = sr;
        uSeqGen_Base::sampleRateRcpr = 1.f / sr;
    }

    void addMessageHandler(const String& name, MessageHandlerFunction func)
    {
        msgHandlerMap[name] = func;
    }

    void message(String s, command_data_message_data& data)
    {
        // println("Message: " + s + " " + String(value));
        auto it = msgHandlerMap.find(s);
        if (it != msgHandlerMap.end())
        {
            it->second(data);
        }
        else
        {
            println("Message handler not found for: " + s);
        }
    }

    void listQueues()
    {
        String msg = "Queues:\n";
        msg += "Inputs:\n";
        if (inputQueues.size() == 0)
        {
            msg += "  (none)\n";
        }
        for (auto& q : inputQueues)
        {
            msg += "  In  " + String(q.key) + " " + String(q.nFloats) + "\n";
        }
        msg += "Outputs:\n";
        if (outputQueues.size() == 0)
        {
            msg += "  (none)\n";
        }
        for (auto& q : outputQueues)
        {
            msg += "  Out " + String(q.key) + " " + String(q.nFloats) + "\n";
        }
        println(msg);
    }

    static float sampleRate;
    static float sampleRateRcpr;

protected:
    queue_t* q_message;

    void send_message(String s)
    {
        DSPQ::response_info resp;
        resp.response             = DSPQ::RESPONSES::MESSAGE;
        resp.data.ugenMessage.key = key;
        std::strncpy(resp.data.ugenMessage.msg, s.c_str(), 63);
        resp.data.ugenMessage.msg[63] = '\0';
        queue_try_add(q_message, &resp);
    }

    std::map<String, MessageHandlerFunction> msgHandlerMap;

private:
};

#endif // USEQGEN_BASE_H