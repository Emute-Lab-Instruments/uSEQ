#include "temporal_query.h"

#if USEQ_HAS_TEMPORAL_QUERY

#include "eval_ops.h"
#include "program_semantics.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace sig {
namespace {

constexpr Sample PI = 3.14159265358979323846264338327950288;
constexpr Sample TWO_PI = 2.0 * PI;

struct AffineSegment {
    Sample start;
    Sample end;
    Sample slope;
    Sample offset;
};

using Segments = std::vector<AffineSegment>;

Sample affine_value(const AffineSegment& segment, Sample t) {
    return segment.slope * t + segment.offset;
}

bool near(Sample a, Sample b, Sample tolerance) {
    return std::fabs(a - b) <= tolerance;
}

bool finite_arc(TemporalArc arc) {
    return std::isfinite(arc.start) && std::isfinite(arc.end) &&
           arc.start < arc.end;
}

bool constant_node_value(const NodePool& pool, const CellStore& cells,
                         uint16_t node_index, Sample& value,
                         uint16_t depth = 0) {
    if (node_index == NODE_NONE || node_index >= pool.node_count || depth > 64)
        return false;

    const Node& node = pool.nodes[node_index];
    Sample a = 0.0;
    Sample b = 0.0;
    Sample c = 0.0;
    auto child = [&](uint16_t index, Sample& result) {
        return constant_node_value(pool, cells, index, result, depth + 1);
    };

    switch (node.op) {
        case NodeOp::Const:
            value = node.imm;
            return std::isfinite(value);
        case NodeOp::CellLoad: {
            const uint16_t index = static_cast<uint16_t>(node.imm);
            if (index >= MAX_CELLS) return false;
            value = cells.cells[index].value;
            return std::isfinite(value);
        }
        case NodeOp::SlotLoad: {
            const uint16_t index = static_cast<uint16_t>(node.imm);
            if (index >= pool.live_slot_count) return false;
            value = pool.live_slots[index].value;
            return std::isfinite(value);
        }
        case NodeOp::VecIndex: {
            if (!child(node.input_a, a)) return false;
            const uint16_t table = static_cast<uint16_t>(node.imm);
            if (table >= cells.data_table_count) return false;
            const uint16_t length = cells.data_lengths[table];
            if (length == 0) {
                value = 0.0;
                return true;
            }
            const Sample floored = std::floor(a);
            if (floored < static_cast<Sample>(std::numeric_limits<int>::min()) ||
                floored > static_cast<Sample>(std::numeric_limits<int>::max()))
                return false;
            const int raw_index = static_cast<int>(floored);
            const int wrapped =
                ((raw_index % static_cast<int>(length)) +
                 static_cast<int>(length)) % static_cast<int>(length);
            value = cells.data_pool[cells.data_offsets[table] + wrapped];
            return std::isfinite(value);
        }
        case NodeOp::VecLerp: {
            if (!child(node.input_a, a)) return false;
            const uint16_t table = static_cast<uint16_t>(node.imm);
            if (table >= cells.data_table_count) return false;
            const uint16_t length = cells.data_lengths[table];
            if (length == 0) {
                value = 0.0;
                return true;
            }
            if (length == 1) {
                value = cells.data_pool[cells.data_offsets[table]];
                return std::isfinite(value);
            }
            const Sample scaled = a * static_cast<Sample>(length - 1);
            if (scaled < static_cast<Sample>(std::numeric_limits<int>::min()) ||
                scaled > static_cast<Sample>(std::numeric_limits<int>::max()))
                return false;
            int lower = static_cast<int>(std::floor(scaled));
            lower = std::max(0, std::min(lower, static_cast<int>(length) - 2));
            const int upper = lower + 1;
            const Sample fraction = std::max(
                static_cast<Sample>(0.0),
                std::min(static_cast<Sample>(1.0), scaled - lower));
            const uint16_t offset = cells.data_offsets[table];
            value = cells.data_pool[offset + lower] +
                    (cells.data_pool[offset + upper] -
                     cells.data_pool[offset + lower]) * fraction;
            return std::isfinite(value);
        }
        case NodeOp::RawTimeLoad:
        case NodeOp::GroundTimeLoad:
        case NodeOp::InputLoad:
        case NodeOp::PrevOutputLoad:
        case NodeOp::LoadState:
        case NodeOp::LoadDt:
            return false;
        default:
            break;
    }

    const bool has_a = node.input_a != NODE_NONE;
    const bool has_b = node.input_b != NODE_NONE;
    const bool has_c = node.input_c != NODE_NONE;
    if (has_a && !child(node.input_a, a)) return false;
    if (has_b && !child(node.input_b, b)) return false;
    if (has_c && !child(node.input_c, c)) return false;

    switch (node.op) {
        case NodeOp::Neg:
        case NodeOp::Abs:
        case NodeOp::Floor:
        case NodeOp::Ceil:
        case NodeOp::Frac:
        case NodeOp::Sqrt:
        case NodeOp::Sin:
        case NodeOp::Cos:
        case NodeOp::Tan:
        case NodeOp::USin:
        case NodeOp::UCos:
        case NodeOp::Tri:
        case NodeOp::Sqr:
        case NodeOp::Not:
        case NodeOp::BiToUni:
        case NodeOp::UniToBi:
        case NodeOp::HashIndex:
            value = eval_unary_op(node.op, a);
            break;
        case NodeOp::Add:
        case NodeOp::Sub:
        case NodeOp::Mul:
        case NodeOp::Div:
        case NodeOp::Mod:
        case NodeOp::Expt:
        case NodeOp::Min:
        case NodeOp::Max:
        case NodeOp::Pulse:
        case NodeOp::CmpGt:
        case NodeOp::CmpLt:
        case NodeOp::CmpGe:
        case NodeOp::CmpLe:
        case NodeOp::CmpEq:
        case NodeOp::And:
        case NodeOp::Or:
            value = eval_binary_op(node.op, a, b);
            break;
        case NodeOp::Clamp:
        case NodeOp::Lerp:
        case NodeOp::Scale:
        case NodeOp::Select:
            value = eval_ternary_op(node.op, a, b, c);
            break;
        default:
            return false;
    }
    return std::isfinite(value);
}

struct WaveModel {
    uint16_t angle_node = NODE_NONE;
    Sample angle_multiplier = 1.0;
    Sample angle_offset = 0.0;
    Sample amplitude = 1.0;
    Sample output_offset = 0.0;
};

bool extract_wave_model(const NodePool& pool, const CellStore& cells,
                        uint16_t node_index, WaveModel& model,
                        uint16_t depth = 0) {
    if (node_index == NODE_NONE || node_index >= pool.node_count || depth > 64)
        return false;

    const Node& node = pool.nodes[node_index];
    switch (node.op) {
        case NodeOp::Sin:
            model.angle_node = node.input_a;
            return true;
        case NodeOp::Cos:
            model.angle_node = node.input_a;
            model.angle_offset = PI / 2.0;
            return true;
        case NodeOp::USin:
            model.angle_node = node.input_a;
            model.angle_multiplier = TWO_PI;
            model.amplitude = 0.5;
            model.output_offset = 0.5;
            return true;
        case NodeOp::UCos:
            model.angle_node = node.input_a;
            model.angle_multiplier = TWO_PI;
            model.angle_offset = PI / 2.0;
            model.amplitude = 0.5;
            model.output_offset = 0.5;
            return true;
        default:
            break;
    }

    Sample constant = 0.0;
    WaveModel child;
    switch (node.op) {
        case NodeOp::Neg:
            if (!extract_wave_model(pool, cells, node.input_a, child, depth + 1))
                return false;
            child.amplitude = -child.amplitude;
            child.output_offset = -child.output_offset;
            model = child;
            return true;
        case NodeOp::BiToUni:
            if (!extract_wave_model(pool, cells, node.input_a, child, depth + 1))
                return false;
            child.amplitude *= 0.5;
            child.output_offset = (child.output_offset + 1.0) * 0.5;
            model = child;
            return true;
        case NodeOp::UniToBi:
            if (!extract_wave_model(pool, cells, node.input_a, child, depth + 1))
                return false;
            child.amplitude *= 2.0;
            child.output_offset = child.output_offset * 2.0 - 1.0;
            model = child;
            return true;
        case NodeOp::Add:
            if (constant_node_value(pool, cells, node.input_b, constant) &&
                extract_wave_model(pool, cells, node.input_a, child, depth + 1)) {
                child.output_offset += constant;
                model = child;
                return true;
            }
            if (constant_node_value(pool, cells, node.input_a, constant) &&
                extract_wave_model(pool, cells, node.input_b, child, depth + 1)) {
                child.output_offset += constant;
                model = child;
                return true;
            }
            return false;
        case NodeOp::Sub:
            if (constant_node_value(pool, cells, node.input_b, constant) &&
                extract_wave_model(pool, cells, node.input_a, child, depth + 1)) {
                child.output_offset -= constant;
                model = child;
                return true;
            }
            if (constant_node_value(pool, cells, node.input_a, constant) &&
                extract_wave_model(pool, cells, node.input_b, child, depth + 1)) {
                child.amplitude = -child.amplitude;
                child.output_offset = constant - child.output_offset;
                model = child;
                return true;
            }
            return false;
        case NodeOp::Mul:
            if (constant_node_value(pool, cells, node.input_b, constant) &&
                extract_wave_model(pool, cells, node.input_a, child, depth + 1)) {
                child.amplitude *= constant;
                child.output_offset *= constant;
                model = child;
                return true;
            }
            if (constant_node_value(pool, cells, node.input_a, constant) &&
                extract_wave_model(pool, cells, node.input_b, child, depth + 1)) {
                child.amplitude *= constant;
                child.output_offset *= constant;
                model = child;
                return true;
            }
            return false;
        case NodeOp::Div:
            if (!constant_node_value(pool, cells, node.input_b, constant) ||
                constant == 0.0 ||
                !extract_wave_model(pool, cells, node.input_a, child, depth + 1))
                return false;
            child.amplitude /= constant;
            child.output_offset /= constant;
            model = child;
            return true;
        case NodeOp::Scale: {
            Sample minimum = 0.0;
            Sample maximum = 0.0;
            if (!constant_node_value(pool, cells, node.input_a, minimum) ||
                !constant_node_value(pool, cells, node.input_b, maximum) ||
                !extract_wave_model(pool, cells, node.input_c, child, depth + 1))
                return false;
            const Sample span = maximum - minimum;
            child.amplitude *= span;
            child.output_offset = child.output_offset * span + minimum;
            model = child;
            return true;
        }
        case NodeOp::Lerp: {
            Sample from = 0.0;
            Sample to = 0.0;
            if (!constant_node_value(pool, cells, node.input_a, from) ||
                !constant_node_value(pool, cells, node.input_b, to) ||
                !extract_wave_model(pool, cells, node.input_c, child, depth + 1))
                return false;
            const Sample span = to - from;
            child.amplitude *= span;
            child.output_offset = child.output_offset * span + from;
            model = child;
            return true;
        }
        default:
            return false;
    }
}

class PiecewiseAffineAnalyzer {
public:
    PiecewiseAffineAnalyzer(const NodePool& node_pool,
                            const CellStore& cell_store,
                            TemporalArc query_arc,
                            TemporalQueryOptions query_options)
        : pool(node_pool), cells(cell_store), arc(query_arc),
          options(query_options), memo(node_pool.node_count) {}

