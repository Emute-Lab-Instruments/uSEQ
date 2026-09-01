// Temporal condition queries over compiled pure signal graphs.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "src/signal_engine/signal_engine.h"

#include <cmath>
#include <cstring>
#include <string>

using namespace sig;

namespace {

struct QueryHarness {
    SignalEngine engine;
    TemporalEvent events[256] = {};

    QueryHarness() { engine.init_defaults(); }

    void assign(const char* expression) {
        const std::string source = std::string("(a1 ") + expression + ")";
        const EvalResult result = eval_cold(
            source.c_str(), static_cast<uint32_t>(source.size()), engine);
        INFO("source: " << source);
        if (result.kind == EvalResult::Error && result.diagnostic_count > 0) {
            INFO("diagnostic: " << result.diagnostics[0].message);
        }
        REQUIRE(result.kind != EvalResult::Error);
        engine.pool.rebuild_execution_order();
    }

    TemporalQueryResult query(TemporalArc arc,
                              TemporalCondition condition) {
        for (TemporalEvent& event : events) event = {};
        return query_temporal_output(engine.pool, engine.cells, 0, arc,
                                     condition, events, 256);
    }
};

void require_instant(const TemporalEvent& event, double time,
                     TemporalDirection direction) {
    REQUIRE(event.kind == TemporalEventKind::Instant);
    REQUIRE(event.part.start == Approx(time).margin(1e-9));
    REQUIRE(event.part.end == Approx(time).margin(1e-9));
    REQUIRE(event.direction == direction);
    REQUIRE(event.whole_known);
}

void require_span(const TemporalEvent& event, double start, double end) {
    REQUIRE(event.kind == TemporalEventKind::Span);
    REQUIRE(event.part.start == Approx(start).margin(1e-9));
    REQUIRE(event.part.end == Approx(end).margin(1e-9));
}

EvalResult eval(SignalEngine& engine, const char* source) {
    return eval_cold(source, static_cast<uint32_t>(std::strlen(source)), engine);
}

std::string text_of(const EvalResult& result) {
    REQUIRE(result.kind == EvalResult::Text);
    return std::string(result.text, result.text_length);
}

} // namespace

TEST_CASE("temporal query distinguishes roots, crossings, and tangencies",
          "[signal_engine][temporal_query]") {
    QueryHarness harness;
    harness.assign("(bsin t)");

    TemporalQueryResult roots = harness.query(
        {0.0, 1.0}, TemporalCondition::equal_to(0.0));
    REQUIRE(roots.status == TemporalQueryStatus::Complete);
    REQUIRE(roots.segment_count == 1);
    REQUIRE(roots.event_count == 2);
    require_instant(harness.events[0], 0.0, TemporalDirection::Rising);
    require_instant(harness.events[1], 0.5, TemporalDirection::Falling);

    TemporalQueryResult rising = harness.query(
        {0.0, 1.0},
        TemporalCondition::crosses(0.0, TemporalDirection::Rising));
    REQUIRE(rising.status == TemporalQueryStatus::Complete);
    REQUIRE(rising.event_count == 1);
    require_instant(harness.events[0], 0.0, TemporalDirection::Rising);

    // Bare sin is unipolar in the current language: zero is a trough touch,
    // not a sign-changing crossing.
    harness.assign("(sin t)");
    TemporalQueryResult touches = harness.query(
        {0.0, 1.0}, TemporalCondition::equal_to(0.0));
    REQUIRE(touches.status == TemporalQueryStatus::Complete);
    REQUIRE(touches.event_count == 1);
    require_instant(harness.events[0], 0.75, TemporalDirection::Tangent);

    TemporalQueryResult no_crossing = harness.query(
        {0.0, 1.0}, TemporalCondition::crosses(0.0));
    REQUIRE(no_crossing.status == TemporalQueryStatus::Complete);
    REQUIRE(no_crossing.event_count == 0);
}

TEST_CASE("temporal query returns maximal clipped range spans",
          "[signal_engine][temporal_query]") {
    QueryHarness harness;
    harness.assign("(bsin t)");

    TemporalQueryResult result = harness.query(
        {0.0, 1.0}, TemporalCondition::between(-0.5, 0.5));
    REQUIRE(result.status == TemporalQueryStatus::Complete);
    REQUIRE(result.event_count == 3);
    require_span(harness.events[0], 0.0, 1.0 / 12.0);
    require_span(harness.events[1], 5.0 / 12.0, 7.0 / 12.0);
    require_span(harness.events[2], 11.0 / 12.0, 1.0);
    REQUIRE_FALSE(harness.events[0].whole_known);
    REQUIRE(harness.events[1].whole_known);
    REQUIRE_FALSE(harness.events[2].whole_known);
}

