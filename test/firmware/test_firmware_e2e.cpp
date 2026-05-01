// Firmware end-to-end tests — categories A through G
// Tests the full tick loop: eval → compile → execute → capture outputs.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"
#include "firmware_test_harness.h"
#include "dsp_helpers.h"

// ============================================================================
// A. Input Waveform Fidelity [e2e][input]
// ============================================================================

TEST_CASE("A1: Sine passthrough — ain1 to a1", "[e2e][input]") {
    FirmwareTestHarness h;
    h.init();

    // Inject 10Hz sine into ain1 (input index 2)
    h.set_input_sine(2, 10.0, 0.5, 0.5);

    // Route input directly to output: (a1 ain1)
    auto r = h.eval("(a1 ain1)");
    REQUIRE(r.kind != sig::EvalResult::Error);

    h.start_capture();
    h.run_ticks(1000); // 1 second at 1kHz

    auto input_buf  = h.get_captured(0);  // a1 = output index 0
    REQUIRE(dsp::all_finite(input_buf));
    REQUIRE(input_buf.size() == 1000);

    // Generate expected sine for comparison
    auto expected = dsp::generate_sine(1000, 10.0, 1000.0, 0.5, 0.5);

    // Output should correlate strongly with the expected sine
    double corr = dsp::correlate(input_buf, expected);
    REQUIRE(corr > 0.99);

    // Peak-to-peak should match input amplitude (2 * 0.5 = 1.0)
    double pp = dsp::peak_to_peak(input_buf);
    REQUIRE(pp == Approx(1.0).margin(0.05));
}

TEST_CASE("A2: Square gate passthrough — in1 to d1", "[e2e][input]") {
    FirmwareTestHarness h;
    h.init();

    // Inject 2Hz square into in1 (input index 0)
    h.set_input_square(0, 2.0, 0.5, 0.5);

    // Route gate input to digital output
    auto r = h.eval("(d1 in1)");
    REQUIRE(r.kind != sig::EvalResult::Error);

    h.start_capture();
    h.run_ticks(1000); // 1 second

    auto out = h.get_captured(8); // d1 = output index 8
    REQUIRE(dsp::all_finite(out));

    // At 2Hz over 1 second, expect 2 rising edges (2 full cycles)
    int edges = dsp::count_rising_edges(out, 0.5);
    REQUIRE(edges == Approx(2).margin(1));
}

TEST_CASE("A3: Multiple inputs simultaneously", "[e2e][input]") {
    FirmwareTestHarness h;
    h.init();

    // ain1 (index 2) = 5Hz sine, ain2 (index 3) = 3Hz triangle
    h.set_input_sine(2, 5.0, 0.4, 0.5);
    h.set_input_triangle(3, 3.0, 0.3, 0.5);

    auto r1 = h.eval("(a1 ain1)");
    REQUIRE(r1.kind != sig::EvalResult::Error);
    auto r2 = h.eval("(a2 ain2)");
    REQUIRE(r2.kind != sig::EvalResult::Error);

    h.start_capture();
    h.run_ticks(1000);

    auto out_a1 = h.get_captured(0);
    auto out_a2 = h.get_captured(1);

    REQUIRE(dsp::all_finite(out_a1));
    REQUIRE(dsp::all_finite(out_a2));

    // a1 should have 5Hz characteristics
    double freq_a1 = dsp::frequency_estimate(out_a1, 1000.0, 0.5);
    REQUIRE(freq_a1 == Approx(5.0).margin(1.0));

    // a2 should have 3Hz characteristics
    double freq_a2 = dsp::frequency_estimate(out_a2, 1000.0, 0.5);
    REQUIRE(freq_a2 == Approx(3.0).margin(1.0));

    // They should be independent (low correlation)
    double corr = dsp::correlate(out_a1, out_a2);
    REQUIRE(std::abs(corr) < 0.5);
}

TEST_CASE("A4: Input range — constant values", "[e2e][input]") {
    FirmwareTestHarness h;
    h.init();

    auto r = h.eval("(a1 ain1)");
    REQUIRE(r.kind != sig::EvalResult::Error);

    // Test 0.0
    h.set_input_value(2, 0.0);
    h.run_ticks(1);
    REQUIRE(h.get_output(0) == Approx(0.0).margin(0.01));

    // Test 1.0
    h.set_input_value(2, 1.0);
    h.run_ticks(1);
    REQUIRE(h.get_output(0) == Approx(1.0).margin(0.01));

    // Test 0.5
    h.set_input_value(2, 0.5);
    h.run_ticks(1);
    REQUIRE(h.get_output(0) == Approx(0.5).margin(0.01));
}

