#include "modulisp_interpreter.h"
#include "../utils.h"

// Constructor with dependency injection
ModuLispInterpreter::ModuLispInterpreter(ErrorManager* error_mgr, Environment* env, uLispParser* parser, IClock* clk, ILogger* log, IRandomGenerator* rng)
    : Interpreter(env, parser, error_mgr), clock(clk), logger(log) {
    
    // Create managers with dependency injection
    m_time_manager = std::make_unique<TimeManager>(clk);
    m_scheduler = std::make_unique<Scheduler>();
    
    // Use provided random generator or create default
    if (rng) {
        m_random_generator.reset(rng);
    } else {
        m_random_generator = std::make_unique<SimpleRandomGenerator>();
    }
    
    // Initialize default BPM to avoid divide-by-zero (member variables already initialized with defaults)
    set_bpm(130.0, 0.0);
    
    // Initialize default CQP AST (only if parser is available)
    if (get_parser()) {
        m_scheduler->set_cqp_ast(get_parser()->parse("bar"));
    } else {
        m_scheduler->set_cqp_ast(Value::atom("bar"));
    }
}

// Destructor
ModuLispInterpreter::~ModuLispInterpreter() = default;


void ModuLispInterpreter::run_scheduled_items() {
    DBG("ModuLispInterpreter::runScheduledItems");
    
    size_t current_time = static_cast<size_t>(m_time_manager->get_transport_time());
    auto items_to_run = m_scheduler->get_items_to_run(current_time);
    
    for (auto* item : items_to_run) {
        if (item && !item->ast.is_nil()) {
            // TODO: Add proper error handling without exceptions
            Value result = eval(item->ast);
            // Note: Without exception handling, errors will propagate up
            (void)result; // Suppress unused variable warning
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