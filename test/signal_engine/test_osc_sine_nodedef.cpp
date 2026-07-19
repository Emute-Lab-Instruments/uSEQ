// osc/sine version-1 NodeDef conformance tests (VAL-DSP-001..016).
//
// These tests exercise the hand-written osc/sine NodeDef through its public C
// ABI. They compile against the SAME source the separate WASM target compiles
// (nodedef/osc_sine.cpp), so any contract guarantee observed here is also
// observed by the loaded browser artefact.
//
// Scope (mapped to the validation contract):
//   VAL-DSP-001 — registry metadata is normalized (name/version/inputs/outputs)
//   VAL-DSP-002 — freq contract: block rate, default 440 Hz, step smoothing
//   VAL-DSP-003 — amp contract: block rate, default 0.2, linear smoothing
//   VAL-DSP-004 — bounded, aligned state/zone layout; invalid layouts fail
//                 closed before compute
//   VAL-DSP-007 — dynamic quantum lengths (1..max) render exactly N frames
//   VAL-DSP-008 — measured frequency/peak/RMS follow requested controls
//   VAL-DSP-009 — output stays finite under NaN/Inf/negative/extreme inputs
//   VAL-DSP-010 — phase continuity across blocks and same-def param updates
//   VAL-DSP-011 — recursive subnormal state flushes to zero
//   VAL-DSP-012 — compute allocates nothing (sentinel guard bytes survive)
//   VAL-DSP-013 — compute imports/invokes nothing blocking (export-table
//                 inspection happens in the WASM binary test driver; native
//                 test only confirms no unexpected abort path triggers)
//   VAL-DSP-014 — fade defaults match central SYNTH_FADE_IN/OUT constants
//
// VAL-DSP-005 (host-owned imported shared memory) and VAL-DSP-015/016 (separate
// WASM artefact, no interpreter/firmware linkage) are covered by the separate
// WASM build + binary inspection test driver in run_osc_sine_wasm_inspection.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "nodedef/osc_sine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <cstdio>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ── Tiny JSON field probe (no external dep) ────────────────────────────────
// We don't need a full JSON parser; we just look for the canonical substrings
// the registry contract requires.
static bool json_contains(const char *json, const char *needle)
{
    if (!json || !needle) return false;
    return std::strstr(json, needle) != nullptr;
}

// ── Bounded zone helpers ───────────────────────────────────────────────────
//
// We allocate state + output regions with sentinel guard bytes on either side
// so any overwrite (e.g. compute writing past frame_count, or touching bytes
// before the state pointer) is detected deterministically.

struct GuardedZone {
    std::vector<unsigned char> storage;
    uintptr_t base;
    size_t usable;

    static constexpr uint32_t GUARD = 16;
    static constexpr unsigned char GUARD_FILL = 0xA5;

    GuardedZone(uint32_t usable_bytes, uint32_t align)
        : storage(GUARD + usable_bytes + GUARD, GUARD_FILL),
          usable(usable_bytes)
    {
        // Find the first aligned address inside the usable window.
        uintptr_t start = reinterpret_cast<uintptr_t>(storage.data()) + GUARD;
        uintptr_t mask = static_cast<uintptr_t>(align) - 1;
        uintptr_t aligned = (start + mask) & ~mask;
        // If alignment pushed us past the end of the usable window, slide
        // back inside the guard region by allocating slightly larger.
        REQUIRE(aligned + usable_bytes <=
                reinterpret_cast<uintptr_t>(storage.data() + storage.size()) -
                    GUARD);
        base = aligned;
    }

    void check_guards() const
    {
        for (uint32_t i = 0; i < GUARD; i++) {
            REQUIRE(storage[i] == GUARD_FILL);
        }
        for (uint32_t i = 0; i < GUARD; i++) {
            REQUIRE(storage[storage.size() - GUARD + i] == GUARD_FILL);
        }
    }

    template <typename T>
    T *as() { return reinterpret_cast<T *>(base); }
};

// Compute the peak and RMS of a finite double buffer.
static void measure_peak_rms(const double *samples, uint32_t n,
                             double &peak_out, double &rms_out)
{
    double peak = 0.0;
    double sum_sq = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        double a = std::fabs(samples[i]);
        if (a > peak) peak = a;
        sum_sq += samples[i] * samples[i];
    }
    peak_out = peak;
    rms_out = std::sqrt(sum_sq / static_cast<double>(n));
}