// ============================================================================
// B. Signal Processing Chains [e2e][signal_chain]
// ============================================================================

TEST_CASE("B1: Amplitude scaling", "[e2e][signal_chain]") {
    FirmwareTestHarness h;
    h.init();

    // Input: sine(10Hz, amp=0.5, offset=0.5) → range [0.0, 1.0]
    h.set_input_sine(2, 10.0, 0.5, 0.5);

    auto r = h.eval("(a1 (* ain1 0.5))");
    REQUIRE(r.kind != sig::EvalResult::Error);

    h.start_capture();
    h.run_ticks(1000);

    auto out = h.get_captured(0);
    REQUIRE(dsp::all_finite(out));

    // Input p2p = 1.0, output should be halved = 0.5
    double pp = dsp::peak_to_peak(out);
    REQUIRE(pp == Approx(0.5).margin(0.05));
}

TEST_CASE("B2: DC offset shift", "[e2e][signal_chain]") {
    FirmwareTestHarness h;
    h.init();

    // Input: sine(10Hz, amp=0.25, offset=0.5) → DC = 0.5
    h.set_input_sine(2, 10.0, 0.25, 0.5);

    auto r = h.eval("(a1 (+ ain1 0.1))");
    REQUIRE(r.kind != sig::EvalResult::Error);

    h.start_capture();
    h.run_ticks(1000);

    auto out = h.get_captured(0);
    REQUIRE(dsp::all_finite(out));

    // DC should be shifted from 0.5 to 0.6
    double dc = dsp::dc_offset(out);
    REQUIRE(dc == Approx(0.6).margin(0.02));
}

TEST_CASE("B3: Clamp", "[e2e][signal_chain]") {
    FirmwareTestHarness h;
    h.init();

    // Input: sine(10Hz, amp=0.5, offset=0.5) → range [0.0, 1.0]
    h.set_input_sine(2, 10.0, 0.5, 0.5);

    auto r = h.eval("(a1 (clamp ain1 0.3 0.7))");
    REQUIRE(r.kind != sig::EvalResult::Error);

    h.start_capture();
    h.run_ticks(1000);

    auto out = h.get_captured(0);
    REQUIRE(dsp::all_finite(out));

    // All values must be within [0.3, 0.7]
    REQUIRE(dsp::is_within_range(out, 0.29, 0.71));

    // Should not be constant (the input isn't flat)
    REQUIRE_FALSE(dsp::is_constant(out, 0.01));
}

TEST_CASE("B4: Inversion", "[e2e][signal_chain]") {
    FirmwareTestHarness h;
    h.init();

    h.set_input_sine(2, 10.0, 0.5, 0.5);

    auto r = h.eval("(a1 (- 1.0 ain1))");
    REQUIRE(r.kind != sig::EvalResult::Error);

    h.start_capture();
    h.run_ticks(1000);

    auto out = h.get_captured(0);
    REQUIRE(dsp::all_finite(out));

    // Build the raw input for correlation
    auto expected_input = dsp::generate_sine(1000, 10.0, 1000.0, 0.5, 0.5);

    // Inverted signal should anti-correlate
    double corr = dsp::correlate(out, expected_input);
    REQUIRE(corr == Approx(-1.0).margin(0.05));
}

TEST_CASE("B5: Rectification — abs doubles frequency", "[e2e][signal_chain]") {
    FirmwareTestHarness h;
    h.init();

    // Sine centered at 0.5, so (- ain1 0.5) ranges [-0.5, 0.5]
    // abs of that ranges [0, 0.5], and has double the zero-crossings at the
    // abs midpoint.
    h.set_input_sine(2, 10.0, 0.5, 0.5);

    auto r = h.eval("(a1 (abs (- ain1 0.5)))");
    REQUIRE(r.kind != sig::EvalResult::Error);

    h.start_capture();
    h.run_ticks(1000);

    auto out = h.get_captured(0);
    REQUIRE(dsp::all_finite(out));

    // All values should be non-negative
    REQUIRE(dsp::min_value(out) >= -0.01);

    // Frequency should be doubled (20Hz from 10Hz input)
    // The rectified signal crosses the midpoint at double rate
    double freq = dsp::frequency_estimate(out, 1000.0, 0.25);
    REQUIRE(freq == Approx(20.0).margin(3.0));
}

