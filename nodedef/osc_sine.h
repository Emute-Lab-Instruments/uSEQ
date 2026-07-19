#ifndef NODEDEF_OSC_SINE_H
#define NODEDEF_OSC_SINE_H

// ── osc/sine version-1 NodeDef (synth-nodes.md §2 / VAL-DSP-001..016) ────────
//
// Hand-written, source-agnostic NodeDef shipped as a SEPARATE build target
// from the ModuLisp interpreter and firmware sources (architecture.md §5.3,
// §7; VAL-DSP-015, VAL-DSP-016). The module is compiled twice from this
// single source file:
//
//   * native C++  — for meson/ninja conformance tests (test/meson.build);
//   * Emscripten  — for the separate WASM artefact loaded by the worklet
//                   host (nodedef/build_osc_sine_wasm.sh).
//
// ── Memory model (VAL-DSP-004, VAL-DSP-005) ─────────────────────────────────
//
// The WASM build imports host-owned shared WebAssembly.Memory and never
// defines or exports its own. The host allocates a contiguous per-instance
// zone (state + scratch + output) and passes base pointers into `init` and
// `compute`. Per-instance state size and alignment are exposed via
// `osc_sine_state_bytes()` / `osc_sine_state_align()` so the host can size
// the zone before instantiation; misaligned or undersized zones fail
// `osc_sine_validate_layout()` without touching memory.

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Registry identity (VAL-DSP-001) ─────────────────────────────────────────
//
// Stable string identifiers consumed by the source-agnostic host adapter.
// These mirror sig::NodeDefDescriptor for osc/sine v1: zero audio inputs, one
// mono audio output, no voice fan-out, freq/amp block-rate controls.

#define OSC_SINE_NODEDEF_NAME    "osc/sine"
#define OSC_SINE_NODEDEF_VERSION 1

// Returns a stable, null-terminated JSON registry descriptor that the
// source-agnostic host adapter reads without linking against the DSP source.
// Shape (kept stable across native and WASM builds):
//   {
//     "name":"osc/sine","version":1,
//     "audio_inputs":0,"audio_outputs":1,"voice_fanout":false,
//     "params":[
//       {"name":"freq","default":440.0,"rate":"block","smoothing":"step"},
//       {"name":"amp", "default":0.2,  "rate":"block","smoothing":"linear"}
//     ],
//     "fade_in_ms":10,"fade_out_ms":30,
//     "state_bytes":<N>,"state_align":<A>,
//     "control_stride":8,"output_stride":8,
//     "min_quantum":1,"max_quantum":8192,
//     "sample_rate":48000
//   }
//
// The returned pointer is valid for the lifetime of the module; callers MUST
// NOT free it.
const char *osc_sine_registry_json(void);

// ── Layout contract (VAL-DSP-004) ───────────────────────────────────────────
//
// Per-instance state size and alignment. The host allocates a region of at
// least `osc_sine_state_bytes()` bytes, aligned to `osc_sine_state_align()`,
// for each osc/sine instance. An output buffer of `frame_count *
// sizeof(double)` bytes (8-byte aligned) is supplied per compute call.

uint32_t osc_sine_state_bytes(void);
uint32_t osc_sine_state_align(void);

// Stride helpers. osc/sine keeps both control inputs and audio output as
// little-endian IEEE-754 doubles (8-byte aligned).
uint32_t osc_sine_control_stride_bytes(void);
uint32_t osc_sine_output_stride_bytes(void);

// Supported runtime quantum range. Compute rejects frame counts outside
// [min, max] without writing any output.
uint32_t osc_sine_min_quantum(void);
uint32_t osc_sine_max_quantum(void);

// Sample rate the module was compiled against (Hz).
uint32_t osc_sine_sample_rate(void);

// Fade metadata (VAL-DSP-014). Must match the central synthesis constants
// SYNTH_FADE_IN_MS = 10 and SYNTH_FADE_OUT_MS = 30.
uint32_t osc_sine_fade_in_ms(void);
uint32_t osc_sine_fade_out_ms(void);

// Validate that a host-provided zone is large enough and correctly aligned
// for a per-instance state block. Returns 1 if the layout is usable, 0
// otherwise. Callers MUST consult this before `osc_sine_init` so an
// undersized or misaligned zone fails closed (VAL-DSP-004).
uint32_t osc_sine_validate_layout(uintptr_t state_ptr, uint32_t state_bytes);

// ── Lifecycle ──────────────────────────────────────────────────────────────
//
// `init` resets the per-instance state to the registry defaults (440 Hz,
// amp 0.2, phase 0). It performs no allocation; the state pointer must
// already satisfy osc_sine_validate_layout().
//
// Returns 1 on success, 0 on invalid layout.
uint32_t osc_sine_init(uintptr_t state_ptr, uint32_t state_bytes);

// `compute` writes exactly `frame_count` samples to `output_ptr` (an
// 8-byte-aligned array of doubles). The state pointer must previously have
// been initialised. `freq_ptr` and `amp_ptr` point to the block-rate control
// samples for the upcoming block (one double each).
//
// Guarantees:
//   * exactly `frame_count` frames are written (VAL-DSP-007);
//   * phase advances continuously across blocks (VAL-DSP-010);
//   * amp ramps linearly from the previously-applied value to the new value
//     over the block (VAL-DSP-003, declared smoothing);
//   * NaN/Inf frequencies clamp to the last finite value, negative or
//     extreme frequencies clamp to [0, sample_rate/2], extreme amplitude
//     clamps to [0, 1] — every written sample is finite (VAL-DSP-009);
//   * recursive subnormal state is flushed to exact zero on each call
//     (VAL-DSP-011);
//   * no allocation or blocking call is performed (VAL-DSP-012, VAL-DSP-013).
//
// Returns 1 on success, 0 on invalid frame_count (frame_count==0 or outside
// [min_quantum, max_quantum]) or invalid pointers. On failure, no output
// samples are written.
uint32_t osc_sine_compute(uintptr_t state_ptr,
                          uintptr_t freq_ptr,
                          uintptr_t amp_ptr,
                          uintptr_t output_ptr,
                          uint32_t frame_count);

// ── Inspection (for conformance tests) ──────────────────────────────────────
//
// Read back the per-instance phase and smoothed amplitude. The host never
// needs these in production; they exist so native conformance tests can
// assert phase continuity and smoothing behaviour deterministically.
double osc_sine_get_phase(uintptr_t state_ptr);
double osc_sine_get_smoothed_amp(uintptr_t state_ptr);

// Reset phase to zero and smoothed amplitude to default. Used by tests that
// need a clean starting point without re-initialising the whole state block.
void osc_sine_reset_phase(uintptr_t state_ptr);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NODEDEF_OSC_SINE_H