    bool analyze(uint16_t node_index, Segments& output) {
        if (node_index == NODE_NONE || node_index >= pool.node_count) {
            fail(TemporalQueryStatus::UnsupportedSignal, node_index);
            return false;
        }
        MemoEntry& entry = memo[node_index];
        if (entry.state == 2) {
            output = entry.segments;
            return true;
        }
        if (entry.state == 1) {
            fail(TemporalQueryStatus::UnsupportedSignal, node_index);
            return false;
        }

        entry.state = 1;
        Segments built;
        if (!build(node_index, built)) {
            entry.state = 0;
            return false;
        }
        entry.segments = built;
        entry.state = 2;
        output = entry.segments;
        return true;
    }

    TemporalQueryStatus status() const { return failure_status; }
    uint16_t detail_node() const { return failure_node; }

private:
    struct MemoEntry {
        uint8_t state = 0;
        Segments segments;
    };

    const NodePool& pool;
    const CellStore& cells;
    TemporalArc arc;
    TemporalQueryOptions options;
    std::vector<MemoEntry> memo;
    TemporalQueryStatus failure_status = TemporalQueryStatus::Complete;
    uint16_t failure_node = NODE_NONE;

    void fail(TemporalQueryStatus status, uint16_t node_index) {
        if (failure_status == TemporalQueryStatus::Complete) {
            failure_status = status;
            failure_node = node_index;
        }
    }