// ============================================================================
// C. Temporal Accuracy [e2e][temporal]
// ============================================================================

TEST_CASE("C1: Beat phasor at 120 BPM — one beat in 0.5s", "[e2e][temporal]") {
    FirmwareTestHarness h;
    h.init();
    // Default BPM is 120 → one beat = 0.5 seconds = 500 ticks at 1kHz

    auto r = h.eval("(a1 beat)");
    REQUIRE(r.kind != sig::EvalResult::Error);

    h.start_capture();
    h.run_ticks(500); // exactly one beat

    auto out = h.get_captured(0);
    REQUIRE(dsp::all_finite(out));

    // Should be monotonically increasing (one full ramp within one beat)
    REQUIRE(dsp::is_monotonic_increasing(out, 0.005));

    // Should start near 0 and end near 1
    REQUIRE(out.front() == Approx(0.0).margin(0.05));
    REQUIRE(out.back() == Approx(1.0).margin(0.05));
}

TEST_CASE("C2: Beat phasor at 60 BPM — one beat in 1.0s", "[e2e][temporal]") {
    FirmwareTestHarness h;
    h.init();

    // Set BPM to 60
    auto r1 = h.eval("(set-bpm 60)");
    REQUIRE(r1.kind != sig::EvalResult::Error);

    auto r2 = h.eval("(a1 beat)");
    REQUIRE(r2.kind != sig::EvalResult::Error);

    h.start_capture();
    h.run_ticks(1000); // 1 second = 1 beat at 60 BPM

    auto out = h.get_captured(0);
    REQUIRE(dsp::all_finite(out));
    REQUIRE(dsp::is_monotonic_increasing(out, 0.005));
    REQUIRE(out.front() == Approx(0.0).margin(0.05));
    REQUIRE(out.back() == Approx(1.0).margin(0.05));
}

TEST_CASE("C3: Square wave from beat phasor", "[e2e][temporal]") {
    FirmwareTestHarness h;
    h.init();
    // 120 BPM → 1 beat = 500 ticks

    auto r = h.eval("(d1 (sqr beat))");
    REQUIRE(r.kind != sig::EvalResult::Error);

    h.start_capture();
    h.run_ticks(500);

    auto out = h.get_captured(8); // d1 = index 8
    REQUIRE(dsp::all_finite(out));

    // sqr of a 0→1 ramp should be high for first half, low for second half
    // (sqr returns 1 when phase < 0.5, 0 when phase >= 0.5)
    // Check first quarter is high
    REQUIRE(dsp::segment_is(out, 0, 200, 1.0, 0.1));
    // Check last quarter is low
    REQUIRE(dsp::segment_is(out, 300, 500, 0.0, 0.1));
}

TEST_CASE("C4: Bar phasor at 120 BPM (4/4)", "[e2e][temporal]") {
    FirmwareTestHarness h;
    h.init();
    // 120 BPM, 4/4 → one bar = 4 beats = 2 seconds = 2000 ticks

    auto r = h.eval("(a1 bar)");
    REQUIRE(r.kind != sig::EvalResult::Error);

    h.start_capture();
    h.run_ticks(2000);

    auto out = h.get_captured(0);
    REQUIRE(dsp::all_finite(out));
    REQUIRE(dsp::is_monotonic_increasing(out, 0.005));
    REQUIRE(out.front() == Approx(0.0).margin(0.05));
    REQUIRE(out.back() == Approx(1.0).margin(0.05));
}

