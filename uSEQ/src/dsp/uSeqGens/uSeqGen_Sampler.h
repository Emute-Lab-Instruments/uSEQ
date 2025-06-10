#ifndef USEQGEN_SAMPLER_H
#define USEQGEN_SAMPLER_H

#include "uSeqGen_Base.h"

#include <cstdint>
// Flash memory address where audio data is loaded
#define AUDIO_FLASH_ADDRESS    0x10200000U
#define AUDIO_MAGIC            0x4F434950U  // 'PICO'
#define AUDIO_VERSION          1U



// Sample information structure
typedef struct {
    const char* name;           // File name
    const float* samples;       // Pointer to audio samples
    uint32_t sample_count;      // Number of samples
    float duration;             // Duration in seconds
    bool found;                 // Whether the file was found
} sample_info_t;

// Binary format structures
typedef struct {
    uint32_t magic;        // 'PICO' magic number
    uint32_t version;      // Format version
    uint32_t file_count;   // Number of audio files
    uint32_t sample_rate;  // Sample rate in Hz
} audio_header_t;

typedef struct {
    char name[16];         // Null-terminated filename
    uint32_t offset;       // Offset to audio data
    uint32_t sample_count; // Number of samples
    float duration;        // Duration in seconds
    uint32_t reserved;     // Reserved for future use
} audio_file_entry_t;

class uSeqGen_Sampler final : public uSeqGen_Base
{
public:
    uSeqGen_Sampler(queue_t *q, size_t key) : uSeqGen_Base(q, key)
    {
        SetInputCount_(2);
        SetOutputCount_(1);
        println("uSeqGen_Sampler initialized");

        if (!get_sample_info("drone1", &sample_info)) {
            println("Error: Sample  not found in audio data.");
        }else{
            println("Sample found: " + String(sample_info.name) + ", count: " + String(sample_info.sample_count) + ", duration: " + String(sample_info.duration));
        }

        addMessageHandler("rate", [this](float value) {
            rate = value;
        });        
    }


protected:
    void __force_inline Process_(DSPatch::SignalBus& inputs, DSPatch::SignalBus& outputs) override
    {
        if (sample_info.found) {
            const float rateInput = GET_INPUT_SAFE(inputs, float, 0, rate);
            const float phaseInput = GET_INPUT_SAFE(inputs, float, 1, 0.0f);

            if (fabs(phaseInput - lastTrigValue) > changeThreshold) {
                // Jump to new phase if the input phase has changed significantly
                phase = phaseInput * sample_info.sample_count;
                lastTrigValue = phaseInput;
            }

            //cast phase with rounding
            outputs.SetValue(0, sample_info.samples[static_cast<size_t>(phase+0.5f)]);


            phase += rateInput;
            if (rateInput >=0.f) {
                if (phase >= sample_info.sample_count) {
                    phase -= sample_info.sample_count;
                }
            } else {
                if (phase < 0) {
                    phase += sample_info.sample_count;
                }
            }
        }
    }

    /**
     * Get sample information by filename (without .wav extension)
     * 
     * @param filename Name of the file without .wav extension (e.g., "intro", "beep")
     * @param info Pointer to sample_info_t structure to fill
     * @return true if file found, false otherwise
     */
    bool get_sample_info(const char* filename, sample_info_t* info) {
        println("get_sample_info called with filename: " + String(filename));
        if (!filename || !info) {
            return false;
        }
        
        // Initialize info structure
        memset(info, 0, sizeof(sample_info_t));
        
        // Read pointers from memory based on flash address
        const uint8_t* binary_data = (const uint8_t*)AUDIO_FLASH_ADDRESS;
        const audio_header_t* header = (const audio_header_t*)binary_data;
        const audio_file_entry_t* file_table = (const audio_file_entry_t*)(binary_data + 16);
        
        // Verify binary is valid
        if (header->magic != AUDIO_MAGIC) {
            return false;
        }
        
        // Search for the file by name
        for (uint32_t i = 0; i < header->file_count; i++) {
            if (strcmp(file_table[i].name, filename) == 0) {
                // Found the file!
                info->name = file_table[i].name;
                info->samples = (const float*)(binary_data + file_table[i].offset);
                info->sample_count = file_table[i].sample_count;
                info->duration = file_table[i].duration;
                info->found = true;
                return true;
            }
        }
        
        // File not found
        info->found = false;
        return false;
    }

private:
    float rate = 1;
    float phase=0.f;
    float lastTrigValue = 0.0f;
    const float changeThreshold = 1.f/128.f;

    sample_info_t sample_info;  // Sample information structure
};

#endif // USEQGEN_MUL_H
