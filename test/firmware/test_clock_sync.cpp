// Tests for the external clock sync module: tempo estimation, cold-eval
// command handling, and the ClockSync state machine.
//
// Hardware-side ISR / mailbox plumbing is not exercised here (would need
// hardware or a pulse-injection seam). The TempoEstimator algorithm and the
// cold-eval surface are the meat of the work; both can be tested in isolation.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "../../uSEQ/src/firmware/clock_sync.h"
#include "../../uSEQ/src/signal_engine/cold_eval.h"
#include "../../uSEQ/src/signal_engine/graph_builder.h"

using firmware::TempoEstimator;

// ── TempoEstimator ──────────────────────────────────────────────────────────

TEST_CASE("TempoEstimator: first pulse returns 0 (no prior interval)", "[clock_sync]") {
    TempoEstimator est;
    REQUIRE(est.process(1000) == 0.0);
}

TEST_CASE("TempoEstimator: warms up after WINDOW intervals at 120 BPM", "[clock_sync]") {
    TempoEstimator est;
    // 120 BPM pulses at "1 pulse per beat" = 500ms = 500_000 us.
    constexpr uint32_t period_us = 500000;
    uint32_t t = 0;

    REQUIRE(est.process(t) == 0.0);                     // first pulse: prime
    for (size_t i = 0; i < TempoEstimator::WINDOW; i++) {
        t += period_us;
        double bpm = est.process(t);
        if (i + 1 < TempoEstimator::WINDOW) {
            // Filter still warming up — value may be valid, may be 0; we
            // only require it's non-negative.
            REQUIRE(bpm >= 0.0);
        } else {
            // Fully warm — expect 120 BPM.
            REQUIRE(bpm == Approx(120.0));
        }
    }
}

TEST_CASE("TempoEstimator: tracks tempo at 60 BPM and 240 BPM", "[clock_sync]") {
    auto bpm_after_warmup = [](uint32_t period_us) {
        TempoEstimator est;
        uint32_t t = 0;
        est.process(t);
        for (size_t i = 0; i < TempoEstimator::WINDOW; i++) {
            t += period_us;
        }
        // Replay (last call returns the bpm)
        TempoEstimator est2;
        uint32_t t2 = 0;
        est2.process(t2);
        double bpm = 0.0;
        for (size_t i = 0; i < TempoEstimator::WINDOW; i++) {
            t2 += period_us;
            bpm = est2.process(t2);
        }
        return bpm;
    };
    REQUIRE(bpm_after_warmup(1000000) == Approx(60.0));   // 1s period
    REQUIRE(bpm_after_warmup(250000)  == Approx(240.0));  // 250ms period
}

TEST_CASE("TempoEstimator: rejects glitches < 1ms and gaps > 5s", "[clock_sync]") {
    TempoEstimator est;
    est.process(0);
    REQUIRE(est.process(500) == 0.0);          // 500us — sub-ms, rejected
    REQUIRE(est.process(10000000) == 0.0);     // 10s gap, rejected
    // After rejection, valid_count is unchanged, so we still need WINDOW
    // intervals to warm up. (This documents the guard — gaps don't poison
    // the filter, they're simply ignored.)
    REQUIRE(est.valid_count == 0);
}

TEST_CASE("TempoEstimator: reset clears state", "[clock_sync]") {
    TempoEstimator est;
    est.process(0);
    est.process(500000);
    est.process(1000000);
    REQUIRE(est.valid_count == 2);

    est.reset();
    REQUIRE(est.valid_count == 0);
    REQUIRE(est.has_prior == false);
    // First pulse after reset should once again return 0 (priming).
    REQUIRE(est.process(2000000) == 0.0);
}

TEST_CASE("TempoEstimator: averages over window for jittery input", "[clock_sync]") {
    // Alternating 480ms / 520ms intervals — true mean is 500ms = 120 BPM.
    TempoEstimator est;
    uint32_t t = 0;
    est.process(t);
    double bpm = 0.0;
    for (size_t i = 0; i < TempoEstimator::WINDOW; i++) {
        t += (i & 1) ? 480000 : 520000;
        bpm = est.process(t);
    }
    REQUIRE(bpm == Approx(120.0).margin(2.0));
}

// ── Cold-eval surface ───────────────────────────────────────────────────────

