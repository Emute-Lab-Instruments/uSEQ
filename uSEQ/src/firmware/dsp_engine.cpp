#include "dsp_engine.h"

#ifdef ENABLE_DSP_ENGINE

namespace firmware {

// ── DSPEngine::init ───────────────────────────────────────────────────────
// Set up the inter-core command queue.
// On RP2040 this uses the pico SDK queue_t which is lock-free and
// safe for single-producer / single-consumer across cores.

void DSPEngine::init() {
#ifdef ARDUINO
    queue_init(&cmd_queue, sizeof(DSPCommand), 32);
#else
    // Desktop: ring buffer is already zero-initialized.
    queue_head = 0;
    queue_tail = 0;
#endif
    running = false;
}

// ── DSPEngine::tick ───────────────────────────────────────────────────────
// Called from loop1() (core 1) on the RP2040.
// Drains pending commands, then does any per-tick DSP work.

void DSPEngine::tick() {
    process_commands();

    // Future: run DSPatch circuit tick or custom DSP processing here.
    // The old code used a repeating_timer_t to drive circuit->Tick()
    // at audio rate.  For now this is a no-op placeholder for the
    // DSP graph tick, since the full DSPatch integration is P3.
}

// ── DSPEngine::enqueue_command ────────────────────────────────────────────
// Called from core 0 (main tick loop / LISP builtins).
// Returns false if the queue is full.

bool DSPEngine::enqueue_command(const DSPCommand& cmd) {
#ifdef ARDUINO
    return queue_try_add(&cmd_queue, &cmd);
#else
    std::lock_guard<std::mutex> lock(queue_mtx);
    size_t next_head = (queue_head + 1) % QUEUE_CAPACITY;
    if (next_head == queue_tail) {
        // Queue full.
        return false;
    }
    queue_buf[queue_head] = cmd;
    queue_head = next_head;
    return true;
#endif
}

// ── DSPEngine::process_commands ───────────────────────────────────────────
// Drain all pending commands from the queue.
// Called from tick() on core 1.

void DSPEngine::process_commands() {
    DSPCommand cmd;

#ifdef ARDUINO
    while (queue_try_remove(&cmd_queue, &cmd)) {
#else
    // Desktop: drain under lock.
    while (true) {
        {
            std::lock_guard<std::mutex> lock(queue_mtx);
            if (queue_tail == queue_head)
                break;
            cmd = queue_buf[queue_tail];
            queue_tail = (queue_tail + 1) % QUEUE_CAPACITY;
        }
#endif
        switch (cmd.kind) {
            case DSPCommand::Kind::Start:
                running = true;
                break;

            case DSPCommand::Kind::Stop:
                running = false;
                break;

            case DSPCommand::Kind::Reset:
                running = false;
                // Future: tear down DSP graph, clear state.
                break;

            case DSPCommand::Kind::SetParam:
                // Future: route cmd.target / cmd.value to DSP graph node.
                break;

            case DSPCommand::Kind::None:
            default:
                break;
        }
    }
}

} // namespace firmware

#endif // ENABLE_DSP_ENGINE
