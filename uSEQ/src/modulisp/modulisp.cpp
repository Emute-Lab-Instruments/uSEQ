#include "modulisp.h"
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
        m_random_generator = std::make_unique<SimpleRandomGenerator>(m_random_seed);
    }
    
    // Initialize with default BPM to avoid divide-by-zero
    m_phasor_manager->set_bpm(m_defaultBPM, 0.0);
    
    // Initialize default CQP AST
    m_scheduler->set_cqp_ast(parse("bar"));
    
    // Sync compatibility layer
    sync_compatibility_layer();
}

// Destructor
ModuLispInterpreter::~ModuLispInterpreter() = default;

// Sync managers -> compatibility layer (for backward compatibility)
void ModuLispInterpreter::sync_compatibility_layer() {
    // Sync time values
    m_time_since_boot = m_time_manager->get_time_since_boot();
    m_transport_time = m_time_manager->get_transport_time();
    m_transport_time_offset = m_time_manager->get_transport_offset();
    
    // Sync phasor lengths
    const auto& lengths = m_phasor_manager->get_lengths();
    m_beat_length = lengths.beat_length;
    m_bar_length = lengths.bar_length;
    m_phrase_length = lengths.phrase_length;
    m_section_length = lengths.section_length;
    
    // Sync phasor values
    PhasorState state;
    m_phasor_manager->update_phasor_state(m_transport_time, state);
    m_beat_phase = state.beat_phase;
    m_bar_phase = state.bar_phase;
    m_phrase_phase = state.phrase_phase;
    m_section_phase = state.section_phase;
    m_current_beat_num = state.current_beat_num;
    m_current_bar_num = state.current_bar_num;
    
    // Sync tempo and meter
    const auto& tempo = m_phasor_manager->get_tempo();
    const auto& meter = m_phasor_manager->get_meter();
    m_bpm = tempo.bpm;
    m_defaultBPM = tempo.default_bpm;
    meter_numerator = meter.numerator;
    meter_denominator = meter.denominator;
    m_bars_per_phrase = m_phasor_manager->get_bars_per_phrase();
    m_phrases_per_section = m_phasor_manager->get_phrases_per_section();
    
    // Sync scheduler state - convert between struct types
    const auto& scheduler_items = m_scheduler->get_scheduled_items();
    m_scheduledItems.clear();
    for (const auto& item : scheduler_items) {
        scheduledItem compat_item;
        compat_item.ast = item.ast;
        compat_item.period = item.period;
        compat_item.lastRun = item.last_run;
        compat_item.id = item.id;
        m_scheduledItems.push_back(compat_item);
    }
    
    m_runQueue = m_scheduler->get_run_queue();
    m_cqpAST = m_scheduler->get_cqp_ast();
    m_q0AST = m_scheduler->get_q0_ast();
    m_last_CQP = m_scheduler->get_last_cqp();
}

// Sync compatibility layer -> managers (for code using old member variables)
void ModuLispInterpreter::sync_from_compatibility_layer() {
    // Update scheduler items if changed directly
    for (const auto& item : m_scheduledItems) {
        m_scheduler->schedule(item.id, item.ast, item.period);
    }
    
    // Update run queue if changed directly
    m_scheduler->clear_run_queue();
    for (const auto& item : m_runQueue) {
        m_scheduler->add_to_run_queue(item);
    }
    
    // Update ASTs if changed
    m_scheduler->set_cqp_ast(m_cqpAST);
    m_scheduler->set_q0_ast(m_q0AST);
    m_scheduler->set_last_cqp(m_last_CQP);
}

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
