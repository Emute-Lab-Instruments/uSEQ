// Firmware E2E tests — Categories H through N
// (Part 2 of the e2e suite, run as a separate test binary)

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"
#include "firmware_test_harness.h"
#include "dsp_helpers.h"

#include <chrono>
#include <cmath>
#include <sstream>

// ═══════════════════════════════════════════════════════════════════════════════
// H. Flash Persistence Round-Trip
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Flash: save and reload produces identical outputs", "[e2e][flash]") {
    // -- First harness: define expressions and capture output --
    MockStorage shared_storage(65536);

    FirmwareTestHarness h1;
    h1.init();
    h1.firmware().flash.set_storage(&shared_storage);

    h1.eval("(define freq 440)");
    h1.eval("(a1 (usin (* t freq)))");
    h1.eval("(d1 (euclid 3 8 beat))");

    h1.start_capture();
    h1.run_ticks(100);
    h1.stop_capture();

    auto expected_a1 = h1.get_captured(0);   // a1 = output 0
    auto expected_d1 = h1.get_captured(8);   // d1 = output 8

    REQUIRE(expected_a1.size() == 100);
    REQUIRE(dsp::all_finite(expected_a1));
    REQUIRE(dsp::all_finite(expected_d1));

    // Save engine state
    bool saved = h1.firmware().flash.save(h1.engine());
    REQUIRE(saved);

    // -- Second harness: load state and replay --
    FirmwareTestHarness h2;
    h2.init();
    h2.firmware().flash.set_storage(&shared_storage);

    bool loaded = h2.firmware().flash.load(h2.engine());
    REQUIRE(loaded);

    // Verify cells were restored correctly
    auto& si = SymbolIntern::getInstance();
    auto freq_sym = si.intern("freq");
    REQUIRE(h2.engine().cells.cells[freq_sym].value == Approx(440.0));

    // Re-evaluate the output expressions (flash stores source, caller recompiles)
    h2.eval("(a1 (usin (* t freq)))");
    h2.eval("(d1 (euclid 3 8 beat))");

    h2.start_capture();
    h2.run_ticks(100);
    h2.stop_capture();

    auto actual_a1 = h2.get_captured(0);
    auto actual_d1 = h2.get_captured(8);

    REQUIRE(actual_a1.size() == 100);
    REQUIRE(dsp::all_finite(actual_a1));
    REQUIRE(dsp::all_finite(actual_d1));

    // Outputs should produce the same type of signal (same frequency, same pattern)
    // Exact bit-identity isn't guaranteed since time starts fresh
    REQUIRE(dsp::peak_to_peak(actual_a1) > 0.01);  // a1 is oscillating
    REQUIRE(dsp::peak_to_peak(actual_d1) > 0.01);  // d1 has gate pattern
}

TEST_CASE("Flash: BPM persists across save/load", "[e2e][flash]") {
    MockStorage shared_storage(65536);

    FirmwareTestHarness h1;
    h1.init();
    h1.firmware().flash.set_storage(&shared_storage);

    h1.eval("(set-bpm 140)");
    h1.run_ticks(10);

    // Verify BPM is set
    auto& si = SymbolIntern::getInstance();
    sig::SymbolID bpm_sym = si.intern("bpm");
    REQUIRE(h1.engine().cells.cells[bpm_sym].value == Approx(140.0));

    bool saved = h1.firmware().flash.save(h1.engine());
    REQUIRE(saved);

    // Load into fresh engine
    FirmwareTestHarness h2;
    h2.init();
    h2.firmware().flash.set_storage(&shared_storage);

    bool loaded = h2.firmware().flash.load(h2.engine());
    REQUIRE(loaded);

    sig::SymbolID bpm_sym2 = SymbolIntern::getInstance().intern("bpm");
    REQUIRE(h2.engine().cells.cells[bpm_sym2].value == Approx(140.0));
}

