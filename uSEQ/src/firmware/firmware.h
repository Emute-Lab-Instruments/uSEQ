#ifndef FIRMWARE_FIRMWARE_H
#define FIRMWARE_FIRMWARE_H

// Firmware composition root — owns all modules, orchestrates tick loop

#include "../signal_engine/signal_engine.h"
#include "hardware_io.h"
#include "serial_protocol.h"

// Optional modules — only included when feature flags are defined
#if defined(ENABLE_FLASH_STORAGE)
#include "flash_storage.h"
#endif

#if defined(ENABLE_I2C_NETWORKING)
#include "i2c_network.h"
#endif

#if defined(ENABLE_DSP_ENGINE)
#include "dsp_engine.h"
#endif

namespace firmware {

struct Firmware {
    // ── Owned Modules ──────────────────────────────────────────────────────
    HardwareIO io;
    SerialProtocol serial;

#ifdef ENABLE_SIGNAL_ENGINE
    sig::SignalEngine engine;
#endif

#ifdef ENABLE_FLASH_STORAGE
    FlashStorage flash;
#endif

#ifdef ENABLE_I2C_NETWORKING
    I2CNetwork i2c;
#endif

#ifdef ENABLE_DSP_ENGINE
    DSPEngine dsp;
#endif

    // ── Tick State ─────────────────────────────────────────────────────────
    char code_buffer[2048]                        = {};
    double output_values[sig::MAX_OUTPUTS]         = {};
    double prev_tick_time                          = 0.0;

#ifdef ENABLE_SIGNAL_ENGINE
    double cell_snapshot[sig::MAX_CELLS]           = {};
    double workspace[sig::MAX_TOTAL_NODES]         = {};
#endif

    // ── Lifecycle ──────────────────────────────────────────────────────────
    void init();
    void tick();

    // ── Watchdog ──────────────────────────────────────────────────────────
    // Platform-guarded: hardware watchdog on RP2040, no-op on desktop.
    void watchdog_init();
    void watchdog_kick();
};

} // namespace firmware

#endif // FIRMWARE_FIRMWARE_H
