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
    // - boot_led_amber(): all output LEDs on (booting)
    // - boot_led_green(): all output LEDs off (ready — normal tick takes over)
    // - boot_led_error_flash(): rapid flash pattern (error during boot)
    void boot_led_amber();
    void boot_led_green();
    void boot_led_error_flash();
};

} // namespace firmware

#endif // FIRMWARE_HARDWARE_IO_H
