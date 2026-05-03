#include "firmware.h"
#include "../utils/time.h"
#include <cstring>

#ifdef ARDUINO
#include <hardware/watchdog.h>
#endif

namespace firmware {

// ── Firmware::init ─────────────────────────────────────────────────────────
// One-time setup: bring up all modules then signal readiness.
// Called once from main() before entering the tick loop.

void Firmware::init()
{
    // 1. Hardware I/O (pins, DACs, ADCs, LEDs)
    io.init();

    // 2. LED → amber (booting)
    io.boot_led_amber();

    // 3. Serial protocol (USB CDC, baud rate)
    serial.engine = &engine;
    serial.init();

    // 4. Flash storage (on-chip filesystem)
    flash.init();

    // 5. Optional: I2C multi-module network
#ifdef ENABLE_I2C_NETWORKING
    i2c.init();
#endif

    // 6. Signal engine: intern well-known symbols, set timing defaults
    sig::GraphBuilder::init_symbols();
    engine.init_defaults();

    // 7. Flash load + recompile
    if (flash.has_saved_state()) {
        if (flash.load(engine)) {
            sig::recompile_all_outputs(engine);
        } else {
            io.boot_led_error_flash();
        }
    }

    // 8. LED → green (ready)
    io.boot_led_green();

    // 9. Enable watchdog (200ms timeout) — must come after all init
    watchdog_init();

    // 10. Tell the editor we're alive
    serial.send_ready();
}

// ── Firmware::tick ─────────────────────────────────────────────────────────
// Single iteration of the main loop.  Must never allocate or block.
// All scratch buffers are struct members — no stack-allocated arrays.

void Firmware::tick()
{
    // ── 1. Time ────────────────────────────────────────────────────────────
    const double t = get_system_time_seconds() + engine.state.time_offset;

    // ── 2. Read hardware inputs ────────────────────────────────────────────
    io.read_inputs();

    // ── 3. Serial: non-blocking command intake ─────────────────────────────
    if (serial.has_incoming()) {
        if (serial.read_command(code_buffer, sizeof(code_buffer))) {
            // Determine actual length (read_command null-terminates)
            uint32_t len = 0;
            while (len < sizeof(code_buffer) && code_buffer[len] != '\0') {
                len++;
            }

            sig::EvalResult result = sig::eval_cold(code_buffer, len, engine);
            serial.send_eval_response(result);
        }
    }

    // ── 4. Execute signal graph ────────────────────────────────────────────
    if (engine.state.is_playing) {
        // Snapshot cell values for this tick (immutable view for executor)
        engine.cells.snapshot_values(cell_snapshot, sig::MAX_CELLS);

        // Fill execution context — no heap, all members / struct fields
        sig::ExecutionContext ctx;
        ctx.t              = t;
        ctx.dt             = t - prev_tick_time;
        ctx.cell_values    = cell_snapshot;
        ctx.hw_inputs      = io.inputs;
        ctx.data_pool      = engine.cells.data_pool;
        ctx.data_offsets   = engine.cells.data_offsets;
        ctx.data_lengths   = engine.cells.data_lengths;
        ctx.prev_outputs   = engine.pool.prev_output_values;
        ctx.output_values  = output_values;
        ctx.workspace      = workspace;

        sig::execute_all_outputs(engine.pool, ctx);
        sig::commit_state(engine.pool, workspace);
        prev_tick_time = t;
    }
    // When paused: output_values retain their last-known-good values,
    // which get written to hardware below.

    // ── 5. Write outputs and update LEDs ───────────────────────────────────
    for (size_t i = 0; i < sig::MAX_OUTPUTS; ++i) {
        io.outputs[i] = output_values[i];
    }
    io.write_outputs();
    io.update_leds();

    // ── 6. Commit outputs (LKG update for next tick) ───────────────────────
    if (engine.state.is_playing) {
        sig::commit_outputs(engine.pool, output_values);
    }

    // ── 7. Opportunistic stream data for visualisation ─────────────────────
    serial.send_stream_data(output_values, sig::MAX_OUTPUTS);

    // ── 8. Watchdog kick ──────────────────────────────────────────────────
    watchdog_kick();
}

// ── Watchdog ──────────────────────────────────────────────────────────────
// RP2040 hardware watchdog: 200ms timeout.  If tick() ever takes longer
// than 200ms (infinite loop, catastrophic bug), the MCU resets.
// On desktop builds these are no-ops.

void Firmware::watchdog_init()
{
#ifdef ARDUINO
    watchdog_enable(200, true);  // 200ms, pause on debug
#endif
}

void Firmware::watchdog_kick()
{
#ifdef ARDUINO
    watchdog_update();
#endif
}

} // namespace firmware
