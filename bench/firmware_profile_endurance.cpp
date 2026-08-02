// Native endurance harness compiled with USEQ_FIRMWARE_PROFILE.
//
// This preserves the RP2040 compiler/runtime capacities while executing in a
// host process. It is evidence for bounded-resource behaviour and semantic
// recovery, not for RP2040 timing, allocator behaviour, or peripheral I/O.

#include "src/signal_engine/cold_eval.h"
#include "src/signal_engine/executor.h"
#include "src/signal_engine/graph_builder.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace sig;
using Clock = std::chrono::steady_clock;

static SignalEngine engine;
static double cell_snapshot[MAX_CELLS];
static double workspace[MAX_TOTAL_NODES];
static double output_values[MAX_OUTPUTS];
static double hw_inputs[32] = {};

static std::vector<std::string> parse_forms(const std::string& source) {
    std::vector<std::string> forms;
    std::string current;
    int depth = 0;
    bool in_comment = false;
    bool in_string = false;
    for (char ch : source) {
        if (in_comment) {
            if (ch == '\n') in_comment = false;
            continue;
        }
        if (!in_string && ch == ';') {
            in_comment = true;
            continue;
        }
        if (depth == 0 && (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'))
            continue;
        current.push_back(ch);
        if (in_string) {
            if (ch == '"') in_string = false;
            continue;
        }
        if (ch == '"') {
            in_string = true;
        } else if (ch == '(' || ch == '[') {
            depth++;
        } else if (ch == ')' || ch == ']') {
            depth--;
            if (depth == 0) {
                forms.push_back(current);
                current.clear();
            }
        }
    }
    return forms;
}

static bool read_file(const char* path, std::string& output) {
    FILE* stream = std::fopen(path, "rb");
    if (!stream) return false;
    char buffer[4096];
    size_t count = 0;
    while ((count = std::fread(buffer, 1, sizeof(buffer), stream)) > 0)
        output.append(buffer, count);
    std::fclose(stream);
    return true;
}

static bool eval_ok(const std::string& form) {
    EvalResult result = eval_cold(form.c_str(),
                                  static_cast<uint32_t>(form.size()), engine);
    if (result.kind != EvalResult::Error) return true;
    const char* message = result.diagnostic_count > 0
        ? result.diagnostics[0].message : "unknown diagnostic";
    std::fprintf(stderr, "eval failed: %s: %s\n", form.c_str(),
                 message ? message : "unknown diagnostic");
    return false;
}

static bool load_program(const std::vector<std::string>& forms) {
    for (const std::string& form : forms) {
        if (!eval_ok(form)) return false;
    }
    engine.pool.rebuild_execution_order();
    return true;
}

static double percentile(std::vector<double> values, double quantile) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const size_t index = static_cast<size_t>(
        quantile * static_cast<double>(values.size() - 1) + 0.5);
    return values[index];
}

struct Resources {
    uint16_t nodes;
    uint16_t state_slots;
    uint16_t live_slots;
    uint32_t arena;
    uint16_t data_tables;
    uint16_t synth_declarations;
    uint16_t synth_controls;
    uint16_t a1_root;
    uint32_t a1_source_offset;
    uint32_t a1_source_length;
};

static Resources resources() {
    return {
        engine.pool.node_count,
        engine.pool.state_slot_count,
        engine.pool.live_slot_count,
        engine.arena.write_head,
        engine.cells.data_table_count,
        engine.synth_graph.declaration_count(),
        engine.synth_graph.control_count(),
        engine.pool.outputs[0].root_node,
        engine.output_sources[0].arena_offset,
        engine.output_sources[0].arena_length,
    };
}

static bool same_resources(const Resources& left, const Resources& right) {
    return left.nodes == right.nodes &&
           left.state_slots == right.state_slots &&
           left.live_slots == right.live_slots &&
           left.arena == right.arena &&
           left.data_tables == right.data_tables &&
           left.synth_declarations == right.synth_declarations &&
           left.synth_controls == right.synth_controls &&
           left.a1_root == right.a1_root &&
           left.a1_source_offset == right.a1_source_offset &&
           left.a1_source_length == right.a1_source_length;
}

static void update_max(Resources& maximum, const Resources& current) {
    maximum.nodes = std::max(maximum.nodes, current.nodes);
    maximum.state_slots = std::max(maximum.state_slots, current.state_slots);
    maximum.live_slots = std::max(maximum.live_slots, current.live_slots);
    maximum.arena = std::max(maximum.arena, current.arena);
    maximum.data_tables = std::max(maximum.data_tables, current.data_tables);
    maximum.synth_declarations = std::max(
        maximum.synth_declarations, current.synth_declarations);
    maximum.synth_controls = std::max(maximum.synth_controls,
                                      current.synth_controls);
}

int main(int argc, char** argv) {
    const char* program_path = argc > 1
        ? argv[1] : "bench/firmware-corpus/high-combined.useq";
    const int cycles = argc > 2 ? std::atoi(argv[2]) : 2000;
    const int ticks_per_cycle = argc > 3 ? std::atoi(argv[3]) : 32;
    if (cycles <= 0 || ticks_per_cycle <= 0) {
        std::fprintf(stderr, "cycles and ticks-per-cycle must be positive\n");
        return 2;
    }

    std::string source;
    if (!read_file(program_path, source)) {
        std::fprintf(stderr, "cannot read %s\n", program_path);
        return 2;
    }
    const std::vector<std::string> forms = parse_forms(source);
    if (forms.empty()) {
        std::fprintf(stderr, "no forms in %s\n", program_path);
        return 2;
    }

    engine.init_defaults();
    if (!load_program(forms)) return 1;

    const std::string replacements[] = {
        "(a1 (slew (usin beat) 2))",
        "(a1 (slew (ucos beat) 2))",
    };
    if (replacements[0].size() != replacements[1].size()) {
        std::fprintf(stderr, "replacement source slots must have equal size\n");
        return 2;
    }

    uint64_t checksum = 1469598103934665603ULL;
    uint64_t tick_count = 0;
    int invalid_rejections = 0;
    int reloads = 0;
    double time = 0.0;
    double previous_time = 0.0;
    std::vector<double> compile_us;
    std::vector<double> tick_ns;
    compile_us.reserve(static_cast<size_t>(cycles));
    tick_ns.reserve(static_cast<size_t>(cycles));
    Resources maximum = resources();
    uint32_t stable_arena = 0;

    for (int cycle = 0; cycle < cycles; ++cycle) {
        if (cycle > 0 && cycle % 500 == 0) {
            if (!eval_ok("(useq-clear)")) return 1;
            if (!load_program(forms)) return 1;
            previous_time = 0.0;
            time = 0.0;
            stable_arena = 0;
            reloads++;
        }

        const std::string& replacement = replacements[cycle & 1];
        const auto compile_start = Clock::now();
        if (!eval_ok(replacement)) return 1;
        const auto compile_end = Clock::now();
        compile_us.push_back(std::chrono::duration<double, std::micro>(
            compile_end - compile_start).count());

        const Resources after_compile = resources();
        update_max(maximum, after_compile);
        if (stable_arena == 0) {
            stable_arena = after_compile.arena;
        } else if (after_compile.arena != stable_arena) {
            std::fprintf(stderr,
                         "source arena changed within replacement segment: "
                         "%u -> %u at cycle %d\n",
                         stable_arena, after_compile.arena, cycle);
            return 1;
        }

        if (cycle % 97 == 0) {
            const Resources before_invalid = resources();
            const std::string invalid = "(a1 (sin 1 2))";
            const EvalResult rejected = eval_cold(
                invalid.c_str(), static_cast<uint32_t>(invalid.size()), engine);
            if (rejected.kind != EvalResult::Error) {
                std::fprintf(stderr, "invalid replacement was accepted\n");
                return 1;
            }
            if (!same_resources(before_invalid, resources())) {
                std::fprintf(stderr,
                             "invalid replacement changed retained resources\n");
                return 1;
            }
            invalid_rejections++;
        }

        const auto tick_start = Clock::now();
        for (int tick = 0; tick < ticks_per_cycle; ++tick) {
            time += 0.0005;
            for (uint16_t slot = 0; slot < engine.pool.live_slot_count; ++slot) {
                const double value = static_cast<double>((cycle + tick + slot) % 101)
                    / 100.0;
                engine.pool.set_live_slot_value_by_index(slot, value);
            }
            engine.cells.snapshot_values(cell_snapshot, MAX_CELLS);
            ExecutionContext context;
            context.t = time;
            context.dt = time - previous_time;
            context.cell_values = cell_snapshot;
            context.hw_inputs = hw_inputs;
            context.data_pool = engine.cells.data_pool;
            context.data_offsets = engine.cells.data_offsets;
            context.data_lengths = engine.cells.data_lengths;
            context.prev_outputs = engine.pool.prev_output_values;
            context.output_values = output_values;
            context.workspace = workspace;
            execute_all_outputs(engine.pool, context);
            commit_state(engine.pool, workspace);
            commit_outputs(engine.pool, output_values);
            previous_time = time;
            tick_count++;

            for (double value : output_values) {
                if (!std::isfinite(value)) {
                    std::fprintf(stderr,
                                 "non-finite output at cycle %d tick %d\n",
                                 cycle, tick);
                    return 1;
                }
                uint64_t bits = 0;
                std::memcpy(&bits, &value, sizeof(bits));
                checksum ^= bits;
                checksum *= 1099511628211ULL;
            }
        }
        const auto tick_end = Clock::now();
        tick_ns.push_back(std::chrono::duration<double, std::nano>(
            tick_end - tick_start).count() / ticks_per_cycle);
    }

    if (!eval_ok("(a1 0.25)")) return 1;
    engine.pool.rebuild_execution_order();

    std::printf(
        "{\"pass\":true,\"profile\":\"rp2040-firmware-native\","
        "\"cycles\":%d,\"ticks\":%llu,\"invalid_rejections\":%d,"
        "\"reloads\":%d,\"compile_p99_us\":%.6f,"
        "\"tick_p99_ns\":%.6f,\"max_nodes\":%u,"
        "\"max_state_slots\":%u,\"max_live_slots\":%u,"
        "\"max_arena_bytes\":%u,\"max_data_tables\":%u,"
        "\"max_synth_declarations\":%u,\"max_synth_controls\":%u,"
        "\"checksum\":\"%016llx\"}\n",
        cycles, static_cast<unsigned long long>(tick_count),
        invalid_rejections, reloads, percentile(compile_us, 0.99),
        percentile(tick_ns, 0.99), maximum.nodes, maximum.state_slots,
        maximum.live_slots, maximum.arena, maximum.data_tables,
        maximum.synth_declarations, maximum.synth_controls,
        static_cast<unsigned long long>(checksum));
    return 0;
}
