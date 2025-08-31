#ifndef MODULISP_REFACTORED_H_
#define MODULISP_REFACTORED_H_

#include "lisp/interpreter.h"
#include "../ports/IClock.h"
#include "../ports/ILogger.h"
#include "time_manager.h"
#include "phasor_manager.h"
#include "scheduler.h"
#include "random_generator.h"
#include <memory>

#define LISP_FUNC_ARGS_TYPE std::vector<Value> &, Environment &
#define LISP_FUNC_ARGS std::vector<Value> &args, Environment &env
#define LISP_FUNC_RETURN_TYPE Value
#define LISP_FUNC_TYPE LISP_FUNC_RETURN_TYPE(LISP_FUNC_ARGS_TYPE)
#define LISP_FUNC_DECL(__name__) LISP_FUNC_RETURN_TYPE __name__(LISP_FUNC_ARGS)

class ModuLispInterpreter : public Interpreter {
public:
    // Constructor with dependency injection for testability
    explicit ModuLispInterpreter(
        IClock* clk = nullptr, 
        ILogger* log = nullptr,
        IRandomGenerator* rng = nullptr
    );
    
    // Destructor
    ~ModuLispInterpreter();
    
    // Initialization
    void init_builtinfuncs();
    
    // Main update function - updates time and phasors
    void update();
    
    // Time management delegation
    void reset_transport_time() { m_time_manager->reset_transport(); }
    void nudge_time(TimeValue offset) { m_time_manager->set_transport_offset(offset); }
    
    // Tempo and meter management delegation
    void set_bpm(double bpm, double threshold) { 
        m_phasor_manager->set_bpm(bpm, threshold);
        update_lisp_time_variables();
    }
    void set_time_sig(double num, double denom) { 
        m_phasor_manager->set_time_signature(num, denom);
        update_lisp_time_variables();
    }
    
    // Scheduling delegation
    void schedule(const String& id, const Value& ast, size_t period) {
        m_scheduler->schedule(id, ast, period);
    }
    bool unschedule(const String& id) {
        return m_scheduler->unschedule(id);
    }
    void run_scheduled_items();
    
    // Environment creation for time-based evaluation
    Environment make_env_for_time(TimeValue time);
    Environment make_env_with_updated_time_durs(const Environment& env, TimeValue time);
    
    // Special evaluation at specific time
    Value eval_at_time(Value& ast, Environment& env, double time);
    
    // Update LISP environment with current time values
    void update_lisp_time_variables();
    
    // Performance monitoring
    int get_update_speed() const { return m_update_speed; }
    void set_update_speed(int speed) { m_update_speed = speed; }
    
    // Access to managers (for advanced use/testing)
    TimeManager* get_time_manager() { return m_time_manager.get(); }
    PhasorManager* get_phasor_manager() { return m_phasor_manager.get(); }
    Scheduler* get_scheduler() { return m_scheduler.get(); }
    IRandomGenerator* get_random_generator() { return m_random_generator.get(); }
    
    // LISP function declarations (these could be moved to a separate registry)
    LISP_FUNC_DECL(useq_eval_at_time);
    LISP_FUNC_DECL(useq_nudge_time);
    LISP_FUNC_DECL(useq_set_time_offset);
    LISP_FUNC_DECL(useq_slow);
    LISP_FUNC_DECL(useq_fast);
    LISP_FUNC_DECL(useq_offset_time);
    LISP_FUNC_DECL(useq_schedule);
    LISP_FUNC_DECL(useq_unschedule);
    LISP_FUNC_DECL(useq_setbpm);
    LISP_FUNC_DECL(useq_set_time_sig);
    LISP_FUNC_DECL(useq_tri);
    LISP_FUNC_DECL(useq_random);
    LISP_FUNC_DECL(useq_index_rand);
    LISP_FUNC_DECL(useq_loop_at_time);
    LISP_FUNC_DECL(useq_dm);
    LISP_FUNC_DECL(useq_gates);
    LISP_FUNC_DECL(useq_gatesw);
    LISP_FUNC_DECL(useq_trigs);
    LISP_FUNC_DECL(useq_euclidean);
    LISP_FUNC_DECL(useq_eu);
    LISP_FUNC_DECL(useq_ratiotrig);
    LISP_FUNC_DECL(useq_ratiostep);
    LISP_FUNC_DECL(useq_ratioindex);
    LISP_FUNC_DECL(useq_ratiowarp);
    LISP_FUNC_DECL(useq_phasor_offset);
    LISP_FUNC_DECL(useq_flatten);
    LISP_FUNC_DECL(useq_step);
    LISP_FUNC_DECL(useq_fromList);
    LISP_FUNC_DECL(useq_fromFlattenedList);
    LISP_FUNC_DECL(useq_seq);
    LISP_FUNC_DECL(useq_flatseq);
    LISP_FUNC_DECL(useq_interpolate);
    LISP_FUNC_DECL(useq_rewind_logical_time);
    LISP_FUNC_DECL(useq_q0);
    
    // Compatibility methods for gradual migration
    void update_time() { m_time_manager->update(); }
    void reset_logical_time() { m_time_manager->reset_transport(); }
    void update_logical_time(TimeValue t);
    void update_logical_time_variables(TimeValue t);
    void check_code_quant_phasor();
    void update_Q0();
    
protected:
    // Core components (composition over inheritance)
    std::unique_ptr<TimeManager> m_time_manager;
    std::unique_ptr<PhasorManager> m_phasor_manager;
    std::unique_ptr<Scheduler> m_scheduler;
    std::unique_ptr<IRandomGenerator> m_random_generator;
    
    // Logger interface
    ILogger* m_logger;
    
    // Current phasor state (cached for performance)
    PhasorState m_current_phasor_state;
    
    // Performance monitoring
    int m_timestamp = 0;
    int m_update_speed = 0;
    
private:
    // Helper to update phasor state
    void update_phasor_state();
};

#endif // MODULISP_REFACTORED_H_