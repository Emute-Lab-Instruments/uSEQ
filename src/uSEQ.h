#ifndef USEQ_H_
#define USEQ_H_

// #include "lisp/environment.h"
#include "lisp/interpreter.h"
// #include "lisp/library.h"
#include "extern/tempoEstimator.h"
#include "lisp/value.h"
#include "uSEQ/configure.h"
#include "uSEQ/io.h"
#include <memory>

// TODO remove
#include <iostream>
// #include <sys/types.h>

// class Output;
// class Input;

// For declaring builtin functions as class members
#define LISP_FUNC_ARGS_TYPE std::vector<Value>&, Environment&
#define LISP_FUNC_ARGS std::vector<Value>&args, Environment &env
#define LISP_FUNC_RETURN_TYPE Value
#define LISP_FUNC_TYPE LISP_FUNC_RETURN_TYPE(LISP_FUNC_ARGS_TYPE)
#define LISP_FUNC_DECL(__name__) LISP_FUNC_RETURN_TYPE __name__(LISP_FUNC_ARGS_TYPE);

class uSEQ
{
public:
    // uSEQ(int num_continuous_ins, int num_binary_ins, int num_continuous_outs, int num_binary_outs)
    //     : m_num_continuous_outs(num_continuous_outs), m_num_binary_outs(num_binary_outs),
    //       m_num_continuous_ins(num_continuous_ins), m_num_binary_ins(num_binary_ins)
    // {
    //     m_toplevel_env = Environment();
    // }

    uSEQ() : m_interpreter(Interpreter()), m_io(IO())
    {
        // ExternalBuiltinInserter inserter = [this](BuiltinMap& map) { this->load_useq_builtindefs(map); };
        // m_interpreter.insert_external_builtins(inserter);
        init();
    }

    void init();
    void run();

    String eval(const String& code);

    void start_loop_blocking();
    void tick();
    void setTime(size_t);
    void set(String, Value);

    // TODO restore private
private:
    // NOTE this probably can't be static because
    // it needs to insert non-static methods that modify
    // object state (e.g. `a1`, `setbpm` etc)
    void load_useq_builtindefs(BuiltinMap& map);

    Interpreter m_interpreter;
    IO m_io;

    void check_and_handle_user_input();
    void update_inputs();
    void update_signals();
    void update_outputs();

    //     std::vector<Output> m_outputs;
    // std::vector<Input> m_inputs;

    // NOTE these may be useful later on for
    // dynamically adding/removing outputs
    // when running in virtual mode
    uint m_num_continuous_outs = NUM_CONTINUOUS_OUTS;
    uint m_num_binary_outs     = NUM_BINARY_OUTS;
    uint m_num_misc_outs       = NUM_MISC_OUTS;
    uint m_num_continuous_ins  = NUM_CONTINUOUS_INS;
    uint m_num_binary_ins      = NUM_BINARY_INS;
    uint m_num_misc_ins        = NUM_MISC_INS;

    // Flags
    bool m_initialised        = false;
    bool m_should_quit        = false;
    bool m_current_expr_sound = true;

    //// OUTPUTS
    // Output forms
    Value m_q0AST;

    std::vector<Value> m_continuous_ASTs;
    std::vector<double> m_continuous_vals;

    std::vector<Value> m_binary_ASTs;
    std::vector<int> m_binary_vals;

    std::vector<Value> m_misc_ASTs;
    std::vector<int> m_misc_vals;
    // // FIXME
    // // std::vector<double> m_serialInputStreams(MISC_INS, 0.0);

    // Timing
    double lastResetTime = 0.0;
    double time          = 0.0;
    double t             = 0.0; // time since last reset
    double last_t        = 0.0; // timestamp of the previous time update (since reset)
    double beat          = 0.0;
    double bar           = 0.0;
    double phrase        = 0.0;
    double section       = 0.0;

    // Meter
    double meter_numerator   = 4;
    double meter_denominator = 4;

    // TODO: better to have custom specified long phasors e.g. (addPhasor phasorName (lambda () (beats * 17)))
    // - to be stored in std::map<String, Double[2]>
    double barsPerPhrase     = 16;
    double phrasesPerSection = 16;

    // BPM
    double defaultBpm = 130.0;
    double bpm        = 130.0;
    double bps        = 0.0;

    // TODO: builtin functions to set phasor lengths and other timing
    // phasor lengths
    double beatDur    = 0.0;
    double barDur     = 0.0;
    double phraseDur  = 0.0;
    double sectionDur = 0.0;