    bool append(Segments& segments, AffineSegment segment) {
        if (!(segment.start < segment.end)) return true;
        if (!segments.empty()) {
            AffineSegment& previous = segments.back();
            if (near(previous.end, segment.start, options.time_tolerance) &&
                near(previous.slope, segment.slope, options.value_tolerance) &&
                near(previous.offset, segment.offset, options.value_tolerance)) {
                previous.end = segment.end;
                return true;
            }
        }
        if (segments.size() >= options.max_segments) {
            fail(TemporalQueryStatus::SegmentLimit, NODE_NONE);
            return false;
        }
        segments.push_back(segment);
        return true;
    }

    bool integer_cuts(const AffineSegment& segment, Sample spacing,
                      std::vector<Sample>& cuts, uint16_t owner_node) {
        cuts.clear();
        cuts.push_back(segment.start);
        cuts.push_back(segment.end);
        if (near(segment.slope, 0.0, options.value_tolerance)) return true;

        spacing = std::fabs(spacing);
        if (!(spacing > 0.0) || !std::isfinite(spacing)) {
            fail(TemporalQueryStatus::UnsupportedSignal, owner_node);
            return false;
        }

        const Sample first_value = affine_value(segment, segment.start);
        const Sample last_value = affine_value(segment, segment.end);
        const Sample minimum = std::min(first_value, last_value);
        const Sample maximum = std::max(first_value, last_value);
        const long double lower = std::ceil(
            static_cast<long double>(minimum) / spacing);
        const long double upper = std::floor(
            static_cast<long double>(maximum) / spacing);
        if (!std::isfinite(static_cast<double>(lower)) ||
            !std::isfinite(static_cast<double>(upper)) ||
            lower < static_cast<long double>(std::numeric_limits<int64_t>::min()) ||
            upper > static_cast<long double>(std::numeric_limits<int64_t>::max())) {
            fail(TemporalQueryStatus::SegmentLimit, owner_node);
            return false;
        }

        const int64_t first = static_cast<int64_t>(lower);
        const int64_t last = static_cast<int64_t>(upper);
        const long double cut_count = upper >= lower ? upper - lower + 1.0L : 0.0L;
        if (cut_count > static_cast<long double>(options.max_segments)) {
            fail(TemporalQueryStatus::SegmentLimit, owner_node);
            return false;
        }
        for (int64_t multiple = first; multiple <= last; ++multiple) {
            const Sample level = static_cast<Sample>(multiple) * spacing;
            const Sample t = (level - segment.offset) / segment.slope;
            if (t > segment.start + options.time_tolerance &&
                t < segment.end - options.time_tolerance)
                cuts.push_back(t);
            if (multiple == std::numeric_limits<int64_t>::max()) break;
        }
        std::sort(cuts.begin(), cuts.end());
        cuts.erase(std::unique(cuts.begin(), cuts.end(), [&](Sample a, Sample b) {
            return near(a, b, options.time_tolerance);
        }), cuts.end());
        return true;
    }

    template <typename Combine>
    bool combine_segments(const Segments& left, const Segments& right,
                          Segments& output, Combine combine,
                          uint16_t owner_node) {
        size_t left_index = 0;
        size_t right_index = 0;
        while (left_index < left.size() && right_index < right.size()) {
            const AffineSegment& lhs = left[left_index];
            const AffineSegment& rhs = right[right_index];
            const Sample start = std::max(lhs.start, rhs.start);
            const Sample end = std::min(lhs.end, rhs.end);
            if (start < end) {
                AffineSegment combined{start, end, 0.0, 0.0};
                if (!combine(lhs, rhs, combined)) {
                    fail(TemporalQueryStatus::UnsupportedSignal, owner_node);
                    return false;
                }
                if (!append(output, combined)) return false;
            }
            if (lhs.end <= end + options.time_tolerance) ++left_index;
            if (rhs.end <= end + options.time_tolerance) ++right_index;
        }
        return !output.empty();
    }

