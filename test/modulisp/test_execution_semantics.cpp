/**
 * @file test_execution_semantics.cpp
 * @brief Characterization tests for uSEQ::check_and_handle_user_input() execution semantics
 *
 * These tests document the CURRENT behavior of the execution system before refactoring.
 * They capture how code execution works in different scenarios:
 * - Immediate execution (@ marker)
 * - Scheduled execution (default)
 * - Serial stream value updates
 * - Error handling
 *
 * IMPORTANT: These are CHARACTERIZATION tests - they lock down existing behavior,
 * even if that behavior has quirks or imperfections. The goal is to ensure the
 * refactoring preserves exact current functionality.
 *
 * Reference: src-useq/uSEQ/src/uSEQ.cpp lines 662-784
 */

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../../uSEQ/src/modulisp/modulisp_interpreter.h"
#include "../../uSEQ/src/modulisp/scheduler.h"
#include "../../uSEQ/src/utils/log.h"
#include <cmath>

// Ensure builtins are initialized once for all tests in this binary
namespace
{
struct BuiltinsInitOnce
{
    BuiltinsInitOnce() { ModuLispInterpreter::init_builtin_functions(); }
} _builtinsInitOnce;
} // namespace

////////////////////////////////////////////////////////////////////////////////
/// CHARACTERIZATION: Current Execution Semantics
////////////////////////////////////////////////////////////////////////////////

/**
 * CURRENT BEHAVIOR NOTES (from uSEQ.cpp:662-784):
 *
 * 1. Immediate Execution (@ marker):
 *    - set_manual_evaluation(true)
 *    - error_msg_q.clear()
 *    - result = eval(code)
 *    - check error_msg_q for errors
 *    - print result (and errors if any)
 *    - set_manual_evaluation(false)
 *
 * 2. Scheduled Execution (default):
 *    - set_manual_evaluation(true)
 *    - prepend first_byte to code (it's part of the expression)
 *    - echo the code
 *    - parse code -> Value
 *    - add to scheduler run queue
 *    - set_manual_evaluation(false)
 *
 * 3. Serial Stream Updates:
 *    - read channel (1-indexed)
 *    - read 8 bytes as double
 *    - update m_serial_input_streams[channel-1]
 *    - silently ignore invalid channels
 *
 * 4. Error Handling:
 *    - errors stored in global error_msg_q
 *    - cleared before eval
 *    - checked after eval
 *    - printed if non-empty
 *
 * 5. manual_evaluation flag:
 *    - set to true at start
 *    - set to false at end
 *    - used to distinguish user input from automated eval
 */

////////////////////////////////////////////////////////////////////////////////
/// TEST CASE 1: Immediate Execution with @ Marker
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("CHARACTERIZATION: execute immediate code with @ marker",
          "[execution][semantics][immediate]")
{
    // Setup interpreter
    ModuLispInterpreter interp(nullptr);

    SECTION("Basic arithmetic evaluation")
    {
        // CHARACTERIZATION: Simulates the immediate execution path
        // Current behavior (lines 734-756):
        //   - set_manual_evaluation(true)
        //   - error_msg_q.clear()
        //   - result = eval(code)
        //   - check errors
        //   - print result
        //   - set_manual_evaluation(false)

        ModuLispInterpreter::set_manual_evaluation(true);
        error_msg_q.clear();

        String result = interp.eval("(+ 1 2)");

        // EXPECTED: Result is "3", no errors
        REQUIRE(result.indexOf("3") >= 0);
        REQUIRE(error_msg_q.empty());

        ModuLispInterpreter::set_manual_evaluation(false);
        REQUIRE(ModuLispInterpreter::get_manual_evaluation() == false);
    }

    SECTION("Variable definition and access")
    {
        ModuLispInterpreter::set_manual_evaluation(true);
        error_msg_q.clear();

        // Define variable
        String result1 = interp.eval("(define x 42)");

        // EXPECTED: Returns the symbol name
        REQUIRE(result1.indexOf("x") >= 0);
        REQUIRE(error_msg_q.empty());

        // Access variable
        String result2 = interp.eval("x");
        REQUIRE(result2.indexOf("42") >= 0);
        REQUIRE(error_msg_q.empty());

        ModuLispInterpreter::set_manual_evaluation(false);
    }

    SECTION("Function call evaluation")
    {
        ModuLispInterpreter::set_manual_evaluation(true);
        error_msg_q.clear();

        String result = interp.eval("(* 6 7)");

        REQUIRE(result.indexOf("42") >= 0);
        REQUIRE(error_msg_q.empty());

        ModuLispInterpreter::set_manual_evaluation(false);
    }

    SECTION("String return values")
    {
        ModuLispInterpreter::set_manual_evaluation(true);
        error_msg_q.clear();

        String result = interp.eval("\"hello world\"");

        REQUIRE(result.indexOf("hello world") >= 0);
        REQUIRE(error_msg_q.empty());

        ModuLispInterpreter::set_manual_evaluation(false);
    }
}