TEST_CASE("C5: BPM change mid-run — second half advances faster", "[e2e][temporal]") {
    FirmwareTestHarness h;
    h.init();
    // Start at 120 BPM

    auto r = h.eval("(a1 beat)");
    REQUIRE(r.kind != sig::EvalResult::Error);

    h.start_capture();
    h.run_ticks(250); // first quarter-beat at 120 BPM

    // Change to 240 BPM (double speed)
    auto r2 = h.eval("(set-bpm 240)");
    REQUIRE(r2.kind != sig::EvalResult::Error);

    h.run_ticks(250); // at 240 BPM, 250 ticks = 1 beat

    auto out = h.get_captured(0);
    REQUIRE(dsp::all_finite(out));

    // The beat phasor at 120 BPM advances by beat_per_tick = (120/60)/1000 = 0.002
    // After 250 ticks at 120 BPM: phase ≈ 0.5
    // Then at 240 BPM, beat_per_tick = (240/60)/1000 = 0.004
    // After 250 more ticks: phase advances by 1.0 — wraps around

    // The key assertion: the second half should have a higher rate of change.
    // Measure average slope in each half.
    // First 250 samples vs last 250 samples.
    double sum_first = 0.0, sum_second = 0.0;
    for (size_t i = 1; i < 250; i++) {
        sum_first += (out[i] - out[i - 1]);
    }
    for (size_t i = 251; i < 500; i++) {
        sum_second += std::abs(out[i] - out[i - 1]);
    }
    // Second half average absolute slope should be roughly double
    double avg_first  = sum_first / 249.0;
    double avg_second = sum_second / 249.0;
    REQUIRE(avg_second > avg_first * 1.5);
}

// ============================================================================
// D. Euclidean & Rhythm Patterns [e2e][rhythm]
// ============================================================================

TEST_CASE("D1: Euclid 3/8 — 3 hits per 8 steps", "[e2e][rhythm]") {
    FirmwareTestHarness h;
    h.init();
    // 120 BPM → 1 beat = 500 ticks.
    // euclid args: (euclid total active phase)
    // 8 total steps, 3 active, driven by beat phasor.
    // Over one beat (one full cycle of the phasor), expect 3 rising edges
    // (each active step fires a gate with 50% pulse width).

    auto r = h.eval("(d1 (euclid 8 3 beat))");
    REQUIRE(r.kind != sig::EvalResult::Error);

    h.start_capture();
    h.run_ticks(500); // 1 beat = one full cycle

    auto out = h.get_captured(8); // d1 = index 8
    REQUIRE(dsp::all_finite(out));

    // Count rising edges — should be 3 (one per active step in the pattern)
    int edges = dsp::count_rising_edges(out, 0.5);
    REQUIRE(edges >= 2);
    REQUIRE(edges <= 4);
}

TEST_CASE("D2: Euclid 4/4 — all hits, 50% duty cycle gates", "[e2e][rhythm]") {
    FirmwareTestHarness h;
    h.init();

    // (euclid 4 4 beat) — 4 total, 4 active. Every step fires.
    // Default pulse width is 0.5, so each step is high for half its duration.
    // Over a full beat cycle: 4 gates, each 50% duty → overall DC ≈ 0.5
    auto r = h.eval("(d1 (euclid 4 4 beat))");
    REQUIRE(r.kind != sig::EvalResult::Error);

    h.start_capture();
    h.run_ticks(500); // 1 beat

    auto out = h.get_captured(8);
    REQUIRE(dsp::all_finite(out));

    // Should see 4 rising edges (one per step)
    int edges = dsp::count_rising_edges(out, 0.5);
    REQUIRE(edges >= 3);
    REQUIRE(edges <= 5);

    // DC offset should be approximately 0.5 (50% duty cycle)
    double dc = dsp::dc_offset(out);
    REQUIRE(dc == Approx(0.5).margin(0.1));
}

TEST_CASE("D3: Gates pattern [1 0 1 0]", "[e2e][rhythm]") {
    FirmwareTestHarness h;
    h.init();
    // 120 BPM → 1 beat = 500 ticks
    // gates [1 0 1 0] over one beat: 4 steps, each 125 ticks

    auto r = h.eval("(d1 (gates [1 0 1 0] beat))");
    REQUIRE(r.kind != sig::EvalResult::Error);

    h.start_capture();
    h.run_ticks(500); // one beat

    auto out = h.get_captured(8);
    REQUIRE(dsp::all_finite(out));

    // First quarter: high (step 0 = 1)
    REQUIRE(dsp::segment_is(out, 0, 100, 1.0, 0.15));
    // Second quarter: low (step 1 = 0)
    REQUIRE(dsp::segment_is(out, 150, 250, 0.0, 0.15));
    // Third quarter: high (step 2 = 1)
    REQUIRE(dsp::segment_is(out, 250, 350, 1.0, 0.15));
    // Fourth quarter: low (step 3 = 0)
    REQUIRE(dsp::segment_is(out, 400, 500, 0.0, 0.15));
}

