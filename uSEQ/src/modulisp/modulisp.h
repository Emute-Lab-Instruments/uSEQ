#ifndef MODULISP_H_
#define MODULISP_H_

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

using TimeValue = double;
using PhaseValue = double;

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
    
    // Main update functions
    void update_time();
    void reset_logical_time();
    void update_logical_time(TimeValue);
    void update_logical_time_variables(TimeValue);
    void update_lisp_time_variables();
    
    // Time management (delegated to TimeManager)
    TimeValue get_time_since_boot() const { 
        return m_time_manager->get_time_since_boot(); 
    }
    TimeValue get_transport_time() const { 
        return m_time_manager->get_transport_time(); 
    }
    
    // Phasor calculations (delegated to PhasorManager)
    PhaseValue beat_at_time(TimeValue time) { 
        return m_phasor_manager->beat_at_time(time); 
    }
    uint32_t beat_num_at_time(TimeValue time) { 
        return m_phasor_manager->beat_num_at_time(time); 
    }
    PhaseValue bar_at_time(TimeValue time) { 
        return m_phasor_manager->bar_at_time(time); 
    }
    uint32_t bar_num_at_time(TimeValue time) { 
        return m_phasor_manager->bar_num_at_time(time); 
    }
    PhaseValue phrase_at_time(TimeValue time) { 
        return m_phasor_manager->phrase_at_time(time); 
    }
    PhaseValue section_at_time(TimeValue time) { 
        return m_phasor_manager->section_at_time(time); 
    }
    
    // BPM and meter management
    void set_bpm(double newBpm, double changeThreshold);
    void update_bpm_variables();
    void set_time_sig(double num, double denom);
    
    // Scheduling
    void run_scheduled_items();
    void check_code_quant_phasor();
    void update_Q0();
    
    // Environment creation
    Environment make_env_for_time(TimeValue);
    Environment make_env_with_updated_time_durs(const Environment &, TimeValue);
    
    // Special evaluation
    Value eval_at_time(Value &, Environment &, double);
    
    // Random number generation
    double simple_hashing_function(uint32_t index) {
        return m_random_generator->generate_with_index(index);
    }
    
    // LISP function declarations
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
    
    // ===== COMPATIBILITY LAYER =====
    // These members provide backward compatibility during migration
    // They will be removed once all code is updated
    
    // Time state (now managed by TimeManager)
    uint8_t m_overflow_counter = 0;
    size_t m_micros_raw = 0;
    size_t m_micros_raw_last = 0;
    TimeValue m_time_since_boot = 0.0;
    TimeValue m_last_known_time_since_boot = -1;
    TimeValue m_last_transport_reset_time = 0.0;
    TimeValue m_transport_time = 0.0;
    TimeValue m_transport_time_offset = 0.0;
    TimeValue m_last_transport_time = 0.0;
    
    // Phasor state (now managed by PhasorManager)
    TimeValue m_beat_length = 0.0;
    TimeValue m_bar_length = 0.0;
    TimeValue m_phrase_length = 0.0;
    TimeValue m_section_length = 0.0;
    uint32_t m_current_beat_num = 0;
    uint32_t m_current_bar_num = 0;
    PhaseValue m_beat_phase = 0.0;
    PhaseValue m_bar_phase = 0.0;
    PhaseValue m_phrase_phase = 0.0;
    PhaseValue m_section_phase = 0.0;
    
    // Meter and tempo (now in PhasorManager)
    double meter_numerator = 4;
    double meter_denominator = 4;
    double m_bars_per_phrase = 16;
    double m_phrases_per_section = 16;
    double m_defaultBPM = 130;
    double m_bpm = 130;
    
    // Scheduling (now managed by Scheduler)
    struct scheduledItem {
        Value ast;
        size_t period;
        size_t lastRun;
        String id;
    };
    std::vector<scheduledItem> m_scheduledItems;
    std::vector<Value> m_runQueue;
    Value m_cqpAST = parse("bar");
    Value m_q0AST;
    double m_last_CQP = 0.0;
    
    // Random (now managed by RandomGenerator)
    uint32_t m_random_seed = 0x9E3779B9;
    
    // Performance monitoring
    int ts = 0;
    int updateSpeed = 0;
    
  protected:
    // Core components (composition over inheritance)
    std::unique_ptr<TimeManager> m_time_manager;
    std::unique_ptr<PhasorManager> m_phasor_manager;
    std::unique_ptr<Scheduler> m_scheduler;
    std::unique_ptr<IRandomGenerator> m_random_generator;
    
    // Optional injected adapters
    IClock* clock = nullptr;
    ILogger* logger = nullptr;
    
  private:
    // Helper to sync compatibility layer with managers
    void sync_compatibility_layer();
    void sync_from_compatibility_layer();
};

#endif // MODULISP_H_