#ifndef USEQGEN_SAMPLER_H
#define USEQGEN_SAMPLER_H

#include "uSeqGen_Base.h"

#include <cstdint>
// Flash memory address where audio data is loaded
#define AUDIO_FLASH_ADDRESS 0x10200000U
#define AUDIO_MAGIC 0x4F434950U // 'PICO'
#define AUDIO_VERSION 1U

// Sample information structure
typedef struct
{
    const char* name;      // File name
    const float* samples;  // Pointer to audio samples
    uint32_t sample_count; // Number of samples
    float duration;        // Duration in seconds
    bool found;            // Whether the file was found
} sample_info_t;

// Binary format structures
typedef struct
{
    uint32_t magic;       // 'PICO' magic number
    uint32_t version;     // Format version
    uint32_t file_count;  // Number of audio files
    uint32_t sample_rate; // Sample rate in Hz
} audio_header_t;

typedef struct
{
    char name[16];         // Null-terminated filename
    uint32_t offset;       // Offset to audio data
    uint32_t sample_count; // Number of samples
    float duration;        // Duration in seconds
    uint32_t reserved;     // Reserved for future use
} audio_file_entry_t;

class uSeqGen_Sampler final : public uSeqGen_Base
{
public:
    uSeqGen_Sampler(queue_t* q, size_t key) : uSeqGen_Base(q, key)
    {
        SetInputCount_(2);
        SetOutputCount_(1);
        sample_info.found = false;

        // create a queue
        //  queue_init(&q_input, sizeof(float), 1);

        // //share it with the interpreter
        // DSPQ::response_info resp;
        // resp.response = DSPQ::RESPONSES::ADD_INPUT_QUEUE;
        // resp.data.queueInfo.key = key;
        // resp.data.queueInfo.queueptr = &q_input;
        // resp.data.queueInfo.index = 0;
        // resp.data.queueInfo.queueSize = 1;
        // queue_try_add(q_message, &resp);
        createInputQueue(0, q_input);

        addMessageHandler("rate", [this](command_data_message_data& data)
                          { rate = data.floatData.value; });

        addMessageHandler(
            "sample",
            [this](command_data_message_data& data)
            {
                if (!get_sample_info(data.stringData.value, &sample_info))
                {
                    println("Error: Sample  not found in audio data.");
                }
                else
                {
                    println("Sample found: " + String(sample_info.name) +
                            ", count: " + String(sample_info.sample_count) +
                            ", duration: " + String(sample_info.duration));
                }
            });

        addMessageHandler("list", [this](command_data_message_data& data)
                          { list_all_samples(); });

        addMessageHandler("loop",
                          [this](command_data_message_data& data)
                          {
                              looping = data.floatData.value > 0;
                              if (looping)
                              {
                                  playing = true; // Set playing state if looping
                              }
                              println("Looping set to: " + String(looping));
                          });
    }

    ~uSeqGen_Sampler() override { queue_free(&q_input); }

protected:
    void __force_inline Process_(DSPatch::SignalBus& inputs,
                                 DSPatch::SignalBus& outputs) override
    {
        if (sample_info.found)
        {
            const float rateInput  = GET_INPUT_SAFE(inputs, float, 0, rate);
            const float phaseInput = GET_INPUT_SAFE(inputs, float, 1, 0.0f);

            float tmp;
            if (queue_try_remove(&q_input, &tmp))
            {
                if (lastRetrigValue <= 0 && tmp > 0)
                {
                    if (rateInput >= 0.f)
                    {
                        phase = 0.f; // Reset phase on retrigger
                    }
                    else
                    {
                        phase = sample_info.sample_count -
                                1.f; // Reset phase on retrigger for reverse playback
                    }
                    playing = true; // Set playing state
                }
                lastRetrigValue = tmp;
                // println("QueueInput: " + String(tmp));
            }

            if (playing)
            {

                if (fabs(phaseInput - lastTrigValue) > changeThreshold)
                {
                    // Jump to new phase if the input phase has changed significantly
                    phase         = phaseInput * sample_info.sample_count;
                    lastTrigValue = phaseInput;
                }

                // Linear interpolation for smooth playback
                const float float_index = phase;
                const size_t index0 = static_cast<size_t>(float_index);

                // Bounds checking for safety
                if (index0 >= sample_info.sample_count) {
                    outputs.SetValue(0, 0.f);
                } else {
                    const float frac = float_index - static_cast<float>(index0);

                    // Get current and next sample with boundary handling
                    const float sample0 = sample_info.samples[index0];

                    size_t index1;
                    if (looping) {
                        // Wrap around for looping
                        index1 = (index0 + 1) % sample_info.sample_count;
                    } else {
                        // Clamp to last sample for non-looping
                        index1 = (index0 + 1 < sample_info.sample_count) ? index0 + 1 : index0;
                    }
                    const float sample1 = sample_info.samples[index1];

                    // Linear interpolation: sample0 * (1 - frac) + sample1 * frac
                    const float interpolated_sample = sample0 + frac * (sample1 - sample0);
                    outputs.SetValue(0, interpolated_sample);
                }

                phase += rateInput;
                if (rateInput >= 0.f)
                {
                    if (phase >= sample_info.sample_count)
                    {
                        if (looping)
                        {
                            phase -= sample_info.sample_count;
                        }
                        else
                        {
                            playing = false;
                        }
                    }
                }
                else
                {
                    if (phase < 0)
                    {
                        if (looping)
                        {
                            phase += sample_info.sample_count;
                        }
                        else
                        {
                            playing = false;
                        }
                    }
                }
            }
        }
        else
        {
            outputs.SetValue(0, 0.f);
        }
    }

