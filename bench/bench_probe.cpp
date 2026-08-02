// bench_probe — native benchmark harness for the uSEQ signal engine.
//
// Loads a corpus file (a sequence of top-level ModuLisp forms, ';' comments
// allowed), measures:
//   compile:  cold-eval wall time (median + p99 over N runs of the full
//             program into a freshly re-initialised engine), single-cell
//             recompile time (re-evaluating the LAST top-level form),
//             node/exec counts, state slots, live slots.
//             (Node count *before* optimization is not exposed by the public
//             API — CSE/folding happen inline at node construction — so only
//             the post-optimization count is reported.)
//   execute:  ns/tick — median over batches totalling >= 1e6 ticks of the
//             full execute/commit loop (with live-slot churn if slots exist).
//   capacity: retained-resource use and the corresponding compile-time limit.
//
// Emits JSONL: {"workload":..., "phase":..., "metric":..., "value":..., "unit":...}
//
// Build: bench/build.sh (standalone g++ -O2; independent of test/meson.build)

#include "src/signal_engine/cold_eval.h"
#include "src/signal_engine/executor.h"
#include "src/signal_engine/graph_builder.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

using namespace sig;
using Clock = std::chrono::steady_clock;

static SignalEngine engine; // too big for the stack
static double cell_snapshot[MAX_CELLS];
static double workspace[MAX_TOTAL_NODES];
static double output_values[MAX_OUTPUTS];
static double hw_inputs[32] = {};

static const char* g_workload = "?";
static int g_errors = 0;

static void emit(const char* phase, const char* metric, double value, const char* unit) {
    printf("{\"workload\":\"%s\",\"phase\":\"%s\",\"metric\":\"%s\",\"value\":%.6g,\"unit\":\"%s\"}\n",
           g_workload, phase, metric, value, unit);
}

static uint16_t cells_used(const SignalEngine& current) {
    uint16_t used = 0;
    for (uint16_t i = 0; i < MAX_CELLS; ++i) {
        if (current.cells.cells[i].kind != CellKind::Empty) used++;
    }
    return used;
}

static uint16_t data_entries_used(const SignalEngine& current) {
    if (current.cells.data_table_count == 0) return 0;
    const uint16_t last = current.cells.data_table_count - 1;
    return static_cast<uint16_t>(current.cells.data_offsets[last] +
                                 current.cells.data_lengths[last]);
}

// Split file into top-level forms; strip ';' line comments.
static std::vector<std::string> parse_forms(const std::string& src) {
    std::vector<std::string> forms;
    int depth = 0;
    std::string cur;
    bool in_comment = false, in_string = false;
    for (char c : src) {
        if (in_comment) { if (c == '\n') in_comment = false; continue; }
        if (!in_string && c == ';') { in_comment = true; continue; }
        if (depth == 0 && (c == '\n' || c == '\r' || c == ' ' || c == '\t')) continue;
        cur.push_back(c);
        if (in_string) { if (c == '"') in_string = false; continue; }
        if (c == '"') { in_string = true; continue; }
        if (c == '(' || c == '[') depth++;
        else if (c == ')' || c == ']') {
            depth--;
            if (depth == 0) { forms.push_back(cur); cur.clear(); }
        }
    }
    return forms;
}

static void reset_engine() {
    new (&engine) SignalEngine();
    engine.init_defaults();
}

static bool eval_form(const std::string& f) {
    EvalResult r = eval_cold(f.c_str(), (uint32_t)f.size(), engine);
    if (r.kind == EvalResult::Error) {
        const char* msg = r.diagnostic_count > 0 ? r.diagnostics[0].message : "?";
        fprintf(stderr, "[%s] EVAL ERROR in %s: %s\n", g_workload, f.c_str(), msg ? msg : "?");
        g_errors++;
        return false;
    }
    return true;
}

static double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    return n == 0 ? 0.0 : (n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]));
}

static double percentile(std::vector<double> v, double p) {
    std::sort(v.begin(), v.end());
    if (v.empty()) return 0.0;
    size_t i = (size_t)(p * (v.size() - 1) + 0.5);
    return v[i];
}

