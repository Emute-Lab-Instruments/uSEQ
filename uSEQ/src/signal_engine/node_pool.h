#ifndef SIGNAL_ENGINE_NODE_POOL_H
#define SIGNAL_ENGINE_NODE_POOL_H

#include "types.h"
#include <memory>

namespace sig {

// ── Node Operations ─────────────────────────────────────────────────────────

enum class NodeOp : uint8_t {
    // Constants and loads
    Const,          // imm = value
    RawTimeLoad,    // loads the single 't' input
    CellLoad,       // imm = cell_id
    InputLoad,      // imm = input_index (hardware input channel)
    PrevOutputLoad, // imm = output_index; previous tick's value

    // Binary arithmetic
    Add, Sub, Mul, Div, Mod, Expt, Min, Max,

    // Unary math
    Neg, Abs, Floor, Ceil, Frac, Sqrt, Clamp,

    // Trigonometry
    Sin, Cos, Tan,

    // Domain waveforms (operate on phase [0,1))
    USin, UCos,
    Tri, Sqr, Pulse,

    // Comparison (1.0 true, 0.0 false)
    CmpGt, CmpLt, CmpGe, CmpLe, CmpEq,

    // Logic
    Not, And, Or,

    // Control flow
    Select,     // input_a=cond, input_b=true_val, input_c=false_val

    // Vector/data
    VecIndex,   // input_a=fractional_index, imm=table_id; floor, wrap
    VecLerp,    // input_a=fractional_index, imm=table_id; lerp

    // Range conversion
    BiToUni,    // [-1,1] → [0,1]
    UniToBi,    // [0,1] → [-1,1]
    Scale,      // 3 inputs: value, out_min, out_max ([0,1]→[min,max])
    Lerp,       // 3 inputs: a, b, t → a + (b-a)*t

    // Deterministic hash (pure function of input)
    HashIndex,  // input_a = index; deterministic hash → [0,1]

    // Cross-sample state
    LoadState,  // imm = state_slot; reads state_values[slot]
    LoadDt,     // reads dt (time delta since last tick)

    // Live-edit slots (externally-driven inputs from editor UI)
    SlotLoad,   // imm = live_slot_index; reads live_slots[slot].value
};

// ── Node ────────────────────────────────────────────────────────────────────

struct Node {
    NodeOp op         = NodeOp::Const;
    uint8_t flags     = 0;         // FLAG_TIME_INVARIANT etc.
    uint16_t input_a  = NODE_NONE;
    uint16_t input_b  = NODE_NONE;
    uint16_t input_c  = NODE_NONE;
    double imm        = 0.0;       // immediate value
    uint16_t span_start = 0;       // source location for diagnostics
    uint16_t span_len   = 0;
};
// sizeof(Node) == 20 bytes

// ── Output Slot ─────────────────────────────────────────────────────────────

struct OutputSlot {
    uint16_t root_node = NODE_NONE;
    double lkg_value   = 0.0;      // last known good output value
    bool valid         = false;
    uint8_t pad[7]     = {};
};

// ── Per-Output Dependencies ─────────────────────────────────────────────────
// Populated during graph construction. Used for dirty recompilation.

struct OutputDeps {
    SymbolID cells[MAX_OUTPUT_DEPS] = {};
    uint8_t count = 0;

    // Live-edit slot indices referenced by this output's graph
    uint16_t slots[MAX_LIVE_SLOTS] = {};
    uint8_t slot_count = 0;

    void clear();
    void add(SymbolID sym);
    bool contains(SymbolID sym) const;

    void add_slot(uint16_t slot_index);
    bool contains_slot(uint16_t slot_index) const;
};

// ── Node Pool ───────────────────────────────────────────────────────────────

struct NodePool {
    Node nodes[MAX_TOTAL_NODES]    = {};
    uint16_t node_count            = 0;

    // Bounds-checked node accessor. Returns a static null node for
    // NODE_NONE or out-of-range indices, avoiding undefined behaviour.
    const Node& get(uint16_t idx) const {
        static const Node null_node{};
        if (idx == NODE_NONE || idx >= node_count) return null_node;
        return nodes[idx];
    }