TEST_CASE("temporal query solves stepped piecewise-affine frequency",
          "[signal_engine][temporal_query]") {
    QueryHarness harness;
    harness.assign(
        "(bsin (* t (from-list [1 3 2 4] (slow 4 bar))))");

    TemporalQueryResult result = harness.query(
        {0.0, 8.0}, TemporalCondition::crosses(0.0));
    REQUIRE(result.status == TemporalQueryStatus::Complete);
    REQUIRE(result.segment_count == 4);
    REQUIRE(result.event_count == 40);
    require_instant(harness.events[0], 0.0, TemporalDirection::Rising);
    require_instant(harness.events[1], 0.5, TemporalDirection::Falling);
    require_instant(harness.events[4], 2.0, TemporalDirection::Rising);
    require_instant(harness.events[5], 13.0 / 6.0,
                    TemporalDirection::Falling);
    require_instant(harness.events[39], 63.0 / 8.0,
                    TemporalDirection::Falling);
}

TEST_CASE("temporal query reports sign-changing discontinuities",
          "[signal_engine][temporal_query]") {
    QueryHarness harness;
    harness.assign("(bsin (from-list [0.1 0.6] (slow 2 bar)))");

    TemporalQueryResult roots = harness.query(
        {0.0, 4.0}, TemporalCondition::equal_to(0.0));
    REQUIRE(roots.status == TemporalQueryStatus::Complete);
    REQUIRE(roots.event_count == 0);

    TemporalQueryResult crossings = harness.query(
        {0.0, 4.0}, TemporalCondition::crosses(0.0));
    REQUIRE(crossings.status == TemporalQueryStatus::Complete);
    REQUIRE(crossings.event_count == 1);
    require_instant(harness.events[0], 2.0, TemporalDirection::Falling);
}

TEST_CASE("temporal query partitions forward non-negative remainder",
          "[signal_engine][temporal_query]") {
    QueryHarness harness;
    harness.assign("(bsin (% t 1))");

    TemporalQueryResult forward = harness.query(
        {0.0, 2.0}, TemporalCondition::crosses(0.0));
    REQUIRE(forward.status == TemporalQueryStatus::Complete);
    REQUIRE(forward.segment_count == 2);
    REQUIRE(forward.event_count == 4);
    require_instant(harness.events[0], 0.0, TemporalDirection::Rising);
    require_instant(harness.events[1], 0.5, TemporalDirection::Falling);
    require_instant(harness.events[2], 1.0, TemporalDirection::Rising);
    require_instant(harness.events[3], 1.5, TemporalDirection::Falling);

    TemporalQueryResult negative = harness.query(
        {-1.0, 1.0}, TemporalCondition::crosses(0.0));
    REQUIRE(negative.status == TemporalQueryStatus::UnsupportedSignal);
}

TEST_CASE("temporal query refuses nonlinear and singular time substitutions",
          "[signal_engine][temporal_query]") {
    QueryHarness harness;
    harness.assign(
        "(bsin (* t (from-list [1 3 2 4] (slow (% t 1) bar))))");

    TemporalQueryResult result = harness.query(
        {0.1, 2.9}, TemporalCondition::crosses(0.0));
    REQUIRE(result.status == TemporalQueryStatus::UnsupportedSignal);
    REQUIRE(result.detail_node != NODE_NONE);
    REQUIRE(result.event_count == 0);
}

TEST_CASE("temporal query states the exact-expression versus history boundary",
          "[signal_engine][temporal_query][capabilities]") {
    QueryHarness harness;
    harness.assign("(premap (* 2 t) (bsin t))");
    TemporalQueryResult exact = harness.query(
        {0.0, 1.0}, TemporalCondition::crosses(0.0));
    REQUIRE(exact.status == TemporalQueryStatus::Complete);

    // integrate is deliberately the runtime's sampled tick recurrence, not a
    // silently substituted symbolic antiderivative, even for constant input.
    harness.assign("(bsin (integrate 1))");
    REQUIRE(harness.query({0.0, 1.0}, TemporalCondition::crosses(0.0)).status ==
            TemporalQueryStatus::RequiresSchedule);

    harness.assign("(bsin ain1)");
    REQUIRE(harness.query({0.0, 1.0}, TemporalCondition::crosses(0.0)).status ==
            TemporalQueryStatus::ExternalEnvironment);

    harness.assign("(bsin ground-time)");
    REQUIRE(harness.query({0.0, 1.0}, TemporalCondition::crosses(0.0)).status ==
            TemporalQueryStatus::GroundTimeDomain);
}

