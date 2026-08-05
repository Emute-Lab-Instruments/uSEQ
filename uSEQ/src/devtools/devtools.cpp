#include "devtools.h"

#if USEQ_DEVTOOLS

#include "../signal_engine/signal_engine.h"
#include "../utils/json_builder.h"
#include "../utils/log.h"
#include <cstring>
#include <climits>

#if defined(ARDUINO_ARCH_RP2040)
extern "C" {
extern uint8_t __StackBottom;
extern uint8_t __StackTop;
}
#endif

#ifdef USE_STD_IO
#include <chrono>
static uint32_t dt_micros() {
    static const auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(now - start).count());
}
#else
#include <Arduino.h>
static uint32_t dt_micros() { return micros(); }
#endif

// ── Output name table ──────────────────────────────────────────────────────

static const char* output_name(uint16_t idx) {
    static const char* names[] = {
        "a1","a2","a3","a4","a5","a6","a7","a8",
        "d1","d2","d3","d4","d5","d6","d7","d8",
        "s1","s2","s3","s4","s5","s6","s7","s8",
    };
    if (idx < 24) return names[idx];
    return "?";
}

// ── NodeOp name table ──────────────────────────────────────────────────────

static const char* node_op_name(sig::NodeOp op) {
    switch (op) {
        case sig::NodeOp::Const:          return "Const";
        case sig::NodeOp::RawTimeLoad:    return "RawTimeLoad";
        case sig::NodeOp::CellLoad:       return "CellLoad";
        case sig::NodeOp::InputLoad:      return "InputLoad";
        case sig::NodeOp::PrevOutputLoad: return "PrevOutputLoad";
        case sig::NodeOp::Add:   return "Add";
        case sig::NodeOp::Sub:   return "Sub";
        case sig::NodeOp::Mul:   return "Mul";
        case sig::NodeOp::Div:   return "Div";
        case sig::NodeOp::Mod:   return "Mod";
        case sig::NodeOp::Expt:  return "Expt";
        case sig::NodeOp::Min:   return "Min";
        case sig::NodeOp::Max:   return "Max";
        case sig::NodeOp::Neg:   return "Neg";
        case sig::NodeOp::Abs:   return "Abs";
        case sig::NodeOp::Floor: return "Floor";
        case sig::NodeOp::Ceil:  return "Ceil";
        case sig::NodeOp::Frac:  return "Frac";
        case sig::NodeOp::Sqrt:  return "Sqrt";
        case sig::NodeOp::Clamp: return "Clamp";
        case sig::NodeOp::Sin:   return "Sin";
        case sig::NodeOp::Cos:   return "Cos";
        case sig::NodeOp::Tan:   return "Tan";
        case sig::NodeOp::USin:  return "USin";
        case sig::NodeOp::UCos:  return "UCos";
        case sig::NodeOp::Tri:   return "Tri";
        case sig::NodeOp::Sqr:   return "Sqr";
        case sig::NodeOp::Pulse: return "Pulse";
        case sig::NodeOp::CmpGt: return "CmpGt";
        case sig::NodeOp::CmpLt: return "CmpLt";
        case sig::NodeOp::CmpGe: return "CmpGe";
        case sig::NodeOp::CmpLe: return "CmpLe";
        case sig::NodeOp::CmpEq: return "CmpEq";
        case sig::NodeOp::Not:   return "Not";
        case sig::NodeOp::And:   return "And";
        case sig::NodeOp::Or:    return "Or";
        case sig::NodeOp::Select:    return "Select";
        case sig::NodeOp::VecIndex:  return "VecIndex";
        case sig::NodeOp::VecLerp:   return "VecLerp";
        case sig::NodeOp::BiToUni:   return "BiToUni";
        case sig::NodeOp::UniToBi:   return "UniToBi";
        case sig::NodeOp::Scale:     return "Scale";
        case sig::NodeOp::Lerp:      return "Lerp";
        case sig::NodeOp::HashIndex: return "HashIndex";
        case sig::NodeOp::LoadState: return "LoadState";
        case sig::NodeOp::LoadDt:    return "LoadDt";
        case sig::NodeOp::SlotLoad:  return "SlotLoad";
    }
    return "Unknown";
}

static const char* health_name(const sig::NodePool& pool,
                               uint16_t output_index) {
    return sig::output_health_to_cstr(
        sig::output_health(pool, output_index));
}

