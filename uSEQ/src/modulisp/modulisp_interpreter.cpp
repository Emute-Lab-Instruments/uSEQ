#include "modulisp_interpreter.h"
#include "../utils.h"

// Constructor and destructor now implemented in modulisp_interpreter_core.cpp

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
