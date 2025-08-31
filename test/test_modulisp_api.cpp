#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"

#include "../uSEQ/src/modulisp/modulisp.h"
#include "../uSEQ/src/modulisp/lisp/value.h"
#include "../uSEQ/src/modulisp/lisp/environment.h"
#include <cmath>

// Test cases for ModuLisp-specific functions

TEST_CASE("Triangle wave function", "[modulisp][api][useq_tri]") {
    ModuLispInterpreter interp;
    Environment env;
    
    // Test basic triangle wave behavior
    std::vector<Value> args1 = {Value(0.5), Value(0.25)};
    Value result1 = interp.useq_tri(args1, env);
    REQUIRE(result1.as_float() == Approx(0.5).epsilon(0.001));
    
    // Test with different duty cycle
    std::vector<Value> args2 = {Value(0.3), Value(0.15)};
    Value result2 = interp.useq_tri(args2, env);
    REQUIRE(result2.as_float() == Approx(0.5).epsilon(0.001));
    
    // Test at phase > duty (should use different formula)
    std::vector<Value> args3 = {Value(0.3), Value(0.6)};
    Value result3 = interp.useq_tri(args3, env);
    // Result should be computed as: duty - ((phase-duty) * (duty/(1-duty)))
    double expected = 0.3 - ((0.6 - 0.3) * (0.3 / (1 - 0.3)));
    REQUIRE(result3.as_float() == Approx(expected / 0.3).epsilon(0.001));
}

TEST_CASE("Decision making function", "[modulisp][api][useq_dm]") {
    ModuLispInterpreter interp;
    Environment env;
    
    // Test decision making: index > 0 returns v2, else v1
    std::vector<Value> args1 = {Value(1), Value(10.0), Value(20.0)};
    Value result1 = interp.useq_dm(args1, env);
    REQUIRE(result1.as_float() == Approx(20.0).epsilon(0.001));
    
    // Test with index <= 0
    std::vector<Value> args2 = {Value(0), Value(10.0), Value(20.0)};
    Value result2 = interp.useq_dm(args2, env);
    REQUIRE(result2.as_float() == Approx(10.0).epsilon(0.001));
    
    // Test with negative index
    std::vector<Value> args3 = {Value(-1), Value(5.0), Value(15.0)};
    Value result3 = interp.useq_dm(args3, env);
    REQUIRE(result3.as_float() == Approx(5.0).epsilon(0.001));
}

TEST_CASE("Phasor offset function", "[modulisp][api][useq_phasor_offset]") {
    ModuLispInterpreter interp;
    Environment env;
    
    // Test basic phase shifting
    std::vector<Value> args1 = {Value(0.25), Value(0.5)};
    Value result1 = interp.useq_phasor_offset(args1, env);
    REQUIRE(result1.as_float() == Approx(0.75).epsilon(0.001));
    
    // Test wraparound (phase > 1.0)
    std::vector<Value> args2 = {Value(0.7), Value(0.6)};
    Value result2 = interp.useq_phasor_offset(args2, env);
    REQUIRE(result2.as_float() == Approx(0.3).epsilon(0.001)); // fmod(1.3, 1.0) = 0.3
    
    // Test with zero offset
    std::vector<Value> args3 = {Value(0.0), Value(0.8)};
    Value result3 = interp.useq_phasor_offset(args3, env);
    REQUIRE(result3.as_float() == Approx(0.8).epsilon(0.001));
}

TEST_CASE("Step function", "[modulisp][api][useq_step]") {
    ModuLispInterpreter interp;
    Environment env;
    
    // Test basic step function (count=4, phasor=0.5)
    std::vector<Value> args1 = {Value(4), Value(0.5)};
    Value result1 = interp.useq_step(args1, env);
    REQUIRE(result1.as_int() == 2); // floor(0.5 * 4) = 2
    
    // Test with offset
    std::vector<Value> args2 = {Value(4), Value(10.0), Value(0.25)};
    Value result2 = interp.useq_step(args2, env);
    REQUIRE(result2.as_int() == 11); // floor(0.25 * 4) + 10 = 1 + 10 = 11
    
    // Test negative count
    std::vector<Value> args3 = {Value(-4), Value(0.5)};
    Value result3 = interp.useq_step(args3, env);
    REQUIRE(result3.as_int() == -7); // count - 1 - val = -4 - 1 - 2 = -7
}