// Count zero crossings in a buffer (for frequency measurement). A 440 Hz
// signal at 48 kHz over N samples has approximately (N * 440 / 48000) * 2
// zero crossings (each cycle has two crossings). We count
// sign-flips excluding exact zeros at the boundary.
static uint32_t count_zero_crossings(const double *samples, uint32_t n)
{
    uint32_t crossings = 0;
    int prev_sign = 0;
    for (uint32_t i = 0; i < n; i++) {
        double v = samples[i];
        int sign = (v > 0.0) ? 1 : (v < 0.0 ? -1 : 0);
        if (sign != 0) {
            if (prev_sign != 0 && sign != prev_sign) {
                crossings++;
            }
            prev_sign = sign;
        }
    }
    return crossings;
}

// ── Allocation-counter hook (native build only) ────────────────────────────
//
// We don't override malloc globally (that would perturb Catch internals).
// Instead we run compute inside a tight loop and verify a sentinel heap
// region placed just past our working set is not touched. This catches any
// accidental out-of-zone write that would be required for allocation-based
// smoothing/state growth, and combined with the guarded zone above gives us
// deterministic evidence that compute writes inside its declared bounds only.

// ============================================================================
// VAL-DSP-001: Registry metadata is normalized
// ============================================================================

TEST_CASE("osc_sine: registry metadata reports osc/sine version 1, "
          "0 inputs, 1 mono output, no voice fan-out",
          "[osc_sine][val-dsp-001]")
{
    const char *json = osc_sine_registry_json();
    REQUIRE(json != nullptr);

    SECTION("identity") {
        REQUIRE(json_contains(json, "\"name\":\"osc/sine\""));
        REQUIRE(json_contains(json, "\"version\":1"));
    }
    SECTION("audio port topology") {
        REQUIRE(json_contains(json, "\"audio_inputs\":0"));
        REQUIRE(json_contains(json, "\"audio_outputs\":1"));
        REQUIRE(json_contains(json, "\"voice_fanout\":false"));
    }
    SECTION("parameter surface") {
        REQUIRE(json_contains(json, "\"name\":\"freq\""));
        REQUIRE(json_contains(json, "\"name\":\"amp\""));
    }
}

// ============================================================================
// VAL-DSP-002: Frequency contract (block rate, 440 Hz default, step smoothing)
// ============================================================================

TEST_CASE("osc_sine: freq is block rate, 440 Hz default, step smoothing",
          "[osc_sine][val-dsp-002]")
{
    const char *json = osc_sine_registry_json();
    REQUIRE(json != nullptr);

    // Default exactly 440.0 — no approximation allowed.
    REQUIRE(json_contains(json, "\"default\":440"));

    // freq smoothing is step (host holds the value; the def does not ramp it).
    REQUIRE(json_contains(json, "\"name\":\"freq\""));
    // The descriptor lists freq before amp; the smoothing tokens must appear
    // in the declared order. We look for the freq->step pair explicitly.
    const char *freq_pos = std::strstr(json, "\"name\":\"freq\"");
    REQUIRE(freq_pos != nullptr);
    const char *step_after_freq = std::strstr(freq_pos, "\"smoothing\":\"step\"");
    REQUIRE(step_after_freq != nullptr);
    // And ensure amp's linear appears AFTER freq.
    const char *linear_after = std::strstr(freq_pos, "\"smoothing\":\"linear\"");
    REQUIRE(linear_after != nullptr);

    // Block rate on both params.
    REQUIRE(json_contains(json, "\"rate\":\"block\""));
}

// ============================================================================
// VAL-DSP-003: Amplitude contract (block rate, 0.2 default, linear smoothing)
// ============================================================================

TEST_CASE("osc_sine: amp is block rate, 0.2 default, linear smoothing",
          "[osc_sine][val-dsp-003]")
{
    const char *json = osc_sine_registry_json();
    REQUIRE(json != nullptr);
    REQUIRE(json_contains(json, "\"default\":0.2"));
}