////////////////////////////////////////////////////////////////////////////////
/// TEST CASE 2: Scheduled Code Execution
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("CHARACTERIZATION: schedule code for later execution",
          "[execution][semantics][scheduled]")
{
    // Setup interpreter
    ModuLispInterpreter interp(nullptr);

    SECTION("Schedule simple expression")
    {
        // CHARACTERIZATION: Simulates the scheduled execution path
        // Current behavior (lines 758-771):
        //   - set_manual_evaluation(true)
        //   - prepend first_byte to code (if not @ marker)
        //   - echo the code (println)
        //   - parse -> Value
        //   - add to scheduler run queue
        //   - set_manual_evaluation(false)

        ModuLispInterpreter::set_manual_evaluation(true);

        // Simulate code that would be scheduled (first byte is part of expression)
        String code = "(+ 1 2)";

        // Parse the code
        Value expr = interp.get_parser()->parse(code);

        // Get initial scheduler state
        size_t initial_queue_size = interp.get_scheduler()->get_run_queue().size();

        // Add to scheduler
        interp.get_scheduler()->add_to_run_queue(expr);

        // EXPECTED: Code added to run queue
        REQUIRE(interp.get_scheduler()->get_run_queue().size() == initial_queue_size + 1);

        ModuLispInterpreter::set_manual_evaluation(false);
    }

    SECTION("Schedule multiple expressions")
    {
        ModuLispInterpreter::set_manual_evaluation(true);

        Value expr1 = interp.get_parser()->parse("(define a 1)");
        Value expr2 = interp.get_parser()->parse("(define b 2)");

        size_t initial_size = interp.get_scheduler()->get_run_queue().size();

        interp.get_scheduler()->add_to_run_queue(expr1);
        interp.get_scheduler()->add_to_run_queue(expr2);

        // EXPECTED: Both expressions added
        REQUIRE(interp.get_scheduler()->get_run_queue().size() == initial_size + 2);

        ModuLispInterpreter::set_manual_evaluation(false);
    }

    SECTION("First byte prepending behavior")
    {
        // CHARACTERIZATION: In actual usage, first_byte is prepended to code
        // This simulates lines 761-762:
        //   m_last_received_code = String((char)first_byte) + m_last_received_code;

        ModuLispInterpreter::set_manual_evaluation(true);

        // Simulate first_byte = '('
        char first_byte   = '(';
        String code_after = "+ 1 2)";
        String full_code  = String(first_byte) + code_after;

        // Should form valid expression
        Value expr = interp.get_parser()->parse(full_code);

        // EXPECTED: Successfully parsed
        REQUIRE(expr.get_type_enum() != 12); // Not NIL
        REQUIRE(expr.get_type_enum() != 14); // Not ERROR

        ModuLispInterpreter::set_manual_evaluation(false);
    }
}

////////////////////////////////////////////////////////////////////////////////
/// TEST CASE 3: Serial Stream Value Updates
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("CHARACTERIZATION: update serial stream value",
          "[execution][semantics][serial]")
{
    // Note: This test is limited because we can't easily instantiate uSEQ
    // with serial streams in the test environment. We document the expected
    // behavior based on lines 698-712.
    //
    // CURRENT BEHAVIOR:
    //   - Read channel number (1-indexed)
    //   - Read 8 bytes as double
    //   - if (channel > 0 && channel <= m_num_serial_ins)
    //       m_serial_input_streams[(channel - 1)] = value;
    //   - Silently ignore out-of-bounds channels

    SECTION("Document expected channel indexing")
    {
        // CHARACTERIZATION: Channels are 1-indexed externally, 0-indexed internally
        // Channel 1 -> index 0
        // Channel 2 -> index 1
        // etc.

        // Example: If we received channel=2, value=0.75
        size_t channel = 2;
        double value   = 0.75;

        // Expected behavior:
        size_t internal_index = channel - 1; // Should be 1
        REQUIRE(internal_index == 1);

        // Value would be stored at m_serial_input_streams[1]
    }

    SECTION("Document boundary conditions")
    {
        // CHARACTERIZATION: Invalid channels are silently ignored

        // Channel 0 should be ignored (< 1)
        size_t invalid_channel_low = 0;
        REQUIRE(invalid_channel_low < 1);

        // Channel beyond bounds (depends on m_num_serial_ins)
        // Let's assume m_num_serial_ins = 4
        size_t num_serial_ins = 4;
        size_t invalid_channel_high = 5;
        REQUIRE(invalid_channel_high > num_serial_ins);

        // Both should be silently ignored (no error thrown)
    }
}