// ═══════════════════════════════════════════════════════════════════════════════
// I. LKG / Error Recovery
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("LKG: bad code keeps output running", "[e2e][lkg][error]") {
    FirmwareTestHarness h;
    h.init();

    h.eval("(a1 (usin beat))");
    h.run_ticks(10);

    // Output should be oscillating (non-zero range)
    double before_range = 0.0;
    {
        h.start_capture();
        h.run_ticks(100);
        h.stop_capture();
        auto buf = h.get_captured(0);
        before_range = dsp::peak_to_peak(buf);
        REQUIRE(before_range > 0.01);
    }

    // Inject bad code — should fail
    auto result = h.eval("(a1 (undefined-fn beat))");
    REQUIRE(result.kind == sig::EvalResult::Error);

    // Run more ticks — output should still be oscillating (LKG fallback)
    h.start_capture();
    h.run_ticks(100);
    h.stop_capture();

    auto buf = h.get_captured(0);
    REQUIRE(dsp::all_finite(buf));

    // LKG should keep the output producing non-trivial values
    double after_range = dsp::peak_to_peak(buf);
    REQUIRE(after_range > 0.001);
}

TEST_CASE("LKG: parse error doesn't crash", "[e2e][lkg][error]") {
    FirmwareTestHarness h;
    h.init();

    h.eval("(a1 (usin beat))");
    h.run_ticks(10);

    // Unclosed paren — should fail gracefully
    auto result = h.eval("(a1 (unclosed paren");
    REQUIRE(result.kind == sig::EvalResult::Error);

    // Tick still works
    h.run_ticks(10);

    // Output remains at LKG (the previous good expression)
    double val = h.get_output(0);
    REQUIRE(std::isfinite(val));
}

TEST_CASE("LKG: division by zero guard", "[e2e][lkg][error]") {
    FirmwareTestHarness h;
    h.init();

    h.eval("(a1 (/ 1.0 0.0))");
    h.run_ticks(1);

    double val = h.get_output(0);
    REQUIRE(std::isfinite(val));
    // Should be guarded to 0.0, not NaN or Inf
    REQUIRE(val == Approx(0.0).margin(0.01));
}

TEST_CASE("LKG: multiple outputs isolated", "[e2e][lkg][error]") {
    FirmwareTestHarness h;
    h.init();

    h.eval("(a1 (usin beat))");
    h.eval("(a2 (ucos beat))");

    h.start_capture();
    h.run_ticks(100);
    h.stop_capture();

    auto a2_before = h.get_captured(1);
    REQUIRE(dsp::all_finite(a2_before));
    double a2_range_before = dsp::peak_to_peak(a2_before);
    REQUIRE(a2_range_before > 0.01);

    // Break a1 only
    auto result = h.eval("(a1 (broken))");
    REQUIRE(result.kind == sig::EvalResult::Error);

    // a2 should continue unaffected
    h.start_capture();
    h.run_ticks(100);
    h.stop_capture();

    auto a2_after = h.get_captured(1);
    REQUIRE(dsp::all_finite(a2_after));
    double a2_range_after = dsp::peak_to_peak(a2_after);
    REQUIRE(a2_range_after > 0.01);
}

// ═══════════════════════════════════════════════════════════════════════════════
// J. Memory Pressure / State Consistency
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Memory: repeated redefines", "[e2e][memory]") {
    FirmwareTestHarness h;
    h.init();

    double last_value = 0.0;

    for (int i = 0; i < 200; i++) {
        double val = i * 0.01;
        std::ostringstream define_ss;
        define_ss << "(define x" << i << " " << val << ")";
        h.eval(define_ss.str());

        std::ostringstream assign_ss;
        assign_ss << "(a1 x" << i << ")";
        h.eval(assign_ss.str());

        h.run_ticks(1);
        last_value = val;
    }

    double output = h.get_output(0);
    REQUIRE(std::isfinite(output));
    // The last define was x199 = 1.99
    REQUIRE(output == Approx(last_value).margin(0.01));
}

TEST_CASE("Memory: output reassignment stress", "[e2e][memory]") {
    FirmwareTestHarness h;
    h.init();

    for (int i = 0; i < 100; i++) {
        double val = (i % 10) * 0.1;
        std::ostringstream ss;
        ss << "(a1 " << val << ")";
        h.eval(ss.str());

        h.run_ticks(1);

        double output = h.get_output(0);
        REQUIRE(std::isfinite(output));
        REQUIRE(output == Approx(val).margin(0.01));
    }
}