TEST_CASE("ModuLisp q forms expose inline temporal events with target last",
          "[signal_engine][temporal_query][cold_eval]") {
    SignalEngine engine;
    engine.init_defaults();

    const EvalResult zeros = eval(engine, "(q/zeros [0 1] (bsin t))");
    REQUIRE(text_of(zeros) ==
        "[[:instant :rising :whole [0 0] :part [0 0]] "
        "[:instant :falling :whole [0.5 0.5] :part [0.5 0.5]]]");

    const EvalResult rising = eval(
        engine, "(q/crossings 0 [0 1] :rising (bsin t))");
    REQUIRE(text_of(rising) ==
        "[[:instant :rising :whole [0 0] :part [0 0]]]");

    const EvalResult falling = eval(
        engine, "(q/crossings 0 [0 1] :falling (bsin t))");
    REQUIRE(text_of(falling) ==
        "[[:instant :falling :whole [0.5 0.5] :part [0.5 0.5]]]");

    const EvalResult range = eval(
        engine, "(q/range [-0.5 0.5] [0 1] (bsin t))");
    const std::string range_text = text_of(range);
    REQUIRE(range_text.find("[:span :whole nil :part [0 0.0833333333333333]]")
            != std::string::npos);
    REQUIRE(range_text.find(
        "[:span :whole [0.416666666666667 0.583333333333333] ")
        != std::string::npos);
    REQUIRE(range_text.find(
        "[:span :whole nil :part [0.916666666666667 1]]")
        != std::string::npos);
}

TEST_CASE("q forms query published outputs and preserve live graph state",
          "[signal_engine][temporal_query][cold_eval][transaction]") {
    SignalEngine engine;
    engine.init_defaults();
    REQUIRE(eval(engine, "(a1 (bsin t))").kind != EvalResult::Error);
    const uint16_t root_before = engine.pool.outputs[0].root_node;
    const uint16_t nodes_before = engine.pool.node_count;
    const uint8_t tables_before = engine.cells.data_table_count;

    const EvalResult output_query = eval(engine, "(q/zeros [0 1] a1)");
    REQUIRE(text_of(output_query).find(":falling") != std::string::npos);

    const EvalResult inline_query = eval(
        engine,
        "(q/crossings 0 [0 8] "
        "(bsin (* t (from-list [1 3 2 4] (slow 4 bar)))))");
    const std::string inline_text = text_of(inline_query);
    REQUIRE(inline_text.find("7.875") != std::string::npos);

    REQUIRE(engine.pool.outputs[0].root_node == root_before);
    REQUIRE(engine.pool.node_count == nodes_before);
    REQUIRE(engine.cells.data_table_count == tables_before);
}

TEST_CASE("q/explain reports analytic support without pretending unsupported is empty",
          "[signal_engine][temporal_query][cold_eval][explain]") {
    SignalEngine engine;
    engine.init_defaults();

    REQUIRE(text_of(eval(engine, "(q/explain [0 1] (bsin t))")) ==
            "{:status :complete :analysis :analytic-piecewise-affine-wave "
            ":segments 1}");

    const EvalResult unsupported = eval(
        engine,
        "(q/explain [0.1 2.9] "
        "(bsin (* t (from-list [1 3 2 4] (slow (% t 1) bar)))))");
    const std::string unsupported_text = text_of(unsupported);
    REQUIRE(unsupported_text.find(":status :unsupported-signal") !=
            std::string::npos);
    REQUIRE(unsupported_text.find(":detail-node") != std::string::npos);
}

TEST_CASE("every q form rejects a non-final target signal",
          "[signal_engine][temporal_query][cold_eval][arity]") {
    SignalEngine engine;
    engine.init_defaults();
    const char* invalid[] = {
        "(q/zeros [0 1] (bsin t) 0)",
        "(q/crossings 0 [0 1] (bsin t) :rising)",
        "(q/range [-1 1] [0 1] (bsin t) 0)",
        "(q/explain [0 1] (bsin t) 0)",
    };
    for (const char* source : invalid) {
        DYNAMIC_SECTION(source) {
            const EvalResult result = eval(engine, source);
            REQUIRE(result.kind == EvalResult::Error);
            REQUIRE(result.diagnostic_count > 0);
            REQUIRE(std::string(result.diagnostics[0].message).find(
                        "must be the last argument") != std::string::npos);
        }
    }
}