////////////////////////////////////////////////////////////////////////////////
/// TEST CASE 4: Error Handling During Execution
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("CHARACTERIZATION: handle execution errors",
          "[execution][semantics][errors]")
{
    ModuLispInterpreter interp(nullptr);

    SECTION("Undefined function error")
    {
        // CHARACTERIZATION: Errors are stored in error_msg_q
        // Current behavior (lines 738-750):
        //   - error_msg_q.clear() before eval
        //   - eval() populates error_msg_q on error
        //   - check error_msg_q.size() > 0
        //   - print error_msg_q[0]

        ModuLispInterpreter::set_manual_evaluation(true);
        error_msg_q.clear();

        String result = interp.eval("(undefined-func 1 2)");

        // EXPECTED: error_msg_q is populated
        REQUIRE(error_msg_q.size() > 0);

        // EXPECTED: error message mentions the undefined function
        bool has_error_info = false;
        for (const auto& err : error_msg_q) {
            if (err.indexOf("undefined") >= 0 || err.indexOf("not found") >= 0 ||
                err.indexOf("Unknown") >= 0) {
                has_error_info = true;
                break;
            }
        }
        REQUIRE(has_error_info);

        ModuLispInterpreter::set_manual_evaluation(false);
    }

    SECTION("Undefined variable error")
    {
        ModuLispInterpreter::set_manual_evaluation(true);
        error_msg_q.clear();

        String result = interp.eval("nonexistent-var");

        // EXPECTED: error_msg_q has error about undefined variable
        REQUIRE(error_msg_q.size() > 0);

        ModuLispInterpreter::set_manual_evaluation(false);
    }

    SECTION("Syntax error")
    {
        ModuLispInterpreter::set_manual_evaluation(true);
        error_msg_q.clear();

        // Malformed expression (extra closing paren)
        String result = interp.eval("(+ 1 2))");

        // EXPECTED: Some kind of error is reported
        // (exact error handling may vary)
        // At minimum, result shouldn't be "3"
        bool is_error = error_msg_q.size() > 0 || result.indexOf("3") < 0;
        REQUIRE(is_error);

        ModuLispInterpreter::set_manual_evaluation(false);
    }

    SECTION("Type error")
    {
        ModuLispInterpreter::set_manual_evaluation(true);
        error_msg_q.clear();

        // Try to add incompatible types
        String result = interp.eval("(+ 1 \"string\")");

        // EXPECTED: error_msg_q populated
        REQUIRE(error_msg_q.size() > 0);

        ModuLispInterpreter::set_manual_evaluation(false);
    }

    SECTION("Division by zero")
    {
        ModuLispInterpreter::set_manual_evaluation(true);
        error_msg_q.clear();

        // Try to divide by zero
        String result = interp.eval("(/ 1 0)");

        // EXPECTED: May or may not error (depends on implementation)
        // This is more of a documentation test
        // At minimum, we shouldn't crash
        REQUIRE(true);

        ModuLispInterpreter::set_manual_evaluation(false);
    }
}

