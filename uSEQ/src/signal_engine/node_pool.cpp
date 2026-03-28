#include "node_pool.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace sig {

// ── Constant folding evaluation ─────────────────────────────────────────────

double eval_unary(NodeOp op, double a) {
    switch (op) {
        case NodeOp::Neg:     return -a;
        case NodeOp::Abs:     return fabs(a);
        case NodeOp::Floor:   return floor(a);
        case NodeOp::Ceil:    return ceil(a);
        case NodeOp::Frac:    return a - floor(a);
        case NodeOp::Sqrt:    return sqrt(fabs(a));
        case NodeOp::Sin:     return sin(a);
        case NodeOp::Cos:     return cos(a);
        case NodeOp::Tan:     return tan(a);
        case NodeOp::USin:    return (sin(a * 2.0 * M_PI) + 1.0) * 0.5;
        case NodeOp::UCos:    return (cos(a * 2.0 * M_PI) + 1.0) * 0.5;
        case NodeOp::Tri:     return 1.0 - fabs(2.0 * (a - floor(a)) - 1.0);
        case NodeOp::Sqr:     return ((a - floor(a)) < 0.5) ? 1.0 : 0.0;
        case NodeOp::Not:     return (a == 0.0) ? 1.0 : 0.0;
        case NodeOp::BiToUni: return (a + 1.0) * 0.5;
        case NodeOp::UniToBi: return a * 2.0 - 1.0;
        case NodeOp::HashIndex: {
            uint32_t v = (uint32_t)(int32_t)a;
            v = ((v >> 16) ^ v) * 0x45d9f3bu;
            v = ((v >> 16) ^ v) * 0x45d9f3bu;
            v = (v >> 16) ^ v;
            return (double)(v & 0x7fffffffu) / (double)0x7fffffffu;
        }
        default:              return 0.0;
    }
}

double eval_binop(NodeOp op, double a, double b) {
    switch (op) {
        case NodeOp::Add:   return a + b;
        case NodeOp::Sub:   return a - b;
        case NodeOp::Mul:   return a * b;
        case NodeOp::Div:   return (b != 0.0) ? a / b : 0.0;
        case NodeOp::Mod:   return (b != 0.0) ? fmod(a, b) : 0.0;
        case NodeOp::Pow:   return pow(b, a);
        case NodeOp::Min:   return (a < b) ? a : b;
        case NodeOp::Max:   return (a > b) ? a : b;
        case NodeOp::Fmod:  return (b != 0.0) ? fmod(a, b) : 0.0;
        case NodeOp::Pulse: return ((a - floor(a)) < b) ? 1.0 : 0.0;
        case NodeOp::CmpGt: return (a > b)  ? 1.0 : 0.0;
        case NodeOp::CmpLt: return (a < b)  ? 1.0 : 0.0;
        case NodeOp::CmpGe: return (a >= b) ? 1.0 : 0.0;
        case NodeOp::CmpLe: return (a <= b) ? 1.0 : 0.0;
        case NodeOp::CmpEq: return (a == b) ? 1.0 : 0.0;
        case NodeOp::And:   return (a != 0.0 && b != 0.0) ? 1.0 : 0.0;
        case NodeOp::Or:    return (a != 0.0 || b != 0.0) ? 1.0 : 0.0;
        default:            return 0.0;
    }
}

double eval_ternary(NodeOp op, double a, double b, double c) {
    switch (op) {
        case NodeOp::Clamp:  return (a < b) ? b : (a > c) ? c : a;
        case NodeOp::Lerp:   return a + (b - a) * c;
        case NodeOp::Scale:  return a * (c - b) + b;
        case NodeOp::Select: return (a != 0.0) ? b : c;
        default:             return 0.0;
    }
}

// ── OutputDeps ──────────────────────────────────────────────────────────────

void OutputDeps::clear() {
    count = 0;
}

void OutputDeps::add(SymbolID sym) {
    // Deduplicate
    for (uint8_t i = 0; i < count; i++) {
        if (cells[i] == sym) return;
    }
    if (count < MAX_OUTPUT_DEPS) {
        cells[count++] = sym;
    }
}

bool OutputDeps::contains(SymbolID sym) const {
    for (uint8_t i = 0; i < count; i++) {
        if (cells[i] == sym) return true;
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

uint16_t NodePool::make_unary(NodeOp op, uint16_t a) {
    if (a == NODE_NONE) return NODE_NONE;
    const Node& na = nodes[a];

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
    const Node& na = nodes[a];
    const Node& nb = nodes[b];

    // Constant folding
    if (na.op == NodeOp::Const && nb.op == NodeOp::Const) {
        return make_const(eval_binop(op, na.imm, nb.imm));
    }

    // Algebraic simplifications
    if (op == NodeOp::Add && nb.op == NodeOp::Const && nb.imm == 0.0) return a;
    if (op == NodeOp::Add && na.op == NodeOp::Const && na.imm == 0.0) return b;
    if (op == NodeOp::Mul && nb.op == NodeOp::Const && nb.imm == 1.0) return a;
    if (op == NodeOp::Mul && na.op == NodeOp::Const && na.imm == 1.0) return b;
    if (op == NodeOp::Mul && nb.op == NodeOp::Const && nb.imm == 0.0) return make_const(0.0);
    if (op == NodeOp::Mul && na.op == NodeOp::Const && na.imm == 0.0) return make_const(0.0);
    if (op == NodeOp::Sub && a == b) return make_const(0.0);
    if (op == NodeOp::Div && a == b) return make_const(1.0);
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
    const Node& na = nodes[a];
    const Node& nb = nodes[b];
    const Node& nc = nodes[c];

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

void NodePool::rebuild_execution_order() {
    // Mark reachable nodes from output roots
    bool reachable[MAX_TOTAL_NODES] = {};

    // DFS from each output root
    uint16_t stack[MAX_TOTAL_NODES];
    uint16_t stack_top = 0;

    for (uint16_t o = 0; o < MAX_OUTPUTS; o++) {
        if (outputs[o].root_node != NODE_NONE) {
            stack[stack_top++] = outputs[o].root_node;
        }
    }

    while (stack_top > 0) {
        uint16_t idx = stack[--stack_top];
        if (idx >= node_count || reachable[idx]) continue;
        reachable[idx] = true;
        const Node& n = nodes[idx];
        if (n.input_a != NODE_NONE) stack[stack_top++] = n.input_a;
        if (n.input_b != NODE_NONE) stack[stack_top++] = n.input_b;
        if (n.input_c != NODE_NONE) stack[stack_top++] = n.input_c;
    }

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
    // For now, we don't compact — we just rely on rebuild_execution_order
    // to exclude dead nodes from the exec list. Compaction is a Phase 4 concern.
}

void NodePool::reset() {
    node_count = 0;
    exec_count = 0;
    memset(cse_hashes, 0, sizeof(cse_hashes));
    memset(cse_indices, 0, sizeof(cse_indices));
    for (uint16_t i = 0; i < MAX_OUTPUTS; i++) {
        outputs[i] = OutputSlot{};
        output_deps[i].clear();
    }
    memset(prev_output_values, 0, sizeof(prev_output_values));
}

void NodePool::allocate_batch_workspace() {
    if (!batch_workspace) {
        batch_workspace = new double[MAX_TOTAL_NODES * batch_chunk_size];
    }
}

void NodePool::free_batch_workspace() {
    delete[] batch_workspace;
    batch_workspace = nullptr;
}

} // namespace sig
