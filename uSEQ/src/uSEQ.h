#ifndef USEQ_H_
#define USEQ_H_

// Include compiler configuration for warning management
#include "utils/compiler_config.h"

// Only suppress external library warnings for hardware includes
#ifdef ARDUINO
USEQ_SUPPRESS_EXTERNAL_WARNINGS_PUSH
#endif

#define USEQ_FIRMWARE_VERSION "1.2.0"

#include "dsp/tempoEstimator.h"
// #include "dsp/MAFilter.h"
#include "dsp/MedianFilter.h"
#ifndef ARDUINO
#include "hardware_includes.h"
#endif
#include "modulisp/lisp/macros.h"
#include "modulisp/lisp/value.h"
#include "modulisp/modulisp.h"
#include "uSEQ/board.h"
#include "uSEQ/configure.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <sys/types.h>

#ifdef ARDUINO
#include "dsp/dsp-engine.hpp"
#endif

#ifndef ARDUINO
#define NUM_CONTINUOUS_OUTS 3
#define NUM_BINARY_OUTS (6 - NUM_CONTINUOUS_OUTS)
#endif

// Forward declaration
class OutputManager;

class maxiFilter {
  private:
    double z = 0;
    double output = 0;

  public:
    maxiFilter() {}
#ifdef ARDUINO
    double __force_inline lopass(double input, double cutoff);
#else
    double lopass(double input, double cutoff);
#endif
};

class uSEQ : public ModuLispInterpreter {
  public:
    uSEQ() : m_output_manager(nullptr) {}
    ~uSEQ(); // Custom destructor to manage OutputManager lifecycle

#ifdef ARDUINO
    void init_dsp_queues();
    void initDSP();
    void tick_dsp();
#endif
    void init();
    void init_builtinfuncs();  // Override to add hardware-specific functions
    void run();

    void start_loop_blocking();
    void tick();

#ifdef ARDUINO
    void write_flash_env();
    void load_flash_env();
#endif

    static void gpio_irq_gate1();
    static void gpio_irq_gate2();
    tempoEstimator tempoI1, tempoI2;
    void update_clock_from_external(double ts);

    double delme = 928.22234;

    static uSEQ *instance;
    void set_input_val(size_t index, double value);

    enum CLOCK_SOURCES { INTERNAL = 0, EXTERNAL_I1, EXTERNAL_I2 };

    uSEQ::CLOCK_SOURCES getClockSource() { return useq_clock_source; }
    
    // Output management system
    OutputManager& get_output_manager() { return *m_output_manager; }
    
    // Output AST arrays - made public for OutputManager access
    std::vector<Value> m_continuous_ASTs;
    std::vector<SERIAL_OUTPUT_VALUE_TYPE> m_continuous_vals;

    std::vector<Value> m_binary_ASTs;
    std::vector<BINARY_INPUT_VALUE_TYPE> m_binary_vals;

    std::vector<Value> m_serial_ASTs;
    std::vector<std::optional<SERIAL_OUTPUT_VALUE_TYPE>> m_serial_vals;

  private:
    // IO m_io;

    //     std::vector<Output> m_outputs;
    // std::vector<Input> m_inputs;

    // NOTE these may be useful later on for
    // dynamically adding/removing outputs
    // when running in virtual mode
    uint m_num_continuous_outs = NUM_CONTINUOUS_OUTS;
    uint m_num_binary_outs = NUM_BINARY_OUTS;
    uint m_num_serial_outs = NUM_SERIAL_OUTS;
    // uint m_num_continuous_ins  = NUM_CONTINUOUS_INS;
    // uint m_num_binary_ins      = NUM_BINARY_INS;
    uint m_num_serial_ins = NUM_SERIAL_INS;

    // Flags
    bool m_initialised = false;
    bool m_should_quit = false;
    bool m_current_expr_sound = true;
    bool m_waiting_for_sync_trigger = false;

    // Output management (moved to public section)
    OutputManager* m_output_manager;

    double m_input_vals[14];
    // NOTE this was a std vector before, init with 0
    double m_serial_input_streams[NUM_SERIAL_INS];

    // Timing (NOTE: in micros)
    // actual time that module has been running for

#ifdef ARDUINO
    // queue and dsp
    std::array<String, N_OUTPUT_QUEUES> dsp_output_names;
    void check_dsp_output_queues();
#endif