    bool build_mod(uint16_t node_index, const Segments& numerator,
                   const Segments& denominator, Segments& output) {
        size_t left_index = 0;
        size_t right_index = 0;
        std::vector<Sample> cuts;
        while (left_index < numerator.size() && right_index < denominator.size()) {
            const AffineSegment& lhs = numerator[left_index];
            const AffineSegment& rhs = denominator[right_index];
            const Sample start = std::max(lhs.start, rhs.start);
            const Sample end = std::min(lhs.end, rhs.end);
            if (start < end) {
                if (!near(rhs.slope, 0.0, options.value_tolerance) ||
                    rhs.offset == 0.0 || !std::isfinite(rhs.offset)) {
                    fail(TemporalQueryStatus::UnsupportedSignal, node_index);
                    return false;
                }
                const AffineSegment source{start, end, lhs.slope, lhs.offset};
                // With a decreasing argument, floor/fmod's exact value at a
                // discontinuity belongs to the opposite side from our
                // half-open segment convention. Refuse that case until the
                // event model carries endpoint ownership explicitly.
                if (source.slope < -options.value_tolerance) {
                    fail(TemporalQueryStatus::UnsupportedSignal, node_index);
                    return false;
                }
                // std::fmod is truncated rather than floored. Its exact value
                // at negative multiples belongs to the right-hand formula's
                // opposite branch, so the same ownership issue exists even
                // for a forward argument while that argument is negative.
                const Sample source_start = affine_value(source, source.start);
                const Sample source_end = affine_value(source, source.end);
                if (std::min(source_start, source_end) <
                    -options.value_tolerance) {
                    fail(TemporalQueryStatus::UnsupportedSignal, node_index);
                    return false;
                }
                if (!integer_cuts(source, rhs.offset, cuts, node_index))
                    return false;
                for (size_t cut = 0; cut + 1 < cuts.size(); ++cut) {
                    const Sample piece_start = cuts[cut];
                    const Sample piece_end = cuts[cut + 1];
                    const Sample midpoint = piece_start +
                        (piece_end - piece_start) * 0.5;
                    const Sample quotient = std::trunc(
                        affine_value(source, midpoint) / rhs.offset);
                    if (!append(output, {piece_start, piece_end, source.slope,
                                         source.offset - quotient * rhs.offset}))
                        return false;
                }
            }
            if (lhs.end <= end + options.time_tolerance) ++left_index;
            if (rhs.end <= end + options.time_tolerance) ++right_index;
        }
        return !output.empty();
    }

    bool build_integer_unary(uint16_t node_index, const Segments& input,
                             Segments& output, NodeOp operation) {
        std::vector<Sample> cuts;
        for (const AffineSegment& source : input) {
            if (operation == NodeOp::Ceil ||
                source.slope < -options.value_tolerance) {
                fail(TemporalQueryStatus::UnsupportedSignal, node_index);
                return false;
            }
            if (!integer_cuts(source, 1.0, cuts, node_index)) return false;
            for (size_t cut = 0; cut + 1 < cuts.size(); ++cut) {
                const Sample start = cuts[cut];
                const Sample end = cuts[cut + 1];
                const Sample midpoint = start + (end - start) * 0.5;
                const Sample source_value = affine_value(source, midpoint);
                AffineSegment result{start, end, 0.0, 0.0};
                if (operation == NodeOp::Floor) {
                    result.offset = std::floor(source_value);
                } else if (operation == NodeOp::Ceil) {
                    result.offset = std::ceil(source_value);
                } else {
                    const Sample whole = std::floor(source_value);
                    result.slope = source.slope;
                    result.offset = source.offset - whole;
                }
                if (!append(output, result)) return false;
            }
        }
        return !output.empty();
    }

    bool build_vec_index(uint16_t node_index, const Node& node,
                         const Segments& input, Segments& output) {
        const uint16_t table = static_cast<uint16_t>(node.imm);
        if (table >= cells.data_table_count) {
            fail(TemporalQueryStatus::UnsupportedSignal, node_index);
            return false;
        }
        const uint16_t length = cells.data_lengths[table];
        if (length == 0)
            return append(output, {arc.start, arc.end, 0.0, 0.0});

        std::vector<Sample> cuts;
        for (const AffineSegment& source : input) {
            if (source.slope < -options.value_tolerance) {
                fail(TemporalQueryStatus::UnsupportedSignal, node_index);
                return false;
            }
            if (!integer_cuts(source, 1.0, cuts, node_index)) return false;
            for (size_t cut = 0; cut + 1 < cuts.size(); ++cut) {
                const Sample start = cuts[cut];
                const Sample end = cuts[cut + 1];
                const Sample midpoint = start + (end - start) * 0.5;
                const Sample floored = std::floor(affine_value(source, midpoint));
                if (floored < static_cast<Sample>(std::numeric_limits<int>::min()) ||
                    floored > static_cast<Sample>(std::numeric_limits<int>::max())) {
                    fail(TemporalQueryStatus::UnsupportedSignal, node_index);
                    return false;
                }
                const int raw_index = static_cast<int>(floored);
                const int wrapped =
                    ((raw_index % static_cast<int>(length)) +
                     static_cast<int>(length)) % static_cast<int>(length);
                const Sample value =
                    cells.data_pool[cells.data_offsets[table] + wrapped];
                if (!append(output, {start, end, 0.0, value})) return false;
            }
        }
        return !output.empty();
    }

    bool build_abs(uint16_t node_index, const Segments& input,
                   Segments& output) {
        for (const AffineSegment& source : input) {
            std::vector<Sample> cuts{source.start, source.end};
            if (!near(source.slope, 0.0, options.value_tolerance)) {
                const Sample zero = -source.offset / source.slope;
                if (zero > source.start + options.time_tolerance &&
                    zero < source.end - options.time_tolerance)
                    cuts.push_back(zero);
            }
            std::sort(cuts.begin(), cuts.end());
            for (size_t cut = 0; cut + 1 < cuts.size(); ++cut) {
                const Sample start = cuts[cut];
                const Sample end = cuts[cut + 1];
                const Sample midpoint = start + (end - start) * 0.5;
                const Sample sign = affine_value(source, midpoint) < 0.0
                    ? -1.0 : 1.0;
                if (!append(output, {start, end, sign * source.slope,
                                     sign * source.offset}))
                    return false;
            }
        }
        if (output.empty()) fail(TemporalQueryStatus::UnsupportedSignal, node_index);
        return !output.empty();
    }

