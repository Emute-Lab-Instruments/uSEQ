#ifndef USEQ_H_
#define USEQ_H_

#include "dsp/tempoEstimator.h"
#include "lisp/interpreter.h"
#include "lisp/macros.h"
#include "lisp/value.h"
#include "uSEQ/configure.h"
#include <memory>

#define LISP_FUNC_ARGS_TYPE std::vector<Value>&, Environment&
#define LISP_FUNC_ARGS std::vector<Value>&args, Environment &env
#define LISP_FUNC_RETURN_TYPE Value
#define LISP_FUNC_TYPE LISP_FUNC_RETURN_TYPE(LISP_FUNC_ARGS_TYPE)
// For declaring builtin functions as class members
#define LISP_FUNC_DECL(__name__) LISP_FUNC_RETURN_TYPE __name__(LISP_FUNC_ARGS_TYPE);

class uSEQ : public Interpreter
{
public:
    // uSEQ(int num_continuous_ins, int num_binary_ins, int num_continuous_outs, int
    // num_binary_outs)
    //     : m_num_continuous_outs(num_continuous_outs),
    //     m_num_binary_outs(num_binary_outs),
    //       m_num_continuous_ins(num_continuous_ins),
    //       m_num_binary_ins(num_binary_ins)
    // {
    //     m_toplevel_env = Environment();
    // }

    uSEQ() {}

    void init();
    void run();

    // String eval(const String& code);

    void start_loop_blocking();
    void tick();
    void set_time(size_t);
    // void set(String, Value);

    // TODO restore private
private:
    // IO m_io;

    //     std::vector<Output> m_outputs;
    // std::vector<Input> m_inputs;

    // NOTE these may be useful later on for
    // dynamically adding/removing outputs
    // when running in virtual mode
    uint m_num_continuous_outs = NUM_CONTINUOUS_OUTS;
    uint m_num_binary_outs     = NUM_BINARY_OUTS;
    uint m_num_serial_outs     = NUM_SERIAL_OUTS;
    // uint m_num_continuous_ins  = NUM_CONTINUOUS_INS;
    // uint m_num_binary_ins      = NUM_BINARY_INS;
    uint m_num_serial_ins = NUM_SERIAL_INS;

    // Flags
    bool m_initialised        = false;
    bool m_should_quit        = false;
    bool m_current_expr_sound = true;

    //// OUTPUTS
    // Output forms
    Value m_q0AST;

    std::vector<Value> m_continuous_ASTs;
    std::vector<SERIAL_OUTPUT_VALUE_TYPE> m_continuous_vals;

    std::vector<Value> m_binary_ASTs;
    std::vector<BINARY_INPUT_VALUE_TYPE> m_binary_vals;

    std::vector<Value> m_serial_ASTs;
    std::vector<SERIAL_OUTPUT_VALUE_TYPE> m_serial_vals;
    // // std::vector<double> m_serialInputStreams(MISC_INS, 0.0);

    // Timing
    size_t time          = 0;
    size_t lastResetTime = 0; // time /of/ last "transport" reset by user
    size_t t             = 0; // time /since/ last "transport" reset by user
    size_t last_t        = 0; // timestamp of the previous time update (since reset)
    // Phasors (will be normalised before inserted in Lisp env)
    size_t m_beat_phase    = 0;
    size_t m_bar_phase     = 0;
    size_t m_phrase_phase  = 0;
    size_t m_section_phase = 0;
    // Durations (in micros)
    size_t m_beat_length    = 0.0;
    size_t m_bar_length     = 0.0;
    size_t m_phrase_length  = 0.0;
    size_t m_section_length = 0.0;

    // Meter
    double meter_numerator   = 4;
    double meter_denominator = 4;

    // TODO: better to have custom specified long phasors e.g. (addPhasor phasorName
    // (lambda () (beats * 17)))
    // - to be stored in std::map<String, Double[2]>
    double m_barsPerPhrase     = 16;
    double m_phrasesPerSection = 16;

    // BPM
    void set_bpm(double newBpm, double changeThreshold);
    double m_defaultBPM = 90;
    double m_bpm        = 130.0;