    // Hash-cons CSE table
    uint32_t cse_hashes[CSE_TABLE_SIZE]  = {};
    uint16_t cse_indices[CSE_TABLE_SIZE] = {};

    // Topologically sorted execution order
    uint16_t exec_order[MAX_TOTAL_NODES] = {};
    uint16_t exec_count = 0;

    // Per-output metadata
    OutputSlot outputs[MAX_OUTPUTS] = {};
    OutputDeps output_deps[MAX_OUTPUTS] = {};

    // Per-output classification (recomputed after each eval)
    OutputClass output_class[MAX_OUTPUTS] = {};
    uint32_t    output_input_mask[MAX_OUTPUTS] = {};  // bitmask of hw input channels referenced

    // Cross-output reads use previous-tick values
    double prev_output_values[MAX_OUTPUTS] = {};

    // ── Cross-sample state ──────────────────────────────────────────────
    double state_values[MAX_STATE_SLOTS]  = {};       // current state (read during execution)
    uint16_t state_update_roots[MAX_STATE_SLOTS] = {}; // root node for each state's update expr (init to 0, set to NODE_NONE by init)
    uint16_t state_slot_count = 0;

    // ── Live-edit slots (externally written by editor UI) ─────────────

    enum class SlotVariant : uint8_t { Numeric = 0, Boolean = 1, Keyword = 2 };

    struct LiveSlot {
        char id[MAX_LIVE_SLOT_ID] = {};
        double value    = 0.0;
        double min_val  = 0.0;
        double max_val  = 1.0;
        double seed     = 0.0;
        SlotVariant variant = SlotVariant::Numeric;
        double step     = 0.0;
        int precision   = -1;  // -1 means unset
        char options[MAX_LIVE_SLOT_OPTIONS][MAX_LIVE_SLOT_OPTION_LEN] = {};
        uint8_t options_count = 0;
    };
    LiveSlot live_slots[MAX_LIVE_SLOTS] = {};
    uint16_t live_slot_count = 0;

    int16_t find_live_slot(const char* id) const;
    int16_t alloc_live_slot(const char* id, double seed, double min_val, double max_val,
                            SlotVariant variant = SlotVariant::Numeric,
                            double step = 0.0, int precision = -1);
    void set_live_slot_value(const char* id, double value);

    // WASM batch workspace (heap-allocated once at init, null on firmware)
    std::unique_ptr<double[]> batch_workspace;
    uint16_t batch_chunk_size = BATCH_CHUNK_SIZE;

    // ── Node construction (with CSE + constant folding) ─────────────────

    uint16_t make_const(double value);
    uint16_t make_raw_time_load();
    uint16_t make_cell_load(SymbolID cell_id);
    uint16_t make_input_load(uint16_t input_index);
    uint16_t make_prev_output_load(uint16_t output_index);

    uint16_t make_state_load(uint16_t state_slot);
    uint16_t make_slot_load(uint16_t slot_index);
    uint16_t make_dt_load();

    uint16_t make_unary(NodeOp op, uint16_t a);
    uint16_t make_binop(NodeOp op, uint16_t a, uint16_t b);
    uint16_t make_ternary(NodeOp op, uint16_t a, uint16_t b, uint16_t c);
    uint16_t make_select(uint16_t cond, uint16_t true_val, uint16_t false_val);

    // ── Pool operations ─────────────────────────────────────────────────

    // Hash-cons: intern a node, return index (reuse if identical exists).
    uint16_t intern_node(const Node& n);

    // Rebuild topological execution order from live output roots.
    void rebuild_execution_order();

    // Remove nodes not reachable from any output root.
    void gc_unreachable_nodes();

    // Reset the entire pool.
    void reset();

    // Allocate batch workspace (call once at init for WASM builds).
    void allocate_batch_workspace();
    void free_batch_workspace();
};

// ── Constant folding helpers ────────────────────────────────────────────────

double eval_unary(NodeOp op, double a);
double eval_binop(NodeOp op, double a, double b);
double eval_ternary(NodeOp op, double a, double b, double c);

} // namespace sig

#endif // SIGNAL_ENGINE_NODE_POOL_H