    bool build(uint16_t node_index, Segments& output) {
        const Node& node = pool.nodes[node_index];
        Segments left;
        Segments right;

        switch (node.op) {
            case NodeOp::Const:
                return append(output, {arc.start, arc.end, 0.0, node.imm});
            case NodeOp::RawTimeLoad:
                return append(output, {arc.start, arc.end, 1.0, 0.0});
            case NodeOp::CellLoad: {
                const uint16_t index = static_cast<uint16_t>(node.imm);
                if (index >= MAX_CELLS) {
                    fail(TemporalQueryStatus::UnsupportedSignal, node_index);
                    return false;
                }
                return append(output,
                    {arc.start, arc.end, 0.0, cells.cells[index].value});
            }
            case NodeOp::SlotLoad: {
                const uint16_t index = static_cast<uint16_t>(node.imm);
                if (index >= pool.live_slot_count) {
                    fail(TemporalQueryStatus::UnsupportedSignal, node_index);
                    return false;
                }
                return append(output,
                    {arc.start, arc.end, 0.0, pool.live_slots[index].value});
            }
            case NodeOp::Neg:
                if (!analyze(node.input_a, left)) return false;
                for (const AffineSegment& segment : left)
                    if (!append(output, {segment.start, segment.end,
                                         -segment.slope, -segment.offset}))
                        return false;
                return true;
            case NodeOp::BiToUni:
            case NodeOp::UniToBi: {
                if (!analyze(node.input_a, left)) return false;
                const Sample scale = node.op == NodeOp::BiToUni ? 0.5 : 2.0;
                const Sample shift = node.op == NodeOp::BiToUni ? 0.5 : -1.0;
                for (const AffineSegment& segment : left)
                    if (!append(output, {segment.start, segment.end,
                                         segment.slope * scale,
                                         segment.offset * scale + shift}))
                        return false;
                return true;
            }
            case NodeOp::Abs:
                if (!analyze(node.input_a, left)) return false;
                return build_abs(node_index, left, output);
            case NodeOp::Floor:
            case NodeOp::Ceil:
            case NodeOp::Frac:
                if (!analyze(node.input_a, left)) return false;
                return build_integer_unary(node_index, left, output, node.op);
            case NodeOp::VecIndex:
                if (!analyze(node.input_a, left)) return false;
                return build_vec_index(node_index, node, left, output);
            case NodeOp::Add:
            case NodeOp::Sub:
            case NodeOp::Mul:
            case NodeOp::Div:
            case NodeOp::Mod:
                if (!analyze(node.input_a, left) ||
                    !analyze(node.input_b, right)) return false;
                break;
            default:
                fail(TemporalQueryStatus::UnsupportedSignal, node_index);
                return false;
        }

        if (node.op == NodeOp::Mod)
            return build_mod(node_index, left, right, output);

        return combine_segments(
            left, right, output,
            [&](const AffineSegment& lhs, const AffineSegment& rhs,
                AffineSegment& result) {
                switch (node.op) {
                    case NodeOp::Add:
                        result.slope = lhs.slope + rhs.slope;
                        result.offset = lhs.offset + rhs.offset;
                        return true;
                    case NodeOp::Sub:
                        result.slope = lhs.slope - rhs.slope;
                        result.offset = lhs.offset - rhs.offset;
                        return true;
                    case NodeOp::Mul:
                        if (near(lhs.slope, 0.0, options.value_tolerance)) {
                            result.slope = lhs.offset * rhs.slope;
                            result.offset = lhs.offset * rhs.offset;
                            return true;
                        }
                        if (near(rhs.slope, 0.0, options.value_tolerance)) {
                            result.slope = rhs.offset * lhs.slope;
                            result.offset = rhs.offset * lhs.offset;
                            return true;
                        }
                        return false;
                    case NodeOp::Div:
                        if (!near(rhs.slope, 0.0, options.value_tolerance) ||
                            rhs.offset == 0.0 || !std::isfinite(rhs.offset))
                            return false;
                        result.slope = lhs.slope / rhs.offset;
                        result.offset = lhs.offset / rhs.offset;
                        return true;
                    default:
                        return false;
                }
            },
            node_index);
    }
};

struct RootMatch {
    Sample time;
    TemporalDirection direction;
};

Sample wave_value(const WaveModel& model, const AffineSegment& angle,
                  Sample t) {
    return model.output_offset +
           model.amplitude * std::sin(affine_value(angle, t));
}

TemporalDirection derivative_direction(Sample derivative,
                                       Sample tolerance) {
    if (derivative > tolerance) return TemporalDirection::Rising;
    if (derivative < -tolerance) return TemporalDirection::Falling;
    return TemporalDirection::Tangent;
}

bool add_root(std::vector<RootMatch>& roots, RootMatch root,
              const TemporalQueryOptions& options) {
    if (roots.size() >= options.max_events) return false;
    roots.push_back(root);
    return true;
}

bool roots_on_segment(const WaveModel& model, const AffineSegment& angle,
                      Sample target, const TemporalQueryOptions& options,
                      std::vector<RootMatch>& roots) {
    if (near(model.amplitude, 0.0, options.value_tolerance) ||
        near(angle.slope, 0.0, options.value_tolerance))
        return true;

    Sample normalized = (target - model.output_offset) / model.amplitude;
    if (normalized < -1.0 - options.value_tolerance ||
        normalized > 1.0 + options.value_tolerance)
        return true;
    normalized = std::max(static_cast<Sample>(-1.0),
                          std::min(static_cast<Sample>(1.0), normalized));

    const Sample principal = std::asin(normalized);
    const Sample bases[2] = {principal, PI - principal};
    const Sample angle_start = affine_value(angle, angle.start);
    const Sample angle_end = affine_value(angle, angle.end);
    const Sample minimum = std::min(angle_start, angle_end);
    const Sample maximum = std::max(angle_start, angle_end);

    for (Sample base : bases) {
        const long double lower = std::ceil(
            (static_cast<long double>(minimum) - base) / TWO_PI -
            options.value_tolerance);
        const long double upper = std::floor(
            (static_cast<long double>(maximum) - base) / TWO_PI +
            options.value_tolerance);
        if (!std::isfinite(static_cast<double>(lower)) ||
            !std::isfinite(static_cast<double>(upper)) ||
            lower < static_cast<long double>(std::numeric_limits<int64_t>::min()) ||
            upper > static_cast<long double>(std::numeric_limits<int64_t>::max()))
            return false;
        const int64_t first = static_cast<int64_t>(lower);
        const int64_t last = static_cast<int64_t>(upper);
        const long double root_count = upper >= lower
            ? upper - lower + 1.0L : 0.0L;
        if (root_count > static_cast<long double>(options.max_events))
            return false;
        for (int64_t cycle = first; cycle <= last; ++cycle) {
            const Sample theta = base + static_cast<Sample>(cycle) * TWO_PI;
            Sample t = (theta - angle.offset) / angle.slope;
            if (near(t, angle.start, options.time_tolerance)) t = angle.start;
            // Each piece owns its left boundary and excludes its right.
            if (t >= angle.start - options.time_tolerance &&
                t < angle.end - options.time_tolerance) {
                const Sample derivative =
                    model.amplitude * angle.slope * std::cos(theta);
                if (!add_root(roots,
                              {t, derivative_direction(
                                      derivative, options.value_tolerance)},
                              options))
                    return false;
            }
            if (cycle == std::numeric_limits<int64_t>::max()) break;
        }
    }
    return true;
}

void sort_and_deduplicate_roots(std::vector<RootMatch>& roots,
                                Sample tolerance) {
    std::sort(roots.begin(), roots.end(),
              [](const RootMatch& left, const RootMatch& right) {
                  return left.time < right.time;
              });
    std::vector<RootMatch> unique;
    for (const RootMatch& root : roots) {
        if (!unique.empty() && near(unique.back().time, root.time, tolerance)) {
            if (unique.back().direction == TemporalDirection::Tangent &&
                root.direction != TemporalDirection::Tangent)
                unique.back().direction = root.direction;
            else if (unique.back().direction != root.direction &&
                     root.direction != TemporalDirection::Tangent)
                unique.back().direction = TemporalDirection::Any;
            continue;
        }
        unique.push_back(root);
    }
    roots.swap(unique);
}

const AffineSegment* segment_at(const Segments& segments, Sample t,
                                Sample tolerance) {
    for (const AffineSegment& segment : segments) {
        if (t >= segment.start - tolerance && t < segment.end - tolerance)
            return &segment;
    }
    if (!segments.empty() && near(t, segments.back().end, tolerance))
        return &segments.back();
    return nullptr;
}

bool direction_matches(TemporalDirection actual,
                       TemporalDirection requested) {
    return requested == TemporalDirection::Any || actual == requested;
}

void merge_span_events(std::vector<TemporalEvent>& events,
                       TemporalArc query_arc,
                       const TemporalQueryOptions& options) {
    std::sort(events.begin(), events.end(), [](const TemporalEvent& left,
                                               const TemporalEvent& right) {
        if (left.part.start != right.part.start)
            return left.part.start < right.part.start;
        return static_cast<uint8_t>(left.kind) < static_cast<uint8_t>(right.kind);
    });

    std::vector<TemporalEvent> merged;
    for (TemporalEvent event : events) {
        if (event.kind == TemporalEventKind::Span && !merged.empty() &&
            merged.back().kind == TemporalEventKind::Span &&
            event.part.start <= merged.back().part.end + options.time_tolerance) {
            merged.back().part.end = std::max(merged.back().part.end,
                                              event.part.end);
            merged.back().whole = merged.back().part;
            continue;
        }
        merged.push_back(event);
    }

    std::vector<TemporalEvent> without_covered_instants;
    for (const TemporalEvent& event : merged) {
        if (event.kind == TemporalEventKind::Instant) {
            bool covered = false;
            for (const TemporalEvent& candidate : merged) {
                if (candidate.kind == TemporalEventKind::Span &&
                    event.part.start >= candidate.part.start -
                        options.time_tolerance &&
                    event.part.start <= candidate.part.end +
                        options.time_tolerance) {
                    covered = true;
                    break;
                }
            }
            if (covered) continue;
        }
        without_covered_instants.push_back(event);
    }

    for (TemporalEvent& event : without_covered_instants) {
        if (event.kind == TemporalEventKind::Span) {
            event.whole_known =
                event.part.start > query_arc.start + options.time_tolerance &&
                event.part.end < query_arc.end - options.time_tolerance;
            event.whole = event.part;
        }
    }
    events.swap(without_covered_instants);
}

bool build_equal_or_crossing_events(
    const WaveModel& model, const Segments& angles, TemporalArc query_arc,
    TemporalCondition condition, const TemporalQueryOptions& options,
    std::vector<TemporalEvent>& events) {
    std::vector<RootMatch> roots;
    for (const AffineSegment& angle : angles) {
        const Sample value = wave_value(model, angle, angle.start);
        if ((near(model.amplitude, 0.0, options.value_tolerance) ||
             near(angle.slope, 0.0, options.value_tolerance)) &&
            near(value, condition.low, options.value_tolerance)) {
            if (condition.kind == TemporalConditionKind::Equal) {
                events.push_back({{angle.start, angle.end},
                                  {angle.start, angle.end},
                                  TemporalEventKind::Span,
                                  TemporalDirection::Tangent, true});
            }
            continue;
        }
        if (!roots_on_segment(model, angle, condition.low, options, roots))
            return false;
    }

    if (condition.kind == TemporalConditionKind::Crossing) {
        // A discontinuity may cross a level without ever equalling it. Treat
        // that as a directed boundary event, distinct from an equality root.
        for (size_t index = 1; index < angles.size(); ++index) {
            const AffineSegment& left = angles[index - 1];
            const AffineSegment& right = angles[index];
            if (!near(left.end, right.start, options.time_tolerance)) continue;
            const Sample left_value = wave_value(model, left, left.end);
            const Sample right_value = wave_value(model, right, right.start);
            const Sample left_delta = left_value - condition.low;
            const Sample right_delta = right_value - condition.low;
            if (left_delta < -options.value_tolerance &&
                right_delta > options.value_tolerance) {
                if (!add_root(roots, {right.start, TemporalDirection::Rising},
                              options)) return false;
            } else if (left_delta > options.value_tolerance &&
                       right_delta < -options.value_tolerance) {
                if (!add_root(roots, {right.start, TemporalDirection::Falling},
                              options)) return false;
            }
        }
    }

    sort_and_deduplicate_roots(roots, options.time_tolerance);
    for (const RootMatch& root : roots) {
        if (root.time < query_arc.start - options.time_tolerance ||
            root.time >= query_arc.end - options.time_tolerance)
            continue;
        if (condition.kind == TemporalConditionKind::Crossing &&
            (root.direction == TemporalDirection::Tangent ||
             !direction_matches(root.direction, condition.direction)))
            continue;
        events.push_back({{root.time, root.time}, {root.time, root.time},
                          TemporalEventKind::Instant, root.direction, true});
    }
    merge_span_events(events, query_arc, options);
    return events.size() <= options.max_events;
}

bool build_between_events(const WaveModel& model, const Segments& angles,
                          TemporalArc query_arc, TemporalCondition condition,
                          const TemporalQueryOptions& options,
                          std::vector<TemporalEvent>& events) {
    std::vector<Sample> cuts{query_arc.start, query_arc.end};
    for (const AffineSegment& angle : angles) {
        cuts.push_back(angle.start);
        cuts.push_back(angle.end);
        std::vector<RootMatch> boundary_roots;
        if (!roots_on_segment(model, angle, condition.low, options,
                              boundary_roots) ||
            !roots_on_segment(model, angle, condition.high, options,
                              boundary_roots))
            return false;
        for (const RootMatch& root : boundary_roots) cuts.push_back(root.time);
        if (cuts.size() > options.max_events * 2 + options.max_segments * 2)
            return false;
    }

    std::sort(cuts.begin(), cuts.end());
    cuts.erase(std::unique(cuts.begin(), cuts.end(), [&](Sample a, Sample b) {
        return near(a, b, options.time_tolerance);
    }), cuts.end());

    auto inside = [&](Sample t) {
        const AffineSegment* angle =
            segment_at(angles, t, options.time_tolerance);
        if (!angle) return false;
        const Sample value = wave_value(model, *angle, t);
        return value >= condition.low - options.value_tolerance &&
               value <= condition.high + options.value_tolerance;
    };

    for (size_t index = 0; index + 1 < cuts.size(); ++index) {
        const Sample start = std::max(cuts[index], query_arc.start);
        const Sample end = std::min(cuts[index + 1], query_arc.end);
        if (!(start < end)) continue;
        const Sample midpoint = start + (end - start) * 0.5;
        if (inside(midpoint)) {
            events.push_back({{start, end}, {start, end},
                              TemporalEventKind::Span,
                              TemporalDirection::Any, true});
        }
    }

    // Preserve a boundary-only match (for example a sine tangent touching the
    // lower edge of a range) when neither adjacent open interval is inside.
    for (Sample cut : cuts) {
        if (cut < query_arc.start - options.time_tolerance ||
            cut >= query_arc.end - options.time_tolerance || !inside(cut))
            continue;
        bool covered = false;
        for (const TemporalEvent& event : events) {
            if (event.kind == TemporalEventKind::Span &&
                cut >= event.part.start - options.time_tolerance &&
                cut <= event.part.end + options.time_tolerance) {
                covered = true;
                break;
            }
        }
        if (!covered) {
            events.push_back({{cut, cut}, {cut, cut},
                              TemporalEventKind::Instant,
                              TemporalDirection::Tangent, true});
        }
    }

    merge_span_events(events, query_arc, options);
    return events.size() <= options.max_events;
}

} // namespace