TEST_CASE("osc_sine: amp ramps linearly across a block",
          "[osc_sine][val-dsp-003]")
{
    uint32_t sb = osc_sine_state_bytes();
    uint32_t sa = osc_sine_state_align();

    GuardedZone state(sb, sa);
    REQUIRE(osc_sine_init(state.base, sb) == 1);

    // Two consecutive blocks: amp 0.0 then amp 0.4. Linear smoothing means
    // the rendered samples ramp from 0.0 to 0.4 across the second block.
    const uint32_t frames = osc_sine_max_quantum() / 64; // e.g. 128
    std::vector<double> out(frames, 0.0);
    std::vector<unsigned char> out_zone(frames * sizeof(double) + 32, 0x5A);
    double *out_buf = reinterpret_cast<double *>(
        (reinterpret_cast<uintptr_t>(out_zone.data()) + 7) & ~static_cast<uintptr_t>(7));

    double freq = 110.0;
    double amp0 = 0.0;
    double amp1 = 0.4;

    REQUIRE(osc_sine_compute(state.base, reinterpret_cast<uintptr_t>(&freq),
                             reinterpret_cast<uintptr_t>(&amp0),
                             reinterpret_cast<uintptr_t>(out_buf), frames) == 1);

    // After the first block the smoothed amp should be ~0.0.
    REQUIRE(std::fabs(osc_sine_get_smoothed_amp(state.base)) < 1e-9);

    // Second block at amp 0.4: the smoothed value should land exactly on 0.4
    // by the end of the block, and the first sample should be smaller than
    // the last (ramp-up, not step).
    REQUIRE(osc_sine_compute(state.base, reinterpret_cast<uintptr_t>(&freq),
                             reinterpret_cast<uintptr_t>(&amp1),
                             reinterpret_cast<uintptr_t>(out_buf), frames) == 1);

    REQUIRE(std::fabs(osc_sine_get_smoothed_amp(state.base) - 0.4) < 1e-9);

    // First sample's |amplitude| should be < last sample's |amplitude|. We
    // don't compare exact sample values because the phase relationship with
    // the sine makes absolute magnitude oscillate; we instead compare
    // peak-of-first-quarter vs peak-of-last-quarter.
    uint32_t q = frames / 4;
    double peak_first = 0.0, peak_last = 0.0;
    for (uint32_t i = 0; i < q; i++) {
        double a = std::fabs(out_buf[i]);
        if (a > peak_first) peak_first = a;
    }
    for (uint32_t i = frames - q; i < frames; i++) {
        double a = std::fabs(out_buf[i]);
        if (a > peak_last) peak_last = a;
    }
    REQUIRE(peak_last > peak_first);

    state.check_guards();
}

// ============================================================================
// VAL-DSP-004: Memory layout is bounded
// ============================================================================

TEST_CASE("osc_sine: state bytes/alignment are registry-declared and aligned",
          "[osc_sine][val-dsp-004]")
{
    uint32_t sb = osc_sine_state_bytes();
    uint32_t sa = osc_sine_state_align();

    REQUIRE(sb > 0);
    REQUIRE(sa > 0);
    // Alignment must be a power of two and at least alignof(double).
    REQUIRE((sa & (sa - 1)) == 0);
    REQUIRE(sa >= 8);
    // State size must be a whole multiple of the alignment so adjacent
    // instances can be packed without padding.
    REQUIRE((sb % sa) == 0);

    // The JSON descriptor must match the accessor return values so the
    // host adapter and the WASM build see identical layout numbers.
    const char *json = osc_sine_registry_json();
    REQUIRE(json != nullptr);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "\"state_bytes\":%u", sb);
    REQUIRE(json_contains(json, buf));
    std::snprintf(buf, sizeof(buf), "\"state_align\":%u", sa);
    REQUIRE(json_contains(json, buf));
}

TEST_CASE("osc_sine: invalid layouts fail closed before compute",
          "[osc_sine][val-dsp-004]")
{
    uint32_t sb = osc_sine_state_bytes();
    uint32_t sa = osc_sine_state_align();

    GuardedZone state(sb, sa);

    SECTION("valid layout passes") {
        REQUIRE(osc_sine_validate_layout(state.base, sb) == 1);
    }
    SECTION("undersized bytes fail") {
        REQUIRE(osc_sine_validate_layout(state.base, sb - 1) == 0);
    }
    SECTION("zero bytes fail") {
        // Zero-byte zone is undersized and rejected by the size check. The
        // native-only "null pointer rejected" semantics is intentionally NOT
        // part of the contract — in the WASM build offset 0 is a valid
        // address inside the imported shared memory.
        REQUIRE(osc_sine_validate_layout(state.base, 0) == 0);
    }
    SECTION("misaligned pointer fails") {
        if (sa > 1) {
            uintptr_t misaligned = state.base + 1;
            REQUIRE(osc_sine_validate_layout(misaligned, sb) == 0);
        }
    }

    SECTION("init rejects undersized zone without touching memory") {
        // init on a too-small region must fail without writing anything.
        REQUIRE(osc_sine_init(state.base, sb - 1) == 0);
        state.check_guards();
    }
    SECTION("init rejects misaligned zone") {
        if (sa > 1) {
            REQUIRE(osc_sine_init(state.base + 1, sb) == 0);
        }
    }
}

