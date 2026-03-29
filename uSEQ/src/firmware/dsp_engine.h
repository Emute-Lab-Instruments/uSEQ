#ifndef FIRMWARE_DSP_ENGINE_H
#define FIRMWARE_DSP_ENGINE_H

// Optional — DSP processing on core 1 (RP2040 dual-core).
// On desktop builds, the command queue uses a simple mutex-based queue.
// Active only when ENABLE_DSP_ENGINE is defined (see configure.h).

#ifdef ENABLE_DSP_ENGINE

#include <cstdint>

#ifdef ARDUINO
#include "pico/util/queue.h"
#else
#include <mutex>
#include <cstddef>
#endif

namespace firmware {

// ── DSP Command ───────────────────────────────────────────────────────────
// Commands sent from core 0 (LISP) to core 1 (DSP).

struct DSPCommand {
    enum class Kind : uint8_t {
        None     = 0,
        SetParam = 1,
        Reset    = 2,
        Start    = 3,
        Stop     = 4,
    } kind = Kind::None;

    uint16_t target = 0;     // Destination parameter / module ID
    double   value  = 0.0;   // Parameter value
};

// ── DSP Engine ────────────────────────────────────────────────────────────

struct DSPEngine {
    // ── State ─────────────────────────────────────────────────────────────
    bool running = false;

    // ── Command queue ─────────────────────────────────────────────────────
    // On RP2040: pico SDK lock-free queue (safe across cores).
    // On desktop: mutex-guarded ring buffer.
#ifdef ARDUINO
    queue_t cmd_queue;
#else
    static constexpr size_t QUEUE_CAPACITY = 32;
    DSPCommand queue_buf[QUEUE_CAPACITY]   = {};
    size_t     queue_head                  = 0;
    size_t     queue_tail                  = 0;
    std::mutex queue_mtx;
#endif

    // ── Methods ───────────────────────────────────────────────────────────
    void init();
    void tick();  // Called from loop1() on core 1
    bool enqueue_command(const DSPCommand& cmd);

private:
    // Drain pending commands (called from tick).
    void process_commands();
};

} // namespace firmware

#endif // ENABLE_DSP_ENGINE

#endif // FIRMWARE_DSP_ENGINE_H
