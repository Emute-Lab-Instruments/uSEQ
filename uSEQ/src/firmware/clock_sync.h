#ifndef FIRMWARE_CLOCK_SYNC_H
#define FIRMWARE_CLOCK_SYNC_H

// External clock sync — tempo estimation and BPM cell update from gate
// pulses on inputs I1/I2.
//
// Architecture
// ────────────
// 1. ISR (or polled rising-edge detector) records pulse timestamp + sequence
//    counter into a mailbox. See hardware_io.{h,cpp} for the hardware-side
//    state — the ISR does the absolute minimum: timestamp + seq increment.
// 2. ClockSync::tick() runs in the main loop, reads the mailbox, runs the
//    tempo estimator, and updates the bpm cell.
// 3. Updating bpm from external clock calls on_cell_changed so dependent
//    outputs recompile while the graph remains aligned with the new tempo.
//
// BPM convention
// ──────────────
// divisor = pulses per beat. Standard MIDI clock is 24 PPQ → divisor=24.
// A "1 pulse per beat" external clock → divisor=1.
// BPM = 60_000_000 / avg_microseconds_per_pulse / divisor.

#include <cstdint>
#include <cstddef>

namespace sig { struct SignalEngine; }

namespace firmware {

// Fixed-size moving-average tempo estimator. No heap, embedded-friendly.
// Discards the first sample (no valid prior timestamp), then warms up the
// filter window before producing a BPM.
struct TempoEstimator {
    static constexpr size_t WINDOW = 5;

    double  intervals_us[WINDOW] = {};
    uint8_t write_idx            = 0;
    uint8_t valid_count          = 0;     // capped at WINDOW
    uint32_t last_pulse_us       = 0;
    bool    has_prior            = false;

    // Feeds a pulse timestamp; returns BPM, or 0.0 if not enough samples yet
    // (need at least 2 pulses for an interval, returns BPM after that).
    double process(uint32_t pulse_us);

    void reset();
};

// Per-input rolling state for tracking position within a divisor cycle and
// (optionally) within a bar. Reset on (set-clock-ext) or (reset-clock-ext).
struct ExtClockTracker {
    uint8_t pulse_in_division = 0;   // 0..divisor-1
    uint8_t beat_in_bar       = 0;
    void reset() { pulse_in_division = 0; beat_in_bar = 0; }
};

struct ClockSync {
    TempoEstimator tempo_i1;
    TempoEstimator tempo_i2;
    ExtClockTracker tracker;

    uint8_t last_seq_i1 = 0;
    uint8_t last_seq_i2 = 0;

    uint8_t active_source = 0;       // mirrors EngineState::clock_source
    uint8_t divisor       = 1;

    // Called once per firmware tick. Reconfigures from EngineState if the
    // user changed clock source/divisor, drains the ISR mailbox, runs tempo
    // estimation, and updates the bpm cell + nudges the internal transport
    // origin to align phase with the external clock at bar boundaries.
    //
    // No-op when source = 0 (internal).
    void tick(sig::SignalEngine& engine, double now_seconds);

    // Process one already-captured pulse. Public as a deterministic native
    // test seam; production calls it only after the mailbox sequence changes.
    void process_pulse(sig::SignalEngine& engine, uint32_t pulse_us,
                       double now_seconds);

    // Reset the active estimator + tracker (both, to be safe on source switch).
    void reset_all();
};

} // namespace firmware

#endif // FIRMWARE_CLOCK_SYNC_H