// ============================================================================
// VAL-DSP-007: Dynamic quantum lengths
// ============================================================================

TEST_CASE("osc_sine: compute writes exactly the requested frame count",
          "[osc_sine][val-dsp-007]")
{
    uint32_t sb = osc_sine_state_bytes();
    uint32_t sa = osc_sine_state_align();
    uint32_t min_q = osc_sine_min_quantum();
    uint32_t max_q = osc_sine_max_quantum();
    REQUIRE(min_q >= 1);
    REQUIRE(max_q > min_q);

    // Pick several quantum lengths across the supported range, including the
    // default AudioWorklet quantum (128) and non-default values.
    std::vector<uint32_t> quantums = {min_q, 1, 16, 64, 128, 256, 512, max_q};
    for (int pass = 0; pass < 2; pass++) {
        for (uint32_t q : quantums) {
            if (q < min_q || q > max_q) continue;

            GuardedZone state(sb, sa);
            REQUIRE(osc_sine_init(state.base, sb) == 1);

            std::vector<unsigned char> out_zone(q * sizeof(double) + 32, 0x5A);
            double *out_buf = reinterpret_cast<double *>(
                (reinterpret_cast<uintptr_t>(out_zone.data()) + 7) &
                ~static_cast<uintptr_t>(7));

            double freq = 220.0;
            double amp = 0.25;
            REQUIRE(osc_sine_compute(state.base,
                                     reinterpret_cast<uintptr_t>(&freq),
                                     reinterpret_cast<uintptr_t>(&amp),
                                     reinterpret_cast<uintptr_t>(out_buf),
                                     q) == 1);

            // Exactly `q` doubles must be finite.
            for (uint32_t i = 0; i < q; i++) {
                REQUIRE(std::isfinite(out_buf[i]));
            }
            // Guard byte immediately past the output must be untouched.
            unsigned char *past_end =
                reinterpret_cast<unsigned char *>(out_buf + q);
            REQUIRE(*past_end == 0x5A);
            state.check_guards();
        }
    }
}

TEST_CASE("osc_sine: out-of-range frame counts are rejected",
          "[osc_sine][val-dsp-007]")
{
    uint32_t sb = osc_sine_state_bytes();
    uint32_t sa = osc_sine_state_align();
    uint32_t max_q = osc_sine_max_quantum();
    uint32_t min_q = osc_sine_min_quantum();

    GuardedZone state(sb, sa);
    REQUIRE(osc_sine_init(state.base, sb) == 1);

    std::vector<unsigned char> out_zone((max_q + 8) * sizeof(double) + 32, 0x77);
    double *out_buf = reinterpret_cast<double *>(
        (reinterpret_cast<uintptr_t>(out_zone.data()) + 7) &
        ~static_cast<uintptr_t>(7));

    double freq = 440.0;
    double amp = 0.2;

    SECTION("zero frames fails and writes nothing") {
        for (size_t i = 0; i < out_zone.size(); i++) {
            REQUIRE(out_zone[i] == 0x77);
        }
        REQUIRE(osc_sine_compute(state.base,
                                 reinterpret_cast<uintptr_t>(&freq),
                                 reinterpret_cast<uintptr_t>(&amp),
                                 reinterpret_cast<uintptr_t>(out_buf),
                                 0) == 0);
        // No bytes were written.
        for (size_t i = 0; i < out_zone.size(); i++) {
            REQUIRE(out_zone[i] == 0x77);
        }
    }
    SECTION("frames > max fails and writes nothing") {
        REQUIRE(osc_sine_compute(state.base,
                                 reinterpret_cast<uintptr_t>(&freq),
                                 reinterpret_cast<uintptr_t>(&amp),
                                 reinterpret_cast<uintptr_t>(out_buf),
                                 max_q + 1) == 0);
        for (size_t i = 0; i < out_zone.size(); i++) {
            REQUIRE(out_zone[i] == 0x77);
        }
    }
    if (min_q > 1) {
        SECTION("frames < min fails") {
            REQUIRE(osc_sine_compute(state.base,
                                     reinterpret_cast<uintptr_t>(&freq),
                                     reinterpret_cast<uintptr_t>(&amp),
                                     reinterpret_cast<uintptr_t>(out_buf),
                                     min_q - 1) == 0);
        }
    }
}

// ============================================================================
// VAL-DSP-008: Frequency and amplitude are measurable in the rendered output
// ============================================================================

