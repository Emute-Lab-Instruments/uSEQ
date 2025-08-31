#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../uSEQ/src/modulisp/modulisp_refactored.h"
#include "../uSEQ/src/ports/mocks/MockClock.h"
#include "../uSEQ/src/ports/mocks/MockLogger.h"

// Mock random generator for deterministic testing
class MockRandomGenerator : public IRandomGenerator {
public:
    double generate() override { return fixed_value; }
    double generate_with_index(uint32_t index) override { return 0.5; }
    void set_seed(uint32_t seed) override { /* no-op for mock */ }
    
    double fixed_value = 0.42;
};

TEST_CASE("Refactored ModuLispInterpreter with injected dependencies", "[refactored]") {
    
    SECTION("Time management with injected clock") {
        // Arrange
        MockClock mock_clock;
        mock_clock.set_micros(1500000ULL);  // 1.5 seconds
        
        ModuLispInterpreter interp(&mock_clock, nullptr, nullptr);
        
        // Act
        interp.update();
        
        // Assert
        auto time_var = interp.get("time");
        auto transport_var = interp.get("t");
        
        REQUIRE(time_var.has_value());
        REQUIRE(transport_var.has_value());
        REQUIRE(time_var->as_float() == Approx(1.5).epsilon(1e-6));
        REQUIRE(transport_var->as_float() == Approx(1.5).epsilon(1e-6));
    }
    
    SECTION("Phasor calculations are deterministic") {
        // Arrange
        MockClock mock_clock;
        mock_clock.set_micros(500000ULL);  // 0.5 seconds
        
        ModuLispInterpreter interp(&mock_clock, nullptr, nullptr);
        interp.set_bpm(120.0, 0.0);  // 120 BPM = 0.5 seconds per beat
        
        // Act
        interp.update();
        
        // Assert - at 0.5 seconds with 120 BPM, we should be at beat 1.0
        auto beat_var = interp.get("beat");
        auto beat_num_var = interp.get("beatNum");
        
        REQUIRE(beat_var.has_value());
        REQUIRE(beat_num_var.has_value());
        REQUIRE(beat_var->as_float() == Approx(0.0).margin(0.01));  // Just wrapped
        REQUIRE(beat_num_var->as_int() == 1);
    }
    
    SECTION("Random number generation is mockable") {
        // Arrange
        auto* mock_rng = new MockRandomGenerator();
        mock_rng->fixed_value = 0.777;
        
        ModuLispInterpreter interp(nullptr, nullptr, mock_rng);
        
        // Act - call useq_random function
        std::vector<Value> args;
        Environment env(interp);
        Value result = interp.useq_random(args, env);
        
        // Assert
        REQUIRE(result.is_float());
        REQUIRE(result.as_float() == Approx(0.777));
    }
    
    SECTION("Scheduling is isolated and testable") {
        // Arrange
        MockClock mock_clock;
        ModuLispInterpreter interp(&mock_clock, nullptr, nullptr);
        
        // Schedule an item
        Value test_ast = interp.parse("(+ 1 2)");
        interp.schedule("test_item", test_ast, 1000000);  // Run every second
        
        // Act - advance time and update
        mock_clock.set_micros(1500000ULL);  // 1.5 seconds
        interp.update();
        
        // Assert - scheduled item should have run
        auto* scheduler = interp.get_scheduler();
        REQUIRE(scheduler != nullptr);
        REQUIRE(scheduler->get_scheduled_items().size() == 1);
        REQUIRE(scheduler->get_scheduled_items()[0].id == "test_item");
    }
    
    SECTION("Time and phasor managers can be tested independently") {
        // Test TimeManager in isolation
        MockClock mock_clock;
        mock_clock.set_micros(2000000ULL);
        
        TimeManager time_mgr(&mock_clock);
        time_mgr.update();
        
        REQUIRE(time_mgr.get_time_seconds() == Approx(2.0));
        
        // Test PhasorManager in isolation
        PhasorManager phasor_mgr;
        phasor_mgr.set_bpm(60.0, 0.0);  // 60 BPM = 1 beat per second
        
        TimeValue one_second = 1000000.0;
        REQUIRE(phasor_mgr.beat_at_time(one_second) == Approx(0.0).margin(0.01));
        REQUIRE(phasor_mgr.beat_num_at_time(one_second) == 1);
    }
    
    SECTION("Logger injection for testing error handling") {
        // Arrange
        MockLogger mock_logger;
        MockClock mock_clock;
        
        ModuLispInterpreter interp(&mock_clock, &mock_logger, nullptr);
        
        // Schedule an item with invalid AST
        Value invalid_ast;  // Invalid/empty value
        interp.schedule("bad_item", invalid_ast, 1000000);
        
        // Act - trigger scheduled items
        mock_clock.set_micros(1500000ULL);
        interp.run_scheduled_items();
        
        // Assert - logger should have recorded an error
        // (MockLogger implementation would track calls)
        REQUIRE(mock_logger.error_count > 0);
    }
}

TEST_CASE("Refactored components follow Single Responsibility Principle", "[architecture]") {
    
    SECTION("TimeManager only handles time") {
        MockClock clock;
        TimeManager mgr(&clock);
        
        // TimeManager should only expose time-related methods
        clock.set_micros(5000000ULL);
        mgr.update();
        
        REQUIRE(mgr.get_time_seconds() == Approx(5.0));
        REQUIRE(mgr.get_transport_seconds() == Approx(5.0));
        
        // Transport reset
        mgr.reset_transport();
        clock.set_micros(6000000ULL);
        mgr.update();
        
        REQUIRE(mgr.get_time_seconds() == Approx(6.0));
        REQUIRE(mgr.get_transport_seconds() == Approx(1.0));  // Reset at 5s
    }
    
    SECTION("PhasorManager only handles phasors and tempo") {
        PhasorManager mgr;
        
        // Test BPM changes
        mgr.set_bpm(100.0, 0.0);
        REQUIRE(mgr.get_tempo().bpm == 100.0);
        
        // Test time signature changes
        mgr.set_time_signature(3, 4);
        REQUIRE(mgr.get_meter().numerator == 3);
        REQUIRE(mgr.get_meter().denominator == 4);
        
        // Test phasor calculations
        TimeValue half_second = 500000.0;
        mgr.set_bpm(120.0, 0.0);  // 0.5s per beat
        REQUIRE(mgr.beat_at_time(half_second) == Approx(0.0).margin(0.01));
    }
    
    SECTION("Scheduler only handles scheduling") {
        Scheduler scheduler;
        Value ast = Value(42);
        
        // Schedule and unschedule
        scheduler.schedule("task1", ast, 1000);
        REQUIRE(scheduler.get_scheduled_items().size() == 1);
        
        scheduler.unschedule("task1");
        REQUIRE(scheduler.get_scheduled_items().size() == 0);
        
        // Run queue management
        scheduler.add_to_run_queue(Value(1));
        scheduler.add_to_run_queue(Value(2));
        REQUIRE(scheduler.get_run_queue().size() == 2);
        
        scheduler.clear_run_queue();
        REQUIRE(scheduler.get_run_queue().size() == 0);
    }
}