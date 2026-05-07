#include "clock_sync.h"
#include "hardware_io.h"

#include "../signal_engine/cold_eval.h"
#include "../signal_engine/cell_store.h"
#include "../modulisp/lisp/symbol_intern.h"

#include <math.h>

namespace firmware {

// ── TempoEstimator ──────────────────────────────────────────────────────────

double TempoEstimator::process(uint32_t pulse_us) {
    if (!has_prior) {
        last_pulse_us = pulse_us;
        has_prior = true;
        return 0.0;  // first pulse: no interval yet
    }

    // Unsigned subtraction wraps cleanly: micros() rolls over at ~71min,
    // a delta computed via uint32_t subtraction is correct as long as the
    // true delta fits in 32 bits (always, for any sane musical interval).
    uint32_t delta_us = pulse_us - last_pulse_us;
    last_pulse_us = pulse_us;

    // Reject implausible deltas (< 1ms means contact bounce or noise; > 5s
    // means the clock stopped and resumed — start fresh rather than emit a
    // bogus BPM).
    if (delta_us < 1000 || delta_us > 5000000) {
        // Treat as a fresh start: keep last_pulse_us but discard the gap.
        return 0.0;
    }

    intervals_us[write_idx] = (double)delta_us;
    write_idx = (write_idx + 1) % WINDOW;
    if (valid_count < WINDOW) valid_count++;

    double sum = 0.0;
    for (uint8_t i = 0; i < valid_count; i++) sum += intervals_us[i];
    double avg = sum / (double)valid_count;
    if (avg <= 0.0) return 0.0;

    return 60000000.0 / avg;
}

void TempoEstimator::reset() {
    for (auto& v : intervals_us) v = 0.0;
    write_idx     = 0;
    valid_count   = 0;
    last_pulse_us = 0;
    has_prior     = false;
}

// ── ClockSync ───────────────────────────────────────────────────────────────

void ClockSync::reset_all() {
    tempo_i1.reset();
    tempo_i2.reset();
    tracker.reset();
}

void ClockSync::tick(sig::SignalEngine& engine, double now_seconds) {
    // Reconfigure if user changed source or divisor, or asked for a reset.
    const uint8_t requested_source  = engine.state.clock_source;
    const uint8_t requested_divisor = engine.state.ext_clock_divisor == 0
                                          ? 1 : engine.state.ext_clock_divisor;

    if (requested_source != active_source || requested_divisor != divisor) {
        active_source = requested_source;
        divisor       = requested_divisor;
        reset_all();
        // Re-align seq cursors so we don't pretend to have missed pulses
        // that arrived before we became active.
        uint32_t ts;
        uint8_t  seq;
        ext_clock_read_pulse(1, ts, seq); last_seq_i1 = seq;
        ext_clock_read_pulse(2, ts, seq); last_seq_i2 = seq;
    }
    if (engine.state.ext_clock_reset) {
        engine.state.ext_clock_reset = false;
        reset_all();
    }

    if (active_source == 0) return;

    // Drain mailbox: we may see at most one new pulse per tick at the
    // microsecond timescale, but loop in case the tick was delayed.
    uint8_t&        last = (active_source == 1) ? last_seq_i1 : last_seq_i2;

    uint32_t ts_us;
    uint8_t  seq;
    ext_clock_read_pulse(active_source, ts_us, seq);
    if (seq == last) return;

    // Process all unseen pulses up to current seq. We only know the latest
    // timestamp (single-slot mailbox), so for skipped pulses we just bump
    // last; the estimator will see one large interval which it'll filter.
    last = seq;

    process_pulse(engine, ts_us, now_seconds);
}

void ClockSync::process_pulse(sig::SignalEngine& engine, uint32_t pulse_us,
                              double now_seconds) {
    if (active_source == 0) return;

    TempoEstimator& est = (active_source == 1) ? tempo_i1 : tempo_i2;
    double bpm_estimate = est.process(pulse_us);
    if (bpm_estimate <= 0.0) return;
    if (est.valid_count < TempoEstimator::WINDOW) return;  // wait for warm-up

    // BPM convention: divisor = pulses per beat (MIDI). Scale appropriately.
    double new_bpm = bpm_estimate / (double)divisor;
    if (new_bpm < 20.0 || new_bpm > 600.0) return;  // sanity gate

    // Update bpm cell directly. We deliberately skip on_cell_changed: the
    // executor takes a fresh snapshot every tick, so a value-only change to
    // an existing Number cell propagates without graph rebuild.
    auto& si = SymbolIntern::getInstance();
    sig::SymbolID bpm_sym = si.intern("bpm");
    engine.cells.cells[bpm_sym].kind = sig::CellKind::Number;
    engine.cells.cells[bpm_sym].value = new_bpm;
    engine.cells.cells[bpm_sym].revision++;
    engine.cells.store_revision++;

    // Bar-boundary phase nudge: every `divisor` pulses (i.e. once per beat),
    // and at the start of each bar, snap transport time to the nearest bar
    // so the user's beat phasor stays aligned with the external clock.
    tracker.pulse_in_division++;
    if (tracker.pulse_in_division >= divisor) {
        tracker.pulse_in_division = 0;
        // We're at a beat boundary. Read beats-per-bar to know whether
        // we're also at a bar boundary.
        sig::SymbolID bpb_sym = si.intern("beats-per-bar");
        double bpb = engine.cells.cells[bpb_sym].value;
        if (bpb < 1.0) bpb = 4.0;

        tracker.beat_in_bar++;
        if ((double)tracker.beat_in_bar >= bpb) {
            tracker.beat_in_bar = 0;
            // Bar boundary: snap transport time to the nearest bar so beat/bar
            // phasors stay aligned with the external clock. `now_seconds` is
            // the transport time the executor will see this tick (it already
            // includes both user and internal offsets), so adjust only the
            // internal transport origin. The user-visible time_offset remains
            // unchanged, and the snap takes effect on the next tick.
            const double bar_dur = (60.0 / new_bpm) * bpb;
            const double snapped = round(now_seconds / bar_dur) * bar_dur;
            engine.state.transport_offset += (snapped - now_seconds);
        }
    }
}

} // namespace firmware
