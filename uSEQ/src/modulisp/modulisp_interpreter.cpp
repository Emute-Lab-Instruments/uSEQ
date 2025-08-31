#include "modulisp_interpreter.h"
#include "../utils.h"

// Constructor with dependency injection
ModuLispInterpreter::ModuLispInterpreter(IClock* clk, ILogger* log, IRandomGenerator* rng)
    : clock(clk), logger(log) {
    
    // Create managers with dependency injection
    m_time_manager = std::make_unique<TimeManager>(clk);
    m_phasor_manager = std::make_unique<PhasorManager>();
    m_scheduler = std::make_unique<Scheduler>();
    
    // Use provided random generator or create default
    if (rng) {
        m_random_generator.reset(rng);
    } else {
        m_random_generator = std::make_unique<SimpleRandomGenerator>();
    }
    
    // Initialize with default BPM to avoid divide-by-zero
    m_phasor_manager->set_bpm(130.0, 0.0);
    
    // Initialize default CQP AST
    m_scheduler->set_cqp_ast(parse("bar"));
}

// Destructor
ModuLispInterpreter::~ModuLispInterpreter() = default;

void ModuLispInterpreter::update_phasor_state() {
    TimeValue transport_time = m_time_manager->get_transport_time();
    m_phasor_manager->update_phasor_state(transport_time, m_current_phasor_state);
}

void ModuLispInterpreter::run_scheduled_items() {
    DBG("ModuLispInterpreter::runScheduledItems");
    
    size_t current_time = static_cast<size_t>(m_time_manager->get_transport_time());
    auto items_to_run = m_scheduler->get_items_to_run(current_time);
    
    for (auto* item : items_to_run) {
        if (item && !item->ast.is_nil()) {
            try {
                eval(item->ast);
            } catch (...) {
                if (logger) {
                    logger->error("Error executing scheduled item: " + item->id);
                }
            }
        }
    }
}

void ModuLispInterpreter::check_code_quant_phasor() {
    DBG("ModuLispInterpreter::check_code_quant_phasor");
    
    double newCqpVal = eval(m_scheduler->get_cqp_ast()).as_float();
    double last_cqp = m_scheduler->get_last_cqp();
    
    if (newCqpVal < last_cqp) {
        // Phasor wrapped around, execute queued items
        update_Q0();
        auto& run_queue = m_scheduler->get_run_queue();
        for (const auto& item : run_queue) {
            Value res;
            int cmdts = micros();
            res = eval(item);
            cmdts = micros() - cmdts;
            println(res.to_lisp_src());
        }
        m_scheduler->clear_run_queue();
    }
    m_scheduler->set_last_cqp(newCqpVal);
}

void ModuLispInterpreter::update_Q0() {
    const Value& q0_ast = m_scheduler->get_q0_ast();
    if (!q0_ast.is_nil()) {
        Value result = eval(q0_ast);
        if (result.is_error()) {
            println("Error in q0 output function, clearing");
            m_scheduler->set_q0_ast(Value());
        }
    }
}