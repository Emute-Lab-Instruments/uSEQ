#include "node_pool.h"
#include "eval_ops.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace sig {

// ── Constant folding evaluation ─────────────────────────────────────────────
// Thin wrappers that delegate to the shared eval_ops.h functions.

double eval_unary(NodeOp op, double a)                    { return eval_unary_op(op, a); }
double eval_binop(NodeOp op, double a, double b)          { return eval_binary_op(op, a, b); }
double eval_ternary(NodeOp op, double a, double b, double c) { return eval_ternary_op(op, a, b, c); }

// ── OutputDeps ──────────────────────────────────────────────────────────────

void OutputDeps::clear() {
    count = 0;
    slot_count = 0;
}

void OutputDeps::add(CellIndex cell_index) {
    // Deduplicate
    for (uint8_t i = 0; i < count; i++) {
        if (cells[i] == cell_index) return;
    }
    if (count < MAX_OUTPUT_DEPS) {
        cells[count++] = cell_index;
    }
}

bool OutputDeps::contains(SymbolID sym) const {
    if (sym >= MAX_CELLS) return false;
    for (uint8_t i = 0; i < count; i++) {
        if (cells[i] == static_cast<CellIndex>(sym)) return true;
    }
    return false;
}

void OutputDeps::add_slot(uint16_t slot_index) {
    // Deduplicate
    for (uint16_t i = 0; i < slot_count; i++) {
        if (slots[i] == slot_index) return;
    }
    if (slot_count < MAX_LIVE_SLOTS) {
        slots[slot_count++] = slot_index;
    }
}

bool OutputDeps::contains_slot(uint16_t slot_index) const {
    for (uint16_t i = 0; i < slot_count; i++) {
        if (slots[i] == slot_index) return true;
    }
    return false;
}

// ── Hashing for CSE ─────────────────────────────────────────────────────────

static uint32_t hash_node(const Node& n) {
    // FNV-1a hash of the node's identity fields
    uint32_t h = 2166136261u;
    auto mix = [&](uint8_t byte) { h ^= byte; h *= 16777619u; };

    mix((uint8_t)n.op);
    mix((uint8_t)(n.input_a & 0xFF));
    mix((uint8_t)(n.input_a >> 8));
    mix((uint8_t)(n.input_b & 0xFF));
    mix((uint8_t)(n.input_b >> 8));
    mix((uint8_t)(n.input_c & 0xFF));
    mix((uint8_t)(n.input_c >> 8));

    // Hash the immediate value bytes
    const uint8_t* imm_bytes = reinterpret_cast<const uint8_t*>(&n.imm);
    for (int i = 0; i < 8; i++) mix(imm_bytes[i]);

    return h;
}

static bool nodes_equal(const Node& a, const Node& b) {
    return a.op == b.op
        && a.input_a == b.input_a
        && a.input_b == b.input_b
        && a.input_c == b.input_c
        && memcmp(&a.imm, &b.imm, sizeof(double)) == 0;
}

// ── NodePool ────────────────────────────────────────────────────────────────

uint16_t NodePool::intern_node(const Node& n) {
    uint32_t h = hash_node(n);
    uint32_t slot = h % CSE_TABLE_SIZE;

    // Open-addressing probe
    for (uint32_t probe = 0; probe < CSE_TABLE_SIZE; probe++) {
        uint32_t idx = (slot + probe) % CSE_TABLE_SIZE;
        if (cse_hashes[idx] == 0) {
            // Empty slot — insert
            if (node_count >= MAX_TOTAL_NODES) return NODE_NONE; // pool full
            uint16_t ni = node_count++;
            nodes[ni] = n;
            cse_hashes[idx] = h | 1; // ensure non-zero (mark occupied)
            cse_indices[idx] = ni;
            return ni;
        }
        if (cse_hashes[idx] == (h | 1) && nodes_equal(nodes[cse_indices[idx]], n)) {
            return cse_indices[idx]; // CSE hit
        }
    }
    // Table full (shouldn't happen with 2x load factor)
    if (node_count >= MAX_TOTAL_NODES) return NODE_NONE;
    uint16_t ni = node_count++;
    nodes[ni] = n;
    return ni;
}

