#ifndef SIGNAL_ENGINE_NODE_POOL_H
#define SIGNAL_ENGINE_NODE_POOL_H

#include "types.h"
#include <memory>

namespace sig {

constexpr uint16_t LIVE_SLOT_OWNER_NONE = 0xFFFF;

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
    // `valid` is retained as the active-assignment bit for source/API
    // compatibility.  It does NOT mean that a healthy sample has been
    // observed: a freshly compiled program is active before its first tick.
    bool valid         = false;
    bool has_lkg       = false;    // at least one finite root was committed
    uint8_t pad[6]     = {};
};

// ── Per-Output Dependencies ─────────────────────────────────────────────────
// Populated during graph construction. Used for dirty recompilation.

struct OutputDeps {
    CellIndex cells[MAX_OUTPUT_DEPS] = {};
    uint8_t count = 0;

    // Live-edit slot indices referenced by this output's graph.
    // slot_count must be wide enough to actually reach MAX_LIVE_SLOTS: on the
    // WASM build MAX_LIVE_SLOTS == 256, which a uint8_t can never represent, so
    // a `slot_count < MAX_LIVE_SLOTS` guard with a uint8_t counter is always
    // true and never fires. uint16_t covers both the firmware (16) and WASM
    // (256) capacities.
    uint16_t slots[MAX_LIVE_SLOTS] = {};
    uint16_t slot_count = 0;

    void clear();
    void add(CellIndex cell_index);
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

    // Runtime fallback tracking (failure-model.md §2.1/§5): bit i is set when
    // output i substituted its LKG value on the most recent execution pass
    // because a non-finite value reached its root (FailureMode::LkgFallback
    // only). Recomputed on every pass; mutable because execution paths take
    // `const NodePool&` — this is diagnostic bookkeeping, not graph state.
    mutable uint64_t runtime_fallback_mask = 0;
    static_assert(MAX_OUTPUTS <= 64, "runtime_fallback_mask is 64-bit");

    // Bit s is active when state slot s most recently produced a non-finite
    // update candidate.  The previous finite state remains installed until
    // that same update root recovers.  This is separate from output fallback:
    // a named defstate can fail while every consuming output remains finite.
    uint64_t state_update_failure_mask = 0;
    static_assert(MAX_STATE_SLOTS <= 64,
                  "state_update_failure_mask is 64-bit");

    // ── Cross-sample state ──────────────────────────────────────────────
    double state_values[MAX_STATE_SLOTS]  = {};       // current state (read during execution)
    uint16_t state_update_roots[MAX_STATE_SLOTS] = {}; // root node for each state's update expr (init to 0, set to NODE_NONE by init)
    // Compiler context owning the sole update writer. Output contexts are
    // 0..MAX_OUTPUTS-1; other live state sources use disjoint contexts.
    // Runtime fallback freezes only state owned by its failed output.
    uint16_t state_owner_context[MAX_STATE_SLOTS] = {};
    uint16_t state_slot_count = 0;

    // ── Live-edit slots (externally written by editor UI) ─────────────

    enum class SlotVariant : uint8_t { Numeric = 0, Boolean = 1, Keyword = 2 };

    struct LiveSlot {
        char id[MAX_LIVE_SLOT_ID] = {};
        uint16_t owner_context = LIVE_SLOT_OWNER_NONE;
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
                            double step = 0.0, int precision = -1,
                            uint16_t owner_context = LIVE_SLOT_OWNER_NONE);
    void set_live_slot_value(const char* id, double value);
    // Fast-path write addressed by array index (wire-protocol §6.5 binary
    // INPUT_SET). `idx` is a direct index into live_slots[0..live_slot_count).
    // Out-of-range indices are ignored. Applies the same clamp / variant
    // coercion as set_live_slot_value(), but skips the string lookup so it is
    // allocation-free and cheap on the serial RX hot path.
    void set_live_slot_value_by_index(uint16_t idx, double value);

    // WASM batch workspace (heap-allocated once at init, null on firmware)
    std::unique_ptr<double[]> batch_workspace;
    uint16_t batch_chunk_size = BATCH_CHUNK_SIZE;

#if USEQ_HAS_SYNTH_ENGINE
    // ── External root registration (host synth control roots) ───────────
    // Some compiled roots live outside the per-output table — notably
    // synth control channel expressions (synth-nodes.md §7.2). These roots
    // must participate in GC reachability and remap exactly like output
    // roots, otherwise forced GC after a synth eval drops the control
    // expressions (VAL-COMP-011). External clients register root indices
    // here; the GC walks and remaps them. The pool itself does not attach
    // semantics to these slots.
    static constexpr uint16_t MAX_EXTERNAL_ROOTS = MAX_SYNTH_CONTROL_ROOTS;
    uint16_t external_roots[MAX_EXTERNAL_ROOTS] = {};
    uint16_t external_root_count = 0;

    void clear_external_roots() { external_root_count = 0; }
    bool register_external_root(uint16_t node_idx) {
        if (external_root_count >= MAX_EXTERNAL_ROOTS) return false;
        external_roots[external_root_count++] = node_idx;
        return true;
    }
#endif

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