TEST_CASE("Set BPM function", "[modulisp][api][useq_setbpm]") {
    ModuLispInterpreter interp;
    Environment env;
    
    // Test setting BPM
    std::vector<Value> args1 = {Value(120.0)};
    Value result1 = interp.useq_setbpm(args1, env);
    REQUIRE(result1.as_float() == Approx(120.0).epsilon(0.001));
    REQUIRE(interp.get_bpm() == Approx(120.0).epsilon(0.001));
    
    // Test setting BPM with threshold
    std::vector<Value> args2 = {Value(140.0), Value(5.0)};
    Value result2 = interp.useq_setbpm(args2, env);
    REQUIRE(result2.as_float() == Approx(140.0).epsilon(0.001));
}

TEST_CASE("Set time signature function", "[modulisp][api][useq_set_time_sig]") {
    ModuLispInterpreter interp;
    Environment env;
    
    // Test setting time signature
    std::vector<Value> args1 = {Value(3.0), Value(4.0)};
    Value result1 = interp.useq_set_time_sig(args1, env);
    REQUIRE(result1.is_nil());
    REQUIRE(interp.get_meter_numerator() == Approx(3.0).epsilon(0.001));
    REQUIRE(interp.get_meter_denominator() == Approx(4.0).epsilon(0.001));
}

TEST_CASE("Set time offset function", "[modulisp][api][useq_set_time_offset]") {
    ModuLispInterpreter interp;
    Environment env;
    
    // Test setting time offset
    std::vector<Value> args1 = {Value(0.5)};
    Value result1 = interp.useq_set_time_offset(args1, env);
    REQUIRE(result1.as_float() == Approx(0.5).epsilon(0.001));
    REQUIRE(interp.get_time_manager()->get_transport_offset() == Approx(0.5).epsilon(0.001));
}

TEST_CASE("Nudge time function", "[modulisp][api][useq_nudge_time]") {
    ModuLispInterpreter interp;
    Environment env;
    
    // Set initial offset
    interp.get_time_manager()->set_transport_offset(0.2);
    
    // Test nudging time (adding to existing offset)
    std::vector<Value> args1 = {Value(0.1)};
    Value result1 = interp.useq_nudge_time(args1, env);
    REQUIRE(result1.as_float() == Approx(0.1).epsilon(0.001));
    REQUIRE(interp.get_time_manager()->get_transport_offset() == Approx(0.3).epsilon(0.001));
}

// NOTE: Test for useq_flatten is commented out as it requires more complex setup
// with proper evaluation environment
// TEST_CASE(test_useq_flatten) {
//     ModuLispInterpreter interp;
//     Environment env;
//     
//     // Test simple flatten with a single level list
//     std::vector<Value> simple_list = {Value(1), Value(2), Value(3)};
//     std::vector<Value> args1 = {Value(simple_list)};
//     
//     Value result1 = interp.useq_flatten(args1, env);
//     // Just test that flatten returns some result (could be list or vector)
//     ASSERT_FALSE(result1.is_nil());
//     ASSERT_FALSE(result1.is_error());
// }

TEST_CASE("Euclidean rhythm function", "[modulisp][api][useq_euclidean]") {
    ModuLispInterpreter interp;
    Environment env;
    
    // Test Euclidean rhythm: 3 hits in 8 steps at phase 0.0
    std::vector<Value> args1 = {Value(8), Value(3), Value(0), Value(0.5), Value(0.0)};
    Value result1 = interp.useq_euclidean(args1, env);
    // At phase 0.0, i=0, idx = ((0 + 8 - 0) * 3) % 8 = 0, and 0 < 3, so should be 1
    REQUIRE(result1.as_int() == 1);
    
    // Test at different phase
    std::vector<Value> args2 = {Value(8), Value(3), Value(0), Value(0.5), Value(0.5)};
    Value result2 = interp.useq_euclidean(args2, env);
    // At phase 0.5, i=4, idx = ((4 + 8 - 0) * 3) % 8 = 4, and 4 >= 3, so should be 0
    REQUIRE(result2.as_int() == 0);
}