    /**
     * Get sample information by filename (without .wav extension)
     *
     * @param filename Name of the file without .wav extension (e.g., "intro",
     * "beep")
     * @param info Pointer to sample_info_t structure to fill
     * @return true if file found, false otherwise
     */
    bool get_sample_info(const char* filename, sample_info_t* info)
    {
        // println("get_sample_info called with filename: " + String(filename));
        if (!filename || !info)
        {
            return false;
        }

        // Initialize info structure
        memset(info, 0, sizeof(sample_info_t));

        // Read pointers from memory based on flash address
        const uint8_t* binary_data   = (const uint8_t*)AUDIO_FLASH_ADDRESS;
        const audio_header_t* header = (const audio_header_t*)binary_data;
        const audio_file_entry_t* file_table =
            (const audio_file_entry_t*)(binary_data + 16);

        // Verify binary is valid
        if (header->magic != AUDIO_MAGIC)
        {
            return false;
        }

        // Search for the file by name
        for (uint32_t i = 0; i < header->file_count; i++)
        {
            if (strcmp(file_table[i].name, filename) == 0)
            {
                // Found the file!
                info->name    = file_table[i].name;
                info->samples = (const float*)(binary_data + file_table[i].offset);
                info->sample_count = file_table[i].sample_count;
                info->duration     = file_table[i].duration;
                info->found        = true;
                return true;
            }
        }

        // File not found
        info->found = false;
        return false;
    }

    void list_all_samples()
    {
        // Read pointers from memory based on flash address
        const uint8_t* binary_data   = (const uint8_t*)AUDIO_FLASH_ADDRESS;
        const audio_header_t* header = (const audio_header_t*)binary_data;
        const audio_file_entry_t* file_table =
            (const audio_file_entry_t*)(binary_data + 16);

        // Verify binary is valid
        if (header->magic != AUDIO_MAGIC)
        {
            printf("Error: Invalid audio binary at 0x%08x\n", AUDIO_FLASH_ADDRESS);
            return;
        }

        println("Sample Library:");
        println("====================");
        println("Sample Rate: " + String(header->sample_rate) + " Hz");
        println("File Count: " + String(header->file_count));
        println("");

        if (header->file_count == 0)
        {
            println("No samples found.");
            return;
        }

        for (uint32_t i = 0; i < header->file_count; i++)
        {
            const audio_file_entry_t* file = &file_table[i];

            println("[" + String(i) + "] " + String(file->name) + ", " +
                    String(file->duration) + "s");
        }
    }

private:
    float rate                  = 1;
    float phase                 = 0.f;
    float lastTrigValue         = 0.0f;
    float lastRetrigValue       = 0.0f; // Last retrigger value
    const float changeThreshold = 1.f / 128.f;

    sample_info_t sample_info; // Sample information structure

    queue_t q_input;

    bool looping = false;
    bool playing = false;
};

#endif // USEQGEN_MUL_H