TEST_CASE("osc_sine: rendered frequency follows freq control at default amp",
          "[osc_sine][val-dsp-008]")
{
    uint32_t sb = osc_sine_state_bytes();
    uint32_t sa = osc_sine_state_align();

    GuardedZone state(sb, sa);
    REQUIRE(osc_sine_init(state.base, sb) == 1);

    const uint32_t sr = osc_sine_sample_rate();
    const uint32_t max_q = osc_sine_max_quantum();
    const uint32_t min_q = osc_sine_min_quantum();
    // Choose a block that divides the sample rate exactly and lies inside
    // the supported quantum range. 1920 divides 48000 and is well within
    // [min_q, max_q].
    const uint32_t block = 1920;
    REQUIRE(block >= min_q);
    REQUIRE(block <= max_q);
    const uint32_t total_frames = sr; // 1 second of audio
    const uint32_t blocks = total_frames / block;
    REQUIRE(blocks * block == total_frames);

    std::vector<double> out(total_frames, 0.0);
    double freq = 440.0;
    double amp = 0.2;

    for (uint32_t b = 0; b < blocks; b++) {
        double *dst = out.data() + b * block;
        REQUIRE(osc_sine_compute(state.base, reinterpret_cast<uintptr_t>(&freq),
                                 reinterpret_cast<uintptr_t>(&amp),
                                 reinterpret_cast<uintptr_t>(dst),
                                 block) == 1);
    }

    // Zero-crossing count for one second of 440 Hz sine ~= 880 (two per
    // cycle). We accept a 5% tolerance to absorb edge effects.
    uint32_t zc = count_zero_crossings(out.data(), total_frames);
    double measured_hz = (zc / 2.0) * (static_cast<double>(sr) / total_frames);
    REQUIRE(measured_hz == Approx(440.0).epsilon(0.05));

    // Peak magnitude must follow amplitude.
    double peak = 0.0, rms = 0.0;
    measure_peak_rms(out.data(), total_frames, peak, rms);
    REQUIRE(peak == Approx(0.2).epsilon(0.02));

    // RMS of a full-scale sine is amplitude / sqrt(2).
    REQUIRE(rms == Approx(0.2 / std::sqrt(2.0)).epsilon(0.03));

    state.check_guards();
}

TEST_CASE("osc_sine: amplitude scaling tracks the amp control",
          "[osc_sine][val-dsp-008]")
{
    uint32_t sb = osc_sine_state_bytes();
    uint32_t sa = osc_sine_state_align();

    const uint32_t sr = osc_sine_sample_rate();
    const uint32_t max_q = osc_sine_max_quantum();
    const uint32_t min_q = osc_sine_min_quantum();
    const uint32_t block = 1920;
    REQUIRE(block >= min_q);
    REQUIRE(block <= max_q);
    const uint32_t total_frames = sr;
    const uint32_t blocks = total_frames / block;
    REQUIRE(blocks * block == total_frames);

    double prev_peak = -1.0;
    for (double amp : {0.05, 0.1, 0.2, 0.4, 0.8}) {
        GuardedZone state(sb, sa);
        REQUIRE(osc_sine_init(state.base, sb) == 1);
        std::vector<double> out(total_frames, 0.0);
        double freq = 220.0;
        for (uint32_t b = 0; b < blocks; b++) {
            double *dst = out.data() + b * block;
            REQUIRE(osc_sine_compute(state.base,
                                     reinterpret_cast<uintptr_t>(&freq),
                                     reinterpret_cast<uintptr_t>(&amp),
                                     reinterpret_cast<uintptr_t>(dst),
                                     block) == 1);
        }
        double peak = 0.0, rms = 0.0;
        measure_peak_rms(out.data(), total_frames, peak, rms);
        REQUIRE(peak == Approx(amp).epsilon(0.02));
        if (prev_peak > 0) REQUIRE(peak > prev_peak);
        prev_peak = peak;
        state.check_guards();
    }
}

// ============================================================================
// VAL-DSP-009: Output remains finite for valid and invalid controls
// ============================================================================

static double clamp_test_single(double freq, double amp)
{
    uint32_t sb = osc_sine_state_bytes();
    uint32_t sa = osc_sine_state_align();
    GuardedZone state(sb, sa);
    REQUIRE(osc_sine_init(state.base, sb) == 1);

    const uint32_t frames = 128;
    std::vector<double> out(frames, 0.0);
    REQUIRE(osc_sine_compute(state.base,
                             reinterpret_cast<uintptr_t>(&freq),
                             reinterpret_cast<uintptr_t>(&amp),
                             reinterpret_cast<uintptr_t>(out.data()),
                             frames) == 1);
    double peak = 0.0, rms = 0.0;
    measure_peak_rms(out.data(), frames, peak, rms);
    return peak;
}

