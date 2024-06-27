#ifndef USEQ_H_
#define USEQ_H_

#define USEQ_FIRMWARE_VERSION "1.0"
#define USEQ_FIRMWARE_RELEASE_DATE "2024-06-27"

#include "dsp/tempoEstimator.h"
// #include "dsp/MAFilter.h"
#include "dsp/MedianFilter.h"
#include "lisp/interpreter.h"
#include "lisp/macros.h"
#include "lisp/value.h"
#include "uSEQ/configure.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <sys/types.h>
// #include "utils/serial_message.h"

#define LISP_FUNC_ARGS_TYPE std::vector<Value>&, Environment&
#define LISP_FUNC_ARGS std::vector<Value>&args, Environment &env
#define LISP_FUNC_RETURN_TYPE Value
#define LISP_FUNC_TYPE LISP_FUNC_RETURN_TYPE(LISP_FUNC_ARGS_TYPE)
// For declaring builtin functions as class members
#define LISP_FUNC_DECL(__name__) LISP_FUNC_RETURN_TYPE __name__(LISP_FUNC_ARGS_TYPE);

using TimeValue  = double;
using PhaseValue = double;

class maxiFilter
{
private:
    double z      = 0;
    double output = 0;

public:
    maxiFilter() {}
    double lopass(double input, double cutoff);
};

class uSEQ : public Interpreter
{
public:
    uSEQ() {}

    void init();
    void run();

    void start_loop_blocking();
    void tick();
    void update_logical_time_variables(TimeValue);

    // NOTE: this should probably be considered
    // part of the interpreter instead
    Value eval_at_time(Value&, Environment&, double);

    void write_flash_env();
    void load_flash_env();

    static void gpio_irq_gate1();
    static void gpio_irq_gate2();
    tempoEstimator tempoI1, tempoI2;
    void update_clock_from_external(double ts);

    double delme = 928.22234;

    static uSEQ* instance;
    void set_input_val(size_t index, double value);

    enum CLOCK_SOURCES
    {
        INTERNAL = 0,
        EXTERNAL_I1,
        EXTERNAL_I2
    };

    uSEQ::CLOCK_SOURCES getClockSource() { return useq_clock_source; }

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
    std::vector<std::optional<SERIAL_OUTPUT_VALUE_TYPE>> m_serial_vals;

    double m_input_vals[14];
    // NOTE this was a std vector before, init with 0
    double m_serial_input_streams[NUM_SERIAL_INS];

    // Timing (NOTE: in micros)
    // actual time that module has been running for
    u_int8_t m_overflow_counter            = 0;
    size_t m_micros_raw                    = 0;
    size_t m_micros_raw_last               = 0.0;
    TimeValue m_time_since_boot            = 0.0;
    TimeValue m_last_known_time_since_boot = -1;
    // time /of/ last "transport" reset by user
    TimeValue m_last_transport_reset_time = 0.0;
    // time /since/ last "transport" reset by user
    TimeValue m_transport_time = 0.0;
    // last known transport time
    TimeValue m_last_transport_time = 0.0;

    // Durations (NOTE: in micros)
    TimeValue m_beat_length    = 0.0;
    TimeValue m_bar_length     = 0.0;
    TimeValue m_phrase_length  = 0.0;
    TimeValue m_section_length = 0.0;
    // Normalised phasors
    PhaseValue m_beat_phase    = 0.0;
    PhaseValue m_bar_phase     = 0.0;
    PhaseValue m_phrase_phase  = 0.0;
    PhaseValue m_section_phase = 0.0;

    PhaseValue beat_at_time(TimeValue);
    PhaseValue bar_at_time(TimeValue);
    PhaseValue phrase_at_time(TimeValue);
    PhaseValue section_at_time(TimeValue);

    // Meter
    double meter_numerator   = 4;
    double meter_denominator = 4;

    // TODO: better to have custom specified long phasors e.g. (addPhasor phasorName
    // (lambda () (beats * 17)))
    // - to be stored in std::map<String, Double[2]>
    double m_bars_per_phrase     = 16;
    double m_phrases_per_section = 16;

    // BPM
    double m_defaultBPM = 130;
    double m_bpm        = m_defaultBPM;
    void set_bpm(double newBpm, double changeThreshold);
    void update_bpm_variables();

    //// UPDATE methods
    // main user interaction logic
    void check_and_handle_user_input();
    void update_inputs();
    // timing-related stuff
    void update_time();
    void reset_logical_time();
    void update_logical_time(TimeValue);
    void update_lisp_time_variables();

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

    CLOCK_SOURCES useq_clock_source = CLOCK_SOURCES::INTERNAL;
    struct ext_clock_tracking
    {
        size_t beat_count = 0;
        size_t bar_count  = 0;
        size_t count      = 0;
        size_t div        = 1;
    } ext_clock_tracker;

    void reset_ext_tracking()
    {
        ext_clock_tracker.beat_count = ext_clock_tracker.bar_count =
            ext_clock_tracker.count  = 0;
    }

    void set_ext_clock_div(size_t val)
    {
        ext_clock_tracker.div = val;
        reset_ext_tracking();
    }

    unsigned long serial_out_timestamp = 0;

    Value default_continuous_expr = Value::nil();
    Value default_binary_expr     = Value::nil();
    Value default_serial_expr     = Value::nil();

    String m_last_received_code = "";

    LISP_FUNC_DECL(useq_eval_at_time);
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

