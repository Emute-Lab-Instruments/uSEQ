#ifndef FIRMWARE_FIRMWARE_H
#define FIRMWARE_FIRMWARE_H

// Firmware composition root — owns all modules, orchestrates tick loop

#include "../signal_engine/signal_engine.h"
#include "hardware_io.h"
#include "serial_protocol.h"
#include "flash_storage.h"

// Optional modules — only included when feature flags are defined
// (configure.h must be included before this header for these to resolve)
#if defined(ENABLE_I2C_NETWORKING)
#include "i2c_network.h"
#endif

#if defined(ENABLE_DSP_ENGINE)
#include "dsp_engine.h"
#endif

namespace firmware {

// ── Pending Command ────────────────────────────────────────────────────────
// Ring buffer entry for bar-quantized command execution.

struct PendingCommand {
    char code[2048] = {};
    uint16_t length = 0;
    bool occupied   = false;
};

struct Firmware {
    // ── Owned Modules ──────────────────────────────────────────────────────
    sig::SignalEngine engine;
    HardwareIO io;
    SerialProtocol serial;
    FlashStorage flash;

#ifdef ENABLE_I2C_NETWORKING
    I2CNetwork i2c;
#endif

#ifdef ENABLE_DSP_ENGINE
    DSPEngine dsp;
#endif

    // ── Tick State ─────────────────────────────────────────────────────────
    char code_buffer[2048]                        = {};
    double cell_snapshot[sig::MAX_CELLS]           = {};
    double output_values[sig::MAX_OUTPUTS]         = {};
    double workspace[sig::MAX_TOTAL_NODES]         = {};

    // ── Quantization ───────────────────────────────────────────────────────
    static constexpr size_t MAX_PENDING = 8;
    PendingCommand pending_commands[MAX_PENDING]   = {};
    uint8_t pending_head                           = 0;
    uint8_t pending_tail                           = 0;
    double last_bar_phasor                         = 0.0;

    // ── Lifecycle ──────────────────────────────────────────────────────────
    void init();
    void tick();

    // ── Watchdog ──────────────────────────────────────────────────────────
    // Platform-guarded: hardware watchdog on RP2040, no-op on desktop.
    void watchdog_init();
    void watchdog_kick();

private:
    // ── Bar-Quantized Scheduling ──────────────────────────────────────────
    void enqueue_pending(const char* code, uint16_t length);
    void maybe_drain_pending(double t);
};

} // namespace firmware

#endif // FIRMWARE_FIRMWARE_H