TEST_CASE("set-clock-int sets clock_source to 0", "[clock_sync][cold_eval]") {
    sig::SignalEngine engine;
    engine.init_defaults();
    engine.state.clock_source = 1;

    const char src[] = "(set-clock-int)";
    auto r = sig::eval_cold(src, sizeof(src) - 1, engine);
    REQUIRE(r.kind != sig::EvalResult::Error);
    REQUIRE(engine.state.clock_source == 0);
}

TEST_CASE("set-clock-ext sets source and divisor", "[clock_sync][cold_eval]") {
    sig::SignalEngine engine;
    engine.init_defaults();

    const char src[] = "(set-clock-ext 1 4)";
    auto r = sig::eval_cold(src, sizeof(src) - 1, engine);
    REQUIRE(r.kind != sig::EvalResult::Error);
    REQUIRE(engine.state.clock_source == 1);
    REQUIRE(engine.state.ext_clock_divisor == 4);
    REQUIRE(engine.state.ext_clock_reset == true);
}

TEST_CASE("set-clock-ext defaults divisor to 1 when omitted", "[clock_sync][cold_eval]") {
    sig::SignalEngine engine;
    engine.init_defaults();

    const char src[] = "(set-clock-ext 2)";
    auto r = sig::eval_cold(src, sizeof(src) - 1, engine);
    REQUIRE(r.kind != sig::EvalResult::Error);
    REQUIRE(engine.state.clock_source == 2);
    REQUIRE(engine.state.ext_clock_divisor == 1);
}

TEST_CASE("set-clock-ext rejects invalid input number", "[clock_sync][cold_eval]") {
    sig::SignalEngine engine;
    engine.init_defaults();

    const char src[] = "(set-clock-ext 5 1)";
    auto r = sig::eval_cold(src, sizeof(src) - 1, engine);
    REQUIRE(r.kind == sig::EvalResult::Error);
    REQUIRE(engine.state.clock_source == 0);  // unchanged
}

TEST_CASE("set-clock-ext rejects invalid divisors and trailing arguments",
          "[clock_sync][cold_eval]") {
    sig::SignalEngine engine;
    engine.init_defaults();

    const char* invalid[] = {
        "(set-clock-ext 1 0)",
        "(set-clock-ext 1 256)",
        "(set-clock-ext 1 1.5)",
        "(set-clock-ext 1 4 8)",
    };
    for (const char* src : invalid) {
        auto r = sig::eval_cold(src, (uint32_t)strlen(src), engine);
        REQUIRE(r.kind == sig::EvalResult::Error);
        REQUIRE(engine.state.clock_source == 0);
    }
}

TEST_CASE("zero-argument clock commands reject extra input",
          "[clock_sync][cold_eval]") {
    sig::SignalEngine engine;
    engine.init_defaults();

    const char* invalid[] = {
        "(set-clock-int 1)",
        "(get-clock-source 1)",
        "(reset-clock-ext 1)",
        "(reset-clock-int 1)",
    };
    for (const char* src : invalid) {
        auto r = sig::eval_cold(src, (uint32_t)strlen(src), engine);
        REQUIRE(r.kind == sig::EvalResult::Error);
    }
}

TEST_CASE("get-clock-source returns the current source as a number", "[clock_sync][cold_eval]") {
    sig::SignalEngine engine;
    engine.init_defaults();
    engine.state.clock_source = 2;

    const char src[] = "(get-clock-source)";
    auto r = sig::eval_cold(src, sizeof(src) - 1, engine);
    REQUIRE(r.kind == sig::EvalResult::Number);
    REQUIRE(r.number == 2.0);
}

TEST_CASE("reset-clock-ext sets the reset flag", "[clock_sync][cold_eval]") {
    sig::SignalEngine engine;
    engine.init_defaults();
    engine.state.ext_clock_reset = false;

    const char src[] = "(reset-clock-ext)";
    auto r = sig::eval_cold(src, sizeof(src) - 1, engine);
    REQUIRE(r.kind != sig::EvalResult::Error);
    REQUIRE(engine.state.ext_clock_reset == true);
}

TEST_CASE("reset-clock-int rewinds logical time without changing user offset",
          "[clock_sync][cold_eval]") {
    sig::SignalEngine engine;
    engine.init_defaults();
    engine.state.current_wall_time = 10.0;
    engine.state.time_offset = 12.5;
    engine.state.transport_offset = 3.0;

    const char src[] = "(reset-clock-int)";
    auto r = sig::eval_cold(src, sizeof(src) - 1, engine);
    REQUIRE(r.kind != sig::EvalResult::Error);
    REQUIRE(engine.state.time_offset == Approx(12.5));
    REQUIRE(engine.state.logical_time(10.0) == Approx(0.0));
}

