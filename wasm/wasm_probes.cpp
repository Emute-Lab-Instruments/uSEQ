
#include "wasm_wrapper_internal.h"

// ── Probe expression slots (probes.md §1.6) ────────────────────────────
//
// Compile-once, sample-many cache for probe oscilloscopes.  Each slot
// holds a compiled signal graph that persists across ticks.  Recompilation
// is triggered only when the expression text changes or cells are mutated
// by a user eval (g_cell_revision bump).

constexpr int MAX_PROBE_SLOTS = 8;

struct ProbeSlot {
    bool active       = false;
    bool compiled     = false;
    uint32_t revision = 0;       // g_cell_revision at compile time
    char code[512]    = {};
    uint32_t code_len = 0;
};

static ProbeSlot   g_probe_slots[MAX_PROBE_SLOTS] = {};
static sig::NodePool* g_probe_pool = nullptr;
static bool  g_probe_dirty      = false;

void reset_probe_session()
{
    for (ProbeSlot& slot : g_probe_slots) slot = ProbeSlot{};
    if (g_probe_pool) g_probe_pool->reset();
    g_probe_dirty = false;
}

static void recompile_probe_pool() {
    if (!g_engine) return;

    if (!g_probe_pool) {
        g_probe_pool = new sig::NodePool();
    }
    g_probe_pool->reset();

    uint8_t saved_table_count = g_engine->cells.data_table_count;

    for (int i = 0; i < MAX_PROBE_SLOTS; i++) {
        ProbeSlot& slot = g_probe_slots[i];
        g_probe_pool->outputs[i].valid = false;
        slot.compiled = false;

        if (!slot.active || slot.code_len == 0) continue;

        sig::Token tokens[sig::MAX_TOKENS];
        sig::Diagnostic parse_errors[8];
        uint8_t err_count = 0;
        uint16_t count = sig::TokenStream::tokenize(
            slot.code, slot.code_len, tokens, sig::MAX_TOKENS,
            parse_errors, &err_count);
        if (err_count > 0) continue;

        sig::TokenStream ts;
        memcpy(ts.tokens, tokens, count * sizeof(sig::Token));
        ts.count = count;
        ts.pos   = 0;

        sig::GraphBuildResult result = sig::build_output_graph(
            *g_probe_pool, ts, g_engine->cells, g_engine->arena, slot.code);
        if (result.has_error) continue;

        g_probe_pool->outputs[i].root_node = result.root_node;
        g_probe_pool->outputs[i].valid     = true;
        slot.compiled  = true;
        slot.revision  = g_cell_revision;
    }

    g_probe_pool->rebuild_execution_order();
    g_engine->cells.data_table_count = saved_table_count;
    g_probe_dirty = false;
}
extern "C" {

    int useq_probe_set(int slot, const char* code)
    {
        if (!g_engine || slot < 0 || slot >= MAX_PROBE_SLOTS) return -1;

        ProbeSlot& s = g_probe_slots[slot];
        uint32_t len = code ? (uint32_t)strlen(code) : 0;

        // Clear slot
        if (len == 0) {
            if (s.active) { s.active = false; g_probe_dirty = true; }
            s.compiled = false;
            s.code_len = 0;
            return 0;
        }

        if (len >= sizeof(s.code)) {
            s_last_error = "Probe expression too long";
            return -1;
        }

        // Skip if code + cell revision are unchanged and compilation succeeded
        if (s.active && s.compiled &&
            s.revision == g_cell_revision &&
            s.code_len == len && memcmp(s.code, code, len) == 0) {
            return 0;
        }

        memcpy(s.code, code, len);
        s.code[len] = '\0';
        s.code_len  = len;
        s.active    = true;
        s.compiled  = false;
        g_probe_dirty = true;
        return 0;
    }

    int useq_probe_sample(int slot, double start_time, double end_time,
                          int num_samples, int buffer_ptr, int buffer_capacity)
    {
        if (!g_engine || slot < 0 || slot >= MAX_PROBE_SLOTS) return -1;
        if (num_samples < 1 || num_samples > buffer_capacity) return -1;

        // Lazy recompile on first sample after a change
        if (g_probe_dirty) recompile_probe_pool();

        if (!g_probe_pool || !g_probe_pool->outputs[slot].valid) return -1;

        double* buf = reinterpret_cast<double*>(buffer_ptr);
        double dt = (num_samples > 1)
            ? (end_time - start_time) / (double)(num_samples - 1)
            : 0.0;

        // Snapshot cell values once (picks up live define/defstate changes)
        double cell_values[sig::MAX_CELLS];
        g_engine->cells.snapshot_values(cell_values, sig::MAX_CELLS);

        for (int i = 0; i < num_samples; i++) {
            double t = start_time + dt * i;

            double outputs[sig::MAX_OUTPUTS]  = {};
            double workspace[sig::MAX_TOTAL_NODES] = {};

            sig::ExecutionContext ctx;
            ctx.t             = t;
            ctx.dt            = (i > 0) ? dt : (dt > 0 ? dt : 0.001);
            ctx.cell_values   = cell_values;
            ctx.hw_inputs     = g_hw_inputs;
            ctx.data_pool     = g_engine->cells.data_pool;
            ctx.data_offsets  = g_engine->cells.data_offsets;
            ctx.data_lengths  = g_engine->cells.data_lengths;
            ctx.prev_outputs  = g_engine->pool.prev_output_values;
            ctx.output_values = outputs;
            ctx.workspace     = workspace;

            sig::execute_all_outputs(*g_probe_pool, ctx);
            buf[i] = outputs[slot];
        }

        return num_samples;
    }

    void useq_probe_free(int slot)
    {
        if (slot < 0 || slot >= MAX_PROBE_SLOTS) return;
        ProbeSlot& s = g_probe_slots[slot];
        if (s.active) {
            s.active   = false;
            s.compiled = false;
            s.code_len = 0;
            g_probe_dirty = true;
        }
    }

} // extern "C"