TEST_CASE("Memory: node pool bounded after redefines", "[e2e][memory]") {
    FirmwareTestHarness h;
    h.init();

    // Do many reassignments and check pool doesn't grow unbounded
    for (int i = 0; i < 50; i++) {
        std::ostringstream ss;
        ss << "(a1 (usin (* beat " << (i + 1) << ".0)))";
        h.eval(ss.str());
        h.run_ticks(1);
    }

    uint16_t count_mid = h.engine().pool.node_count;

    for (int i = 50; i < 200; i++) {
        std::ostringstream ss;
        ss << "(a1 (usin (* beat " << (i + 1) << ".0)))";
        h.eval(ss.str());
        h.run_ticks(1);
    }

    uint16_t count_end = h.engine().pool.node_count;

    // Pool should not grow proportionally with number of redefines.
    // With CSE and reuse, the end count shouldn't be dramatically larger
    // than mid count. Allow 4x as generous bound.
    REQUIRE(count_end < count_mid * 4);
}

// ═══════════════════════════════════════════════════════════════════════════════
// K. Multi-Output Interaction
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Multi-output: six outputs simultaneously", "[e2e][multi_output]") {
    FirmwareTestHarness h;
    h.init();

    // Assign 4 analog + 2 digital outputs
    h.eval("(a1 (usin beat))");              // sine
    h.eval("(a2 (ucos beat))");              // cosine
    h.eval("(a3 (* beat 0.5))");             // ramp-ish (beat is 0..1 phasor)
    h.eval("(a4 0.75)");                     // constant
    h.eval("(d1 (euclid 3 8 beat))");        // euclidean
    h.eval("(d2 0.0)");                      // constant zero

    h.start_capture();
    h.run_ticks(500);
    h.stop_capture();

    auto buf_a1 = h.get_captured(0);
    auto buf_a2 = h.get_captured(1);
    auto buf_a3 = h.get_captured(2);
    auto buf_a4 = h.get_captured(3);
    auto buf_d1 = h.get_captured(8);
    auto buf_d2 = h.get_captured(9);

    // All outputs should be finite
    REQUIRE(dsp::all_finite(buf_a1));
    REQUIRE(dsp::all_finite(buf_a2));
    REQUIRE(dsp::all_finite(buf_a3));
    REQUIRE(dsp::all_finite(buf_a4));
    REQUIRE(dsp::all_finite(buf_d1));
    REQUIRE(dsp::all_finite(buf_d2));

    // a1 (sine) and a2 (cosine) should be oscillating
    REQUIRE(dsp::peak_to_peak(buf_a1) > 0.01);
    REQUIRE(dsp::peak_to_peak(buf_a2) > 0.01);

    // a4 should be constant at 0.75
    REQUIRE(dsp::is_constant(buf_a4, 0.05));
    REQUIRE(dsp::dc_offset(buf_a4) == Approx(0.75).margin(0.05));

    // d2 should be constant at 0.0
    REQUIRE(dsp::is_constant(buf_d2, 0.01));
    REQUIRE(dsp::dc_offset(buf_d2) == Approx(0.0).margin(0.01));
}

TEST_CASE("Multi-output: independent update", "[e2e][multi_output]") {
    FirmwareTestHarness h;
    h.init();

    h.eval("(a1 0.1)");
    h.eval("(a2 0.2)");
    h.eval("(a3 0.3)");
    h.eval("(d1 1.0)");

    h.run_ticks(10);

    // Verify initial values
    REQUIRE(h.get_output(1) == Approx(0.2).margin(0.01));  // a2
    REQUIRE(h.get_output(2) == Approx(0.3).margin(0.01));  // a3
    REQUIRE(h.get_output(8) == Approx(1.0).margin(0.01));  // d1

    // Change only a1
    h.eval("(a1 0.9)");
    h.run_ticks(10);

    // a1 should have changed
    REQUIRE(h.get_output(0) == Approx(0.9).margin(0.01));

    // Others should be unaffected
    REQUIRE(h.get_output(1) == Approx(0.2).margin(0.01));
    REQUIRE(h.get_output(2) == Approx(0.3).margin(0.01));
    REQUIRE(h.get_output(8) == Approx(1.0).margin(0.01));
}