static bool op_has_imm(sig::NodeOp op) {
    return op == sig::NodeOp::Const || op == sig::NodeOp::CellLoad ||
           op == sig::NodeOp::InputLoad || op == sig::NodeOp::PrevOutputLoad ||
           op == sig::NodeOp::VecIndex || op == sig::NodeOp::VecLerp ||
           op == sig::NodeOp::LoadState || op == sig::NodeOp::SlotLoad;
}

static bool op_is_binary(sig::NodeOp op) {
    return (op >= sig::NodeOp::Add && op <= sig::NodeOp::Max) ||
           (op >= sig::NodeOp::CmpGt && op <= sig::NodeOp::CmpEq) ||
           op == sig::NodeOp::And || op == sig::NodeOp::Or ||
           op == sig::NodeOp::Pulse;
}

static bool op_is_ternary(sig::NodeOp op) {
    return op == sig::NodeOp::Select || op == sig::NodeOp::Scale ||
           op == sig::NodeOp::Lerp || op == sig::NodeOp::Clamp;
}

// ══════════════════════════════════════════════════════════════════════════
// State
// ══════════════════════════════════════════════════════════════════════════

namespace {

enum Channel : uint8_t {
    CH_TICK, CH_GRAPH, CH_STATE, CH_EVAL, CH_RESOURCES, CH_IO, CH_PROTOCOL, CH_COUNT
};

enum Mode : uint8_t { OFF, POLL, STREAM, EVENTS };

struct DevToolsState {
    sig::SignalEngine* engine = nullptr;
    Mode modes[CH_COUNT] = {};
    uint32_t stream_interval_us = 100000;

    struct {
        uint32_t tick_start_us = 0;
        uint32_t phase_start_us = 0;
        uint32_t phase_durations[8] = {};
        const char* phase_names[8] = {};
        uint8_t phase_count = 0;
        uint32_t tick_total_us = 0;
        uint32_t tick_count = 0;
        uint32_t win_min = UINT32_MAX;
        uint32_t win_max = 0;
        uint32_t win_sum = 0;
        uint32_t win_count = 0;
    } tick;

    struct {
        uint32_t start_us = 0;
        uint32_t last_us = 0;
        uint32_t max_us = 0;
        uint32_t count = 0;
        uint32_t error_count = 0;
    } eval;

    struct Event {
        uint32_t ts_us = 0;
        const char* channel = nullptr;
        const char* message = nullptr;
        const char* detail_str = nullptr;
        int detail_int = 0;
        bool has_int = false;
    };
    Event events[32] = {};
    uint8_t ev_head = 0;
    uint8_t ev_count = 0;

    struct Counter { const char* name = nullptr; uint32_t value = 0; };
    Counter counters[16] = {};
    uint8_t counter_count = 0;

    struct Gauge { const char* name = nullptr; uint32_t value = 0; uint32_t capacity = 0; };
    Gauge gauges[16] = {};
    uint8_t gauge_count = 0;

    struct {
        uint32_t heap_free = 0;
        uint32_t heap_min_free = UINT32_MAX;
        uint32_t core0_stack_capacity = 0;
        uint32_t core0_stack_high_water = 0;
        uintptr_t core0_stack_fill_end = 0;
        bool core0_stack_initialized = false;
        bool core0_stack_margin_intact = true;
    } memory;

    uint32_t last_stream_us = 0;
};

static DevToolsState s;

constexpr uint32_t TICK_WINDOW_SIZE = 100;

} // anonymous namespace

// ══════════════════════════════════════════════════════════════════════════
// Collection
// ══════════════════════════════════════════════════════════════════════════

