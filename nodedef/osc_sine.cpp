// osc/sine version-2 NodeDef (synth-nodes.md §2 / VAL-DSP-001..016)
//
// Hand-written, source-agnostic audio-rate DSP module. Built twice from this
// single source:
//
//   * native C++ — meson/ninja conformance test target (test/meson.build);
//   * Emscripten — separate WASM artefact
//     (nodedef/build_osc_sine_wasm.sh → wasm/osc_sine.wasm).
//
// The WASM artefact imports host-owned shared WebAssembly.Memory under the
// import name "env.memory" and never defines or exports its own memory
// (VAL-DSP-005). The host allocates a per-instance zone (state + output) and
// passes base pointers into init/compute via uintptr_t parameters. State
// size and alignment are exposed through `osc_sine_state_bytes()` /
// `osc_sine_state_align()` so a source-agnostic host adapter can size the
// zone from registry metadata alone (VAL-DSP-006, wired app-side).
//
// Real-time safety is a hard contract, not a stretch goal:
//   * no allocation in `compute` (VAL-DSP-012);
//   * no blocking imports (VAL-DSP-013);
//   * recursive subnormal state flushed to zero each call (VAL-DSP-011);
//   * finite-output policy enforced on every control input (VAL-DSP-009);
//   * exactly `frame_count` samples written per call (VAL-DSP-007);
//   * phase advances continuously across calls and same-def updates
//     (VAL-DSP-010).

#include "osc_sine.h"

#include <cmath>
#include <cstdint>
#include <cstring>

#if defined(__EMSCRIPTEN__)
// The WASM build imports host-owned shared memory under the env namespace.
// The host supplies a WebAssembly.Memory with shared:true at instantiation
// time; we never declare our own memory.
//
// We do not need any imports beyond memory itself — compute is
// allocation-free and blocking-free by construction. Mark this explicitly so
// the WASM import table is empty apart from the memory import (VAL-DSP-013).
//
// (No emscripten.h needed; we never call out.)
#endif

