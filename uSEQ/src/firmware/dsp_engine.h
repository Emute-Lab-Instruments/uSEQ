#ifndef FIRMWARE_DSP_ENGINE_H
#define FIRMWARE_DSP_ENGINE_H

// Core 1 is reserved for future audio-rate DSP. Currently unused.
// Active only when ENABLE_DSP_ENGINE is defined in the PlatformIO env.

#ifdef ENABLE_DSP_ENGINE

namespace firmware {

struct DSPEngine {
    void init() {}
    void tick() {}
};

} // namespace firmware

#endif // ENABLE_DSP_ENGINE

#endif // FIRMWARE_DSP_ENGINE_H