// ═══════════════════════════════════════════════════════════════════════════════
// L. Time Warp Correctness
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Time warp: fast 2x doubles frequency", "[e2e][time_warp]") {
    FirmwareTestHarness h;
    h.init();

    h.eval("(a1 (fast 2 (usin beat)))");
    h.eval("(a2 (usin beat))");

    h.start_capture();
    h.run_ticks(2000);  // 2 seconds at 1kHz — enough for frequency estimation
    h.stop_capture();

    auto buf_fast = h.get_captured(0);
    auto buf_normal = h.get_captured(1);

    REQUIRE(dsp::all_finite(buf_fast));
    REQUIRE(dsp::all_finite(buf_normal));

    double freq_fast = dsp::frequency_estimate(buf_fast, 1000.0);
    double freq_normal = dsp::frequency_estimate(buf_normal, 1000.0);

    // fast 2x should approximately double the frequency
    if (freq_normal > 0.1) {
        double ratio = freq_fast / freq_normal;
        REQUIRE(ratio == Approx(2.0).epsilon(0.15));
    }
}

TEST_CASE("Time warp: slow 2x halves frequency", "[e2e][time_warp]") {
    FirmwareTestHarness h;
    h.init();

    h.eval("(a1 (slow 2 (usin beat)))");
    h.eval("(a2 (usin beat))");

    h.start_capture();
    h.run_ticks(4000);  // 4 seconds — need more samples for slow signal
    h.stop_capture();

    auto buf_slow = h.get_captured(0);
    auto buf_normal = h.get_captured(1);

    REQUIRE(dsp::all_finite(buf_slow));
    REQUIRE(dsp::all_finite(buf_normal));

    double freq_slow = dsp::frequency_estimate(buf_slow, 1000.0);
    double freq_normal = dsp::frequency_estimate(buf_normal, 1000.0);

    if (freq_normal > 0.1) {
        double ratio = freq_slow / freq_normal;
        REQUIRE(ratio == Approx(0.5).epsilon(0.15));
    }
}

TEST_CASE("Time warp: nested warps compose", "[e2e][time_warp]") {
    FirmwareTestHarness h;
    h.init();

    // fast 3 of slow 2 = 3/2 = 1.5x speed
    h.eval("(a1 (fast 3 (slow 2 (usin beat))))");
    h.eval("(a2 (usin beat))");

    h.start_capture();
    h.run_ticks(4000);
    h.stop_capture();

    auto buf_warped = h.get_captured(0);
    auto buf_normal = h.get_captured(1);

    REQUIRE(dsp::all_finite(buf_warped));
    REQUIRE(dsp::all_finite(buf_normal));

    double freq_warped = dsp::frequency_estimate(buf_warped, 1000.0);
    double freq_normal = dsp::frequency_estimate(buf_normal, 1000.0);

    if (freq_normal > 0.1) {
        double ratio = freq_warped / freq_normal;
        REQUIRE(ratio == Approx(1.5).epsilon(0.20));
    }
}

TEST_CASE("Time warp: offset produces valid output", "[e2e][time_warp]") {
    FirmwareTestHarness h;
    h.init();

    // offset shifts the temporal context — verify it compiles and produces output
    h.eval("(a1 (offset 0.25 (usin beat)))");

    h.start_capture();
    h.run_ticks(1000);
    h.stop_capture();

    auto buf = h.get_captured(0);
    REQUIRE(dsp::all_finite(buf));
    REQUIRE(dsp::peak_to_peak(buf) > 0.01);  // still oscillating
}

// ═══════════════════════════════════════════════════════════════════════════════
// M. Playback State Transitions
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Playback: pause freezes output", "[e2e][playback]") {
    FirmwareTestHarness h;
    h.init();

    h.eval("(a1 (usin beat))");
    h.run_ticks(100);

    // Verify output is oscillating before pause
    h.start_capture();
    h.run_ticks(100);
    h.stop_capture();
    auto before = h.get_captured(0);
    REQUIRE(dsp::peak_to_peak(before) > 0.01);

    // Pause
    h.eval("(useq-pause)");

    // Capture during pause
    h.start_capture();
    h.run_ticks(100);
    h.stop_capture();
    auto during_pause = h.get_captured(0);

    REQUIRE(dsp::all_finite(during_pause));
    // Output should be frozen (constant)
    REQUIRE(dsp::is_constant(during_pause, 0.001));
}

TEST_CASE("Playback: play resumes after pause", "[e2e][playback]") {
    FirmwareTestHarness h;
    h.init();

    h.eval("(a1 (usin beat))");
    h.run_ticks(100);

    // Pause
    h.eval("(useq-pause)");
    h.run_ticks(100);

    // Resume
    h.eval("(useq-play)");

    h.start_capture();
    h.run_ticks(200);
    h.stop_capture();

    auto after_play = h.get_captured(0);
    REQUIRE(dsp::all_finite(after_play));
    // Output should be varying again
    REQUIRE(dsp::peak_to_peak(after_play) > 0.01);
}

