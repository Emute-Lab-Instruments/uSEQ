#include "uSEQ.h"

#include "lisp/LispLibrary.h"

void uSEQ::update_lisp_time_variables()
{
    DBG("uSEQ::update_lisp_time_variables");

    // These should appear as seconds in Lisp-land
    TimeValue time_s = m_time_since_boot * 1e-6;
    TimeValue t_s    = m_transport_time * 1e-6;
    set("time", Value(time_s));
    set("t", Value(t_s));

    // dbg("time_s = " + String(time_s));
    // dbg("t_s = " + String(t_s));
    // dbg("norm_beat = " + String(norm_beat));
    // dbg("norm_bar = " + String(norm_bar));
    // dbg("norm_phrase = " + String(norm_phrase));
    // dbg("norm_section = " + String(norm_section));

    set("beat", Value(m_beat_phase));
    set("bar", Value(m_bar_phase));
    set("phrase", Value(m_phrase_phase));
    set("section", Value(m_section_phase));
}

void uSEQ::eval_lisp_library()
{
    DBG("eval_lisp_library");

    for (int i = 0; i < LispLibrarySize; i++)
    {
        String code = LispLibrary[i];
        dbg("Evalling code " + String(i) + ":\n" + code);
        eval(code);
    }
}

Value uSEQ::eval(String code) {
    return Interpreter::eval_string(code);
}

Value uSEQ::parse(String code){
    return Interpreter::parse(code);
}

void uSEQ::set(String name, Value val) {
    Interpreter::env.set(name, val);
}

std::optional<Value> uSEQ::get(String name) {
    return Interpreter::env.get(name);
}

void uSEQ::set_expr(String name, Value val) {
    m_def_exprs[name] = val;
    Interpreter::env.set(name, val);
}

std::optional<Value> uSEQ::get_expr(String name) {
    auto it = m_def_exprs.find(name);
    if(it != m_def_exprs.end()) {
        return it->second;
    }
    return std::nullopt;
}

void uSEQ::run_scheduled_items(){

DBG("uSEQ::runScheduledItems");

for (size_t i = 0; i < m_scheduledItems.size(); i++)
{
    // run the statement once every period
    size_t run = static_cast<size_t>(m_bar_phase * m_scheduledItems[i].period);
    //        size_t run_norm = run > m_scheduledItems[i].lastRun ? run : run +
    //        m_scheduledItems[i].period;
    size_t numRuns =
        run >= m_scheduledItems[i].lastRun
            ? run - m_scheduledItems[i].lastRun
            : m_scheduledItems[i].period - m_scheduledItems[i].lastRun;
    for (size_t j = 0; j < numRuns; j++)
    {
        // run the statement
        //             Serial.println(m_scheduledItems[i].id);
        // TODO: #99
        eval(m_scheduledItems[i].ast);
    }
    m_scheduledItems[i].lastRun = run;
}
}

void uSEQ::check_code_quant_phasor()
{
DBG("uSEQ::check_code_quant_phasor");
double newCqpVal = eval(m_cqpAST).as_float();
// double cqpAvgTime = cqpMA.process(newCqpVal - lastCQP);
if (newCqpVal < m_last_CQP)
{
    update_Q0();
    for (size_t q = 0; q < m_runQueue.size(); q++)
    {
        Value res;
        int cmdts = micros();
        res       = eval(m_runQueue[q]);
        cmdts     = micros() - cmdts;
        println(res.to_lisp_src());
    }
    m_runQueue.clear();
}
m_last_CQP = newCqpVal;
}

void uSEQ::update_Q0()
{
Value result = eval(m_q0AST);
if (result.is_error())
{
    Serial.println("Error in q0 output function, clearing");
    m_q0AST = {};
}
}