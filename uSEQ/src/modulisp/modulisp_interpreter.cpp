#include "modulisp_interpreter.h"
#include "../utils.h"

// Constructor and destructor now implemented in modulisp_interpreter_core.cpp

////////////////////////////////////////////////////////////////////////////////
// Execution API - Platform-agnostic code execution semantics
////////////////////////////////////////////////////////////////////////////////

ExecutionResult ModuLispInterpreter::execute_now(const String& code)
{
    // Set manual evaluation mode to distinguish user input from automated eval
    // This flag affects how certain operations behave (e.g., error reporting)
    set_manual_evaluation(true);

    // Clear any previous errors before execution
    // This ensures we only capture errors from THIS execution
    error_msg_q.clear();

    // Execute the code synchronously and capture the result
    String result = eval(code);

    // Build the execution result structure
    ExecutionResult exec_result;
    exec_result.result_text = result;
    exec_result.had_errors = !error_msg_q.empty();
    exec_result.errors = error_msg_q;  // Copy error queue to result
    exec_result.printed = true;         // Immediate execution always prints

    // Restore manual evaluation flag
    set_manual_evaluation(false);

    return exec_result;
}

ExecutionResult ModuLispInterpreter::schedule_code(const String& code)
{
    // Set manual evaluation mode (same rationale as execute_now)
    set_manual_evaluation(true);

    // Parse the code into an AST Value
    // NOTE: Parsing errors would be captured in error_msg_q, but current
    // implementation doesn't check them here (matches original behavior)
    Value expr = get_parser()->parse(code);

    // Add the parsed expression to the scheduler's run queue
    // It will be executed later when check_code_quant_phasor detects a quantum wrap
    get_scheduler()->add_to_run_queue(expr);

    // Build the execution result structure
    ExecutionResult exec_result;
    exec_result.result_text = code;     // Echo the scheduled code
    exec_result.had_errors = false;     // Scheduling itself doesn't produce errors
    exec_result.errors.clear();         // No errors
    exec_result.printed = true;         // Echo confirms scheduling

    // Restore manual evaluation flag
    set_manual_evaluation(false);

    return exec_result;
}

void ModuLispInterpreter::update_stream_value(size_t channel, double value)
{
    // NOTE: This is currently a no-op stub.
    //
    // DESIGN DECISION:
    // Serial input streams currently live in the uSEQ layer (m_serial_input_streams[])
    // because they are hardware-specific. This method exists to complete the execution
    // API contract, making the interface consistent even if the implementation is deferred.
    //
    // WHY STUB:
    // 1. Moving serial stream storage here would require Phase 2 work (output ownership)
    // 2. WASM builds may not need serial streams (or have different stream sources)
    // 3. The uSEQ layer will continue to call its own stream update logic for now
    //
    // FUTURE WORK (Phase 2):
    // When output ownership migrates to ModuLispInterpreter, add:
    //   - std::vector<double> m_serial_input_streams member variable
    //   - size_t m_num_serial_ins configuration
    //   - Implement this method to update m_serial_input_streams[channel-1]
    //   - Update sin1-sin32 builtins to read from interpreter storage
    //
    // For now, this is intentionally a no-op. The uSEQ::check_and_handle_user_input()
    // method will continue to update streams directly in the uSEQ layer.

    (void)channel;  // Suppress unused parameter warning
    (void)value;    // Suppress unused parameter warning

    // Silently ignore stream updates (matches current behavior for out-of-bounds channels)
}

void ModuLispInterpreter::run_scheduled_items()
{
    DBG("ModuLispInterpreter::runScheduledItems");

    size_t current_time = static_cast<size_t>(m_time_manager->get_transport_time());
    auto items_to_run   = m_scheduler->get_items_to_run(current_time);

    for (auto* item : items_to_run)
    {
        if (item && !item->ast.is_nil())
        {
            // TODO: Add proper error handling without exceptions
            Value result = eval(item->ast);
            // Note: Without exception handling, errors will propagate up
            (void)result; // Suppress unused variable warning
        }
    }
}

void ModuLispInterpreter::check_code_quant_phasor()
{
    DBG("ModuLispInterpreter::check_code_quant_phasor");

    double newCqpVal = eval(m_scheduler->get_cqp_ast()).as_float();
    double last_cqp  = m_scheduler->get_last_cqp();

    if (newCqpVal < last_cqp)
    {
        // Phasor wrapped around, execute queued items
        update_Q0();
        auto& run_queue = m_scheduler->get_run_queue();
        for (const auto& item : run_queue)
        {
            Value res;
            int cmdts = micros();
            res       = eval(item);
            cmdts     = micros() - cmdts;
            println(res.to_lisp_src());
        }
        m_scheduler->clear_run_queue();
    }
    m_scheduler->set_last_cqp(newCqpVal);
}

void ModuLispInterpreter::update_Q0()
{
    const Value& q0_ast = m_scheduler->get_q0_ast();
    if (!q0_ast.is_nil())
    {
        Value result = eval(q0_ast);
        if (result.is_error())
        {
            println("Error in q0 output function, clearing");
            m_scheduler->set_q0_ast(Value());
        }
    }
}