uint16_t NodePool::make_const(double value) {
    Node n;
    n.op = NodeOp::Const;
    n.flags = FLAG_TIME_INVARIANT;
    n.imm = value;
    return intern_node(n);
}

uint16_t NodePool::make_raw_time_load() {
    Node n;
    n.op = NodeOp::RawTimeLoad;
    n.flags = 0; // time-varying by definition
    return intern_node(n);
}

uint16_t NodePool::make_cell_load(SymbolID cell_id) {
    Node n;
    n.op = NodeOp::CellLoad;
    n.flags = FLAG_TIME_INVARIANT; // cells don't change per-sample
    n.imm = (double)cell_id;
    return intern_node(n);
}

uint16_t NodePool::make_input_load(uint16_t input_index) {
    Node n;
    n.op = NodeOp::InputLoad;
    n.flags = 0; // hardware inputs can change per-sample
    n.imm = (double)input_index;
    return intern_node(n);
}

uint16_t NodePool::make_prev_output_load(uint16_t output_index) {
    Node n;
    n.op = NodeOp::PrevOutputLoad;
    n.flags = 0;
    n.imm = (double)output_index;
    return intern_node(n);
}

uint16_t NodePool::make_state_load(uint16_t state_slot) {
    Node n;
    n.op = NodeOp::LoadState;
    n.flags = 0;  // state is time-varying (changes each tick)
    n.imm = (double)state_slot;
    return intern_node(n);
}

uint16_t NodePool::make_slot_load(uint16_t slot_index) {
    Node n;
    n.op = NodeOp::SlotLoad;
    n.flags = 0;  // live slots change externally per tick
    n.imm = (double)slot_index;
    return intern_node(n);
}

int16_t NodePool::find_live_slot(const char* id) const {
    for (uint16_t i = 0; i < live_slot_count; i++) {
        if (strncmp(live_slots[i].id, id, MAX_LIVE_SLOT_ID) == 0) {
            return (int16_t)i;
        }
    }
    return -1;
}

int16_t NodePool::alloc_live_slot(const char* id, double seed, double min_val, double max_val,
                                   SlotVariant variant, double step, int precision,
                                   uint16_t owner_context) {
    int16_t existing = find_live_slot(id);
    if (existing >= 0) {
        live_slots[existing].min_val = min_val;
        live_slots[existing].max_val = max_val;
        live_slots[existing].seed = seed;
        live_slots[existing].variant = variant;
        live_slots[existing].step = step;
        live_slots[existing].precision = precision;
        // Reclamp existing value to new bounds (numeric only)
        if (variant == SlotVariant::Numeric) {
            double& v = live_slots[existing].value;
            if (v < min_val) v = min_val;
            if (v > max_val) v = max_val;
        } else if (variant == SlotVariant::Boolean) {
            double& v = live_slots[existing].value;
            v = (v != 0.0) ? 1.0 : 0.0;
        }
        // Keyword: value is index into options, validated by caller
        return existing;
    }
    if (live_slot_count >= MAX_LIVE_SLOTS) return -1;
    uint16_t idx = live_slot_count++;
    strncpy(live_slots[idx].id, id, MAX_LIVE_SLOT_ID - 1);
    live_slots[idx].id[MAX_LIVE_SLOT_ID - 1] = '\0';
    live_slots[idx].owner_context = owner_context;
    live_slots[idx].value = seed;
    live_slots[idx].min_val = min_val;
    live_slots[idx].max_val = max_val;
    live_slots[idx].seed = seed;
    live_slots[idx].variant = variant;
    live_slots[idx].step = step;
    live_slots[idx].precision = precision;
    return (int16_t)idx;
}

