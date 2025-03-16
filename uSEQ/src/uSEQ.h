#ifndef USEQ_H_
#define USEQ_H_

#define USEQ_FIRMWARE_VERSION "1.0.2"

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
#include <Arduino.h>
#include <vector>
#include <string>
#include <optional>

#define LISP_FUNC_ARGS_TYPE std::vector<Value>&, Environment&
#define LISP_FUNC_ARGS std::vector<Value>&args, Environment &env
#define LISP_FUNC_RETURN_TYPE Value
#define LISP_FUNC_TYPE LISP_FUNC_RETURN_TYPE(LISP_FUNC_ARGS_TYPE)
// For declaring builtin functions as class members
#define LISP_FUNC_DECL(__name__) LISP_FUNC_RETURN_TYPE __name__(LISP_FUNC_ARGS_TYPE);

// Forward Declarations
class Value;
class Environment;

// Constants - Moved from uSEQ.cpp for global access
namespace SerialMsg {
    constexpr byte message_begin_marker = 31;
    enum serial_message_types {
        STREAM,
        MIDI
    };
    constexpr unsigned long serial_message_rate_limit = 5000; // microseconds
    constexpr int execute_now_marker = '@';
}

// Define value types
using TimeValue  = double;
using PhaseValue = double;
typedef double SERIAL_OUTPUT_VALUE_TYPE;

// Constants - Moved from uSEQ.cpp for hardware definitions
#ifdef ARDUINO

//Hardware constants
constexpr int NUM_CONTINUOUS_OUTS = 6;
constexpr int NUM_BINARY_OUTS = 6;
constexpr int NUM_SERIAL_OUTS = 8;
constexpr int FLASH_SECTOR_SIZE = 4096;
constexpr int FLASH_PAGE_SIZE = 256; // 256 bytes, always

extern const int useq_output_pins;
extern const int useq_output_led_pins;

#endif

// Structure Definitions
struct scheduledItem {
    String id;
    double period;
    size_t lastRun;
    Value ast;
};

enum CLOCK_SOURCES {
    INTERNAL,
    EXTERNAL_I1,
    EXTERNAL_I2
};

struct extClockStruct {
    size_t div = 1;
    size_t count = 0;
    size_t beat_count = 0;
    size_t bar_count = 0;
};

class maxiFilter {
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
    // Statics
    static uSEQ* instance;

    // Constructor/Destructor
    uSEQ() = default;
    ~uSEQ() = default;

    // Core Functionality
    void run();
    void tick();

    // Initialization
    void init();
    void eval_lisp_library();

    // Loop
    void start_loop_blocking();

    // Time Management
    void update_time();
    void update_logical_time(TimeValue actual_time);
    void reset_logical_time();

    // BPM Management
    void set_bpm(double newBpm, double changeThreshold);

    // Input Management
    void update_inputs();

    // Output Management
    void update_signals();
    void update_outs();

    // Lisp Integration
    void update_lisp_time_variables();
    Value eval(String code);
    Value parse(String code);
    void set(String name, Value val);
    std::optional<Value> get(String name);
    void set_expr(String name, Value val);
    std::optional<Value> get_expr(String name);
    Environment make_env_for_time(TimeValue t_micros);
    Value eval_at_time(Value& expr, Environment& env, TimeValue time_micros);

    // API Functions
    Value ard_useqdw(std::vector<Value>& args, Environment& env);
    Value ard_useqaw(std::vector<Value>& args, Environment& env);
    Value useq_fast(std::vector<Value>& args, Environment& env);
    Value useq_slow(std::vector<Value>& args, Environment& env);
    Value useq_offset_time(std::vector<Value>& args, Environment& env);
    Value useq_schedule(std::vector<Value>& args, Environment& env);
    Value useq_unschedule(std::vector<Value>& args, Environment& env);
    Value useq_setbpm(std::vector<Value>& args, Environment& env);
    Value useq_get_input_bpm(std::vector<Value>& args, Environment& env);
    Value useq_set_time_sig(std::vector<Value>& args, Environment& env);
    Value useq_in1(std::vector<Value>& args, Environment& env);
    Value useq_in2(std::vector<Value>& args, Environment& env);
    Value useq_ain1(std::vector<Value>& args, Environment& env);
    Value useq_ain2(std::vector<Value>& args, Environment& env);