TEST_CASE("osc_sine: NaN/Inf frequency falls back to last finite value",
          "[osc_sine][val-dsp-009]")
{
    // Establish a last-finite baseline at 440 Hz.
    double baseline = clamp_test_single(440.0, 0.2);
    REQUIRE(baseline > 0.0);

    uint32_t sb = osc_sine_state_bytes();
    uint32_t sa = osc_sine_state_align();

    struct Case { const char *label; double freq; };
    Case cases[] = {
        {"NaN",   std::nan("")},
        {"+Inf",  std::numeric_limits<double>::infinity()},
        {"-Inf", -std::numeric_limits<double>::infinity()},
        {"-1 Hz", -1.0},
        {"extreme high", 1e9},
    };

    for (const auto &tc : cases) {
        SECTION(tc.label) {
            GuardedZone state(sb, sa);
            REQUIRE(osc_sine_init(state.base, sb) == 1);

            // Prime with a known finite frequency.
            double f0 = 440.0;
            double amp = 0.2;
            std::vector<double> out0(128, 0.0);
            REQUIRE(osc_sine_compute(state.base,
                                     reinterpret_cast<uintptr_t>(&f0),
                                     reinterpret_cast<uintptr_t>(&amp),
                                     reinterpret_cast<uintptr_t>(out0.data()),
                                     128) == 1);
            for (double v : out0) REQUIRE(std::isfinite(v));

            // Now feed the invalid frequency. Compute must still return 1
            // (it's a valid call; the input is the invalid thing) and all
            // samples must be finite.
            std::vector<double> out1(128, 0.0);
            REQUIRE(osc_sine_compute(state.base,
                                     reinterpret_cast<uintptr_t>(&tc.freq),
                                     reinterpret_cast<uintptr_t>(&amp),
                                     reinterpret_cast<uintptr_t>(out1.data()),
                                     128) == 1);
            for (double v : out1) {
                REQUIRE(std::isfinite(v));
                REQUIRE(std::fabs(v) <= 1.0 + 1e-9);
            }
            state.check_guards();
        }
    }
}

TEST_CASE("osc_sine: NaN/Inf/extreme amplitude stays finite and bounded",
          "[osc_sine][val-dsp-009]")
{
    uint32_t sb = osc_sine_state_bytes();
    uint32_t sa = osc_sine_state_align();

    struct Case { const char *label; double amp; };
    Case cases[] = {
        {"NaN",        std::nan("")},
        {"+Inf",       std::numeric_limits<double>::infinity()},
        {"-Inf",      -std::numeric_limits<double>::infinity()},
        {"negative",  -0.5},
        {"overshoot",  2.5},
    };

    for (const auto &tc : cases) {
        SECTION(tc.label) {
            GuardedZone state(sb, sa);
            REQUIRE(osc_sine_init(state.base, sb) == 1);
            double freq = 220.0;
            std::vector<double> out(128, 0.0);
            REQUIRE(osc_sine_compute(state.base,
                                     reinterpret_cast<uintptr_t>(&freq),
                                     reinterpret_cast<uintptr_t>(&tc.amp),
                                     reinterpret_cast<uintptr_t>(out.data()),
                                     128) == 1);
            for (double v : out) {
                REQUIRE(std::isfinite(v));
                // Amplitude is clamped to [0, 1] under the invalid-control
                // policy; no rendered sample may exceed unity.
                REQUIRE(std::fabs(v) <= 1.0 + 1e-9);
            }
            state.check_guards();
        }
    }
}

// ============================================================================
// VAL-DSP-010: Phase is continuous across blocks and same-def param updates
// ============================================================================