static bool valid_options(TemporalArc arc,
                          const TemporalQueryOptions& options) {
    return finite_arc(arc) && std::isfinite(options.time_tolerance) &&
           std::isfinite(options.value_tolerance) &&
           options.time_tolerance > 0.0 && options.value_tolerance > 0.0 &&
           options.max_segments > 0 && options.max_events > 0;
}

static bool valid_condition_request(TemporalCondition condition,
                                    const TemporalEvent* events,
                                    size_t event_capacity) {
    return std::isfinite(condition.low) && std::isfinite(condition.high) &&
           (event_capacity == 0 || events != nullptr) &&
           (condition.kind != TemporalConditionKind::Between ||
            condition.low <= condition.high) &&
           (condition.kind != TemporalConditionKind::Crossing ||
            condition.direction != TemporalDirection::Tangent);
}

static bool prepare_temporal_root(
    const NodePool& pool,
    const CellStore& cells,
    uint16_t root_node,
    TemporalArc arc,
    TemporalQueryOptions options,
    TemporalQueryResult& result,
    WaveModel& model,
    Segments& angles) {
    if (!valid_options(arc, options)) {
        result.status = TemporalQueryStatus::InvalidArgument;
        return false;
    }
    if (root_node == NODE_NONE || root_node >= pool.node_count) {
        result.status = TemporalQueryStatus::UnsupportedSignal;
        result.detail_node = root_node;
        return false;
    }

    ProgramAnalysis analysis = analyze_program(pool, root_node);
    if (!analysis.valid_finite_dag) {
        result.status = TemporalQueryStatus::UnsupportedSignal;
        result.detail_node = root_node;
        return false;
    }
    if ((analysis.effects & (EffectState | EffectPreviousOutput |
                             EffectDeltaTime)) != 0) {
        // The runtime denotation depends on a tick history. Even when the
        // source resembles a symbolic integral, solving it without a schedule
        // would answer a different semantics from the VM.
        result.status = TemporalQueryStatus::RequiresSchedule;
        result.detail_node = root_node;
        return false;
    }
    if ((analysis.effects & (EffectHardwareInput | EffectLiveInput)) != 0) {
        result.status = TemporalQueryStatus::ExternalEnvironment;
        result.detail_node = root_node;
        return false;
    }
    if ((analysis.effects & EffectGroundTime) != 0) {
        result.status = TemporalQueryStatus::GroundTimeDomain;
        result.detail_node = root_node;
        return false;
    }

    if (!extract_wave_model(pool, cells, root_node, model)) {
        result.status = TemporalQueryStatus::UnsupportedSignal;
        result.detail_node = root_node;
        return false;
    }

    PiecewiseAffineAnalyzer analyzer(pool, cells, arc, options);
    if (!analyzer.analyze(model.angle_node, angles)) {
        result.status = analyzer.status();
        result.detail_node = analyzer.detail_node();
        return false;
    }
    for (AffineSegment& angle : angles) {
        angle.slope *= model.angle_multiplier;
        angle.offset = angle.offset * model.angle_multiplier +
                       model.angle_offset;
    }
    result.segment_count = angles.size();
    result.status = TemporalQueryStatus::Complete;
    return true;
}

