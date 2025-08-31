#include "modulisp_refactored.h"
#include "../utils.h"

ModuLispInterpreter::ModuLispInterpreter(IClock* clk, ILogger* log, IRandomGenerator* rng)
    : m_logger(log) {
    
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
}

ModuLispInterpreter::~ModuLispInterpreter() = default;

void ModuLispInterpreter::update() {
    // Update time
    m_time_manager->update();
    
    // Update phasor state
    update_phasor_state();
    
    // Update LISP environment variables
    update_lisp_time_variables();
    
    // Run scheduled items
    run_scheduled_items();
    
    // Check code quantization phasor
    check_code_quant_phasor();
    
    // Update Q0 if needed
    update_Q0();
    
    // Update performance counter
    m_timestamp++;
}

void ModuLispInterpreter::update_phasor_state() {
    TimeValue transport_time = m_time_manager->get_transport_time();
    m_phasor_manager->update_phasor_state(transport_time, m_current_phasor_state);
}

void ModuLispInterpreter::update_lisp_time_variables() {
    // Update time variables in LISP environment
    set("time", Value(m_time_manager->get_time_seconds()));
    set("t", Value(m_time_manager->get_transport_seconds()));
    
    // Update phasor variables
    set("beat", Value(m_current_phasor_state.beat_phase));
    set("bar", Value(m_current_phasor_state.bar_phase));
    set("phrase", Value(m_current_phasor_state.phrase_phase));
    set("section", Value(m_current_phasor_state.section_phase));
    
    // Update beat/bar numbers
    set("beatNum", Value(static_cast<int>(m_current_phasor_state.current_beat_num)));
    set("barNum", Value(static_cast<int>(m_current_phasor_state.current_bar_num)));
    
    // Update duration variables (in seconds)
    const auto& lengths = m_phasor_manager->get_lengths();
    set("beatDur", Value(lengths.beat_length / 1000000.0));
    set("barDur", Value(lengths.bar_length / 1000000.0));
    set("phraseDur", Value(lengths.phrase_length / 1000000.0));
    set("sectionDur", Value(lengths.section_length / 1000000.0));
    
    // Update tempo and meter
    const auto& tempo = m_phasor_manager->get_tempo();
    const auto& meter = m_phasor_manager->get_meter();
    set("bpm", Value(tempo.bpm));
    set("meterNum", Value(meter.numerator));
    set("meterDenom", Value(meter.denominator));
}

void ModuLispInterpreter::run_scheduled_items() {
    size_t current_time = static_cast<size_t>(m_time_manager->get_transport_time());
    auto items_to_run = m_scheduler->get_items_to_run(current_time);
    
    for (auto* item : items_to_run) {
        if (item && item->ast.is_valid()) {
            try {
                eval(item->ast);
            } catch (...) {
                if (m_logger) {
                    m_logger->error("Error executing scheduled item: " + item->id);
                }
            }
        }
    }
}

Environment ModuLispInterpreter::make_env_for_time(TimeValue time) {
    Environment env(*this);  // Start with current environment
    
    // Calculate phasor values at the specified time
    PhasorState state_at_time;
    m_phasor_manager->update_phasor_state(time, state_at_time);
    
    // Set time-specific variables
    env.set("t", Value(time / 1000000.0));
    env.set("beat", Value(state_at_time.beat_phase));
    env.set("bar", Value(state_at_time.bar_phase));
    env.set("phrase", Value(state_at_time.phrase_phase));
    env.set("section", Value(state_at_time.section_phase));
    env.set("beatNum", Value(static_cast<int>(state_at_time.current_beat_num)));
    env.set("barNum", Value(static_cast<int>(state_at_time.current_bar_num)));
    
    return env;
}

Environment ModuLispInterpreter::make_env_with_updated_time_durs(const Environment& env, TimeValue time) {
    Environment new_env(env);
    
    // Update duration variables based on current tempo settings
    const auto& lengths = m_phasor_manager->get_lengths();
    new_env.set("beatDur", Value(lengths.beat_length / 1000000.0));
    new_env.set("barDur", Value(lengths.bar_length / 1000000.0));
    new_env.set("phraseDur", Value(lengths.phrase_length / 1000000.0));
    new_env.set("sectionDur", Value(lengths.section_length / 1000000.0));
    
    return new_env;
}

Value ModuLispInterpreter::eval_at_time(Value& ast, Environment& env, double time) {
    Environment time_env = make_env_for_time(static_cast<TimeValue>(time * 1000000.0));
    return eval_in(ast, time_env);
}

// Compatibility methods for gradual migration
void ModuLispInterpreter::update_logical_time(TimeValue t) {
    // This is handled by TimeManager::update() now
    // Kept for compatibility
}

void ModuLispInterpreter::update_logical_time_variables(TimeValue t) {
    update_lisp_time_variables();
}

void ModuLispInterpreter::check_code_quant_phasor() {
    double current_cqp = eval_v(m_scheduler->get_cqp_ast()).as_float();
    double last_cqp = m_scheduler->get_last_cqp();
    
    if (current_cqp < last_cqp) {
        // Phasor wrapped around, execute queued items
        auto& run_queue = m_scheduler->get_run_queue();
        for (const auto& item : run_queue) {
            eval(item);
        }
        m_scheduler->clear_run_queue();
    }
    
    m_scheduler->set_last_cqp(current_cqp);
}

void ModuLispInterpreter::update_Q0() {
    const Value& q0_ast = m_scheduler->get_q0_ast();
    if (q0_ast.is_valid()) {
        eval(q0_ast);
    }
}

// LISP function stubs - implementations would be in modulisp_api.cpp
LISP_FUNC_DECL(ModuLispInterpreter::useq_random) {
    return Value(m_random_generator->generate());
}

LISP_FUNC_DECL(ModuLispInterpreter::useq_index_rand) {
    if (args.size() < 1) return Value::nil();
    uint32_t index = static_cast<uint32_t>(args[0].as_int());
    return Value(m_random_generator->generate_with_index(index));
}

LISP_FUNC_DECL(ModuLispInterpreter::useq_setbpm) {
    if (args.size() < 1) return Value::nil();
    double new_bpm = args[0].as_float();
    set_bpm(new_bpm, 0.5);  // Use 0.5 BPM threshold
    return Value(new_bpm);
}

LISP_FUNC_DECL(ModuLispInterpreter::useq_set_time_sig) {
    if (args.size() < 2) return Value::nil();
    double num = args[0].as_float();
    double denom = args[1].as_float();
    set_time_sig(num, denom);
    return Value::nil();
}