void NodePool::set_live_slot_value(const char* id, double value) {
    int16_t idx = find_live_slot(id);
    if (idx < 0) return;
    set_live_slot_value_by_index((uint16_t)idx, value);
}

void NodePool::set_live_slot_value_by_index(uint16_t idx, double value) {
    // Bounds-check against the live slot capacity. Out-of-range indices are
    // ignored (wire-protocol §6.5: garbage/stale slot_index → skip).
    if (idx >= live_slot_count) return;

    // §5.9: reject non-finite numbers — slot retains previous value
    if (!std::isfinite(value)) return;

    switch (live_slots[idx].variant) {
        case SlotVariant::Numeric: {
            // Clamp to [min, max]
            double clamped = value;
            if (clamped < live_slots[idx].min_val) clamped = live_slots[idx].min_val;
            if (clamped > live_slots[idx].max_val) clamped = live_slots[idx].max_val;
            live_slots[idx].value = clamped;
            break;
        }
        case SlotVariant::Boolean:
            // Cast to 0.0 or 1.0
            live_slots[idx].value = (value != 0.0) ? 1.0 : 0.0;
            break;
        case SlotVariant::Keyword: {
            // Validate against options vector length
            int index = (int)value;
            if (index < 0 || index >= (int)live_slots[idx].options_count) return;
            live_slots[idx].value = (double)index;
            break;
        }
    }
}

uint16_t NodePool::make_dt_load() {
    Node n;
    n.op = NodeOp::LoadDt;
    n.flags = 0;  // dt varies per tick
    return intern_node(n);
}

uint16_t NodePool::make_unary(NodeOp op, uint16_t a) {
    if (a == NODE_NONE) return NODE_NONE;
    const Node& na = get(a);

    // Constant folding
    if (na.op == NodeOp::Const) {
        return make_const(eval_unary(op, na.imm));
    }

    uint8_t flags = na.flags & FLAG_TIME_INVARIANT;
    Node n;
    n.op = op;
    n.flags = flags;
    n.input_a = a;
    return intern_node(n);
}

uint16_t NodePool::make_binop(NodeOp op, uint16_t a, uint16_t b) {
    if (a == NODE_NONE || b == NODE_NONE) return NODE_NONE;
    const Node& na = get(a);
    const Node& nb = get(b);

    // Constant folding
    if (na.op == NodeOp::Const && nb.op == NodeOp::Const) {
        return make_const(eval_binop(op, na.imm, nb.imm));
    }

    // Algebraic simplifications
    if (op == NodeOp::Add && nb.op == NodeOp::Const && nb.imm == 0.0) return a;
    if (op == NodeOp::Add && na.op == NodeOp::Const && na.imm == 0.0) return b;
    if (op == NodeOp::Mul && nb.op == NodeOp::Const && nb.imm == 1.0) return a;
    if (op == NodeOp::Mul && na.op == NodeOp::Const && na.imm == 1.0) return b;
    // Do not fold x*0 or x-x without a proof that x is finite. IEEE-754
    // gives NaN for Inf*0, NaN*0, Inf-Inf, and NaN-NaN; replacing those
    // results with zero would suppress the output-root failure signal and
    // change LKG/health semantics. The all-constant case above remains safe
    // because it is evaluated through the same primitive as the hot path.
    // NOTE: no `Div a a -> 1` fold (A10): IEEE 0/0 and Inf/Inf are NaN and
    // must remain visible to output-root health/LKG handling. The constant/
    // constant case is already folded through eval_binop above.
    if (op == NodeOp::Div && nb.op == NodeOp::Const && nb.imm == 1.0) return a;

    uint8_t flags = 0;
    if ((na.flags & FLAG_TIME_INVARIANT) && (nb.flags & FLAG_TIME_INVARIANT)) {
        flags |= FLAG_TIME_INVARIANT;
    }

    Node n;
    n.op = op;
    n.flags = flags;
    n.input_a = a;
    n.input_b = b;
    return intern_node(n);
}