////////////////////////////////////////////////////////////////////////////////
/// TEST CASE 5: manual_evaluation Flag Behavior
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("CHARACTERIZATION: manual_evaluation flag behavior",
          "[execution][semantics][flags]")
{
    ModuLispInterpreter interp(nullptr);

    SECTION("Flag is toggled during execution")
    {
        // CHARACTERIZATION: Flag is set to true at start, false at end
        // Lines 677, 774

        // Initially should be false (or we set it)
        ModuLispInterpreter::set_manual_evaluation(false);
        REQUIRE(ModuLispInterpreter::get_manual_evaluation() == false);

        // Simulate execution path
        ModuLispInterpreter::set_manual_evaluation(true);
        REQUIRE(ModuLispInterpreter::get_manual_evaluation() == true);

        // ... eval happens here ...
        String result = interp.eval("(+ 1 2)");

        // Reset at end
        ModuLispInterpreter::set_manual_evaluation(false);
        REQUIRE(ModuLispInterpreter::get_manual_evaluation() == false);
    }

    SECTION("Flag distinguishes manual from automated eval")
    {
        // CHARACTERIZATION: This flag is used throughout the system
        // to distinguish user-initiated evaluation from automated
        // evaluation during update loops

        // Manual evaluation (user input)
        ModuLispInterpreter::set_manual_evaluation(true);
        REQUIRE(ModuLispInterpreter::get_manual_evaluation() == true);
        ModuLispInterpreter::set_manual_evaluation(false);

        // Automated evaluation (update loop)
        REQUIRE(ModuLispInterpreter::get_manual_evaluation() == false);
    }
}

////////////////////////////////////////////////////////////////////////////////
/// TEST CASE 6: Transport-Specific Behavior Documentation
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("CHARACTERIZATION: transport-specific output routing",
          "[execution][semantics][transport]")
{
    // Note: These tests document the behavior but can't fully test it
    // without the uSEQ class and transport infrastructure

    SECTION("Document I2C vs Serial routing")
    {
        // CHARACTERIZATION: Output routing depends on bNewI2CMessage flag
        // Lines 744-755:
        //   if (bNewI2CMessage)
        //       i2cPrintStr += result;
        //   else
        //       println(result);

        // This behavior is transport-specific and handled in uSEQ layer
        // The interpreter itself doesn't know about I2C vs Serial

        REQUIRE(true); // Documentation test
    }

    SECTION("Document error output routing")
    {
        // CHARACTERIZATION: Errors are also routed by transport
        // Lines 744-749:
        //   if (error_msg_q.size() > 0)
        //       if (bNewI2CMessage)
        //           i2cPrintStr += error_msg_q[0];
        //       else
        //           println(error_msg_q[0]);

        REQUIRE(true); // Documentation test
    }

    SECTION("Document scheduled code echo behavior")
    {
        // CHARACTERIZATION: Scheduled code is echoed to confirm scheduling
        // Lines 764-767:
        //   if (bNewI2CMessage)
        //       i2cPrintStr += m_last_received_code;
        //   else
        //       println(m_last_received_code);

        REQUIRE(true); // Documentation test
    }
}

////////////////////////////////////////////////////////////////////////////////
/// INTEGRATION TEST: Full execution flow simulation
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("CHARACTERIZATION: full execution flow simulation",
          "[execution][semantics][integration]")
{
    ModuLispInterpreter interp(nullptr);

    SECTION("Immediate execution full flow")
    {
        // Simulate complete immediate execution flow
        ModuLispInterpreter::set_manual_evaluation(true);
        error_msg_q.clear();

        String code   = "(+ 1 2)";
        String result = interp.eval(code);

        bool had_errors = error_msg_q.size() > 0;
        REQUIRE_FALSE(had_errors);
        REQUIRE(result.indexOf("3") >= 0);

        // Result would be printed here in actual system
        ModuLispInterpreter::set_manual_evaluation(false);
    }

    SECTION("Scheduled execution full flow")
    {
        // Simulate complete scheduled execution flow
        ModuLispInterpreter::set_manual_evaluation(true);

        String code = "(define test-var 123)";
        Value expr  = interp.get_parser()->parse(code);

        size_t before = interp.get_scheduler()->get_run_queue().size();
        interp.get_scheduler()->add_to_run_queue(expr);
        size_t after = interp.get_scheduler()->get_run_queue().size();

        REQUIRE(after == before + 1);

        // Code would be echoed here in actual system
        ModuLispInterpreter::set_manual_evaluation(false);
    }

    SECTION("Error execution full flow")
    {
        // Simulate complete error handling flow
        ModuLispInterpreter::set_manual_evaluation(true);
        error_msg_q.clear();

        String code   = "(bad-function)";
        String result = interp.eval(code);

        bool had_errors = error_msg_q.size() > 0;
        REQUIRE(had_errors);

        // Error would be printed here in actual system
        ModuLispInterpreter::set_manual_evaluation(false);
    }
}