    Value useq_toggle_select(std::vector<Value>& args, Environment& env);
    Value useq_dm(std::vector<Value>& args, Environment& env);
    Value useq_gates(std::vector<Value>& args, Environment& env);
    Value useq_gatesw(std::vector<Value>& args, Environment& env);
    Value useq_trigs(std::vector<Value>& args, Environment& env);
    Value useq_euclidean(std::vector<Value>& args, Environment& env);
    Value useq_eu(std::vector<Value>& args, Environment& env);
    Value useq_fromList(std::vector<Value>& args, Environment& env);
    Value useq_seq(std::vector<Value>& args, Environment& env);
    Value useq_flatten(std::vector<Value>& args, Environment& env);
    Value useq_flatseq(std::vector<Value>& args, Environment& env);
    Value useq_fromFlattenedList(std::vector<Value>& args, Environment& env);
    Value useq_interpolate(std::vector<Value>& args, Environment& env);
    Value useq_step(std::vector<Value>& args, Environment& env);
    Value useq_ssin(std::vector<Value>& args, Environment& env);
    Value useq_swm(std::vector<Value>& args, Environment& env);
    Value useq_swt(std::vector<Value>& args, Environment& env);
    Value useq_swr(std::vector<Value>& args, Environment& env);
    Value useq_rot(std::vector<Value>& args, Environment& env);

    Value useq_fast(std::vector<Value>& args, Environment& env);
    Value useq_slow(std::vector<Value>& args, Environment& env);
    Value useq_offset_time(std::vector<Value>& args, Environment& env);
    Value useq_tri(std::vector<Value>& args, Environment& env);

    // Flash Storage
    void write_flash_info();
    void load_flash_info();
    void write_flash_env();
    void load_flash_env();
    void autoload_flash();
    void clear_all_outputs();
    void set_my_id(int num);
    void reset_flash_env_var_info();
    std::pair<size_t, size_t> num_bytes_def_strs() const;
    void copy_def_strings_to_buffer(char* buffer);
    bool flash_has_been_written_before();
    void erase_info_flash();

    // MIDI
    void update_midi_out();

    // Accessors
    CLOCK_SOURCES getClockSource() { return useq_clock_source; }
    void set_ext_clock_div(size_t val) { ext_clock_tracker.div = val; }
    void reset_ext_tracking() {
        ext_clock_tracker.beat_count = 0;
        ext_clock_tracker.bar_count = 0;
        tempoI1.reset();
    }

private:

    // Constants
    static constexpr const char* m_flash_stamp_str = "uSEQ";
    static constexpr uint m_flash_stamp_size_bytes = strlen(m_flash_stamp_str) + 1;
    Value default_continuous_expr = Value::number(0.5);
    Value default_binary_expr = Value::number(0.0);
    Value default_serial_expr = Value::nil();
    String exit_command = "@@exit";

    // Time Management
    TimeValue m_micros_raw = 0;
    TimeValue m_micros_raw_last = 0;
    TimeValue m_time_since_boot = 0;
    TimeValue m_last_known_time_since_boot = 0;
    TimeValue m_transport_time = 0;
    TimeValue m_last_transport_reset_time = 0;
    size_t m_overflow_counter = 0;
    double updateSpeed = 0;
    double ts = 0;
    double lastCQP = 0;

    // BPM Management
    double m_defaultBPM = 120.0;
    double m_bpm = m_defaultBPM;
    TimeValue m_beat_length = 0;
    TimeValue m_bar_length = 0;
    TimeValue m_phrase_length = 0;
    TimeValue m_section_length = 0;

