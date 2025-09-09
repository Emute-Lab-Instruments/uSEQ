#ifndef MODULISP_INTERPRETER_H_
#define MODULISP_INTERPRETER_H_

#include "../ports/IClock.h"
#include "../ports/ILogger.h"
#include "lisp/environment.h"
#include "lisp/parser.h"
#include "lisp/error_context.h"
#include "lisp/value.h"
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

class ModuLispInterpreter
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
        init_builtin_functions();
        init_builtinfuncs();
        ModuLispInterpreter::modulisp_instance_ptr = this;
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
        return m_meter_numerator;
    }
    double get_meter_denominator() const
    {
        return m_meter_denominator;
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

    // --- Absorbed Interpreter API ---
    // Evaluation API
    String eval(const String &code);
    Value eval(Value v);
    Value eval_v(const String &code);

    // Static evaluation helpers
    static String eval_in(const String &code, Environment &env);
    static Value eval_in(Value &v, Environment &env);
    static Value apply(Value &f, LispFuncArgsVec &args, Environment &env);
    static void eval_args(std::vector<Value> &args, Environment &env);
    static void init_builtin_functions();
    void loadBuiltinDefs();

    // Factory for tests
    static std::unique_ptr<ModuLispInterpreter> create_fresh_interpreter();

    // Error handling and accessors
    ErrorManager* get_error_manager() { return m_error_manager; }
    const ErrorManager* get_error_manager() const { return m_error_manager; }
    Environment* get_environment() { return m_environment; }
    const Environment* get_environment() const { return m_environment; }
    uLispParser* get_parser() { return m_parser; }
    const uLispParser* get_parser() const { return m_parser; }

    // Atom evaluation tracking and flags
    static void set_atom_currently_being_evaluated(const String& atom_name) { m_atom_currently_being_evaluated = atom_name; }
    static bool get_attempt_expr_eval_first() { return m_attempt_expr_eval_first; }
    static void set_attempt_expr_eval_first(bool value) { m_attempt_expr_eval_first = value; }
    static bool get_update_loop_evaluation() { return m_update_loop_evaluation; }
    static void set_update_loop_evaluation(bool value) { m_update_loop_evaluation = value; }
    static bool get_manual_evaluation() { return m_manual_evaluation; }
    static void set_manual_evaluation(bool value) { m_manual_evaluation = value; }

    // Instance pointer helper for uSEQ integration
    static void set_useq_instance_ptr(uSEQ* ptr) { useq_instance_ptr = ptr; }

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
    double m_meter_numerator = 4.0;   // Time signature numerator (default 4/4)
    double m_meter_denominator = 4.0; // Time signature denominator (default 4/4)

private:
    // Absorbed Interpreter state
    bool m_builtindefs_init = false;

    Environment* m_environment;
    uLispParser* m_parser;
    ErrorManager* m_error_manager;

    std::unique_ptr<Environment> m_fallback_environment;
    std::unique_ptr<uLispParser> m_fallback_parser;
    std::unique_ptr<ErrorManager> m_fallback_error_manager;

    // Static flags and shared state
    static bool m_attempt_expr_eval_first;
    static bool m_eval_expr_if_def_not_found;
    static bool m_manual_evaluation;
    static bool m_update_loop_evaluation;
    static String m_atom_currently_being_evaluated;

    static uSEQ* useq_instance_ptr;
    static ModuLispInterpreter* modulisp_instance_ptr;
};

#endif // MODULISP_INTERPRETER_H_