namespace dt {

namespace {
void sample_core0_stack() {
#if defined(ARDUINO_ARCH_RP2040)
    if (s.memory.core0_stack_initialized) {
        constexpr uintptr_t SDK_STACK_GUARD_BYTES = 32;
        constexpr uint8_t STACK_PATTERN = 0xA5;
        const uintptr_t bottom = reinterpret_cast<uintptr_t>(&__StackBottom);
        const uintptr_t top = reinterpret_cast<uintptr_t>(&__StackTop);
        const uintptr_t sample_begin = bottom + SDK_STACK_GUARD_BYTES;
        uintptr_t first_touched = sample_begin;
        while (first_touched < s.memory.core0_stack_fill_end &&
               *reinterpret_cast<volatile uint8_t*>(first_touched) ==
                   STACK_PATTERN) {
            first_touched++;
        }
        const uint32_t used = static_cast<uint32_t>(top - first_touched);
        if (used > s.memory.core0_stack_high_water)
            s.memory.core0_stack_high_water = used;
        s.memory.core0_stack_margin_intact =
            *reinterpret_cast<volatile uint8_t*>(sample_begin) ==
            STACK_PATTERN;
    }
#endif
}
} // namespace

void init(sig::SignalEngine* engine) {
    s = DevToolsState{};
    s.engine = engine;

#if defined(ARDUINO_ARCH_RP2040)
    // The RP2040 SDK may configure the lowest aligned 32-byte stack
    // subregion as an MPU guard. Keep the watermark outside that region.
    constexpr uintptr_t SDK_STACK_GUARD_BYTES = 32;
    constexpr uintptr_t STACK_SAMPLE_GUARD = 128;
    constexpr uint8_t STACK_PATTERN = 0xA5;
    uintptr_t stack_pointer = 0;
    asm volatile("mov %0, sp" : "=r"(stack_pointer));
    const uintptr_t bottom = reinterpret_cast<uintptr_t>(&__StackBottom);
    const uintptr_t top = reinterpret_cast<uintptr_t>(&__StackTop);
    s.memory.core0_stack_capacity = static_cast<uint32_t>(top - bottom);
    const uintptr_t sample_begin = bottom + SDK_STACK_GUARD_BYTES;
    if (stack_pointer > sample_begin + STACK_SAMPLE_GUARD &&
        stack_pointer <= top) {
        const uintptr_t fill_end = stack_pointer - STACK_SAMPLE_GUARD;
        std::memset(reinterpret_cast<void*>(sample_begin), STACK_PATTERN,
                    fill_end - sample_begin);
        s.memory.core0_stack_fill_end = fill_end;
        s.memory.core0_stack_initialized = true;
    }
#endif
    sample_runtime_memory();
}

void sample_runtime_memory() {
    int heap = free_heap();
    if (heap < 0) heap = 0;
    s.memory.heap_free = static_cast<uint32_t>(heap);
    if (s.memory.heap_free < s.memory.heap_min_free)
        s.memory.heap_min_free = s.memory.heap_free;
}

void tick_begin() {
    s.tick.tick_start_us = dt_micros();
    s.tick.phase_start_us = s.tick.tick_start_us;
    s.tick.phase_count = 0;
}

void mark(const char* phase) {
    uint32_t now = dt_micros();
    if (s.tick.phase_count < 8) {
        s.tick.phase_durations[s.tick.phase_count] = now - s.tick.phase_start_us;
        s.tick.phase_names[s.tick.phase_count] = phase;
        s.tick.phase_count++;
    }
    s.tick.phase_start_us = now;
}

void tick_end() {
    s.tick.tick_total_us = dt_micros() - s.tick.tick_start_us;
    s.tick.tick_count++;

    uint32_t t = s.tick.tick_total_us;
    if (t < s.tick.win_min) s.tick.win_min = t;
    if (t > s.tick.win_max) s.tick.win_max = t;
    s.tick.win_sum += t;
    s.tick.win_count++;

    if (s.tick.win_count >= TICK_WINDOW_SIZE) {
        s.tick.win_min = UINT32_MAX;
        s.tick.win_max = 0;
        s.tick.win_sum = 0;
        s.tick.win_count = 0;
    }
}

void eval_begin() {
    s.eval.start_us = dt_micros();
    event("eval", "begin");
}

void eval_end(bool success) {
    s.eval.last_us = dt_micros() - s.eval.start_us;
    if (s.eval.last_us > s.eval.max_us) s.eval.max_us = s.eval.last_us;
    s.eval.count++;
    count("eval_count");
    if (!success) s.eval.error_count++;
    event("eval", success ? "done" : "error",
          static_cast<int>(s.eval.last_us));
}

void event(const char* channel, const char* message, const char* detail) {
    auto& e = s.events[s.ev_head];
    e.ts_us = dt_micros();
    e.channel = channel;
    e.message = message;
    e.detail_str = detail;
    e.has_int = false;
    s.ev_head = (s.ev_head + 1) & 31;
    if (s.ev_count < 32) s.ev_count++;
}

void event(const char* channel, const char* message, int detail) {
    auto& e = s.events[s.ev_head];
    e.ts_us = dt_micros();
    e.channel = channel;
    e.message = message;
    e.detail_str = nullptr;
    e.detail_int = detail;
    e.has_int = true;
    s.ev_head = (s.ev_head + 1) & 31;
    if (s.ev_count < 32) s.ev_count++;
}

void count(const char* name) {
    for (uint8_t i = 0; i < s.counter_count; ++i) {
        if (std::strcmp(s.counters[i].name, name) == 0) {
            s.counters[i].value++;
            return;
        }
    }
    if (s.counter_count < 16) {
        s.counters[s.counter_count] = {name, 1};
        s.counter_count++;
    }
}

void gauge(const char* name, uint32_t value) {
    for (uint8_t i = 0; i < s.gauge_count; ++i) {
        if (std::strcmp(s.gauges[i].name, name) == 0) {
            s.gauges[i].value = value;
            return;
        }
    }
    if (s.gauge_count < 16) {
        s.gauges[s.gauge_count] = {name, value, 0};
        s.gauge_count++;
    }
}

void gauge(const char* name, uint32_t value, uint32_t capacity) {
    for (uint8_t i = 0; i < s.gauge_count; ++i) {
        if (std::strcmp(s.gauges[i].name, name) == 0) {
            s.gauges[i].value = value;
            s.gauges[i].capacity = capacity;
            return;
        }
    }
    if (s.gauge_count < 16) {
        s.gauges[s.gauge_count] = {name, value, capacity};
        s.gauge_count++;
    }
}

// ══════════════════════════════════════════════════════════════════════════
// JSON helpers
// ══════════════════════════════════════════════════════════════════════════

static void send_json(WriteFn write_fn, const String& json) {
    write_fn(json.c_str(), json.length());
}

static const char* mode_str(Mode m) {
    switch (m) {
        case OFF:    return "off";
        case POLL:   return "poll";
        case STREAM: return "stream";
        case EVENTS: return "events";
    }
    return "off";
}

static Mode parse_mode(const char* str) {
    if (!str) return OFF;
    if (strcmp(str, "poll") == 0)   return POLL;
    if (strcmp(str, "stream") == 0) return STREAM;
    if (strcmp(str, "events") == 0) return EVENTS;
    return OFF;
}

static size_t extract_str(const char* json, size_t len,
                          const char* key, char* out, size_t out_sz) {
    size_t klen = strlen(key);
    for (size_t i = 0; i + klen + 3 < len; ++i) {
        if (json[i] == '"' && memcmp(json + i + 1, key, klen) == 0 &&
            json[i + 1 + klen] == '"') {
            size_t vs = i + klen + 2;
            while (vs < len && (json[vs] == ':' || json[vs] == ' ')) vs++;
            if (vs < len && json[vs] == '"') {
                vs++;
                size_t ve = vs;
                while (ve < len && json[ve] != '"') ve++;
                size_t copy = (ve - vs < out_sz - 1) ? ve - vs : out_sz - 1;
                memcpy(out, json + vs, copy);
                out[copy] = '\0';
                return copy;
            }
        }
    }
    out[0] = '\0';
    return 0;
}

static int extract_int(const char* json, size_t len, const char* key, int def) {
    size_t klen = strlen(key);
    for (size_t i = 0; i + klen + 3 < len; ++i) {
        if (json[i] == '"' && memcmp(json + i + 1, key, klen) == 0 &&
            json[i + 1 + klen] == '"') {
            size_t vs = i + klen + 2;
            while (vs < len && (json[vs] == ':' || json[vs] == ' ')) vs++;
            if (vs < len && json[vs] >= '0' && json[vs] <= '9') {
                int val = 0;
                while (vs < len && json[vs] >= '0' && json[vs] <= '9') {
                    val = val * 10 + (json[vs] - '0');
                    vs++;
                }
                return val;
            }
        }
    }
    return def;
}

// ══════════════════════════════════════════════════════════════════════════
// Channel serializers
// ══════════════════════════════════════════════════════════════════════════

static String serialize_tick() {
    JsonBuilder j;
    j.object_begin()
        .field("tick_count", static_cast<int>(s.tick.tick_count))
        .field("last_total_us", static_cast<int>(s.tick.tick_total_us));

    {
        JsonBuilder phases;
        phases.object_begin();
        for (uint8_t i = 0; i < s.tick.phase_count; ++i) {
            if (s.tick.phase_names[i])
                phases.field(s.tick.phase_names[i], static_cast<int>(s.tick.phase_durations[i]));
        }
        phases.object_end();
        j.field_raw("phases", phases.build());
    }

    uint32_t avg = s.tick.win_count > 0 ? s.tick.win_sum / s.tick.win_count : 0;
    j.field("win_min_us", static_cast<int>(s.tick.win_min == UINT32_MAX ? 0 : s.tick.win_min))
     .field("win_max_us", static_cast<int>(s.tick.win_max))
     .field("win_avg_us", static_cast<int>(avg))
     .field("win_count", static_cast<int>(s.tick.win_count));

    j.object_end();
    return j.build();
}

static String serialize_graph(const char* output_filter) {
    if (!s.engine) return "[]";
    const auto& pool = s.engine->pool;

    JsonBuilder j;
    j.array_begin_unkeyed();

    for (uint16_t oi = 0; oi < 24; ++oi) {
        const auto& slot = pool.outputs[oi];
        if (slot.root_node == sig::NODE_NONE) continue;

        const char* name = output_name(oi);
        if (output_filter && output_filter[0] != '\0' && strcmp(output_filter, name) != 0)
            continue;

        j.object_begin()
            .field("name", name)
            .field("health", health_name(pool, oi))
            .field("root", static_cast<int>(slot.root_node));

        // Nodes
        j.array_begin("nodes");
        for (uint16_t ei = 0; ei < pool.exec_count; ++ei) {
            uint16_t ni = pool.exec_order[ei];
            if (ni >= pool.node_count) continue;
            const auto& node = pool.nodes[ni];

            j.object_begin()
                .field("id", static_cast<int>(ni))
                .field("op", node_op_name(node.op));

            if (op_has_imm(node.op))
                j.field("imm", static_cast<int>(node.imm));

            if (node.input_a != sig::NODE_NONE)
                j.field("a", static_cast<int>(node.input_a));
            if ((op_is_binary(node.op) || op_is_ternary(node.op)) &&
                node.input_b != sig::NODE_NONE)
                j.field("b", static_cast<int>(node.input_b));
            if (op_is_ternary(node.op) && node.input_c != sig::NODE_NONE)
                j.field("c", static_cast<int>(node.input_c));

            j.object_end();
        }
        j.array_end();

        // Source text
        if (oi < sig::MAX_OUTPUTS && s.engine->output_sources[oi].has_source) {
            const auto& os = s.engine->output_sources[oi];
            const char* src = s.engine->arena.read(os.arena_offset);
            if (src)
                j.field("source", String(src, static_cast<unsigned int>(os.arena_length)));
        }

        // Dependencies
        const auto& deps = pool.output_deps[oi];
        if (deps.count > 0) {
            JsonBuilder da;
            da.array_begin_unkeyed();
            for (uint8_t d = 0; d < deps.count; ++d) {
                const String& sym = getSymbolString(deps.cells[d]);
                if (sym.length() > 0)
                    da.field("", sym); // unkeyed string won't work; use raw
            }
            da.array_end();
            j.field_raw("deps", da.build());
        }

        j.object_end();
    }
    j.array_end();
    return j.build();
}

static String serialize_state() {
    if (!s.engine) return "{}";
    const auto& engine = *s.engine;

    JsonBuilder j;
    j.object_begin()
        .field("is_playing", engine.state.is_playing);

    // Cells
    j.array_begin("cells");
    for (uint16_t i = 1; i < sig::MAX_CELLS; ++i) {
        const auto& cell = engine.cells.cells[i];
        if (cell.kind == sig::CellKind::Empty) continue;
        const String& name = getSymbolString(static_cast<sig::SymbolID>(i));
        j.object_begin().field("name", name);
        switch (cell.kind) {
            case sig::CellKind::Number:   j.field("kind", "number"); break;
            case sig::CellKind::Data:     j.field("kind", "data"); break;
            case sig::CellKind::Callable: j.field("kind", "callable"); break;
            case sig::CellKind::Nil:      j.field("kind", "nil"); break;
            default: break;
        }
        j.object_end();
    }
    j.array_end();

    // Outputs
    j.array_begin("outputs");
    for (uint16_t i = 0; i < 24; ++i) {
        j.object_begin()
            .field("name", output_name(i))
            .field("health", health_name(engine.pool, i))
            .object_end();
    }
    j.array_end();

    // State slots
    if (engine.pool.state_slot_count > 0) {
        j.array_begin("state_slots");
        for (uint16_t i = 0; i < engine.pool.state_slot_count; ++i) {
            j.object_begin()
                .field("id", static_cast<int>(i))
                .object_end();
        }
        j.array_end();
    }

    j.object_end();
    return j.build();
}

static String serialize_eval_timing() {
    JsonBuilder j;
    j.object_begin()
        .field("last_us", static_cast<int>(s.eval.last_us))
        .field("max_us", static_cast<int>(s.eval.max_us))
        .field("count", static_cast<int>(s.eval.count))
        .field("error_count", static_cast<int>(s.eval.error_count))
        .object_end();
    return j.build();
}

static String serialize_eval() {
    JsonBuilder j;
    j.object_begin()
        .field("last_us", static_cast<int>(s.eval.last_us))
        .field("max_us", static_cast<int>(s.eval.max_us))
        .field("count", static_cast<int>(s.eval.count))
        .field("error_count", static_cast<int>(s.eval.error_count));
    {
        JsonBuilder events;
        events.array_begin_unkeyed();
        uint8_t count = s.ev_count < 32 ? s.ev_count : 32;
        for (uint8_t i = 0; i < count; ++i) {
            uint8_t idx = (s.ev_head - 1 - i) & 31;
            const auto& e = s.events[idx];
            if (!e.channel) continue;
            events.object_begin()
                .field("ts_us", static_cast<int>(e.ts_us))
                .field("channel", e.channel)
                .field("message", e.message);
            if (e.has_int)
                events.field("detail", e.detail_int);
            else if (e.detail_str)
                events.field("detail", e.detail_str);
            events.object_end();
        }
        events.array_end();
        j.field_raw("events", events.build());
    }
    j.object_end();
    return j.build();
}

static String serialize_resources() {
    sample_runtime_memory();
    sample_core0_stack();
    JsonBuilder j;
    j.object_begin()
        .field("heap_free", static_cast<int>(s.memory.heap_free))
        .field("heap_min_free", static_cast<int>(s.memory.heap_min_free))
        .field("core0_stack_margin_intact",
               s.memory.core0_stack_margin_intact);
    {
        JsonBuilder stack;
        stack.object_begin()
            .field("initialized", s.memory.core0_stack_initialized)
            .field("used", static_cast<int>(s.memory.core0_stack_high_water))
            .field("capacity", static_cast<int>(s.memory.core0_stack_capacity))
            .object_end();
        j.field_raw("core0_stack", stack.build());
    }
    if (s.engine) {
        uint16_t cells_used = 0;
        for (uint16_t i = 0; i < sig::MAX_CELLS; ++i) {
            if (s.engine->cells.cells[i].kind != sig::CellKind::Empty)
                cells_used++;
        }
        uint16_t data_entries_used = 0;
        if (s.engine->cells.data_table_count > 0) {
            const uint16_t last = s.engine->cells.data_table_count - 1;
            data_entries_used = static_cast<uint16_t>(
                s.engine->cells.data_offsets[last] +
                s.engine->cells.data_lengths[last]);
        }
        auto ratio = [&j](const char* name, uint32_t used,
                          uint32_t capacity) {
            JsonBuilder value;
            value.object_begin()
                .field("used", static_cast<int>(used))
                .field("capacity", static_cast<int>(capacity))
                .object_end();
            j.field_raw(name, value.build());
        };
        ratio("nodes", s.engine->pool.node_count, sig::MAX_TOTAL_NODES);
        ratio("arena", s.engine->arena.write_head, sig::SOURCE_ARENA_SIZE);
        ratio("cells", cells_used, sig::MAX_CELLS);
        ratio("data_entries", data_entries_used, sig::MAX_DATA_ENTRIES);
        ratio("state_slots", s.engine->pool.state_slot_count,
              sig::MAX_STATE_SLOTS);
        ratio("live_slots", s.engine->pool.live_slot_count,
              sig::MAX_LIVE_SLOTS);
#if USEQ_HAS_SYNTH_ENGINE
        ratio("synth_declarations", s.engine->synth_graph.declaration_count(),
              sig::MAX_SYNTH_DECLARATIONS);
        ratio("synth_controls", s.engine->synth_graph.control_count(),
              sig::MAX_SYNTH_CONTROLS);
#endif
    }
    for (uint8_t i = 0; i < s.gauge_count; ++i) {
        if (s.gauges[i].capacity > 0) {
            JsonBuilder g;
            g.object_begin()
                .field("used", static_cast<int>(s.gauges[i].value))
                .field("capacity", static_cast<int>(s.gauges[i].capacity))
                .object_end();
            j.field_raw(s.gauges[i].name, g.build());
        } else {
            j.field(s.gauges[i].name, static_cast<int>(s.gauges[i].value));
        }
    }
    j.object_end();
    return j.build();
}

static String serialize_io() {
    if (!s.engine) return "{}";
    JsonBuilder j;
    j.object_begin();
    j.array_begin("outputs");
    for (uint16_t i = 0; i < 24; ++i) {
        j.object_begin()
            .field("name", output_name(i))
            .object_end();
    }
    j.array_end();
    j.object_end();
    return j.build();
}

static String serialize_protocol() {
    JsonBuilder j;
    j.object_begin();
    for (uint8_t i = 0; i < s.counter_count; ++i)
        j.field(s.counters[i].name, static_cast<int>(s.counters[i].value));
    if (s.tick.tick_count > 0 && s.tick.win_count > 0) {
        uint32_t avg_us = s.tick.win_sum / s.tick.win_count;
        uint32_t uptime_s = static_cast<uint32_t>(
            (static_cast<uint64_t>(s.tick.tick_count) * avg_us) / 1000000ULL);
        j.field("uptime_s", static_cast<int>(uptime_s));
    }
    j.object_end();
    return j.build();
}

// ══════════════════════════════════════════════════════════════════════════
// Protocol handler
// ══════════════════════════════════════════════════════════════════════════

static const struct { const char* name; Channel ch; } s_ch_map[] = {
    {"tick", CH_TICK}, {"graph", CH_GRAPH}, {"state", CH_STATE},
    {"eval", CH_EVAL}, {"resources", CH_RESOURCES},
    {"io", CH_IO}, {"protocol", CH_PROTOCOL},
};

static Channel parse_channel_name(const char* name) {
    for (const auto& m : s_ch_map)
        if (strcmp(name, m.name) == 0) return m.ch;
    return CH_COUNT;
}

static void handle_capabilities(const char* json, size_t len, WriteFn write_fn) {
    char req_id[64] = {};
    extract_str(json, len, "requestId", req_id, sizeof(req_id));

    static const struct { const char* name; const char* modes; const char* desc; } ch_info[] = {
        {"tick",      "[\"off\",\"poll\",\"stream\"]", "Per-phase tick timing (us)"},
        {"graph",     "[\"off\",\"poll\"]",            "Signal graph topology and node values"},
        {"state",     "[\"off\",\"poll\"]",            "Cells, outputs, health, LKG"},
        {"eval",      "[\"off\",\"events\"]",          "Compilation events and recompile cascades"},
        {"resources", "[\"off\",\"poll\",\"stream\"]", "Heap, node pool, arena utilization"},
        {"io",        "[\"off\",\"poll\",\"stream\"]", "Hardware input and output values"},
        {"protocol",  "[\"off\",\"poll\",\"stream\"]", "Message counters, drops, backpressure"},
    };

    JsonBuilder j;
    j.object_begin()
        .field("type", "response")
        .field("requestId", req_id)
        .field("success", true)
        .field("devtools", true);

    j.array_begin("channels");
    for (int i = 0; i < CH_COUNT; ++i) {
        j.object_begin()
            .field("name", ch_info[i].name)
            .field_raw("modes", ch_info[i].modes)
            .field("current", mode_str(s.modes[i]))
            .field("description", ch_info[i].desc)
            .object_end();
    }
    j.array_end();
    j.object_end();

    send_json(write_fn, j.build());
}

static void handle_configure(const char* json, size_t len, WriteFn write_fn) {
    char req_id[64] = {};
    extract_str(json, len, "requestId", req_id, sizeof(req_id));

    for (const auto& cm : s_ch_map) {
        char mode_buf[16] = {};
        if (extract_str(json, len, cm.name, mode_buf, sizeof(mode_buf)) > 0)
            s.modes[cm.ch] = parse_mode(mode_buf);
    }

    int rate = extract_int(json, len, "streamRateHz", 0);
    if (rate >= 1 && rate <= 100)
        s.stream_interval_us = 1000000 / static_cast<uint32_t>(rate);

    JsonBuilder j;
    j.object_begin()
        .field("type", "response")
        .field("requestId", req_id)
        .field("success", true);
    {
        JsonBuilder ch;
        ch.object_begin();
        for (const auto& cm : s_ch_map)
            ch.field(cm.name, mode_str(s.modes[cm.ch]));
        ch.object_end();
        j.field_raw("channels", ch.build());
    }
    j.object_end();
    send_json(write_fn, j.build());
}

static void handle_query(const char* json, size_t len, WriteFn write_fn) {
    char req_id[64] = {};
    char channel[32] = {};
    char output[8] = {};
    extract_str(json, len, "requestId", req_id, sizeof(req_id));
    extract_str(json, len, "channel", channel, sizeof(channel));
    extract_str(json, len, "output", output, sizeof(output));

    String data;
    Channel ch = parse_channel_name(channel);
    switch (ch) {
        case CH_TICK:      data = serialize_tick(); break;
        case CH_GRAPH:     data = serialize_graph(output); break;
        case CH_STATE:     data = serialize_state(); break;
        case CH_EVAL:
            data = strcmp(output, "timing") == 0
                ? serialize_eval_timing() : serialize_eval();
            break;
        case CH_RESOURCES: data = serialize_resources(); break;
        case CH_IO:        data = serialize_io(); break;
        case CH_PROTOCOL:  data = serialize_protocol(); break;
        default: {
            JsonBuilder err;
            err.object_begin()
                .field("type", "response")
                .field("requestId", req_id)
                .field("success", false)
                .field("error", "unknown channel")
                .object_end();
            send_json(write_fn, err.build());
            return;
        }
    }

    JsonBuilder j;
    j.object_begin()
        .field("type", "response")
        .field("requestId", req_id)
        .field("success", true)
        .field("channel", channel)
        .field_raw("data", data)
        .object_end();
    send_json(write_fn, j.build());
}

static void handle_status(const char* json, size_t len, WriteFn write_fn) {
    char req_id[64] = {};
    extract_str(json, len, "requestId", req_id, sizeof(req_id));

    JsonBuilder j;
    j.object_begin()
        .field("type", "response")
        .field("requestId", req_id)
        .field("success", true);
    {
        JsonBuilder ch;
        ch.object_begin();
        for (const auto& cm : s_ch_map)
            ch.field(cm.name, mode_str(s.modes[cm.ch]));
        ch.object_end();
        j.field_raw("channels", ch.build());
    }
    j.field("streamRateHz", static_cast<int>(1000000 / s.stream_interval_us));
    j.object_end();
    send_json(write_fn, j.build());
}

bool handle_debug_message(const char* json, size_t len, WriteFn write_fn) {
    char action[32] = {};
    extract_str(json, len, "action", action, sizeof(action));

    if (strcmp(action, "capabilities") == 0) { handle_capabilities(json, len, write_fn); return true; }
    if (strcmp(action, "configure") == 0)     { handle_configure(json, len, write_fn); return true; }
    if (strcmp(action, "query") == 0)         { handle_query(json, len, write_fn); return true; }
    if (strcmp(action, "status") == 0)        { handle_status(json, len, write_fn); return true; }
    return false;
}

void emit_streaming(WriteFn write_fn, bool can_write) {
    if (!can_write) return;

    uint32_t now = dt_micros();
    if (now - s.last_stream_us < s.stream_interval_us) return;
    s.last_stream_us = now;

    auto emit = [&](Channel ch, const char* name, String (*fn)()) {
        if (s.modes[ch] != STREAM) return;
        JsonBuilder j;
        j.object_begin()
            .field("type", "debug")
            .field("channel", name)
            .field_raw("data", fn())
            .object_end();
        send_json(write_fn, j.build());
    };

    emit(CH_TICK,      "tick",      serialize_tick);
    emit(CH_RESOURCES, "resources", serialize_resources);
    emit(CH_IO,        "io",        serialize_io);
    emit(CH_PROTOCOL,  "protocol",  serialize_protocol);
}

} // namespace dt

#endif // USEQ_DEVTOOLS
