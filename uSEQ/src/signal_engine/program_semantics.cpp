#include "program_semantics.h"

#include <cmath>
#include <cstdint>

#if USEQ_HAS_TEMPORAL_QUERY
#include <vector>
#endif

namespace sig {
namespace {

uint32_t local_effects(NodeOp op) {
    switch (op) {
        case NodeOp::RawTimeLoad:    return EffectLocalTime;
        case NodeOp::GroundTimeLoad: return EffectGroundTime;
        case NodeOp::CellLoad:       return EffectCell;
        case NodeOp::InputLoad:      return EffectHardwareInput;
        case NodeOp::SlotLoad:       return EffectLiveInput;
        case NodeOp::PrevOutputLoad: return EffectPreviousOutput;
        case NodeOp::LoadState:      return EffectState;
        case NodeOp::LoadDt:         return EffectDeltaTime;
        case NodeOp::VecIndex:
            return EffectDataTable | EffectDiscontinuous;
        case NodeOp::VecLerp:        return EffectDataTable;
        case NodeOp::Floor:
        case NodeOp::Ceil:
        case NodeOp::Frac:
        case NodeOp::Sqr:
        case NodeOp::Pulse:
        case NodeOp::CmpGt:
        case NodeOp::CmpLt:
        case NodeOp::CmpGe:
        case NodeOp::CmpLe:
        case NodeOp::CmpEq:
        case NodeOp::Select:
            return EffectDiscontinuous;
        case NodeOp::Div:
        case NodeOp::Mod:
        case NodeOp::Expt:
        case NodeOp::Tan:
            return EffectMayBeNonFinite;
        default:
            return EffectNone;
    }
}

} // namespace

ProgramAnalysis analyze_program(const NodePool& pool, uint16_t root_node) {
    ProgramAnalysis result;
    if (root_node == NODE_NONE || root_node >= pool.node_count)
        return result;

    bool visited[MAX_TOTAL_NODES] = {};
    uint16_t stack[MAX_TOTAL_NODES] = {};
    uint16_t stack_size = 0;
    stack[stack_size++] = root_node;
    result.valid_finite_dag = true;

    while (stack_size > 0) {
        uint16_t index = stack[--stack_size];
        if (index >= pool.node_count) {
            result.valid_finite_dag = false;
            continue;
        }
        if (visited[index]) continue;
        visited[index] = true;
        result.reachable_nodes++;

        const Node& node = pool.nodes[index];
        result.effects |= local_effects(node.op);
        if (node.op == NodeOp::LoadState) {
            uint16_t slot = static_cast<uint16_t>(node.imm);
            if (slot >= pool.state_slot_count) {
                result.valid_finite_dag = false;
            } else {
                uint16_t update_root = pool.state_update_roots[slot];
                if (update_root != NODE_NONE && !visited[update_root]) {
                    if (update_root >= pool.node_count ||
                        stack_size >= MAX_TOTAL_NODES)
                        result.valid_finite_dag = false;
                    else
                        stack[stack_size++] = update_root;
                }
            }
        }
        const uint16_t children[3] = {
            node.input_a, node.input_b, node.input_c
        };
        for (uint16_t child : children) {
            if (child == NODE_NONE) continue;
            // Compiler allocation order is a compact termination witness:
            // every dependency must precede the node that consumes it.
            if (child >= pool.node_count || child >= index) {
                result.valid_finite_dag = false;
                continue;
            }
            if (!visited[child]) {
                if (stack_size >= MAX_TOTAL_NODES) {
                    result.valid_finite_dag = false;
                    continue;
                }
                stack[stack_size++] = child;
            }
        }
    }
    return result;
}

#if USEQ_HAS_TEMPORAL_QUERY
namespace {

constexpr Sample PI = 3.14159265358979323846264338327950288;

class ReferenceEvaluator {
public:
    ReferenceEvaluator(const ProgramView& compiled,
                       const ReferenceEnvironment& environment)
        : program(compiled), env(environment),
          marks(compiled.nodes().size(), 0),
          values(compiled.nodes().size(), 0.0) {}

    ReferenceEvalResult run() {
        if (!program.analysis().valid_finite_dag ||
            program.root_node() >= program.nodes().size())
            return {ReferenceEvalStatus::InvalidProgram, 0.0,
                    program.root_node()};
        Sample value = 0.0;
        if (!evaluate(program.root_node(), value))
            return {status, 0.0, detail};
        return {ReferenceEvalStatus::Complete, value, NODE_NONE};
    }

private:
    bool reference(const Sample* values_ptr, uint16_t index, uint16_t limit,
                   Sample fallback, Sample& value) {
        if (index >= limit) return fail(ReferenceEvalStatus::InvalidReference);
        if (!values_ptr) {
            value = fallback;
            return true;
        }
        value = values_ptr[index];
        return true;
    }