    //// UPDATE methods
    // main user interaction logic
    void check_and_handle_user_input();
    void update_inputs();
    // timing-related stuff

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

    CLOCK_SOURCES useq_clock_source = CLOCK_SOURCES::INTERNAL;
    struct ext_clock_tracking {
        size_t beat_count = 0;
        size_t bar_count = 0;
        size_t count = 0;
        size_t div = 1;
    } ext_clock_tracker;

    void reset_ext_tracking() {
        ext_clock_tracker.beat_count = ext_clock_tracker.bar_count =
            ext_clock_tracker.count = 0;
    }

    void set_ext_clock_div(size_t val) {
        ext_clock_tracker.div = val;
        reset_ext_tracking();
    }

    unsigned long serial_out_timestamp = 0;

    Value default_continuous_expr = Value::nil();
    Value default_binary_expr = Value::nil();
    Value default_serial_expr = Value::nil();

    String m_last_received_code = "";

    // expr-updating methods
    // a
    LISP_FUNC_DECL(useq_a1);
    LISP_FUNC_DECL(useq_a2);
    LISP_FUNC_DECL(useq_a3);
    LISP_FUNC_DECL(useq_a4);
    LISP_FUNC_DECL(useq_a5);
    LISP_FUNC_DECL(useq_a6);
    LISP_FUNC_DECL(useq_a7);
    LISP_FUNC_DECL(useq_a8);

    // d
    LISP_FUNC_DECL(useq_d1);
    LISP_FUNC_DECL(useq_d2);
    LISP_FUNC_DECL(useq_d3);
    LISP_FUNC_DECL(useq_d4);
    LISP_FUNC_DECL(useq_d5);
    LISP_FUNC_DECL(useq_d6);
    LISP_FUNC_DECL(useq_d7);
    LISP_FUNC_DECL(useq_d8);
    // s
    LISP_FUNC_DECL(useq_s1);
    LISP_FUNC_DECL(useq_s2);
    LISP_FUNC_DECL(useq_s3);
    LISP_FUNC_DECL(useq_s4);
    LISP_FUNC_DECL(useq_s5);
    LISP_FUNC_DECL(useq_s6);
    LISP_FUNC_DECL(useq_s7);
    LISP_FUNC_DECL(useq_s8);

    // echoed output values
    LISP_FUNC_DECL(useq_get_a1);
    LISP_FUNC_DECL(useq_get_a2);
    LISP_FUNC_DECL(useq_get_a3);
    LISP_FUNC_DECL(useq_get_a4);
    LISP_FUNC_DECL(useq_get_a5);
    LISP_FUNC_DECL(useq_get_a6);
    LISP_FUNC_DECL(useq_get_a7);
    LISP_FUNC_DECL(useq_get_a8);

    LISP_FUNC_DECL(useq_get_d1);
    LISP_FUNC_DECL(useq_get_d2);
    LISP_FUNC_DECL(useq_get_d3);
    LISP_FUNC_DECL(useq_get_d4);
    LISP_FUNC_DECL(useq_get_d5);
    LISP_FUNC_DECL(useq_get_d6);
    LISP_FUNC_DECL(useq_get_d7);
    LISP_FUNC_DECL(useq_get_d8);

    // LISP_FUNC_DECL(useq_get_s1);
    // LISP_FUNC_DECL(useq_get_s2);
    // LISP_FUNC_DECL(useq_get_s3);
    // LISP_FUNC_DECL(useq_get_s4);
    // LISP_FUNC_DECL(useq_get_s5);
    // LISP_FUNC_DECL(useq_get_s6);
    // LISP_FUNC_DECL(useq_get_s7);
    // LISP_FUNC_DECL(useq_get_s8);

    LISP_FUNC_DECL(useq_get_input_bpm);

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

    LISP_FUNC_DECL(useq_q0);

#ifdef ARDUINO
    LISP_FUNC_DECL(ard_useqaw);
    LISP_FUNC_DECL(ard_useqdw);
#else
    // Desktop build versions
    LISP_FUNC_DECL(ard_useqaw);
    LISP_FUNC_DECL(ard_useqdw);
#endif

#ifdef ARDUINO
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
#endif

    LISP_FUNC_DECL(useq_stop_all);

    LISP_FUNC_DECL(useq_firmware_info);
    LISP_FUNC_DECL(useq_report_firmware_info);