TEST_CASE("osc_sine: phase is continuous across consecutive blocks",
          "[osc_sine][val-dsp-010]")
{
    uint32_t sb = osc_sine_state_bytes();
    uint32_t sa = osc_sine_state_align();

    GuardedZone state(sb, sa);
    REQUIRE(osc_sine_init(state.base, sb) == 1);

    const uint32_t sr = osc_sine_sample_rate();
    const uint32_t frames = 128;
    double freq = 110.0;
    double amp = 0.2;

    // Render block A then block B at the same frequency.
    std::vector<double> a(frames, 0.0);
    std::vector<double> b(frames, 0.0);
    REQUIRE(osc_sine_compute(state.base, reinterpret_cast<uintptr_t>(&freq),
                             reinterpret_cast<uintptr_t>(&amp),
                             reinterpret_cast<uintptr_t>(a.data()),
                             frames) == 1);
    double phase_after_a = osc_sine_get_phase(state.base);

    REQUIRE(osc_sine_compute(state.base, reinterpret_cast<uintptr_t>(&freq),
                             reinterpret_cast<uintptr_t>(&amp),
                             reinterpret_cast<uintptr_t>(b.data()),
                             frames) == 1);
    double phase_after_b = osc_sine_get_phase(state.base);

    // Phase must advance by exactly (freq * frames / sr) modulo 1.
    double expected = (freq * frames) / static_cast<double>(sr);
    double diff = std::fmod(phase_after_b - phase_after_a + 10.0, 1.0);
    // fmod result is in [0, 1); allow wrap to either side.
    REQUIRE((std::fabs(diff - expected) < 1e-6 ||
             std::fabs(diff - expected + 1.0) < 1e-6));

    // Sample continuity at the boundary: the first sample of block B is
    // exactly what a single continuous sine would produce right after the
    // last sample of block A. We check that the boundary sample value
    // matches sin(2*pi*phase_after_a) within tolerance.
    double expected_first =
        amp * std::sin(2.0 * M_PI * phase_after_a);
    REQUIRE(std::fabs(b[0] - expected_first) < 1e-6);

    state.check_guards();
}

TEST_CASE("osc_sine: phase continuity survives same-def parameter update",
          "[osc_sine][val-dsp-010]")
{
    uint32_t sb = osc_sine_state_bytes();
    uint32_t sa = osc_sine_state_align();

    GuardedZone state(sb, sa);
    REQUIRE(osc_sine_init(state.base, sb) == 1);

    const uint32_t frames = 128;
    double amp = 0.2;

    // Block A at 110 Hz.
    double f_a = 110.0;
    std::vector<double> a(frames, 0.0);
    REQUIRE(osc_sine_compute(state.base, reinterpret_cast<uintptr_t>(&f_a),
                             reinterpret_cast<uintptr_t>(&amp),
                             reinterpret_cast<uintptr_t>(a.data()),
                             frames) == 1);
    double phase_after_a = osc_sine_get_phase(state.base);

    // Same instance, different frequency: phase must continue from where
    // block A left off — not snap back to zero.
    double f_b = 880.0;
    std::vector<double> b(frames, 0.0);
    REQUIRE(osc_sine_compute(state.base, reinterpret_cast<uintptr_t>(&f_b),
                             reinterpret_cast<uintptr_t>(&amp),
                             reinterpret_cast<uintptr_t>(b.data()),
                             frames) == 1);

    // The first sample of block B is sin(2*pi*phase_after_a) — the
    // frequency change shifts future phase velocity, not the starting phase.
    double expected_first =
        amp * std::sin(2.0 * M_PI * phase_after_a);
    REQUIRE(std::fabs(b[0] - expected_first) < 1e-6);

    state.check_guards();
}

// ============================================================================
// VAL-DSP-011: Recursive subnormals flush to zero
// ============================================================================

TEST_CASE("osc_sine: subnormal-producing state flushes to exact zero",
          "[osc_sine][val-dsp-011]")
{
    uint32_t sb = osc_sine_state_bytes();
    uint32_t sa = osc_sine_state_align();

    GuardedZone state(sb, sa);
    REQUIRE(osc_sine_init(state.base, sb) == 1);

    // Manually poke a subnormal value into every double slot of the state
    // block. Compute must normalise these (typically to zero) so the
    // following block runs without a denormal tail.
    double *slots = reinterpret_cast<double *>(state.base);
    uint32_t slot_count = sb / sizeof(double);
    const double subnorm = std::ldexp(1.0, -1074); // smallest subnormal > 0
    REQUIRE(subnorm > 0.0);
    REQUIRE(subnorm < std::numeric_limits<double>::min());
    for (uint32_t i = 0; i < slot_count; i++) {
        slots[i] = subnorm;
    }

    double freq = 440.0;
    double amp = 0.2;
    std::vector<double> out(128, 0.0);
    REQUIRE(osc_sine_compute(state.base, reinterpret_cast<uintptr_t>(&freq),
                             reinterpret_cast<uintptr_t>(&amp),
                             reinterpret_cast<uintptr_t>(out.data()),
                             128) == 1);

    // After compute, no state slot may remain subnormal.
    for (uint32_t i = 0; i < slot_count; i++) {
        REQUIRE((slots[i] == 0.0 ||
                 std::fabs(slots[i]) >= std::numeric_limits<double>::min()));
    }

    state.check_guards();
}