    bool fail(ReferenceEvalStatus next_status) {
        status = next_status;
        return false;
    }

    bool evaluate(uint16_t index, Sample& value) {
        if (index >= marks.size()) {
            detail = index;
            return fail(ReferenceEvalStatus::InvalidReference);
        }
        if (marks[index] == 2) {
            value = values[index];
            return true;
        }
        if (marks[index] == 1) {
            detail = index;
            return fail(ReferenceEvalStatus::Cycle);
        }
        marks[index] = 1;
        detail = index;

        const Node& node = program.nodes()[index];
        Sample a = 0.0;
        Sample b = 0.0;
        Sample c = 0.0;
        if (node.input_a != NODE_NONE && !evaluate(node.input_a, a)) return false;
        if (node.input_b != NODE_NONE && !evaluate(node.input_b, b)) return false;
        if (node.input_c != NODE_NONE && !evaluate(node.input_c, c)) return false;

        switch (node.op) {
            case NodeOp::Const: value = node.imm; break;
            case NodeOp::RawTimeLoad: value = env.local_time; break;
            case NodeOp::GroundTimeLoad: value = env.ground_time; break;
            case NodeOp::CellLoad: {
                uint16_t i = static_cast<uint16_t>(node.imm);
                if (i >= program.cell_values().size())
                    return fail(ReferenceEvalStatus::InvalidReference);
                value = program.cell_values()[i];
                break;
            }
            case NodeOp::InputLoad: {
                uint16_t i = static_cast<uint16_t>(node.imm);
                if (!reference(env.hardware_inputs, i, 32, 0.0, value)) return false;
                break;
            }
            case NodeOp::PrevOutputLoad: {
                uint16_t i = static_cast<uint16_t>(node.imm);
                if (i >= MAX_OUTPUTS)
                    return fail(ReferenceEvalStatus::InvalidReference);
                if (!reference(env.previous_outputs, i, MAX_OUTPUTS,
                               program.previous_outputs()[i], value))
                    return false;
                break;
            }
            case NodeOp::LoadState: {
                uint16_t i = static_cast<uint16_t>(node.imm);
                if (i >= program.state_values().size())
                    return fail(ReferenceEvalStatus::InvalidReference);
                value = env.state_values ? env.state_values[i]
                                         : program.state_values()[i];
                break;
            }
            case NodeOp::LoadDt: value = env.wall_dt; break;
            case NodeOp::SlotLoad: {
                uint16_t i = static_cast<uint16_t>(node.imm);
                if (i >= program.live_slot_values().size())
                    return fail(ReferenceEvalStatus::InvalidReference);
                value = program.live_slot_values()[i];
                break;
            }
            case NodeOp::VecIndex: {
                uint16_t table = static_cast<uint16_t>(node.imm);
                if (table >= program.data_lengths().size())
                    return fail(ReferenceEvalStatus::InvalidReference);
                uint16_t length = program.data_lengths()[table];
                if (length == 0) { value = 0.0; break; }
                int position = static_cast<int>(std::floor(a));
                int wrapped = ((position % length) + length) % length;
                value = program.data_pool()[
                    program.data_offsets()[table] + wrapped];
                break;
            }
            case NodeOp::VecLerp: {
                uint16_t table = static_cast<uint16_t>(node.imm);
                if (table >= program.data_lengths().size())
                    return fail(ReferenceEvalStatus::InvalidReference);
                uint16_t length = program.data_lengths()[table];
                if (length == 0) { value = 0.0; break; }
                uint16_t offset = program.data_offsets()[table];
                if (length == 1) { value = program.data_pool()[offset]; break; }
                Sample scaled = a * (length - 1);
                int lower = static_cast<int>(std::floor(scaled));
                if (lower < 0) lower = 0;
                if (lower >= length - 1) lower = length - 2;
                Sample fraction = scaled - lower;
                if (fraction < 0.0) fraction = 0.0;
                if (fraction > 1.0) fraction = 1.0;
                value = program.data_pool()[offset + lower]
                    + (program.data_pool()[offset + lower + 1]
                       - program.data_pool()[offset + lower]) * fraction;
                break;
            }
            case NodeOp::Neg: value = -a; break;
            case NodeOp::Abs: value = std::fabs(a); break;
            case NodeOp::Floor: value = std::floor(a); break;
            case NodeOp::Ceil: value = std::ceil(a); break;
            case NodeOp::Frac: value = a - std::floor(a); break;
            case NodeOp::Sqrt: value = std::sqrt(std::fabs(a)); break;
            case NodeOp::Sin: value = std::sin(a); break;
            case NodeOp::Cos: value = std::cos(a); break;
            case NodeOp::Tan: value = std::tan(a); break;
            case NodeOp::USin: value = (std::sin(a * 2.0 * PI) + 1.0) * 0.5; break;
            case NodeOp::UCos: value = (std::cos(a * 2.0 * PI) + 1.0) * 0.5; break;
            case NodeOp::Tri: value = 1.0 - std::fabs(2.0 * (a - std::floor(a)) - 1.0); break;
            case NodeOp::Sqr: value = (a - std::floor(a)) < 0.5 ? 1.0 : 0.0; break;
            case NodeOp::Not: value = a == 0.0 ? 1.0 : 0.0; break;
            case NodeOp::BiToUni: value = (a + 1.0) * 0.5; break;
            case NodeOp::UniToBi: value = a * 2.0 - 1.0; break;
            case NodeOp::HashIndex: {
                uint32_t hashed = static_cast<uint32_t>(static_cast<int32_t>(a));
                hashed = ((hashed >> 16) ^ hashed) * 0x45d9f3bu;
                hashed = ((hashed >> 16) ^ hashed) * 0x45d9f3bu;
                hashed = (hashed >> 16) ^ hashed;
                value = static_cast<Sample>(hashed & 0x7fffffffu)
                    / static_cast<Sample>(0x7fffffffu);
                break;
            }
            case NodeOp::Add: value = a + b; break;
            case NodeOp::Sub: value = a - b; break;
            case NodeOp::Mul: value = a * b; break;
            case NodeOp::Div: value = a / b; break;
            case NodeOp::Mod: value = std::fmod(a, b); break;
            case NodeOp::Expt: value = std::pow(a, b); break;
            case NodeOp::Min: value = a < b ? a : b; break;
            case NodeOp::Max: value = a > b ? a : b; break;
            case NodeOp::Pulse: value = (a - std::floor(a)) < b ? 1.0 : 0.0; break;
            case NodeOp::CmpGt: value = a > b ? 1.0 : 0.0; break;
            case NodeOp::CmpLt: value = a < b ? 1.0 : 0.0; break;
            case NodeOp::CmpGe: value = a >= b ? 1.0 : 0.0; break;
            case NodeOp::CmpLe: value = a <= b ? 1.0 : 0.0; break;
            case NodeOp::CmpEq: value = a == b ? 1.0 : 0.0; break;
            case NodeOp::And: value = (a != 0.0 && b != 0.0) ? 1.0 : 0.0; break;
            case NodeOp::Or: value = (a != 0.0 || b != 0.0) ? 1.0 : 0.0; break;
            case NodeOp::Clamp: value = c < a ? a : (c > b ? b : c); break;
            case NodeOp::Lerp: value = a + (b - a) * c; break;
            case NodeOp::Scale: value = c * (b - a) + a; break;
            case NodeOp::Select: value = a != 0.0 ? b : c; break;
        }

        marks[index] = 2;
        values[index] = value;
        return true;
    }

