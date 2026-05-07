#ifndef FIRMWARE_HARDWARE_IO_H
#define FIRMWARE_HARDWARE_IO_H

// All physical pin access — reading inputs, writing outputs, LEDs

#include "../signal_engine/types.h"
#include <cstdint>

namespace firmware {

constexpr size_t MAX_HW_INPUTS = 32;

struct HardwareIO {
    // ── Configuration ──────────────────────────────────────────────────────
    uint8_t num_continuous_outs = 3;
    uint8_t num_binary_outs    = 3;
    uint8_t num_serial_outs    = 9;
    uint8_t num_hw_inputs      = 0;

    // ── State ──────────────────────────────────────────────────────────────
    double inputs[MAX_HW_INPUTS]       = {};
    double outputs[sig::MAX_OUTPUTS]   = {};

    // ── Methods ────────────────────────────────────────────────────────────
    void init();
    void read_inputs();
    void write_outputs();
    void update_leds();

    // ── Boot LED Sequence ─────────────────────────────────────────────────
    // Uses output LEDs to signal boot state.
    // - boot_led_animation(): variant-specific LED chase (or amber fallback)
    // - boot_led_amber(): all output LEDs on (booting — used as a static fallback)
    // - boot_led_green(): all output LEDs off (ready — normal tick takes over)
    // - boot_led_error_flash(): rapid flash pattern (error during boot)
    void boot_led_animation();
    void boot_led_amber();
    void boot_led_green();
    void boot_led_error_flash();
};

// External-clock pulse mailbox. Populated by the gate ISR (USEQHARDWARE_1_0)
// or by polled rising-edge detection in read_inputs() (other variants).
// The seq counter wraps at 256 — callers compare against their last-seen
// value to detect new pulses, so the absolute count doesn't matter.
//
// `input` is 1 or 2 (matching I1 / I2). Out-of-range inputs return seq=0.
void ext_clock_read_pulse(uint8_t input, uint32_t& out_us, uint8_t& out_seq);

} // namespace firmware

#endif // FIRMWARE_HARDWARE_IO_H
