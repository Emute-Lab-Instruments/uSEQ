#ifndef DEVTOOLS_H
#define DEVTOOLS_H

// DevTools: compile-time-gated instrumentation for firmware telemetry.
//
// When USEQ_DEVTOOLS is defined: collects timing, events, counters, gauges
// into fixed-size buffers and exposes them via the "debug" wire protocol.
// When not defined: every function is an empty inline stub that vanishes at -O1.
//
// See docs/specs/devtools.md for the full specification.

#include <cstdint>
#include <cstddef>

// Forward-declare SignalEngine so devtools can accept it without pulling headers
namespace sig { struct SignalEngine; }

namespace dt {

#if USEQ_DEVTOOLS

// ── Tick Profiling ─────────────────────────────────────────────────────────
void tick_begin();
void mark(const char* phase);
void tick_end();

// Measure the compiler/evaluator interval around one wire-level eval. The
// duration excludes serial parsing and response emission, and is retained for
// poll-based target acceptance.
void eval_begin();
void eval_end(bool success);

// ── Events ─────────────────────────────────────────────────────────────────
void event(const char* channel, const char* message,
           const char* detail = nullptr);
void event(const char* channel, const char* message, int detail);

// ── Counters (monotonic) ───────────────────────────────────────────────────
void count(const char* name);

// ── Gauges (point-in-time) ─────────────────────────────────────────────────
void gauge(const char* name, uint32_t value);
void gauge(const char* name, uint32_t value, uint32_t capacity);

// ── Initialization ─────────────────────────────────────────────────────────
// Called once from Firmware::init(). Stores the engine pointer for
// graph/state queries. Must be called before any other dt:: function.
void init(sig::SignalEngine* engine);

// Sample heap usage into the monotonic runtime low-water record. The compiler
// calls this immediately after its largest transient allocations; the
// firmware tick samples steady state. The retained stack canary is scanned
// when the resources channel is queried or emitted.
void sample_runtime_memory();

// ── Protocol Integration ───────────────────────────────────────────────────
// Called by SerialProtocol::dispatch_message() for type "debug".
// write_fn emits a JSON string to serial (caller provides the function).
// Returns true if the message was handled.
using WriteFn = void(*)(const char*, size_t);
bool handle_debug_message(const char* json, size_t len, WriteFn write_fn);

// Called once per tick after tick_end(). Emits data for streaming channels.
// Checks backpressure via can_write before emitting.
void emit_streaming(WriteFn write_fn, bool can_write);

#else // !USEQ_DEVTOOLS — everything compiles away

inline void tick_begin() {}
inline void mark(const char*) {}
inline void tick_end() {}
inline void eval_begin() {}
inline void eval_end(bool) {}

inline void event(const char*, const char*, const char* = nullptr) {}
inline void event(const char*, const char*, int) {}

inline void count(const char*) {}

inline void gauge(const char*, uint32_t) {}
inline void gauge(const char*, uint32_t, uint32_t) {}

inline void init(sig::SignalEngine*) {}
inline void sample_runtime_memory() {}

using WriteFn = void(*)(const char*, size_t);
inline bool handle_debug_message(const char*, size_t, WriteFn) { return false; }
inline void emit_streaming(WriteFn, bool) {}

#endif // USEQ_DEVTOOLS

} // namespace dt

#endif // DEVTOOLS_H