    // Manipulating time
    LISP_FUNC_DECL(useq_slow);
    LISP_FUNC_DECL(useq_fast);
    LISP_FUNC_DECL(useq_offset_time);

    LISP_FUNC_DECL(useq_schedule);
    LISP_FUNC_DECL(useq_unschedule);

    LISP_FUNC_DECL(useq_setbpm);
    LISP_FUNC_DECL(useq_get_input_bpm);
    LISP_FUNC_DECL(useq_set_time_sig);

    LISP_FUNC_DECL(useq_in1);
    LISP_FUNC_DECL(useq_in2);
    LISP_FUNC_DECL(useq_ain1);
    LISP_FUNC_DECL(useq_ain2);

    LISP_FUNC_DECL(useq_set_clock_internal);
    LISP_FUNC_DECL(useq_set_clock_external);
    LISP_FUNC_DECL(useq_get_clock_source);
    LISP_FUNC_DECL(useq_reset_internal_clock);
    LISP_FUNC_DECL(useq_reset_external_clock_tracking);

#ifdef MUSICTHING
    LISP_FUNC_DECL(useq_mt_knob);
    LISP_FUNC_DECL(useq_mt_knobx);
    LISP_FUNC_DECL(useq_mt_knoby);
    LISP_FUNC_DECL(useq_mt_swz);
#endif

    LISP_FUNC_DECL(useq_ssin);
    LISP_FUNC_DECL(useq_swm);
    LISP_FUNC_DECL(useq_swt);
    LISP_FUNC_DECL(useq_toggle_select);
    LISP_FUNC_DECL(useq_swr);
    LISP_FUNC_DECL(useq_rot);

    LISP_FUNC_DECL(useq_q0);

    LISP_FUNC_DECL(useq_dm);
    LISP_FUNC_DECL(useq_gates);
    LISP_FUNC_DECL(useq_gatesw);
    LISP_FUNC_DECL(useq_trigs);
    LISP_FUNC_DECL(useq_loopPhasor);
    LISP_FUNC_DECL(useq_euclidean);
    LISP_FUNC_DECL(useq_eu);

    LISP_FUNC_DECL(useq_flatten);
    LISP_FUNC_DECL(useq_step);
    LISP_FUNC_DECL(useq_fromList);
    LISP_FUNC_DECL(useq_fromFlattenedList);
    LISP_FUNC_DECL(useq_seq);
    LISP_FUNC_DECL(useq_flatseq);
    LISP_FUNC_DECL(useq_interpolate);

    LISP_FUNC_DECL(ard_useqaw);
    LISP_FUNC_DECL(ard_useqdw);

    LISP_FUNC_DECL(useq_load_flash_info);
    LISP_FUNC_DECL(useq_write_flash_info);
    LISP_FUNC_DECL(useq_reboot);
    LISP_FUNC_DECL(useq_set_my_id);
    LISP_FUNC_DECL(useq_get_my_id);
    // LISP_FUNC_DECL(useq_test_flash);

    LISP_FUNC_DECL(useq_memory_save);
    LISP_FUNC_DECL(useq_memory_restore);
    LISP_FUNC_DECL(useq_memory_erase);

    LISP_FUNC_DECL(useq_load_flash_env);
    LISP_FUNC_DECL(useq_write_flash_env);
    LISP_FUNC_DECL(useq_autoload_flash);

    LISP_FUNC_DECL(useq_stop_all);
    LISP_FUNC_DECL(useq_rewind_logical_time);

    LISP_FUNC_DECL(useq_firmware_info);
    LISP_FUNC_DECL(useq_report_firmware_info);

    void clear_all_outputs();
    void erase_info_flash();

    void set_my_id(int num);

    void analog_write_with_led(int output, CONTINUOUS_OUTPUT_VALUE_TYPE val);
    void digital_write_with_led(int output, BINARY_OUTPUT_VALUE_TYPE val);
    void serial_write(int out, SERIAL_OUTPUT_VALUE_TYPE val);

    void set_time_sig(double, double);

    std::vector<Value> m_runQueue;
    Value m_cqpAST = parse("bar");

    struct scheduledItem
    {
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
    int ts          = 0;
    int updateSpeed = 0;

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
    static constexpr u_int8_t m_serial_stream_begin_marker = 31;
    static constexpr char m_execute_now_marker             = '@';

    Environment make_env_for_time(TimeValue);

    void load_flash_info();
    void write_flash_info();
    void reset_flash_env_var_info();

    int m_my_id          = -1;
    bool m_is_env_stored = false;

    uintptr_t m_FLASH_ENV_SECTOR_SIZE         = 0;
    uintptr_t m_FLASH_ENV_DEFS_SIZE           = 0;
    uintptr_t m_FLASH_ENV_EXPRS_SIZE          = 0;
    uintptr_t m_FLASH_ENV_SECTOR_OFFSET_START = 0;
    uintptr_t m_FLASH_ENV_SECTOR_OFFSET_END   = 0;
    uintptr_t m_FLASH_ENV_STRING_BUFFER_SIZE  = 0;

    void reboot();

    std::pair<size_t, size_t> num_bytes_def_strs() const;
    void copy_def_strings_to_buffer(char*);

    static constexpr const char* m_flash_stamp_str = "uSEQ";
    static constexpr uint m_flash_stamp_size_bytes = strlen(m_flash_stamp_str) + 1;

    bool flash_has_been_written_before();
    void autoload_flash();

    // void clear_non_program_flash();
    static String current_output_being_processed;
};

#endif // USEQ_H_