    // Meter Management
    double meter_denominator = 4.0;
    double meter_numerator = 4.0;
    double m_bars_per_phrase = 4;
    double m_phrases_per_section = 4;

    // UI
    bool m_should_quit = false;
    bool m_manual_evaluation = false;
    String m_last_received_code = "";
    String m_atom_currently_being_evaluated = "";
    bool m_attempt_expr_eval_first = false;
    bool m_update_loop_evaluation = false;

    // Hardware
    bool m_initialised = false;

    // Scheduled Items
    std::vector<scheduledItem> m_scheduledItems;

    // Lisp
    Value m_q0AST;
    double m_last_CQP = 0;
    std::vector<Value> m_runQueue;

    // Output Signals
    int m_num_binary_outs = NUM_BINARY_OUTS;
    std::vector<Value> m_binary_ASTs;
    std::vector<int> m_binary_vals;

    int m_num_continuous_outs = NUM_CONTINUOUS_OUTS;
    std::vector<Value> m_continuous_ASTs;
    std::vector<double> m_continuous_vals;

    int m_num_serial_outs = NUM_SERIAL_OUTS;
    std::vector<Value> m_serial_ASTs;
    std::vector<std::optional<SERIAL_OUTPUT_VALUE_TYPE>> m_serial_vals;
    unsigned long serial_out_timestamp = 0;

    // Input Signals
    int m_num_serial_ins = 8; // Arbitrary limit for now
    std::vector<double> m_serial_input_streams = std::vector<double>(m_num_serial_ins, 0.0);
    std::vector<double> m_input_vals = std::vector<double>(16, 0.0);

    // Flash Storage
    int m_my_id = 0;
    size_t m_FLASH_ENV_SECTOR_SIZE = 0;
    size_t m_FLASH_ENV_DEFS_SIZE = 0;
    size_t m_FLASH_ENV_EXPRS_SIZE = 0;
    uintptr_t m_FLASH_ENV_SECTOR_OFFSET_START = 0;
    uintptr_t m_FLASH_ENV_SECTOR_OFFSET_END = 0;
    size_t m_FLASH_ENV_STRING_BUFFER_SIZE = 0;
    bool m_is_env_stored = false;

    // Structures
    CLOCK_SOURCES useq_clock_source = CLOCK_SOURCES::INTERNAL;
    extClockStruct ext_clock_tracker;

    // Lisp API
    void init_builtinfuncs();
    void update_signals();

    PhaseValue beat_at_time(TimeValue time);
    PhaseValue bar_at_time(TimeValue time);
    PhaseValue phrase_at_time(TimeValue time);
    PhaseValue section_at_time(TimeValue time);

    // IO
    void update_continuous_outs();
    void update_binary_outs();
    void update_serial_outs();
    void check_and_handle_user_input();
    void run_scheduled_items();
    void check_code_quant_phasor();
    void update_Q0();

    // Internal Functions
    void analog_write_with_led(int output, double val);
    void digital_write_with_led(int output, int val);
    void serial_write(int out, double val);
    void update_bpm_variables();
    void init_ASTs();
    void reboot();

    //Hardware functions
    void gpio_irq_gate1();
    void gpio_irq_gate2();
    void setup_IO();
    void setup_outs();
    void setup_continuous_outs();
    void setup_discrete_outs();
    void setup_digital_ins();
    void setup_switches();
#ifdef USEQHARDWARE_0_2
    void setup_rotary_encoder();
    void read_rotary_encoders();
#endif
    void led_animation();
    void update_clock_from_external(double ts);
    void set_input_val(size_t index, double value);
#ifdef ANALOG_INPUTS
    void setup_analog_ins();
#endif

    // MIDI
#ifdef MIDIOUT
    std::map<int, Value> useqMDOMap;
    LISP_FUNC_DECL(useq_mdo);
#endif

    // Variables
    Environment env;
    ValueMap m_defs;
    ValueMap m_def_exprs;

    tempoEstimator tempoI1, tempoI2;

    String current_output_being_processed;

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

    LISP_FUNC_DECL(useq_tri);

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