TemporalQueryResult inspect_temporal_root(
    const NodePool& pool,
    const CellStore& cells,
    uint16_t root_node,
    TemporalArc arc,
    TemporalQueryOptions options) {
    TemporalQueryResult result;
    WaveModel model;
    Segments angles;
    prepare_temporal_root(pool, cells, root_node, arc, options,
                          result, model, angles);
    return result;
}

TemporalQueryResult query_temporal_root(
    const NodePool& pool,
    const CellStore& cells,
    uint16_t root_node,
    TemporalArc arc,
    TemporalCondition condition,
    TemporalEvent* events,
    size_t event_capacity,
    TemporalQueryOptions options) {
    TemporalQueryResult result;
    if (!valid_condition_request(condition, events, event_capacity)) {
        result.status = TemporalQueryStatus::InvalidArgument;
        return result;
    }

    WaveModel model;
    Segments angles;
    if (!prepare_temporal_root(pool, cells, root_node, arc, options,
                               result, model, angles))
        return result;

    std::vector<TemporalEvent> found;
    const bool complete = condition.kind == TemporalConditionKind::Between
        ? build_between_events(model, angles, arc, condition, options, found)
        : build_equal_or_crossing_events(model, angles, arc, condition,
                                         options, found);
    if (!complete) {
        result.status = TemporalQueryStatus::EventLimit;
    } else {
        result.status = TemporalQueryStatus::Complete;
    }

    const size_t copied = std::min(found.size(), event_capacity);
    for (size_t index = 0; index < copied; ++index) events[index] = found[index];
    result.event_count = copied;
    if (found.size() > event_capacity)
        result.status = TemporalQueryStatus::EventLimit;
    return result;
}