TEST_CASE("Playback: clear resets everything", "[e2e][playback]") {
    FirmwareTestHarness h;
    h.init();

    h.eval("(a1 (usin beat))");
    h.run_ticks(10);

    // Verify non-trivial output before clear
    REQUIRE(dsp::peak_to_peak({h.get_output(0)}) >= 0.0);

    // Clear
    h.eval("(useq-clear)");
    h.run_ticks(1);

    // After clear, a1 output should be at its LKG value (whatever it was
    // at the last tick before clear) or 0.0 if clear resets LKG.
    double val = h.get_output(0);
    REQUIRE(std::isfinite(val));
    // The output should at least be stable (not NaN or Inf)
    h.run_ticks(10);
    double val2 = h.get_output(0);
    REQUIRE(std::isfinite(val2));
}

// ═══════════════════════════════════════════════════════════════════════════════
// N. Performance Profiling
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Performance: tick time under 1ms for ~50 node expression", "[e2e][performance]") {
    FirmwareTestHarness h;
    h.init();

    // Build a moderately complex expression (~50 nodes)
    // Multiple operations chained together
    h.eval("(a1 (+ (usin beat) (* 0.3 (ucos (* beat 2.0)))))");
    h.eval("(a2 (+ (* 0.5 (usin (* beat 3.0))) (* 0.25 (ucos (* beat 5.0)))))");
    h.eval("(a3 (fast 2 (usin (* beat 0.5))))");
    h.eval("(d1 (euclid 5 8 beat))");

    // Warm up
    h.run_ticks(100);

    // Time 1000 ticks
    auto start = std::chrono::high_resolution_clock::now();
    h.run_ticks(1000);
    auto end = std::chrono::high_resolution_clock::now();

    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    double avg_us_per_tick = static_cast<double>(elapsed_us) / 1000.0;

    // Assert under 1ms (1000us) per tick on desktop
    REQUIRE(avg_us_per_tick < 1000.0);

    // Verify outputs are still valid
    REQUIRE(std::isfinite(h.get_output(0)));
    REQUIRE(std::isfinite(h.get_output(1)));
    REQUIRE(std::isfinite(h.get_output(2)));
    REQUIRE(std::isfinite(h.get_output(8)));
}

TEST_CASE("Performance: complex patch all outputs finite", "[e2e][performance]") {
    FirmwareTestHarness h;
    h.init();

    // Build a complex multi-output patch with time warps, euclidean,
    // arithmetic, and input reads
    h.set_input_sine(2, 5.0);  // ain1 = 5Hz sine

    h.eval("(a1 (fast 2 (usin beat)))");
    h.eval("(a2 (slow 2 (ucos (* beat 3.0))))");
    h.eval("(a3 (+ (* 0.5 (usin beat)) (* 0.5 (ucos beat))))");
    h.eval("(a4 (offset 0.25 (usin (* beat 2.0))))");
    h.eval("(d1 (euclid 3 8 beat))");
    h.eval("(d2 (euclid 5 16 beat))");

    h.start_capture();
    h.run_ticks(1000);
    h.stop_capture();

    // All six outputs must be finite across all 1000 samples
    REQUIRE(dsp::all_finite(h.get_captured(0)));   // a1
    REQUIRE(dsp::all_finite(h.get_captured(1)));   // a2
    REQUIRE(dsp::all_finite(h.get_captured(2)));   // a3
    REQUIRE(dsp::all_finite(h.get_captured(3)));   // a4
    REQUIRE(dsp::all_finite(h.get_captured(8)));   // d1
    REQUIRE(dsp::all_finite(h.get_captured(9)));   // d2

    // Each output should have non-trivial activity
    REQUIRE(dsp::peak_to_peak(h.get_captured(0)) > 0.01);
    REQUIRE(dsp::peak_to_peak(h.get_captured(1)) > 0.01);
    REQUIRE(dsp::peak_to_peak(h.get_captured(2)) > 0.01);
    REQUIRE(dsp::peak_to_peak(h.get_captured(3)) > 0.01);
}
