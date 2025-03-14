#include "uSEQ_Scheduler.h"
#include "uSEQ.h"
#include "utils/log.h"
#include "uSEQ_Modules.h"

#if USE_NEW_MODULES && USE_SCHEDULER_MODULE
// Global variables 
String exit_command = "@@exit";

// Scheduler implementations
void uSEQ::check_and_handle_user_input() {
    DBG("uSEQ::check_and_handle_user_input");
    if (is_new_code_waiting()) {
        m_manual_evaluation = true;
        int first_byte;
        if (bNewI2CMessage)
            first_byte = i2cInBuff[0];
        else
            first_byte = Serial.read();

        // Handle serial stream message
        if (first_byte == SerialMsg::message_begin_marker) {
            size_t channel = Serial.read();
            char buffer[8];
            Serial.readBytes(buffer, 8);
            if (channel > 0 && channel <= m_num_serial_ins) {
                double v = 0;
                memcpy(&v, buffer, 8);
                m_serial_input_streams[(channel - 1)] = v;
            }
        } else {
            // Handle code input
            m_last_received_code = get_code_waiting();
            if (m_last_received_code == exit_command) {
                m_should_quit = true;
                return;
            }
            Value ast = parse(m_last_received_code);
            if (ast.is_error()) {
                println("Parse error");
                return;
            }
            if (m_last_received_code.length() > 0 && m_last_received_code[0] == '@') {
                Value result = eval(ast);
                println(result.to_lisp_src());
            } else {
                m_runQueue.push_back(ast);
            }
        }
    }
}

void uSEQ::check_code_quant_phasor() {
    DBG("uSEQ::check_code_quant_phasor");
    double newCqpVal = eval(m_cqpAST).as_float();
    
    if (newCqpVal < m_last_CQP) {
        update_Q0();
        for (size_t q = 0; q < m_runQueue.size(); q++) {
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

void uSEQ::run_scheduled_items() {
    DBG("uSEQ::runScheduledItems");
    for (size_t i = 0; i < m_scheduledItems.size(); i++) {
        size_t run = static_cast<size_t>(m_bar_phase * m_scheduledItems[i].period);
        size_t numRuns = run >= m_scheduledItems[i].lastRun 
            ? run - m_scheduledItems[i].lastRun
            : m_scheduledItems[i].period - m_scheduledItems[i].lastRun;
            
        for (size_t j = 0; j < numRuns; j++) {
            eval(m_scheduledItems[i].ast);
        }
        m_scheduledItems[i].lastRun = run;
    }
}

#endif // USE_NEW_MODULES && USE_SCHEDULER_MODULE