uint16_t NodePool::make_ternary(NodeOp op, uint16_t a, uint16_t b, uint16_t c) {
    if (a == NODE_NONE || b == NODE_NONE || c == NODE_NONE) return NODE_NONE;
    const Node& na = get(a);
    const Node& nb = get(b);
    const Node& nc = get(c);

    // Constant folding
    if (na.op == NodeOp::Const && nb.op == NodeOp::Const && nc.op == NodeOp::Const) {
        return make_const(eval_ternary(op, na.imm, nb.imm, nc.imm));
    }

    // Select with constant condition
    if (op == NodeOp::Select && na.op == NodeOp::Const) {
        return (na.imm != 0.0) ? b : c;
    }

    uint8_t flags = 0;
    if ((na.flags & FLAG_TIME_INVARIANT) && (nb.flags & FLAG_TIME_INVARIANT)
        && (nc.flags & FLAG_TIME_INVARIANT)) {
        flags |= FLAG_TIME_INVARIANT;
    }

    Node n;
    n.op = op;
    n.flags = flags;
    n.input_a = a;
    n.input_b = b;
    n.input_c = c;
    return intern_node(n);
}

uint16_t NodePool::make_select(uint16_t cond, uint16_t true_val, uint16_t false_val) {
    return make_ternary(NodeOp::Select, cond, true_val, false_val);
}

// ── Topological sort ────────────────────────────────────────────────────────

static void mark_reachable_nodes(const NodePool& pool, bool* marked) {
    memset(marked, 0, MAX_TOTAL_NODES * sizeof(bool));
    uint16_t stack[MAX_TOTAL_NODES];
    uint16_t stack_top = 0;

    // Mark on discovery, before enqueueing. Mark-on-pop allows a shared child
    // to occupy the pending stack once per incoming edge; a high-sharing DAG
    // can then fill a MAX_TOTAL_NODES stack with duplicates and silently drop
    // a genuinely undiscovered dependency. With discovery marking, every
    // valid node is pushed at most once, so stack_top is bounded by node_count
    // (and node_count itself is bounded by MAX_TOTAL_NODES).
    auto discover = [&](uint16_t child) {
        if (child == NODE_NONE || child >= pool.node_count || marked[child]) return;
        marked[child] = true;
        stack[stack_top++] = child;
    };

    for (uint16_t o = 0; o < MAX_OUTPUTS; o++) {
        discover(pool.outputs[o].root_node);
    }

    // Include state update roots in reachability
    for (uint16_t s = 0; s < pool.state_slot_count; s++) {
        discover(pool.state_update_roots[s]);
    }

#if USEQ_HAS_SYNTH_ENGINE
    // External roots are executable synth-control roots, not merely GC pins.
    for (uint16_t e = 0; e < pool.external_root_count; e++) {
        discover(pool.external_roots[e]);
    }
#endif

    while (stack_top > 0) {
        uint16_t idx = stack[--stack_top];
        const Node& n = pool.nodes[idx];
        discover(n.input_a);
        discover(n.input_b);
        discover(n.input_c);
    }
}

void NodePool::rebuild_execution_order() {
    // Mark reachable nodes from every executable/publication root.
    bool reachable[MAX_TOTAL_NODES];
    mark_reachable_nodes(*this, reachable);

    // Topological sort via Kahn's algorithm on reachable nodes
    // Since node indices are allocated in dependency order (inputs before outputs),
    // a simple forward scan of reachable nodes IS a valid topological order.
    exec_count = 0;
    for (uint16_t i = 0; i < node_count; i++) {
        if (reachable[i]) {
            exec_order[exec_count++] = i;
        }
    }
}

