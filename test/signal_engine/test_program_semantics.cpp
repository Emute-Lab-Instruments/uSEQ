// Proof-facing tests for the finite-DAG witness, dependency capabilities,
// independent reference denotation, and the function-profunctor laws.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "src/signal_engine/signal_engine.h"

#include <cmath>
#include <cstring>
#include <functional>
#include <string>

using namespace sig;

namespace {

struct CompiledExpression {
    SignalEngine engine;
    Sample cells[MAX_CELLS] = {};
    Sample inputs[32] = {};
    Sample outputs[MAX_OUTPUTS] = {};
    Sample workspace[MAX_TOTAL_NODES] = {};

    explicit CompiledExpression(const char* expression) {
        engine.init_defaults();
        std::string source = std::string("(a1 ") + expression + ")";
        EvalResult result = eval_cold(
            source.c_str(), static_cast<uint32_t>(source.size()), engine);
        INFO("source: " << source);
        REQUIRE(result.kind != EvalResult::Error);
        engine.cells.snapshot_values(cells, MAX_CELLS);
    }

    uint16_t root() const { return engine.pool.outputs[0].root_node; }

    Sample execute(Sample time, Sample ground_time = 0.0,
                   Sample wall_dt = 0.0) {
        ExecutionContext context;
        context.t = time;
        context.dt = wall_dt;
        context.cell_values = cells;
        context.hw_inputs = inputs;
        context.data_pool = engine.cells.data_pool;
        context.data_offsets = engine.cells.data_offsets;
        context.data_lengths = engine.cells.data_lengths;
        context.prev_outputs = engine.pool.prev_output_values;
        context.output_values = outputs;
        context.workspace = workspace;
        context.ground_t = ground_time;
        execute_all_outputs(engine.pool, context);
        return outputs[0];
    }

    ReferenceEvalResult reference(Sample time, Sample ground_time = 0.0,
                                  Sample wall_dt = 0.0) {
        ProgramView view = make_program_view(engine.pool, engine.cells, root());
        ReferenceEnvironment environment;
        environment.local_time = time;
        environment.ground_time = ground_time;
        environment.wall_dt = wall_dt;
        environment.hardware_inputs = inputs;
        environment.previous_outputs = engine.pool.prev_output_values;
        environment.state_values = engine.pool.state_values;
        return evaluate_reference(view, environment);
    }
};

Sample dimap(const std::function<Sample(Sample)>& pre,
             const std::function<Sample(Sample)>& post,
             const std::function<Sample(Sample)>& arrow,
             Sample input) {
    return post(arrow(pre(input)));
}

} // namespace

TEST_CASE("Program analysis exposes a finite-DAG termination witness and effects",
          "[program-semantics][analysis]") {
    CompiledExpression pure("(premap (+ t 0.25) (sin beat))");
    ProgramAnalysis pure_analysis = analyze_program(pure.engine.pool, pure.root());
    REQUIRE(pure_analysis.guaranteed_to_terminate());
    REQUIRE(pure_analysis.reachable_nodes > 0);
    REQUIRE(pure_analysis.stateless());
    REQUIRE(pure_analysis.schedule_independent());
    REQUIRE(pure_analysis.closed_function_of_time());
    REQUIRE((pure_analysis.effects & EffectLocalTime) != 0);

    CompiledExpression ground("ground-time");
    ProgramAnalysis ground_analysis =
        analyze_program(ground.engine.pool, ground.root());
    REQUIRE(ground_analysis.stateless());
    REQUIRE(ground_analysis.schedule_independent());
    REQUIRE_FALSE(ground_analysis.closed_function_of_time());
    REQUIRE((ground_analysis.effects & EffectGroundTime) != 0);

    CompiledExpression stateful("(rate-as 2 (integrate 1))");
    ProgramAnalysis state_analysis =
        analyze_program(stateful.engine.pool, stateful.root());
    REQUIRE(state_analysis.guaranteed_to_terminate());
    REQUIRE_FALSE(state_analysis.stateless());
    REQUIRE_FALSE(state_analysis.schedule_independent());
    REQUIRE((state_analysis.effects & EffectState) != 0);
    REQUIRE((state_analysis.effects & EffectDeltaTime) != 0);

    NodePool malformed;
    malformed.reset();
    malformed.node_count = 1;
    malformed.nodes[0].op = NodeOp::Neg;
    malformed.nodes[0].input_a = 0;
    REQUIRE_FALSE(analyze_program(malformed, 0).guaranteed_to_terminate());
}