    //// UPDATE methods
    // main user interaction logic
    void check_and_handle_user_input();
    void update_inputs();
    // timing-related stuff
    void update_time();
    void update_lisp_time_variables();
    void update_bpm_variables();
    // updating the (cached) outputs of stored forms
    void update_signals();
    void update_continuous_signals();
    void update_binary_signals();
    void update_serial_signals();
    // updating (i.e. writing to) the actual outputs
    void update_outs();
    void update_continuous_outs();
    void update_binary_outs();
    void update_serial_outs();
    void update_Q0();

    Value default_continuous_expr = parse("(sine t)");
    Value default_binary_expr     = parse("(square bar)");
    Value default_serial_expr     = parse("(sine t)");

    String m_last_received_code = "";

    // expr-updating methods
    // a
    LISP_FUNC_DECL(useq_a1);
    LISP_FUNC_DECL(useq_a2);
    LISP_FUNC_DECL(useq_a3);
    LISP_FUNC_DECL(useq_a4);
    LISP_FUNC_DECL(useq_a5);
    LISP_FUNC_DECL(useq_a6);
    // d
    LISP_FUNC_DECL(useq_d1);
    LISP_FUNC_DECL(useq_d2);
    LISP_FUNC_DECL(useq_d3);
    LISP_FUNC_DECL(useq_d4);
    LISP_FUNC_DECL(useq_d5);
    LISP_FUNC_DECL(useq_d6);
    // s
    LISP_FUNC_DECL(useq_s1);
    LISP_FUNC_DECL(useq_s2);
    LISP_FUNC_DECL(useq_s3);
    LISP_FUNC_DECL(useq_s4);
    LISP_FUNC_DECL(useq_s5);
    LISP_FUNC_DECL(useq_s6);
    LISP_FUNC_DECL(useq_s7);
    LISP_FUNC_DECL(useq_s8);
    // Others
    LISP_FUNC_DECL(useq_fast);
    LISP_FUNC_DECL(useq_slow);
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

    LISP_FUNC_DECL(ard_useqaw);
    LISP_FUNC_DECL(ard_useqdw);

    void analog_write_with_led(int output, CONTINUOUS_OUTPUT_VALUE_TYPE val);
    void digital_write_with_led(int output, BINARY_OUTPUT_VALUE_TYPE val);
    void serial_write(int out, SERIAL_OUTPUT_VALUE_TYPE val);

    // // TODO confirm
    // std::vector<scheduledItem> m_scheduled_items;
    double m_input_vals[14];
    // NOTE this was a std vector before, init with 0
    double m_serial_input_streams[NUM_SERIAL_INS];

    tempoEstimator tempoI1, tempoI2;

    void set_time_signature(double, double);

    std::vector<Value> m_runQueue;
    Value m_cqpAST = parse("bar");

    struct scheduledItem
    {
        //    Value statement;
        std::vector<Value> ast;
        size_t period;
        size_t lastRun;
        String id;
    };

    std::vector<scheduledItem> m_scheduledItems;
    // SCHEDULED ITEMS
    void run_scheduled_items();

    void check_code_quant_phasor();

    double m_last_CQP;

#ifdef MIDIOUT
    void update_midi_out();
    std::map<int, Value> useqMDOMap;

    LISP_FUNC_DECL(useq_mdo);
#endif

    // INIT
    void init_ASTs();
    void init_builtinfuncs();

    // SETUP
    void setup_IO();
    void setup_outs();
    void setup_continuous_outs();
    void setup_discrete_outs();
    void setup_switches();

#ifdef USEQHARDWARE_0_2
    void setup_rotary_encoder();
    void read_rotary_encoders();
#endif

#ifdef ANALOG_INPUTS
    void setup_analog_ins();
#endif
    void eval_lisp_library();
    void setup_digital_ins();
    void led_animation();

    int m_num_tick_starts = 0;
    int m_num_tick_ends   = 0;

    static constexpr u_int8_t m_serial_stream_begin_marker = 31;
    static constexpr char m_execute_now_marker             = '@';
};

#endif // USEQ_H_
