#include "firmware.h"
#include "../utils/time.h"
#include "../devtools/devtools.h"
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

    // 2. LED boot animation (variant-specific chase, or amber fallback)
#ifdef ENABLE_LED_CONTROL
    io.boot_led_animation();
#endif

    // 3. Serial protocol (USB CDC, baud rate)
#ifdef ENABLE_SIGNAL_ENGINE
    serial.engine = &engine;
#endif
    serial.num_serial_outs = io.num_serial_outs;
    serial.num_serial_ins  = 2; // ain1, ain2 — the only inputs the editor subscribes to
    serial.init();

    // 4. Flash storage (on-chip filesystem)
#ifdef ENABLE_FLASH_STORAGE
    flash.init();
#endif

    // 5. Optional: I2C multi-module network
#ifdef ENABLE_I2C_NETWORKING
    i2c.init();
#endif

    // 6. Signal engine: intern well-known symbols, set timing defaults
#ifdef ENABLE_SIGNAL_ENGINE
    sig::GraphBuilder::init_symbols();
    engine.init_defaults();

    // 7. Flash load + recompile
#ifdef ENABLE_FLASH_STORAGE
    if (flash.has_saved_state()) {
        if (flash.load(engine)) {
            sig::recompile_all_outputs(engine);
        } else {
            io.boot_led_error_flash();
        }
    }
#endif
#endif // ENABLE_SIGNAL_ENGINE

    // 8. DevTools (compile-time gated)
#ifdef ENABLE_SIGNAL_ENGINE
    dt::init(&engine);
#else
    dt::init(nullptr);
#endif
#ifdef ARDUINO
    dt::gauge("watchdog_reboot", watchdog_caused_reboot() ? 1u : 0u);
#if USEQ_DEVTOOLS
    dt::gauge("cpu_hz", static_cast<uint32_t>(rp2040.f_cpu()));
#endif
#endif

    // 9. LED → green (ready)
#ifdef ENABLE_LED_CONTROL
    io.boot_led_green();
#endif

    // 10. Enable watchdog (200ms timeout) — must come after all init
    watchdog_init();

    // 11. Tell the editor we're alive
    serial.send_ready();
}

// ── Firmware::tick ─────────────────────────────────────────────────────────
// Single iteration of the main loop.  Must never allocate or block.
// All scratch buffers are struct members — no stack-allocated arrays.

void Firmware::tick()
{
    dt::tick_begin();

    // ── 1. Time ────────────────────────────────────────────────────────────
#ifdef ENABLE_SIGNAL_ENGINE
    const double wall_t = get_system_time_seconds();
    double t = engine.state.logical_time(wall_t);
    engine.state.current_wall_time = wall_t;
    engine.state.current_time = t;
    engine.state.current_dt = engine.state.reset_dt_on_next_tick
        ? 0.0 : t - prev_tick_time;
#else
    const double t = get_system_time_seconds();
#endif

    // ── 2. Read hardware inputs ────────────────────────────────────────────
    dt::mark("input");
    io.read_inputs();

    // ── 3. Serial: non-blocking command intake ─────────────────────────────
    dt::mark("serial");
#ifdef ENABLE_SIGNAL_ENGINE
    if (serial.has_incoming()) {
        if (serial.read_command(code_buffer, sizeof(code_buffer))) {
            uint32_t len = 0;
            while (len < sizeof(code_buffer) && code_buffer[len] != '\0') {
                len++;
            }

            dt::eval_begin();
            sig::EvalResult result = sig::eval_cold(code_buffer, len, engine);
            dt::eval_end(result.kind != sig::EvalResult::Error);
            serial.send_eval_response(result);

            // A transport command may have changed the wall-to-logical-time
            // mapping (pause, resume, rewind, or stop). Apply it in this same
            // tick so no stale pre-command time reaches the executor/stream.
            t = engine.state.logical_time(wall_t);
            engine.state.current_time = t;
            engine.state.current_dt = engine.state.reset_dt_on_next_tick
                ? 0.0 : t - prev_tick_time;
        }
    }
#else
    // Without signal engine, still process protocol messages (hello, ping, etc.)
    // If editor sends an eval, respond with an error instead of silently dropping.
    if (serial.has_incoming()) {
        if (serial.read_command(code_buffer, sizeof(code_buffer))) {
            sig::EvalResult err;
            err.kind = sig::EvalResult::Error;
            static const char msg[] = "signal engine not available";
            err.text = msg;
            err.text_length = sizeof(msg) - 1;
            serial.send_eval_response(err);
        }
    }
#endif

    // ── 4. Execute signal graph ────────────────────────────────────────────
    dt::mark("execute");
#ifdef ENABLE_SIGNAL_ENGINE
    if (engine.state.is_playing) {
        // Snapshot cell values for this tick (immutable view for executor).
        // Skip the full-array copy when no cell has changed since the last
        // snapshot (A12) — this copy measured ~40% of the engine tick.
        if (cell_snapshot_revision != engine.cells.store_revision) {
            engine.cells.snapshot_values(cell_snapshot, sig::MAX_CELLS);
            cell_snapshot_revision = engine.cells.store_revision;
        }

        // Fill execution context — no heap, all members / struct fields
        sig::ExecutionContext ctx;
        ctx.t              = t;
        ctx.dt             = engine.state.reset_dt_on_next_tick
            ? 0.0 : t - prev_tick_time;
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
        engine.state.current_time = t;
        engine.state.current_dt = ctx.dt;
        engine.state.reset_dt_on_next_tick = false;
        prev_tick_time = t;
    }
#endif
    // When paused (or no engine): output_values retain their last-known-good
    // values, which get written to hardware below.

    // ── 5. Write outputs and update LEDs ───────────────────────────────────
    // io.outputs mirrors engine output_values directly (engine layout:
    // a1-a8=0-7, d1-d8=8-15, s1-s8=16-23). hardware_io::write_outputs()
    // knows the layout and routes to the right pins.
    dt::mark("output");
    for (size_t i = 0; i < sig::MAX_OUTPUTS; ++i)
        io.outputs[i] = output_values[i];
    io.write_outputs();
#ifdef ENABLE_LED_CONTROL
    io.update_leds();
#endif

    // ── 6. Commit outputs (LKG update for next tick) ───────────────────────
    dt::mark("lkg");
#ifdef ENABLE_SIGNAL_ENGINE
    if (engine.state.is_playing) {
        sig::commit_outputs(engine.pool, output_values);
    }
#endif

    // ── 7. Opportunistic stream data for visualisation ─────────────────────
    dt::mark("stream");
    serial.m_current_time = t;
    serial.send_stream_data(output_values, sig::MAX_OUTPUTS,
                            io.inputs, firmware::MAX_HW_INPUTS);
    dt::tick_end();
    dt::sample_runtime_memory();
#if USEQ_DEVTOOLS
    {
        bool can_wr = true;
#ifdef ARDUINO
        can_wr = Serial.availableForWrite() > 0;
#endif
        dt::emit_streaming([](const char* s, size_t n) {
#ifdef ARDUINO
            Serial.write(reinterpret_cast<const uint8_t*>(s), n);
            Serial.write('\n');
#else
            fwrite(s, 1, n, stdout);
            putchar('\n');
            fflush(stdout);
#endif
        }, can_wr);
    }
#endif

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
