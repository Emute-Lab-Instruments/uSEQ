#ifndef FIRMWARE_FIRMWARE_H
#define FIRMWARE_FIRMWARE_H

// Firmware composition root — owns all modules, orchestrates tick loop

#include "../signal_engine/signal_engine.h"
#include "hardware_io.h"
#include "serial_protocol.h"
#include "clock_sync.h"

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
    ClockSync clock_sync;
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
    // ── Hw-input edge debounce state (wire-protocol.md §5.10) ────────────
    // A raw level change is only reported once it has stayed stable for
    // HW_INPUT_DEBOUNCE_MS, so a bouncing contact yields exactly one frame
    // per real edge. debounced_* is the level last reported to the editor;
    // pending_* is the candidate level and the wall time it first appeared.
    bool   debounced_event_inputs[14]              = {};
    bool   pending_event_inputs[14]                = {};
    double pending_event_since[14]                 = {};
    bool event_inputs_initialized                  = false;

#ifdef ENABLE_SIGNAL_ENGINE
    double cell_snapshot[sig::MAX_CELLS]           = {};
    // CellStore revision the snapshot was taken at (A12). 0 = never taken;
    // store_revision starts at 1 so the first tick always snapshots.
    uint32_t cell_snapshot_revision                = 0;
    double workspace[sig::MAX_TOTAL_NODES]         = {};
    sig::TickTransition tick_transition             = {};
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