TemporalQueryResult query_temporal_output(
    const NodePool& pool,
    const CellStore& cells,
    uint16_t output_index,
    TemporalArc arc,
    TemporalCondition condition,
    TemporalEvent* events,
    size_t event_capacity,
    TemporalQueryOptions options) {
    if (!valid_options(arc, options) ||
        !valid_condition_request(condition, events, event_capacity)) {
        TemporalQueryResult result;
        result.status = TemporalQueryStatus::InvalidArgument;
        return result;
    }
    if (output_index >= MAX_OUTPUTS || !pool.outputs[output_index].valid ||
        pool.outputs[output_index].root_node == NODE_NONE) {
        TemporalQueryResult result;
        result.status = TemporalQueryStatus::InactiveOutput;
        return result;
    }
    return query_temporal_root(
        pool, cells, pool.outputs[output_index].root_node, arc, condition,
        events, event_capacity, options);
}

const char* temporal_query_status_to_cstr(TemporalQueryStatus status) {
    switch (status) {
        case TemporalQueryStatus::Complete: return "complete";
        case TemporalQueryStatus::InvalidArgument: return "invalid-argument";
        case TemporalQueryStatus::InactiveOutput: return "inactive-output";
        case TemporalQueryStatus::UnsupportedSignal: return "unsupported-signal";
        case TemporalQueryStatus::RequiresSchedule: return "requires-schedule";
        case TemporalQueryStatus::ExternalEnvironment: return "external-environment";
        case TemporalQueryStatus::GroundTimeDomain: return "ground-time-domain";
        case TemporalQueryStatus::SegmentLimit: return "segment-limit";
        case TemporalQueryStatus::EventLimit: return "event-limit";
    }
    return "invalid-argument";
}

} // namespace sig

#endif // USEQ_HAS_TEMPORAL_QUERY
