#ifndef MODULISP_INTERPRETER_H_
#define MODULISP_INTERPRETER_H_

#include "../ports/IClock.h"
#include "../ports/ILogger.h"
#include "lisp/interpreter.h"
#include "random_generator.h"
#include "scheduler.h"
#include "time_manager.h"
#include <memory>
#include <cmath>

#define LISP_FUNC_ARGS_TYPE std::vector<Value>&, Environment&
#define LISP_FUNC_ARGS std::vector<Value>&args, Environment &env
#define LISP_FUNC_RETURN_TYPE Value
#define LISP_FUNC_TYPE LISP_FUNC_RETURN_TYPE(LISP_FUNC_ARGS_TYPE)
#define LISP_FUNC_DECL(__name__) LISP_FUNC_RETURN_TYPE __name__(LISP_FUNC_ARGS)

using TimeValue  = double;
using PhaseValue = double;

class ModuLispInterpreter : public Interpreter
{
public:
    // Constructor with dependency injection for testability
    // NOTE: unused for now
    explicit ModuLispInterpreter(ErrorManager* error_mgr, Environment* env = nullptr,
                                 uLispParser* parser = nullptr,
                                 IClock* clk = nullptr, ILogger* log = nullptr,
                                 IRandomGenerator* rng = nullptr);

    void init()
    {
        loadBuiltinDefs();
        init_builtinfuncs();
        Interpreter::modulisp_instance_ptr = this;
    }

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

    // Time management accessors
    TimeValue get_time_since_boot() const
    {
        return m_time_manager->get_time_since_boot();
    }
    TimeValue get_transport_time() const
    {
        return m_time_manager->get_transport_time();
    }

    // Phasor calculations
    PhaseValue beat_at_time(TimeValue time)
    {
        return std::fmod(time, m_beat_length) / m_beat_length;
    }
    uint32_t beat_num_at_time(TimeValue time)
    {
        if (m_beat_length <= 0) return 0;
        return static_cast<uint32_t>(time / m_beat_length);
    }
    PhaseValue bar_at_time(TimeValue time)
    {
        return std::fmod(time, m_bar_length) / m_bar_length;
    }
    uint32_t bar_num_at_time(TimeValue time)
    {
        if (m_bar_length <= 0) return 0;
        return static_cast<uint32_t>(time / m_bar_length);
    }
    PhaseValue phrase_at_time(TimeValue time)
    {
        return std::fmod(time, m_phrase_length) / m_phrase_length;
    }
    PhaseValue section_at_time(TimeValue time)
    {
        return std::fmod(time, m_section_length) / m_section_length;
    }

    // BPM and meter management
    void set_bpm(double newBpm, double changeThreshold);
    void update_bpm_variables();
    void set_time_sig(double num, double denom);

    // Get current settings
    double get_bpm() const { return m_bpm; }
    double get_meter_numerator() const
    {
        return 4.0; // Default 4/4 time - can be made configurable later
    }
    double get_meter_denominator() const
    {
        return 4.0; // Default 4/4 time - can be made configurable later
    }
    
    // Phase structure getters
    double get_bars_per_phrase() const { return m_bars_per_phrase; }
    double get_phrases_per_section() const { return m_phrases_per_section; }

    // Scheduling
    void run_scheduled_items();
    void check_code_quant_phasor();
    void update_Q0();

    // Environment creation
    Environment make_env_for_time(TimeValue);
    Environment make_env_with_updated_time_durs(const Environment&, TimeValue);

    // Special evaluation
    Value eval_at_time(Value&, Environment&, double);

    // Random number generation
    double simple_hashing_function(uint32_t index)
    {
        return m_random_generator->generate_with_index(index);
    }

    // Access to managers (for testing and advanced use)
    TimeManager* get_time_manager() { return m_time_manager.get(); }
    Scheduler* get_scheduler() { return m_scheduler.get(); }
    IRandomGenerator* get_random_generator() { return m_random_generator.get(); }

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

    // Performance monitoring
    int ts          = 0;
    int updateSpeed = 0;

protected:
    // Core components (composition over inheritance)
    std::unique_ptr<TimeManager> m_time_manager;
    std::unique_ptr<Scheduler> m_scheduler;
    std::unique_ptr<IRandomGenerator> m_random_generator;

    // Optional injected adapters
    IClock* clock   = nullptr;
    ILogger* logger = nullptr;

    // Phase timing (direct implementation like old uSEQ)
    double m_beat_length = 500000.0;    // microseconds per beat (120 BPM default)
    double m_bar_length = 2000000.0;    // microseconds per bar
    double m_phrase_length = 8000000.0; // microseconds per phrase
    double m_section_length = 32000000.0; // microseconds per section
    double m_bpm = 120.0;
    double m_bars_per_phrase = 4.0;
    double m_phrases_per_section = 4.0;

private:
};

#endif // MODULISP_INTERPRETER_H_