#ifndef USEQGEN_MIXER_H
#define USEQGEN_MIXER_H

#include "uSeqGen_Base.h"
#include <vector>

class uSeqGen_Mixer final : public uSeqGen_Base
{
public:
    uSeqGen_Mixer(queue_t *q, size_t key, size_t numInputs = 2) : uSeqGen_Base(q, key)
    {
        inputCount=2;
        SetInputCount_(inputCount);
        SetOutputCount_(1);
        
        
        // Add message handlers
        addMessageHandler("master_gain", [this](command_data_message_data &data) {
            setMasterGain(data.floatData.value);
        });
        
        
        addMessageHandler("set_inputs", [this](command_data_message_data &data) {
            const size_t nInputs = static_cast<size_t>(data.floatData.value);
            if (nInputs >= 1) {
                setInputCount(nInputs);
            }
        });
        
        addMessageHandler("mute", [this](command_data_message_data &data) {
            const size_t index = static_cast<size_t>(data.floatData.value);
            if (index >= 0) {
                muteInput(index, true);
            }
        });
        
        addMessageHandler("unmute", [this](command_data_message_data &data) {
            const size_t index = static_cast<size_t>(data.floatData.value);
            if (index >= 0) {
                muteInput(index, false);
            }
        });
    }

    void setMasterGain(float gain) {
        masterGain = gain;
    }
    
    void setInputCount(size_t count) {
        if (count != inputCount) {
            inputCount = count;
            muteStates.resize(inputCount, false);
            SetInputCount_(inputCount);
        }
    }
    
    void muteInput(size_t inputIndex, bool mute) {
        if (static_cast<size_t>(inputIndex) < muteStates.size()) {
            muteStates[inputIndex] = mute;
        }
    }
    
    float getMasterGain() const { return masterGain; }
    bool isInputMuted(size_t inputIndex) const {
        return (static_cast<size_t>(inputIndex) < muteStates.size()) ? muteStates[inputIndex] : true;
    }

protected:
    void __force_inline Process_(DSPatch::SignalBus& inputs, DSPatch::SignalBus& outputs) override
    {
        float mixedOutput = 0.0f;
        
        // Sum all inputs with their respective gains
        for (size_t i = 0; i < inputCount; ++i) {
            if (!muteStates[i]) {  // Only process non-muted inputs
                float inputSample = GET_INPUT_SAFE(inputs, float, i, 0.f);
                mixedOutput += inputSample;
            }
        }
        
        // Apply master gain and output
        mixedOutput *= masterGain;
        outputs.SetValue(0, mixedOutput);
    }

private:
    size_t inputCount;
    float masterGain = 1.0f;
    std::vector<bool> muteStates;
};

#endif // USEQGEN_MIXER_H