    //// METHODS
    void execute_code(String code);
    void execute_code(String code, Environment& env);

    // UPDATE methods
    void update_time();
    void updateTimeVariables();
    void updateBpmVariables();
    void update_continuous_signals();
    void update_binary_signals();
    void update_continuous_outs();
    void update_binary_outs();
    void update_misc_outs();
    void updateQ0();

    // INIT
    void initASTs();
    // void init_inputs();
    // void init_outputs();

    void setBpm(double newBpm, double changeThreshold);
    double m_defaultBPM = 90;

    // Value default_continuous_form = m_parser.parse("(sine t)");
    // Value default_binary_form     = m_parser.parse("(square t)");
    // FIXME should be initialised somewhere?
    Value default_continuous_form = m_interpreter.parse("(sine t)");
    Value default_binary_form     = m_interpreter.parse("(square t)");
    ;

    String m_last_received_code = "";

    // update form methods
    // a
    //
    LISP_FUNC_DECL(a1);
    LISP_FUNC_DECL(a2);
    LISP_FUNC_DECL(a3);
    LISP_FUNC_DECL(a4);
    LISP_FUNC_DECL(a5);
    LISP_FUNC_DECL(a6);
    // d
    LISP_FUNC_DECL(d1);
    LISP_FUNC_DECL(d2);
    LISP_FUNC_DECL(d3);
    LISP_FUNC_DECL(d4);
    LISP_FUNC_DECL(d5);
    LISP_FUNC_DECL(d6);
    // s
    LISP_FUNC_DECL(s1);
    LISP_FUNC_DECL(s2);
    LISP_FUNC_DECL(s3);
    LISP_FUNC_DECL(s4);
    LISP_FUNC_DECL(s5);
    LISP_FUNC_DECL(s6);
    LISP_FUNC_DECL(s7);
    LISP_FUNC_DECL(s8);
    // Others
    LISP_FUNC_DECL(lisp_fast);
    LISP_FUNC_DECL(lisp_slow);
    LISP_FUNC_DECL(lisp_schedule);
    LISP_FUNC_DECL(lisp_unschedule);

    LISP_FUNC_DECL(useq_setbpm);
    LISP_FUNC_DECL(useq_getbpm);
    LISP_FUNC_DECL(useq_settimesig);

    LISP_FUNC_DECL(useq_in1);
    LISP_FUNC_DECL(useq_in2);
    LISP_FUNC_DECL(useq_ain1);
    LISP_FUNC_DECL(useq_ain2);

#ifdef MUSICTHING
    LISP_FUNC_DECL(useq_mt_knob);
    LISP_FUNC_DECL(useq_mt_knobx);
    LISP_FUNC_DECL(useq_mt_knoby);
    LISP_FUNC_DECL(useq_mt_swz);
#endif

    LISP_FUNC_DECL(useq_ssin);
    LISP_FUNC_DECL(useq_swm);
    LISP_FUNC_DECL(useq_swt);
    LISP_FUNC_DECL(useq_swr);
    LISP_FUNC_DECL(useq_rot);

    LISP_FUNC_DECL(useq_q0);

    LISP_FUNC_DECL(useq_dm);
    LISP_FUNC_DECL(useq_gates);
    LISP_FUNC_DECL(useq_gatesw);
    LISP_FUNC_DECL(useq_trigs);
    LISP_FUNC_DECL(useq_loopPhasor);
    LISP_FUNC_DECL(useq_euclidean);

    LISP_FUNC_DECL(useq_step);
    LISP_FUNC_DECL(useq_fromList);
    LISP_FUNC_DECL(useq_fromFlattenedList);
    LISP_FUNC_DECL(useq_flatten);
    LISP_FUNC_DECL(useq_interpolate);

    struct scheduledItem
    {
        //    Value statement;
        std::vector<Value> ast;
        size_t period;
        size_t lastRun;
        String id;
    };

    // TODO confirm
    std::vector<scheduledItem> m_scheduled_items;
    double m_input_vals[14];
    // NOTE this was a std vector before, init with 0
    double m_serialInputStreams[NUM_MISC_INS];
    // std::vector<double> m_input_vals;
    tempoEstimator tempoI1, tempoI2;

    void setTimeSignature(double, double);

    // std::vector<BuiltinFunc> m_builtinfuncs;
    void insert_own_builtinfuncs();
    BuiltinMap m_builtins;
};

#endif // USEQ_H_};};