    const ProgramView& program;
    const ReferenceEnvironment& env;
    std::vector<uint8_t> marks;
    std::vector<Sample> values;
    ReferenceEvalStatus status = ReferenceEvalStatus::InvalidProgram;
    uint16_t detail = NODE_NONE;
};

} // namespace

ProgramView make_program_view(const NodePool& pool, const CellStore& cells,
                              uint16_t root_node) {
    ProgramView view;
    view.nodes_.assign(pool.nodes, pool.nodes + pool.node_count);
    view.cell_values_.resize(MAX_CELLS);
    cells.snapshot_values(view.cell_values_.data(), MAX_CELLS);

    view.data_offsets_.assign(cells.data_offsets,
                              cells.data_offsets + cells.data_table_count);
    view.data_lengths_.assign(cells.data_lengths,
                              cells.data_lengths + cells.data_table_count);
    size_t data_count = 0;
    if (cells.data_table_count > 0) {
        uint16_t last = cells.data_table_count - 1;
        data_count = static_cast<size_t>(cells.data_offsets[last])
            + cells.data_lengths[last];
    }
    view.data_pool_.assign(cells.data_pool, cells.data_pool + data_count);

    view.live_slot_values_.reserve(pool.live_slot_count);
    for (uint16_t i = 0; i < pool.live_slot_count; ++i)
        view.live_slot_values_.push_back(pool.live_slots[i].value);
    view.previous_outputs_.assign(pool.prev_output_values,
                                  pool.prev_output_values + MAX_OUTPUTS);
    view.state_values_.assign(pool.state_values,
                              pool.state_values + pool.state_slot_count);
    view.root_node_ = root_node;
    view.analysis_ = analyze_program(pool, root_node);
    return view;
}

ReferenceEvalResult evaluate_reference(const ProgramView& program,
                                       const ReferenceEnvironment& env) {
    return ReferenceEvaluator(program, env).run();
}

#endif

} // namespace sig