TEST_CASE("Independent reference evaluator agrees with the optimized executor",
          "[program-semantics][reference][differential]") {
    const char* expressions[] = {
        "(+ 3 (* 2 t))",
        "(sin (+ (* 0.75 t) 0.125))",
        "(if (> (frac t) 0.5) (tri t) (sqr t))",
        "(interp [0 1 0 -1] (frac t))",
        "(premap (+ (* 2 t) 0.25) (scale -1 1 (sin beat)))",
        "(+ ground-time (* 10 dt))",
    };
    const Sample times[] = {-1.25, -0.1, 0.0, 0.125, 0.75, 2.5, 17.0};

    for (const char* expression : expressions) {
        CompiledExpression compiled(expression);
        for (Sample time : times) {
            Sample ground = 100.0 + time;
            Sample dt = 0.0075;
            ReferenceEvalResult reference =
                compiled.reference(time, ground, dt);
            INFO("expression: " << expression << ", time: " << time);
            REQUIRE(reference.status == ReferenceEvalStatus::Complete);
            REQUIRE(reference.value ==
                    Approx(compiled.execute(time, ground, dt)).margin(1e-12));
        }
    }
}

TEST_CASE("ProgramView owns a stable snapshot of mutable engine inputs",
          "[program-semantics][snapshot]") {
    SignalEngine engine;
    engine.init_defaults();
    const char* define = "(define k 2)";
    REQUIRE(eval_cold(define, static_cast<uint32_t>(std::strlen(define)),
                      engine).kind != EvalResult::Error);
    const char* assign =
        "(a1 (+ k (live-edit 1 :id \"snapshot-x\" :min 0 :max 10)))";
    REQUIRE(eval_cold(assign, static_cast<uint32_t>(std::strlen(assign)),
                      engine).kind != EvalResult::Error);

    ProgramView frozen = make_program_view(
        engine.pool, engine.cells, engine.pool.outputs[0].root_node);
    ReferenceEnvironment environment;
    ReferenceEvalResult before = evaluate_reference(frozen, environment);
    REQUIRE(before.status == ReferenceEvalStatus::Complete);
    REQUIRE(before.value == Approx(3.0));

    const char* redefine = "(define k 9)";
    REQUIRE(eval_cold(redefine,
                      static_cast<uint32_t>(std::strlen(redefine)),
                      engine).kind != EvalResult::Error);
    engine.pool.set_live_slot_value("snapshot-x", 7.0);

    ReferenceEvalResult after = evaluate_reference(frozen, environment);
    REQUIRE(after.status == ReferenceEvalStatus::Complete);
    REQUIRE(after.value == Approx(3.0));
}

TEST_CASE("Pure ModuLisp denotations obey function-profunctor laws",
          "[program-semantics][profunctor][laws]") {
    CompiledExpression expression("(+ (sin (* 0.5 t)) (* 0.1 t))");
    auto arrow = [&](Sample time) {
        ReferenceEvalResult result = expression.reference(time);
        REQUIRE(result.status == ReferenceEvalStatus::Complete);
        return result.value;
    };
    const auto identity = [](Sample value) { return value; };
    const auto pre = [](Sample value) { return value * 2.0 + 0.25; };
    const auto pre_prime = [](Sample value) { return value - 0.75; };
    const auto post = [](Sample value) { return value * 3.0; };
    const auto post_prime = [](Sample value) { return value + 4.0; };

    for (Sample input : {-2.0, -0.125, 0.0, 0.5, 3.0}) {
        REQUIRE(dimap(identity, identity, arrow, input) ==
                Approx(arrow(input)).margin(1e-12));

        auto inner = [&](Sample value) {
            return dimap(pre_prime, post_prime, arrow, value);
        };
        Sample sequential = dimap(pre, post, inner, input);
        auto composed_pre = [&](Sample value) {
            return pre_prime(pre(value));
        };
        auto composed_post = [&](Sample value) {
            return post(post_prime(value));
        };
        Sample composed = dimap(composed_pre, composed_post, arrow, input);
        REQUIRE(sequential == Approx(composed).margin(1e-12));
    }
}

TEST_CASE("premap identity and composition agree extensionally",
          "[program-semantics][premap][laws]") {
    constexpr const char* body = "(+ (sin (* 0.5 t)) (* 0.1 t))";
    CompiledExpression base(body);
    CompiledExpression identity(
        "(premap t (+ (sin (* 0.5 t)) (* 0.1 t)))");
    CompiledExpression nested(
        "(premap (+ t 1) (premap (* 2 t) (+ (sin (* 0.5 t)) (* 0.1 t))))");
    CompiledExpression composed(
        "(premap (* 2 (+ t 1)) (+ (sin (* 0.5 t)) (* 0.1 t)))");

    for (Sample time : {-1.0, 0.0, 0.125, 0.75, 4.0}) {
        REQUIRE(identity.reference(time).value ==
                Approx(base.reference(time).value).margin(1e-12));
        REQUIRE(nested.reference(time).value ==
                Approx(composed.reference(time).value).margin(1e-12));
    }
}