int main(int argc, char* argv[]) {
    if (argc < 2) { fprintf(stderr, "usage: bench_probe <corpus.useq> [workload-name]\n"); return 2; }
    const char* path = argv[1];
    // derive workload name from filename if not given
    std::string wl;
    if (argc >= 3) wl = argv[2];
    else {
        wl = path;
        size_t s = wl.find_last_of('/');
        if (s != std::string::npos) wl = wl.substr(s + 1);
        size_t d = wl.rfind(".useq");
        if (d != std::string::npos) wl = wl.substr(0, d);
    }
    g_workload = wl.c_str();

    FILE* fp = fopen(path, "rb");
    if (!fp) { fprintf(stderr, "cannot open %s\n", path); return 2; }
    std::string src;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, fp)) > 0) src.append(buf, n);
    fclose(fp);

    std::vector<std::string> forms = parse_forms(src);
    if (forms.empty()) { fprintf(stderr, "no forms in %s\n", path); return 2; }

    GraphBuilder::init_symbols();

    // ── Compile: cold-eval median + p99 over COLD_RUNS runs ────────────────
    const int COLD_RUNS = 60;
    std::vector<double> cold_ms;
    cold_ms.reserve(COLD_RUNS);
    for (int run = 0; run < COLD_RUNS; run++) {
        reset_engine();
        auto t0 = Clock::now();
        for (const auto& f : forms) eval_form(f);
        auto t1 = Clock::now();
        cold_ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        if (g_errors) break; // bail early on broken corpus
    }
    if (g_errors) {
        emit("compile", "eval_errors", g_errors, "count");
        return 1;
    }
    emit("compile", "cold_eval_median", median(cold_ms), "ms");
    emit("compile", "cold_eval_p99", percentile(cold_ms, 0.99), "ms");

    // ── Compile: single-cell recompile (re-eval last form repeatedly) ──────
    {
        // NOTE: the source arena is a bump allocator — every re-eval of an
        // output/define stores its text again, so long recompile loops can
        // exhaust the arena. Stop at the first error and report successful
        // samples only (plus how many re-evals fit before exhaustion).
        const std::string& last = forms.back();
        const int RE_RUNS = 200;
        std::vector<double> re_us;
        re_us.reserve(RE_RUNS);
        int re_fail_at = -1;
        for (int i = 0; i < RE_RUNS; i++) {
            auto t0 = Clock::now();
            EvalResult r = eval_cold(last.c_str(), (uint32_t)last.size(), engine);
            auto t1 = Clock::now();
            if (r.kind == EvalResult::Error) { re_fail_at = i; break; }
            re_us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
        emit("compile", "recompile_median", median(re_us), "us");
        emit("compile", "recompile_success_count", re_us.size(), "evals");
        if (re_fail_at >= 0)
            emit("compile", "recompile_arena_exhausted_at", re_fail_at, "evals");
    }

    // ── Static counts (post-optimization; pre-opt counts not exposed) ─────
    emit("compile", "node_count", engine.pool.node_count, "nodes");
    emit("compile", "exec_count", engine.pool.exec_count, "nodes");
    emit("compile", "state_slots", engine.pool.state_slot_count, "slots");
    emit("compile", "live_slots", engine.pool.live_slot_count, "slots");
    emit("capacity", "nodes_used", engine.pool.node_count, "nodes");
    emit("capacity", "nodes_capacity", MAX_TOTAL_NODES, "nodes");
    emit("capacity", "cells_used", cells_used(engine), "cells");
    emit("capacity", "cells_capacity", MAX_CELLS, "cells");
    emit("capacity", "arena_used", engine.arena.write_head, "bytes");
    emit("capacity", "arena_capacity", SOURCE_ARENA_SIZE, "bytes");
    emit("capacity", "data_entries_used", data_entries_used(engine), "entries");
    emit("capacity", "data_entries_capacity", MAX_DATA_ENTRIES, "entries");
    emit("capacity", "state_slots_used", engine.pool.state_slot_count, "slots");
    emit("capacity", "state_slots_capacity", MAX_STATE_SLOTS, "slots");
    emit("capacity", "live_slots_used", engine.pool.live_slot_count, "slots");
    emit("capacity", "live_slots_capacity", MAX_LIVE_SLOTS, "slots");
    emit("capacity", "synth_declarations_used",
         engine.synth_graph.declaration_count(), "declarations");
    emit("capacity", "synth_declarations_capacity", MAX_SYNTH_DECLARATIONS,
         "declarations");
    emit("capacity", "synth_controls_used", engine.synth_graph.control_count(),
         "controls");
    emit("capacity", "synth_controls_capacity", MAX_SYNTH_CONTROLS, "controls");

    // ── Execute: ns/tick, median over batches totalling >= 1e6 ticks ──────
    engine.pool.rebuild_execution_order();
    const int BATCHES = 50;
    const int TICKS_PER_BATCH = 20000; // 50 * 20k = 1e6
    std::vector<double> batch_ns_per_tick;
    batch_ns_per_tick.reserve(BATCHES);
    double t = 0.0, prev_t = 0.0;
    const double DT = 0.0005; // 2 kHz tick
    uint16_t churn_slots = engine.pool.live_slot_count;
    double churn_val = 0.0;
    for (int b = 0; b < BATCHES; b++) {
        auto t0 = Clock::now();
        for (int i = 0; i < TICKS_PER_BATCH; i++) {
            t += DT;
            if (churn_slots) {
                churn_val += 0.001;
                if (churn_val > 1.0) churn_val = 0.0;
                engine.pool.set_live_slot_value_by_index(
                    (uint16_t)(i % churn_slots), churn_val);
            }
            engine.cells.snapshot_values(cell_snapshot, MAX_CELLS);
            ExecutionContext ctx;
            ctx.t             = t;
            ctx.dt            = t - prev_t;
            ctx.cell_values   = cell_snapshot;
            ctx.hw_inputs     = hw_inputs;
            ctx.data_pool     = engine.cells.data_pool;
            ctx.data_offsets  = engine.cells.data_offsets;
            ctx.data_lengths  = engine.cells.data_lengths;
            ctx.prev_outputs  = engine.pool.prev_output_values;
            ctx.output_values = output_values;
            ctx.workspace     = workspace;
            execute_all_outputs(engine.pool, ctx);
            for (double value : output_values) {
                if (!std::isfinite(value)) {
                    emit("execute", "non_finite_outputs", 1, "count");
                    return 1;
                }
            }
            commit_state(engine.pool, workspace);
            commit_outputs(engine.pool, output_values);
            prev_t = t;
        }
        auto t1 = Clock::now();
        double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        batch_ns_per_tick.push_back(ns / TICKS_PER_BATCH);
    }
    emit("execute", "ns_per_tick_median", median(batch_ns_per_tick), "ns");
    // sink so the loop can't be optimized away
    fprintf(stderr, "[%s] sink=%f\n", g_workload, output_values[0]);
    return 0;
}