TEST_CASE("Random function", "[modulisp][api][useq_random]") {
    ModuLispInterpreter interp;
    Environment env;
    
    // Note: Beat number is now managed internally by PhasorManager
    // The random function will use the current beat from the phasor state
    
    // Test random with no scaling (should be 0-1)
    std::vector<Value> args1 = {};
    Value result1 = interp.useq_random(args1, env);
    REQUIRE(result1.is_number());
    REQUIRE(result1.as_float() >= 0.0);
    REQUIRE(result1.as_float() <= 1.0);
}

TEST_CASE("Index-based random function", "[modulisp][api][useq_index_rand]") {
    ModuLispInterpreter interp;
    Environment env;
    
    // Test index-based random with index only
    std::vector<Value> args1 = {Value(100)};
    Value result1 = interp.useq_index_rand(args1, env);
    REQUIRE(result1.as_float() >= 0.0);
    REQUIRE(result1.as_float() <= 1.0);
    
    // Same index should give same result
    std::vector<Value> args2 = {Value(100)};
    Value result2 = interp.useq_index_rand(args2, env);
    REQUIRE(result2.as_float() == Approx(result1.as_float()).epsilon(0.001));
    
    // Different index should give different result
    std::vector<Value> args3 = {Value(101)};
    Value result3 = interp.useq_index_rand(args3, env);
    REQUIRE(std::abs(result1.as_float() - result3.as_float()) > 0.001);
}

// NOTE: Gates function commented out as it requires evaluation context for the pattern
// TEST_CASE(test_useq_gates) {
//     ModuLispInterpreter interp;
//     Environment env;
//     
//     // Create a simple gate pattern
//     std::vector<Value> pattern = {Value(1), Value(0), Value(1), Value(0)};
//     
//     // Test basic gates functionality - just verify it returns a numeric result
//     std::vector<Value> args1 = {Value(pattern), Value(0.0)};
//     Value result1 = interp.useq_gates(args1, env);
//     ASSERT_TRUE(result1.is_number());
// }

// NOTE: Interpolate function commented out as it requires proper list evaluation
// TEST_CASE(test_useq_interpolate) {
//     ModuLispInterpreter interp;
//     Environment env;
//     
//     // Create a list for interpolation
//     std::vector<Value> values = {Value(0.0), Value(10.0), Value(20.0)};
//     
//     // Test basic interpolation functionality - just verify it returns a number
//     std::vector<Value> args1 = {Value(values), Value(0.5)};
//     Value result1 = interp.useq_interpolate(args1, env);
//     ASSERT_TRUE(result1.is_number());
// }

// NOTE: Ratio functions commented out as they require more careful setup
// TEST_CASE(test_useq_ratiotrig) {
//     ModuLispInterpreter interp;
//     Environment env;
//     
//     // Create ratio pattern
//     std::vector<Value> ratios = {Value(1.0), Value(2.0), Value(1.0)};
//     
//     // Test basic functionality - just verify it returns a number
//     std::vector<Value> args1 = {Value(ratios), Value(0.1), Value(0.0)};
//     Value result1 = interp.useq_ratiotrig(args1, env);
//     ASSERT_TRUE(result1.is_number());
// }

// TEST_CASE(test_useq_ratiostep) {
//     ModuLispInterpreter interp;
//     Environment env;
//     
//     // Create ratio pattern
//     std::vector<Value> ratios = {Value(1.0), Value(2.0), Value(1.0)};
//     
//     // Test basic functionality - just verify it returns a number
//     std::vector<Value> args1 = {Value(ratios), Value(0.0)};
//     Value result1 = interp.useq_ratiostep(args1, env);
//     ASSERT_TRUE(result1.is_number());
// }

TEST_CASE("Scheduling functions", "[modulisp][api][scheduling]") {
    ModuLispInterpreter interp;
    Environment env;
    
    // Test scheduling an item
    std::vector<Value> schedule_args = {Value::string("test_item"), Value(1.0), Value(42)};
    Value schedule_result = interp.useq_schedule(schedule_args, env);
    REQUIRE(schedule_result.is_nil());
    REQUIRE(interp.get_scheduler()->get_scheduled_items().size() == 1);
    REQUIRE(interp.get_scheduler()->get_scheduled_items()[0].id == "test_item");
    
    // Test unscheduling the item
    std::vector<Value> unschedule_args = {Value::string("test_item")};
    Value unschedule_result = interp.useq_unschedule(unschedule_args, env);
    REQUIRE(unschedule_result.is_nil());
    REQUIRE(interp.get_scheduler()->get_scheduled_items().size() == 0);
}

