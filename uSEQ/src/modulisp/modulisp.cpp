#include "modulisp.h"
#include "../utils.h"

// Defaulted constructor defined in header now; no additional implementation

void ModuLispInterpreter::run_scheduled_items() {
    DBG("uSEQ::runScheduledItems");

    for (size_t i = 0; static_cast<size_t>(i) < m_scheduledItems.size(); i++) {
        // run the statement once every period
        size_t run =
            static_cast<size_t>(m_bar_phase * m_scheduledItems[i].period);
        //        size_t run_norm = run > m_scheduledItems[i].lastRun ? run :
        //        run + m_scheduledItems[i].period;
        size_t numRuns =
            run >= m_scheduledItems[i].lastRun
                ? run - m_scheduledItems[i].lastRun
                : m_scheduledItems[i].period - m_scheduledItems[i].lastRun;
        for (size_t j = 0; j < numRuns; j++) {
            // run the statement
            //             println(m_scheduledItems[i].id);
            // TODO: #99
            eval(m_scheduledItems[i].ast);
        }
        m_scheduledItems[i].lastRun = run;
    }
}


void ModuLispInterpreter::check_code_quant_phasor() {
    DBG("uSEQ::check_code_quant_phasor");
    double newCqpVal = eval(m_cqpAST).as_float();
    // double cqpAvgTime = cqpMA.process(newCqpVal - lastCQP);
    if (newCqpVal < m_last_CQP) {
        update_Q0();
        for (size_t q = 0; static_cast<size_t>(q) < m_runQueue.size(); q++) {
            Value res;
            int cmdts = micros();
            res = eval(m_runQueue[q]);
            cmdts = micros() - cmdts;
            println(res.to_lisp_src());
        }
        m_runQueue.clear();
    }
    m_last_CQP = newCqpVal;
}

void ModuLispInterpreter::update_Q0() {
    Value result = eval(m_q0AST);
    if (result.is_error()) {
        println("Error in q0 output function, clearing");
        m_q0AST = {};
    }
}