void NodePool::gc_unreachable_nodes() {
    // 1. Mark reachable from output roots
    bool live[MAX_TOTAL_NODES];
    mark_reachable_nodes(*this, live);

    // 2. Build remap table and compact live nodes to front
    uint16_t remap[MAX_TOTAL_NODES];
    memset(remap, 0xFF, sizeof(remap)); // NODE_NONE default
    uint16_t new_count = 0;
    for (uint16_t i = 0; i < node_count; i++) {
        if (live[i]) {
            remap[i] = new_count;
            if (new_count != i) nodes[new_count] = nodes[i];
            new_count++;
        }
    }

    // 3. Update references in compacted nodes
    for (uint16_t i = 0; i < new_count; i++) {
        if (nodes[i].input_a != NODE_NONE) nodes[i].input_a = remap[nodes[i].input_a];
        if (nodes[i].input_b != NODE_NONE) nodes[i].input_b = remap[nodes[i].input_b];
        if (nodes[i].input_c != NODE_NONE) nodes[i].input_c = remap[nodes[i].input_c];
    }

    // 4. Update output roots
    for (uint16_t o = 0; o < MAX_OUTPUTS; o++) {
        if (outputs[o].root_node != NODE_NONE)
            outputs[o].root_node = remap[outputs[o].root_node];
    }

    // 4b. Update state update roots
    for (uint16_t s = 0; s < state_slot_count; s++) {
        if (state_update_roots[s] != NODE_NONE)
            state_update_roots[s] = remap[state_update_roots[s]];
    }

#if USEQ_HAS_SYNTH_ENGINE
    // 4c. Update host synth control roots.
    for (uint16_t e = 0; e < external_root_count; e++) {
        if (external_roots[e] != NODE_NONE)
            external_roots[e] = remap[external_roots[e]];
    }
#endif

    node_count = new_count;

    // 5. Rebuild CSE table from scratch (indices changed)
    memset(cse_hashes, 0, sizeof(cse_hashes));
    memset(cse_indices, 0, sizeof(cse_indices));
    for (uint16_t i = 0; i < node_count; i++) {
        uint32_t h = hash_node(nodes[i]);
        uint32_t slot = h % CSE_TABLE_SIZE;
        for (uint32_t probe = 0; probe < CSE_TABLE_SIZE; probe++) {
            uint32_t idx = (slot + probe) % CSE_TABLE_SIZE;
            if (cse_hashes[idx] == 0) {
                cse_hashes[idx] = h | 1;
                cse_indices[idx] = i;
                break;
            }
        }
    }
}

void NodePool::reset() {
    for (uint16_t i = 0; i < MAX_TOTAL_NODES; i++) nodes[i] = Node{};
    node_count = 0;
    memset(exec_order, 0, sizeof(exec_order));
    exec_count = 0;
    memset(cse_hashes, 0, sizeof(cse_hashes));
    memset(cse_indices, 0, sizeof(cse_indices));
    for (uint16_t i = 0; i < MAX_OUTPUTS; i++) {
        outputs[i] = OutputSlot{};
        output_deps[i] = OutputDeps{};
    }
    memset(output_class, 0, sizeof(output_class));
    memset(output_input_mask, 0, sizeof(output_input_mask));
    memset(prev_output_values, 0, sizeof(prev_output_values));
    runtime_fallback_mask = 0;
    state_update_failure_mask = 0;
    memset(state_values, 0, sizeof(state_values));
    for (uint16_t s = 0; s < MAX_STATE_SLOTS; s++) {
        state_update_roots[s] = NODE_NONE;
        state_owner_context[s] = NODE_NONE;
    }
    state_slot_count = 0;
    for (uint16_t i = 0; i < MAX_LIVE_SLOTS; i++)
        live_slots[i] = LiveSlot{};
    live_slot_count = 0;
#if USEQ_HAS_SYNTH_ENGINE
    memset(external_roots, 0, sizeof(external_roots));
    external_root_count = 0;
#endif
}

void NodePool::allocate_batch_workspace() {
    if (!batch_workspace) {
        batch_workspace.reset(new double[MAX_TOTAL_NODES * batch_chunk_size]);
    }
}

void NodePool::free_batch_workspace() {
    batch_workspace.reset();
}

} // namespace sig
