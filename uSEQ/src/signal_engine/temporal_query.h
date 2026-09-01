#ifndef SIGNAL_ENGINE_TEMPORAL_QUERY_H
#define SIGNAL_ENGINE_TEMPORAL_QUERY_H

#include "build_profile.h"

#if USEQ_HAS_TEMPORAL_QUERY

#include "cell_store.h"
#include "node_pool.h"

#include <cstddef>

namespace sig {

// Host cold-path query over an already-compiled signal graph. This is
// deliberately separate from per-sample execution: it describes where a
// condition holds over an interval rather than adding an event value to the
// signal hot path.

struct TemporalArc {
    Sample start = 0.0;
    Sample end = 0.0;
};

enum class TemporalConditionKind : uint8_t {
    Equal,
    Crossing,
    Between,
};

enum class TemporalDirection : uint8_t {
    Any,
    Rising,
    Falling,
    Tangent,
};

struct TemporalCondition {
    TemporalConditionKind kind = TemporalConditionKind::Equal;
    Sample low = 0.0;
    Sample high = 0.0;
    TemporalDirection direction = TemporalDirection::Any;

    static TemporalCondition equal_to(Sample value) {
        return {TemporalConditionKind::Equal, value, value,
                TemporalDirection::Any};
    }

    static TemporalCondition crosses(
        Sample value, TemporalDirection requested = TemporalDirection::Any) {
        return {TemporalConditionKind::Crossing, value, value, requested};
    }

    static TemporalCondition between(Sample minimum, Sample maximum) {
        return {TemporalConditionKind::Between, minimum, maximum,
                TemporalDirection::Any};
    }
};

enum class TemporalEventKind : uint8_t {
    Instant,
    Span,
};

struct TemporalEvent {
    // `part` is always the portion returned for the requested query arc.
    // `whole_known` is false when a span touches a query edge and this first
    // prototype did not inspect far enough outside the query to recover the
    // complete connected component. This mirrors the whole/part distinction
    // needed by an eventual Tidal-style event surface without guessing it.
    TemporalArc whole = {};
    TemporalArc part = {};
    TemporalEventKind kind = TemporalEventKind::Instant;
    TemporalDirection direction = TemporalDirection::Any;
    bool whole_known = true;
};

enum class TemporalQueryStatus : uint8_t {
    Complete,
    InvalidArgument,
    InactiveOutput,
    UnsupportedSignal,
    RequiresSchedule,
    ExternalEnvironment,
    GroundTimeDomain,
    SegmentLimit,
    EventLimit,
};

enum class TemporalAnalysisKind : uint8_t {
    AnalyticPiecewiseAffineWave,
};

struct TemporalQueryOptions {
    Sample time_tolerance = 1e-9;
    Sample value_tolerance = 1e-9;
    size_t max_segments = 512;
    size_t max_events = 1024;
};

struct TemporalQueryResult {
    TemporalQueryStatus status = TemporalQueryStatus::InvalidArgument;
    TemporalAnalysisKind analysis =
        TemporalAnalysisKind::AnalyticPiecewiseAffineWave;
    size_t event_count = 0;
    size_t segment_count = 0;
    uint16_t detail_node = NODE_NONE;
};

// Inspect whether a graph root belongs to the currently supported analytic
// fragment without constructing condition events. Used by q/explain.
TemporalQueryResult inspect_temporal_root(
    const NodePool& pool,
    const CellStore& cells,
    uint16_t root_node,
    TemporalArc arc,
    TemporalQueryOptions options = {});

// Query an arbitrary compiled graph root. This is the primitive used by the
// ModuLisp q/* forms after their final signal argument has been compiled into
// the scratch pool.
TemporalQueryResult query_temporal_root(
    const NodePool& pool,
    const CellStore& cells,
    uint16_t root_node,
    TemporalArc arc,
    TemporalCondition condition,
    TemporalEvent* events,
    size_t event_capacity,
    TemporalQueryOptions options = {});

// Query one published output over the half-open interval [arc.start, arc.end).
// The current complete fragment is a linearly post-mapped sine/cosine whose
// angle is piecewise affine in `t`. It includes forward affine time warps,
// non-negative modulo / floor partitions, and stepped data (`step` /
// `from-list`).
// Unsupported state, input, nonlinear-time, reversed discontinuous partitions,
// or opaque nodes fail explicitly; no sampling fallback is silently
// substituted.
TemporalQueryResult query_temporal_output(
    const NodePool& pool,
    const CellStore& cells,
    uint16_t output_index,
    TemporalArc arc,
    TemporalCondition condition,
    TemporalEvent* events,
    size_t event_capacity,
    TemporalQueryOptions options = {});

const char* temporal_query_status_to_cstr(TemporalQueryStatus status);

} // namespace sig

#endif // USEQ_HAS_TEMPORAL_QUERY

#endif // SIGNAL_ENGINE_TEMPORAL_QUERY_H
