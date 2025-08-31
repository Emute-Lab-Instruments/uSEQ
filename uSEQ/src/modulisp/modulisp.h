#ifndef MODULISP_H_
#define MODULISP_H_

#include "lisp/interpreter.h"
#include "../ports/IClock.h"
#include "../ports/ILogger.h"

#define LISP_FUNC_ARGS_TYPE std::vector<Value> &, Environment &
#define LISP_FUNC_ARGS std::vector<Value> &args, Environment &env
#define LISP_FUNC_RETURN_TYPE Value
#define LISP_FUNC_TYPE LISP_FUNC_RETURN_TYPE(LISP_FUNC_ARGS_TYPE)
// For declaring builtin functions as class members
#define LISP_FUNC_DECL(__name__) LISP_FUNC_RETURN_TYPE __name__(LISP_FUNC_ARGS)

using TimeValue = double;
using PhaseValue = double;

class ModuLispInterpreter : public Interpreter {
  public:
    // Allow DI of time and logging for deterministic testing
    explicit ModuLispInterpreter(IClock* clk = nullptr, ILogger* log = nullptr)
        : clock(clk), logger(log) {}

    // FIXME decide what should be public
    void update_logical_time_variables(TimeValue);

    void init_builtinfuncs();

    // NOTE: this should probably be considered
    // part of the interpreter instead
    Value eval_at_time(Value &, Environment &, double);

    // TIMING

    uint8_t m_overflow_counter = 0;
    size_t m_micros_raw = 0;
    size_t m_micros_raw_last = 0.0;
    TimeValue m_time_since_boot = 0.0;
    TimeValue m_last_known_time_since_boot = -1;
    // time /of/ last "transport" reset by user
    TimeValue m_last_transport_reset_time = 0.0;
    // time /since/ last "transport" reset by user
    TimeValue m_transport_time = 0.0;
    TimeValue m_transport_time_offset = 0.0;
    // last known transport time
    TimeValue m_last_transport_time = 0.0;

    // Durations (NOTE: in micros)
    TimeValue m_beat_length = 0.0;
    TimeValue m_bar_length = 0.0;
    TimeValue m_phrase_length = 0.0;
    TimeValue m_section_length = 0.0;
    uint32_t m_current_beat_num = 0;
    uint32_t m_current_bar_num = 0;
    // Normalised phasors
    PhaseValue m_beat_phase = 0.0;
    PhaseValue m_bar_phase = 0.0;
    PhaseValue m_phrase_phase = 0.0;
    PhaseValue m_section_phase = 0.0;

    PhaseValue beat_at_time(TimeValue);
    uint32_t beat_num_at_time(TimeValue);
    PhaseValue bar_at_time(TimeValue);
    uint32_t bar_num_at_time(TimeValue);
    PhaseValue phrase_at_time(TimeValue);
    PhaseValue section_at_time(TimeValue);

    double simple_hashing_function(uint32_t);

    // Meter
    double meter_numerator = 4;
    double meter_denominator = 4;

    // TODO: better to have custom specified long phasors e.g. (addPhasor
    // phasorName (lambda () (beats * 17)))
    // - to be stored in std::map<String, Double[2]>
    double m_bars_per_phrase = 16;
    double m_phrases_per_section = 16;

    // BPM
    double m_defaultBPM = 130;
    double m_bpm = m_defaultBPM;
    void set_bpm(double newBpm, double changeThreshold);
    void update_bpm_variables();

    void update_time();
    void reset_logical_time();
    void update_logical_time(TimeValue);
    void update_lisp_time_variables();

    LISP_FUNC_DECL(useq_eval_at_time);

    // time nudging
    LISP_FUNC_DECL(useq_nudge_time);
    LISP_FUNC_DECL(useq_set_time_offset);

    // Manipulating time
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

    Environment make_env_for_time(TimeValue);

    Environment make_env_with_updated_time_durs(const Environment &, TimeValue);

    void set_time_sig(double, double);

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

    uint32_t m_random_seed = 0x9E3779B9;


    std::vector<Value> m_runQueue;
    Value m_cqpAST = parse("bar");

    struct scheduledItem {
        //    Value statement;
        Value ast;
        size_t period;
        size_t lastRun;
        String id;
    };

    std::vector<scheduledItem> m_scheduledItems;
    // SCHEDULED ITEMS
    void run_scheduled_items();

    void check_code_quant_phasor();

    double m_last_CQP;

    // performance
    int ts = 0;
    int updateSpeed = 0;


    void update_Q0();

    LISP_FUNC_DECL(useq_q0);

    Value m_q0AST;

  protected:
    // Optional injected adapters (nullptr means use legacy global functions)
    IClock* clock = nullptr;
    ILogger* logger = nullptr;
};

#endif // MODULISP_H_