namespace {

// ── Compile-time synthesis constants (mirror of src/contracts/synthesisControlAbi.ts) ──
// VAL-DSP-014: these must match SYNTH_FADE_IN_MS / SYNTH_FADE_OUT_MS exactly.
constexpr uint32_t FADE_IN_MS = 10;
constexpr uint32_t FADE_OUT_MS = 30;
constexpr uint32_t DEFAULT_SAMPLE_RATE_HZ = 48000;
constexpr uint32_t SAMPLE_RATE_ABI_VERSION = 1;

// Supported render-quantum range. The default WebAudio quantum is 128
// samples; we admit any frame count in [1, 8192] so future agents with
// larger blocks work without a rebuild.
constexpr uint32_t MIN_QUANTUM = 1;
constexpr uint32_t MAX_QUANTUM = 8192;

// ── Per-instance state layout ────────────────────────────────────────────────
//
// Fixed-size, 8-byte aligned. Phase is kept in [0, 1) (cycles). Smoothed amp
// holds the last-applied amplitude; the def ramps linearly from this value
// to the new control value across each block (declared smoothing on :amp).
//
// `last_freq` is the last finite frequency we applied — used by the NaN/Inf
// fallback policy (VAL-DSP-009).
struct OscSineState {
    double phase;          // current phase in cycles, [0, 1)
    double smoothed_amp;   // last applied amplitude target
    double last_freq;      // last finite frequency, for invalid-control policy
};

// Round `n` up to the nearest multiple of `align` (align must be a power of 2).
constexpr uint32_t round_up(uint32_t n, uint32_t align)
{
    return (n + align - 1) & ~(align - 1);
}

// State block size, padded so adjacent instances can be packed without
// extra alignment padding.
constexpr uint32_t STATE_BYTES_INNER = sizeof(OscSineState);
constexpr uint32_t STATE_ALIGN_INNER = alignof(OscSineState) < 8 ? 8 : alignof(OscSineState);
constexpr uint32_t STATE_BYTES = round_up(STATE_BYTES_INNER, STATE_ALIGN_INNER);

// ── Finite-input policy ────────────────────────────────────────────────────
//
// VAL-DSP-009: NaN, infinity, negative, and extreme values must never reach
// DSP state. The policy is deterministic and side-effect-free:
//   * NaN / Inf frequency  → substitute the last finite frequency
//                            (defaults to 440 Hz on the first block)
//   * Negative frequency   → absolute value, then clamp to Nyquist
//   * > Nyquist frequency  → clamp to Nyquist (sample_rate / 2)
//   * NaN / Inf / negative amplitude → substitute 0
//   * > 1 amplitude        → clamp to 1
// Returned samples are always finite and lie in [-1, 1].

inline bool is_finite_value(double v)
{
    // std::isfinite is the right call; this wrapper exists only to keep the
    // call site readable inside the policy helper below.
    return std::isfinite(v);
}

double sanitize_frequency(double v, double fallback, uint32_t sample_rate_hz)
{
    // Re-clamp the selected fallback at every render rate. An instance can
    // move from 96 kHz to 44.1 kHz between calls, so a formerly valid stored
    // frequency may now exceed Nyquist.
    double a = is_finite_value(v) ? std::fabs(v) : std::fabs(fallback);
    if (!is_finite_value(a)) a = 440.0;
    const double nyquist_hz =
        static_cast<double>(sample_rate_hz) / 2.0;
    if (a > nyquist_hz) {
        return nyquist_hz;
    }
    return a;
}

double sanitize_amplitude(double v)
{
    if (!is_finite_value(v) || v < 0.0) {
        return 0.0;
    }
    if (v > 1.0) {
        return 1.0;
    }
    return v;
}

// ── Recursive subnormal flush (VAL-DSP-011) ────────────────────────────────
//
// WASM has no hardware FTZ mode, so we flush subnormal state to zero on every
// compute call before reading it. This guarantees denormal tails cannot build
// up across blocks and silently burn the CPU budget.
inline void flush_subnormals(double &v)
{
    if (v != 0.0 && std::fabs(v) < std::numeric_limits<double>::min()) {
        v = 0.0;
    }
}

void flush_state_subnormals(OscSineState &s)
{
    flush_subnormals(s.phase);
    flush_subnormals(s.smoothed_amp);
    flush_subnormals(s.last_freq);
}

// ── Registry JSON (built once, stable for module lifetime) ──────────────────
//
// VAL-DSP-001: the descriptor is normalised so a source-agnostic host adapter
// can drive the def without linking against this C++ source. We render it as
// a plain string with no dynamic formatting — the state_bytes / state_align
// numbers are compile-time constants. The static_assert below guarantees
// these numbers stay in sync with OscSineState; if the layout ever changes,
// update both the literal and the assert.
//
// Layout numbers currently in the descriptor:
//   state_bytes  = 24 (3 * sizeof(double) = phase + smoothed_amp + last_freq)
//   state_align  = 8  (alignof(double))
constexpr char REGISTRY_JSON[] =
    "{\"name\":\"osc/sine\",\"version\":2,"
    "\"audio_inputs\":1,\"audio_input_names\":[\"fm\"],"
    "\"audio_outputs\":1,\"voice_fanout\":false,"
    "\"params\":["
    "{\"name\":\"freq\",\"default\":440.0,\"rate\":\"block\",\"smoothing\":\"step\"},"
    "{\"name\":\"amp\",\"default\":0.2,\"rate\":\"block\",\"smoothing\":\"linear\"}"
    "],"
    "\"fade_in_ms\":10,\"fade_out_ms\":30,"
    "\"state_bytes\":24,\"state_align\":8,"
    "\"control_stride\":8,\"output_stride\":8,"
    "\"min_quantum\":1,\"max_quantum\":8192,"
    "\"sample_rate\":48000}";

// Assert the compile-time state layout matches the literal in the JSON. If
// this fails, update the state_bytes / state_align fields above.
static_assert(STATE_BYTES == 24,
              "REGISTRY_JSON state_bytes must match OscSineState layout");
static_assert(STATE_ALIGN_INNER == 8,
              "REGISTRY_JSON state_align must match OscSineState alignment");

} // namespace

// ── Public C ABI ────────────────────────────────────────────────────────────

