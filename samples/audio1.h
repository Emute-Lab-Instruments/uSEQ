// Auto-generated audio defines for flash-loaded data
// Samples: 91054, Sample Rate: 22050 Hz
// Load with: picotool load -x buttercup.bin -o 0x10200000

#ifndef AUDIO_FLASH_H
#define AUDIO_FLASH_H

#include <stdint.h>

// Flash memory address where audio data is loaded
#define AUDIO_FLASH_ADDRESS 0x10200000U
#define AUDIO_SAMPLE_RATE 22050U
#define AUDIO_NUM_SAMPLES 91054U
#define AUDIO_DATA_SIZE 364224U

// Pointer to audio data in flash
#define AUDIO_DATA_PTR ((const uint8_t*)AUDIO_FLASH_ADDRESS)

#endif // AUDIO_FLASH_H