// ── ClockSync state transitions ─────────────────────────────────────────────

TEST_CASE("ClockSync::tick is a no-op when source is internal", "[clock_sync]") {
    sig::SignalEngine engine;
    engine.init_defaults();
    sig::GraphBuilder::init_symbols();

    firmware::ClockSync cs;
    cs.tick(engine, 1.0);   // no pulses, internal mode → nothing happens
    REQUIRE(cs.active_source == 0);
}

TEST_CASE("ClockSync::tick reconfigures when EngineState changes source", "[clock_sync]") {
    sig::SignalEngine engine;
    engine.init_defaults();
    sig::GraphBuilder::init_symbols();

    firmware::ClockSync cs;
    engine.state.clock_source = 1;
    engine.state.ext_clock_divisor = 4;
    cs.tick(engine, 1.0);

    REQUIRE(cs.active_source == 1);
    REQUIRE(cs.divisor == 4);
}

TEST_CASE("ClockSync::tick honours the one-shot reset flag", "[clock_sync]") {
    sig::SignalEngine engine;
    engine.init_defaults();
    sig::GraphBuilder::init_symbols();

    firmware::ClockSync cs;
    cs.tracker.beat_in_bar = 3;
    engine.state.ext_clock_reset = true;
    cs.tick(engine, 0.0);

    REQUIRE(engine.state.ext_clock_reset == false);
    REQUIRE(cs.tracker.beat_in_bar == 0);
}

TEST_CASE("ClockSync pulse publication refreshes the cached cell generation",
          "[clock_sync]") {
    sig::SignalEngine engine;
    engine.init_defaults();
    firmware::ClockSync cs;
    cs.active_source = 1;
    cs.divisor = 1;

    uint32_t pulse_us = 0;
    cs.process_pulse(engine, pulse_us, 0.0);
    const uint32_t revision_before = engine.cells.store_revision;
    for (size_t i = 0; i < firmware::TempoEstimator::WINDOW; i++) {
        pulse_us += 500000;
        cs.process_pulse(engine, pulse_us, pulse_us / 1000000.0);
    }

    auto& si = SymbolIntern::getInstance();
    sig::SymbolID bpm_sym = si.intern("bpm");
    REQUIRE(engine.cells.cells[bpm_sym].value == Approx(120.0));
    REQUIRE(engine.cells.store_revision > revision_before);
}

TEST_CASE("ClockSync BPM changes recompile expressions that read bpm",
          "[clock_sync][bpm]") {
    sig::SignalEngine engine;
    engine.init_defaults(60.0);
    firmware::ClockSync cs;
    cs.active_source = 1;
    cs.divisor = 1;
    constexpr char expression[] = "(a2 (/ bpm 240))";
    REQUIRE(sig::eval_cold(expression, sizeof(expression) - 1, engine).kind ==
            sig::EvalResult::Ok);
    const uint16_t root = engine.pool.outputs[1].root_node;
    REQUIRE(engine.pool.nodes[root].op == sig::NodeOp::Const);
    REQUIRE(engine.pool.nodes[root].imm == Approx(0.25));

    uint32_t pulse_us = 0;
    cs.process_pulse(engine, pulse_us, 0.0);
    for (size_t i = 0; i < firmware::TempoEstimator::WINDOW; i++) {
        pulse_us += 500000;
        cs.process_pulse(engine, pulse_us, pulse_us / 1000000.0);
    }

    const uint16_t updated_root = engine.pool.outputs[1].root_node;
    REQUIRE(engine.pool.nodes[updated_root].op == sig::NodeOp::Const);
    REQUIRE(engine.pool.nodes[updated_root].imm == Approx(0.5));
}

TEST_CASE("ClockSync phase lock changes only the internal transport origin",
          "[clock_sync]") {
    sig::SignalEngine engine;
    engine.init_defaults();
    engine.state.time_offset = 0.25;

    firmware::ClockSync cs;
    cs.active_source = 1;
    cs.divisor = 1;

    uint32_t pulse_us = 0;
    cs.process_pulse(engine, pulse_us, 0.0);
    for (size_t i = 1; i <= 8; i++) {
        pulse_us += 500000;
        const double now = i == 8 ? 4.1 : pulse_us / 1000000.0;
        cs.process_pulse(engine, pulse_us, now);
    }

    REQUIRE(engine.state.time_offset == Approx(0.25));
    REQUIRE(engine.state.transport_offset == Approx(-0.1));
}