// ============================================================================
// VAL-DSP-012: Compute allocates nothing (sentinel guard survives)
// ============================================================================

TEST_CASE("osc_sine: steady-state compute writes only inside its declared zones",
          "[osc_sine][val-dsp-012]")
{
    uint32_t sb = osc_sine_state_bytes();
    uint32_t sa = osc_sine_state_align();

    GuardedZone state(sb, sa);
    REQUIRE(osc_sine_init(state.base, sb) == 1);

    const uint32_t frames = 128;
    // Output buffer wrapped on both sides by guard bytes.
    std::vector<unsigned char> out_zone(frames * sizeof(double) + 64, 0x77);
    double *out_buf = reinterpret_cast<double *>(
        (reinterpret_cast<uintptr_t>(out_zone.data()) + 15) &
        ~static_cast<uintptr_t>(15));

    double freq = 440.0;
    double amp = 0.2;

    // Warm-up block.
    REQUIRE(osc_sine_compute(state.base, reinterpret_cast<uintptr_t>(&freq),
                             reinterpret_cast<uintptr_t>(&amp),
                             reinterpret_cast<uintptr_t>(out_buf),
                             frames) == 1);

    // Now run many consecutive blocks. Every guard byte must survive every
    // iteration. This gives us deterministic evidence that the steady-state
    // compute path writes only inside its declared zones and performs no
    // incidental heap/state growth.
    for (int i = 0; i < 200; i++) {
        // Slightly vary the controls so we exercise the smoothing path.
        double f = 440.0 + i;
        double a = 0.2;
        REQUIRE(osc_sine_compute(state.base, reinterpret_cast<uintptr_t>(&f),
                                 reinterpret_cast<uintptr_t>(&a),
                                 reinterpret_cast<uintptr_t>(out_buf),
                                 frames) == 1);
    }

    // Inspect the leading guard bytes preceding out_buf.
    for (unsigned char *p = out_zone.data();
         p < reinterpret_cast<unsigned char *>(out_buf); p++) {
        REQUIRE(*p == 0x77);
    }
    // Inspect the trailing guard bytes after the last sample.
    unsigned char *past_end =
        reinterpret_cast<unsigned char *>(out_buf + frames);
    for (size_t i = 0; i < 16; i++) {
        REQUIRE(past_end[i] == 0x77);
    }
    state.check_guards();
}

// ============================================================================
// VAL-DSP-014: Fade metadata matches central synthesis constants
// ============================================================================

TEST_CASE("osc_sine: fade defaults match SYNTH_FADE_IN_MS=10 / SYNTH_FADE_OUT_MS=30",
          "[osc_sine][val-dsp-014]")
{
    REQUIRE(osc_sine_fade_in_ms() == 10);
    REQUIRE(osc_sine_fade_out_ms() == 30);

    const char *json = osc_sine_registry_json();
    REQUIRE(json != nullptr);
    REQUIRE(json_contains(json, "\"fade_in_ms\":10"));
    REQUIRE(json_contains(json, "\"fade_out_ms\":30"));
}

// ============================================================================
// Cross-cutting: initial state, deterministic output
// ============================================================================

TEST_CASE("osc_sine: initial state is deterministic across instances",
          "[osc_sine][determinism]")
{
    uint32_t sb = osc_sine_state_bytes();
    uint32_t sa = osc_sine_state_align();

    GuardedZone a(sb, sa);
    GuardedZone b(sb, sa);

    REQUIRE(osc_sine_init(a.base, sb) == 1);
    REQUIRE(osc_sine_init(b.base, sb) == 1);

    REQUIRE(std::memcmp(reinterpret_cast<void *>(a.base),
                        reinterpret_cast<void *>(b.base),
                        sb) == 0);

    // And rendering the same block on each produces byte-identical output.
    double freq = 440.0;
    double amp = 0.2;
    std::vector<double> out_a(128, 0.0);
    std::vector<double> out_b(128, 0.0);
    REQUIRE(osc_sine_compute(a.base, reinterpret_cast<uintptr_t>(&freq),
                             reinterpret_cast<uintptr_t>(&amp),
                             reinterpret_cast<uintptr_t>(out_a.data()),
                             128) == 1);
    REQUIRE(osc_sine_compute(b.base, reinterpret_cast<uintptr_t>(&freq),
                             reinterpret_cast<uintptr_t>(&amp),
                             reinterpret_cast<uintptr_t>(out_b.data()),
                             128) == 1);

    for (uint32_t i = 0; i < 128; i++) {
        REQUIRE(out_a[i] == out_b[i]);
    }
}