extern "C" {

const char *osc_sine_registry_json(void)
{
    return REGISTRY_JSON;
}

uint32_t osc_sine_state_bytes(void) { return STATE_BYTES; }
uint32_t osc_sine_state_align(void) { return STATE_ALIGN_INNER; }
uint32_t osc_sine_control_stride_bytes(void) { return sizeof(double); }
uint32_t osc_sine_output_stride_bytes(void) { return sizeof(double); }
uint32_t osc_sine_min_quantum(void) { return MIN_QUANTUM; }
uint32_t osc_sine_max_quantum(void) { return MAX_QUANTUM; }
uint32_t osc_sine_sample_rate_abi_version(void)
{
    return SAMPLE_RATE_ABI_VERSION;
}

uint32_t osc_sine_sample_rate(void) { return DEFAULT_SAMPLE_RATE_HZ; }
uint32_t osc_sine_fade_in_ms(void) { return FADE_IN_MS; }
uint32_t osc_sine_fade_out_ms(void) { return FADE_OUT_MS; }

uint32_t osc_sine_validate_layout(uintptr_t state_ptr, uint32_t state_bytes)
{
    // Reject undersized or misaligned zones. We deliberately do NOT reject
    // `state_ptr == 0`: in the WASM build the host adapter passes offsets
    // into the imported shared memory, and offset 0 is a valid address. The
    // host adapter is responsible for never passing a non-memory-backed
    // pointer (e.g. a real null in a native embedding).
    if (state_bytes < STATE_BYTES) return 0;
    if ((state_ptr & (STATE_ALIGN_INNER - 1)) != 0) return 0;
    return 1;
}

uint32_t osc_sine_init(uintptr_t state_ptr, uint32_t state_bytes)
{
    if (!osc_sine_validate_layout(state_ptr, state_bytes)) return 0;
    OscSineState *s = reinterpret_cast<OscSineState *>(state_ptr);
    s->phase = 0.0;
    // A freshly-instantiated node enters silence; the host's fade-in
    // envelope ramps the gain up over SYNTH_FADE_IN_MS, modelled as a linear
    // ramp from 0 to the target amp. We therefore seed smoothed_amp at 0 so
    // the first compute call ramps from silence rather than jumping to the
    // control value (synth-nodes.md §5.5).
    s->smoothed_amp = 0.0;
    s->last_freq = 440.0;  // registry default for the invalid-control fallback.
    return 1;
}

uint32_t osc_sine_compute(uintptr_t state_ptr,
                          uintptr_t freq_ptr,
                          uintptr_t amp_ptr,
                          uintptr_t output_ptr,
                          uint32_t frame_count)
{
    return osc_sine_compute_at_sample_rate(state_ptr, freq_ptr, amp_ptr,
                                           output_ptr, frame_count,
                                           DEFAULT_SAMPLE_RATE_HZ);
}

namespace {

uint32_t compute_impl(uintptr_t state_ptr,
                      uintptr_t freq_ptr,
                      uintptr_t amp_ptr,
                      uintptr_t fm_ptr,
                      bool has_fm,
                      uintptr_t output_ptr,
                      uint32_t frame_count,
                      uint32_t sample_rate_hz)
{
    // Reject out-of-range frame counts up front. No output bytes are written
    // on the failure path (VAL-DSP-007).
    //
    // Note on pointer validation: in the WASM build, the host adapter
    // supplies offsets into the imported shared memory; offset 0 is a
    // perfectly valid address, so we deliberately do NOT reject zero
    // pointers. The host adapter is the source-agnostic contract owner and
    // is required to pass aligned, in-bounds offsets (validated up front
    // through osc_sine_validate_layout for state, and through buffer sizing
    // for control/output). The alignment checks below catch the remaining
    // contract violation class cheaply.
    if (sample_rate_hz == 0 ||
        frame_count < MIN_QUANTUM || frame_count > MAX_QUANTUM) {
        return 0;
    }
    // Alignment must hold; the host adapter guarantees this but we check
    // defensively. Misaligned access would trap in WASM.
    if ((state_ptr & (STATE_ALIGN_INNER - 1)) != 0) return 0;
    if ((output_ptr & 7u) != 0) return 0;
    if ((freq_ptr & 7u) != 0) return 0;
    if ((amp_ptr & 7u) != 0) return 0;
    if (has_fm && (fm_ptr & 7u) != 0) return 0;

    OscSineState *s = reinterpret_cast<OscSineState *>(state_ptr);
    flush_state_subnormals(*s);

    // Read the control block-rate values. We treat the pointers as pointing
    // to one double each (block rate = one sample per block).
    double raw_freq = *reinterpret_cast<double *>(freq_ptr);
    double raw_amp = *reinterpret_cast<double *>(amp_ptr);

    double freq = sanitize_frequency(raw_freq, s->last_freq, sample_rate_hz);
    double amp_target = sanitize_amplitude(raw_amp);

    // Remember the now-finite frequency for the next block's fallback.
    s->last_freq = freq;

    // Linear amplitude ramp from the previously-applied value to the new
    // target across the block (declared smoothing on :amp). Step smoothing
    // on :freq means the new frequency applies from sample 0 of this block
    // with no ramp (synth-nodes.md §2.4).
    double amp_start = s->smoothed_amp;
    double amp_step = (amp_target - amp_start) / static_cast<double>(frame_count);

    double *out = reinterpret_cast<double *>(output_ptr);

    const double* fm = has_fm ? reinterpret_cast<double*>(fm_ptr) : nullptr;
    const double nyquist_hz = static_cast<double>(sample_rate_hz) / 2.0;
    const double two_pi = 6.28318530717958647692;

    double phase = s->phase;
    double amp = amp_start;
    for (uint32_t i = 0; i < frame_count; i++) {
        double sample = amp * std::sin(two_pi * phase);
        // Defensive FTZ on output: a subnormal could only arise here from a
        // denormal amp; we already flushed amp on entry, but a future
        // smoothing tweak could regress. Cheap to keep.
        if (sample != 0.0 &&
            std::fabs(sample) < std::numeric_limits<double>::min()) {
            sample = 0.0;
        }
        out[i] = sample;
        double fm_hz = has_fm && is_finite_value(fm[i]) ? fm[i] : 0.0;
        double instantaneous_hz = freq + fm_hz;
        if (!is_finite_value(instantaneous_hz)) {
            instantaneous_hz = instantaneous_hz < 0.0 ? 0.0 : nyquist_hz;
        } else if (instantaneous_hz < 0.0) {
            instantaneous_hz = 0.0;
        } else if (instantaneous_hz > nyquist_hz) {
            instantaneous_hz = nyquist_hz;
        }
        phase += instantaneous_hz / static_cast<double>(sample_rate_hz);
        if (phase >= 1.0) {
            // Wrap into [0, 1). std::floor keeps this branch-free and exact
            // for sane inputs; the `if` guards against an expensive modf on
            // the common case.
            phase -= std::floor(phase);
        }
        amp += amp_step;
    }

    // Commit the new phase and the end-of-block amplitude.
    s->phase = phase;
    s->smoothed_amp = amp_target;

    // Flush once more so the next call sees clean state.
    flush_state_subnormals(*s);

    return 1;
}

} // namespace

uint32_t osc_sine_compute_at_sample_rate(uintptr_t state_ptr,
                                         uintptr_t freq_ptr,
                                         uintptr_t amp_ptr,
                                         uintptr_t output_ptr,
                                         uint32_t frame_count,
                                         uint32_t sample_rate_hz)
{
    return compute_impl(state_ptr, freq_ptr, amp_ptr, 0, false, output_ptr,
                        frame_count, sample_rate_hz);
}

uint32_t osc_sine_compute_fm(uintptr_t state_ptr,
                             uintptr_t freq_ptr,
                             uintptr_t amp_ptr,
                             uintptr_t fm_ptr,
                             uintptr_t output_ptr,
                             uint32_t frame_count)
{
    return osc_sine_compute_fm_at_sample_rate(
        state_ptr, freq_ptr, amp_ptr, fm_ptr, output_ptr, frame_count,
        DEFAULT_SAMPLE_RATE_HZ);
}

uint32_t osc_sine_compute_fm_at_sample_rate(uintptr_t state_ptr,
                                            uintptr_t freq_ptr,
                                            uintptr_t amp_ptr,
                                            uintptr_t fm_ptr,
                                            uintptr_t output_ptr,
                                            uint32_t frame_count,
                                            uint32_t sample_rate_hz)
{
    return compute_impl(state_ptr, freq_ptr, amp_ptr, fm_ptr, true, output_ptr,
                        frame_count, sample_rate_hz);
}

double osc_sine_get_phase(uintptr_t state_ptr)
{
    if (state_ptr == 0) return 0.0;
    OscSineState *s = reinterpret_cast<OscSineState *>(state_ptr);
    return s->phase;
}

double osc_sine_get_smoothed_amp(uintptr_t state_ptr)
{
    if (state_ptr == 0) return 0.0;
    OscSineState *s = reinterpret_cast<OscSineState *>(state_ptr);
    return s->smoothed_amp;
}

void osc_sine_reset_phase(uintptr_t state_ptr)
{
    if (state_ptr == 0) return;
    OscSineState *s = reinterpret_cast<OscSineState *>(state_ptr);
    s->phase = 0.0;
}

} // extern "C"