// ============================================================================
// E. Cross-Output References [e2e][cross_output]
// ============================================================================
// NOTE: Cross-output references (reading one output from another, e.g.
// "(a2 a1)") are not currently exposed in the ModuLisp surface syntax.
// The PrevOutputLoad node type exists in the node pool but has no user-facing
// syntax. These tests are skipped until the language adds cross-output refs.

// ============================================================================
// F. Dependency Recompilation [e2e][dependency]
// ============================================================================

TEST_CASE("F1: Define + set — frequency change via cell update", "[e2e][dependency]") {
    FirmwareTestHarness h;
    h.init();

    // Define a cell and use it in an output expression
    auto r1 = h.eval("(define freq 5)");
    REQUIRE(r1.kind != sig::EvalResult::Error);

    auto r2 = h.eval("(a1 (usin (* beat freq)))");
    REQUIRE(r2.kind != sig::EvalResult::Error);

    h.start_capture();
    h.run_ticks(1000); // 2 beats at 120 BPM
    auto buf1 = h.get_captured(0);

    // Change the cell value
    auto r3 = h.eval("(set freq 10)");
    REQUIRE(r3.kind != sig::EvalResult::Error);

    h.start_capture();
    h.run_ticks(1000);
    auto buf2 = h.get_captured(0);

    REQUIRE(dsp::all_finite(buf1));
    REQUIRE(dsp::all_finite(buf2));

    // The second buffer should have roughly double the frequency of the first.
    // Use crossing count as a proxy.
    int crossings1 = dsp::count_crossings(buf1, 0.5);
    int crossings2 = dsp::count_crossings(buf2, 0.5);

    // With doubled freq, expect roughly double the crossings
    REQUIRE(crossings2 > crossings1);
}

TEST_CASE("F2: Multiple dependents — amplitude cell affects two outputs", "[e2e][dependency]") {
    FirmwareTestHarness h;
    h.init();

    auto r1 = h.eval("(define amp 0.5)");
    REQUIRE(r1.kind != sig::EvalResult::Error);

    auto r2 = h.eval("(a1 (* amp (usin beat)))");
    REQUIRE(r2.kind != sig::EvalResult::Error);

    auto r3 = h.eval("(a2 (* amp (ucos beat)))");
    REQUIRE(r3.kind != sig::EvalResult::Error);

    h.start_capture();
    h.run_ticks(500);
    auto buf1_a1 = h.get_captured(0);
    auto buf1_a2 = h.get_captured(1);
    double pp1_a1 = dsp::peak_to_peak(buf1_a1);
    double pp1_a2 = dsp::peak_to_peak(buf1_a2);

    // Halve the amplitude
    auto r4 = h.eval("(set amp 0.25)");
    REQUIRE(r4.kind != sig::EvalResult::Error);

    h.start_capture();
    h.run_ticks(500);
    auto buf2_a1 = h.get_captured(0);
    auto buf2_a2 = h.get_captured(1);
    double pp2_a1 = dsp::peak_to_peak(buf2_a1);
    double pp2_a2 = dsp::peak_to_peak(buf2_a2);

    REQUIRE(dsp::all_finite(buf1_a1));
    REQUIRE(dsp::all_finite(buf2_a1));
    REQUIRE(dsp::all_finite(buf1_a2));
    REQUIRE(dsp::all_finite(buf2_a2));

    // Both outputs' peak-to-peak should roughly halve
    REQUIRE(pp2_a1 == Approx(pp1_a1 * 0.5).margin(0.1));
    REQUIRE(pp2_a2 == Approx(pp1_a2 * 0.5).margin(0.1));
}

// ============================================================================
// G. Bar Quantization — REMOVED
// ============================================================================
// The pending_commands ring buffer was dead code (wire-protocol.md §10.1,
// firmware.md §6.2). It has been removed. Future in-language quantisation
// (firmware.md §6.3) will land separately and add new tests here.