    // i2c
    LISP_FUNC_DECL(useq_i2c_host_start);
    LISP_FUNC_DECL(useq_i2c_send_to);

#ifdef ARDUINO
    LISP_FUNC_DECL(useq_enter_bootloader_mode);
#endif

    // SYNCING FUNCTIONS
#ifdef ARDUINO
    LISP_FUNC_DECL(useq_enter_sync_mode);
    LISP_FUNC_DECL(useq_send_sync_trigger);
#endif

    void clear_all_outputs();
#ifdef ARDUINO
    void erase_info_flash();
#endif

    void set_my_id(int num);

    void analog_write_with_led(int output, CONTINUOUS_OUTPUT_VALUE_TYPE val);
    void digital_write_with_led(int output, BINARY_OUTPUT_VALUE_TYPE val);
    void serial_write(int out, SERIAL_OUTPUT_VALUE_TYPE val);

#ifdef MIDIOUT
    void update_midi_out();
    std::map<int, Value> useqMDOMap;

    LISP_FUNC_DECL(useq_mdo);
#endif

    // INIT
    void init_ASTs();

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
    static constexpr char m_execute_now_marker = '@';

#ifdef ARDUINO
    void load_flash_info();
    void write_flash_info();
    void reset_flash_env_var_info();

    int m_my_id = -1;
    bool m_is_env_stored = false;

    uintptr_t m_FLASH_ENV_SECTOR_SIZE = 0;
    uintptr_t m_FLASH_ENV_DEFS_SIZE = 0;
    uintptr_t m_FLASH_ENV_EXPRS_SIZE = 0;
    uintptr_t m_FLASH_ENV_SECTOR_OFFSET_START = 0;
    uintptr_t m_FLASH_ENV_SECTOR_OFFSET_END = 0;
    uintptr_t m_FLASH_ENV_STRING_BUFFER_SIZE = 0;

    void reboot();

    std::pair<size_t, size_t> num_bytes_def_strs() const;
    void copy_def_strings_to_buffer(char *);

    static constexpr const char *m_flash_stamp_str = "uSEQ";
    static constexpr uint m_flash_stamp_size_bytes =
        strlen(m_flash_stamp_str) + 1;

    bool flash_has_been_written_before();
    void autoload_flash();
#endif

    // void clear_non_program_flash();
    static String current_output_being_processed;

#ifdef ARDUINO
    // DSP ENGINE

    struct ugenOutputQueue {
        queue_t *q;
        size_t index;
        size_t queueSize;
        size_t key;
        std::vector<float> lastValue = {0.f}; // TODO: expand for list outputs
    };

    struct ugenInputQueue {
        queue_t *q;
        size_t index;
        size_t queueSize;
        size_t key;
    };

    struct dsp_engine_info {
        std::unique_ptr<uSEQDSPEngine> obj;
        size_t nextKey = 0;

        std::unordered_map<size_t, String> ugenInstances;
        std::unordered_map<size_t, ugenOutputQueue> ugenOutputQueues;
        std::unordered_map<size_t, ugenInputQueue> ugenInputQueues;
    } dspEngine;

    LISP_FUNC_DECL(useq_dsp_start);
    LISP_FUNC_DECL(useq_dsp_stop);
    LISP_FUNC_DECL(useq_dsp_create);
    LISP_FUNC_DECL(useq_dsp_kill);
    LISP_FUNC_DECL(useq_dsp_connect);
    LISP_FUNC_DECL(useq_dsp_disconnect);
    LISP_FUNC_DECL(useq_dsp_getugens);
    LISP_FUNC_DECL(useq_dsp_qget);
    LISP_FUNC_DECL(useq_dsp_qset);
    LISP_FUNC_DECL(useq_dsp_reset);
    LISP_FUNC_DECL(useq_dsp_message);
    LISP_FUNC_DECL(useq_dsp_listqueues);

    LISP_FUNC_DECL(useq_send_sync_trigger_i2c);

#endif

    LISP_FUNC_DECL(useq_swr);
    LISP_FUNC_DECL(useq_rot);

#ifdef ARDUINO
    LISP_FUNC_DECL(useq_swm);
    LISP_FUNC_DECL(useq_swt);
    LISP_FUNC_DECL(useq_toggle_pick);
    LISP_FUNC_DECL(useq_ssin);
#endif

    void init_useq_builtinfuncs();
};

#ifdef ARDUINO
USEQ_SUPPRESS_EXTERNAL_WARNINGS_POP
#endif

#endif // USEQ_H_
