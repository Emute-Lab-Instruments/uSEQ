#ifndef USEQ_H_
#define USEQ_H_

// Include first for build flags etc
#include "uSEQ/configure.h"


// Include compiler configuration for warning management
#include "utils/compiler_config.h"

// Only suppress external library warnings for hardware includes
#ifdef ARDUINO
USEQ_SUPPRESS_EXTERNAL_WARNINGS_PUSH
#endif

#define USEQ_FIRMWARE_VERSION "1.2.0"

// Functional module headers are included within the class definition


#ifdef ENABLE_TEMPO_ESTIMATOR
#include "dsp/tempoEstimator.h"
#endif
// #include "dsp/MAFilter.h"
#include "dsp/MedianFilter.h"
#ifndef ARDUINO
#include "hardware_includes.h"
#endif
#include "modulisp/lisp/macros.h"
#include "modulisp/lisp/value.h"
#include "modulisp/lisp/error_context.h"
#include "modulisp/lisp/environment.h"
#include "modulisp/lisp/parser.h"
#include "modulisp/modulisp_interpreter.h"
#include "uSEQ/board.h"
#include "ports/IIo.h"
#ifdef ENABLE_I2C_NETWORKING
#include "ports/II2CBus.h"
#endif
#ifdef ENABLE_FLASH_STORAGE
#include "ports/IStorage.h"
#endif
#include <cstdint>
#include <cstring>
#include <memory>
#include <sys/types.h>

#ifdef ARDUINO
#ifdef ENABLE_DSP_ENGINE
#include "dsp/dsp-engine.hpp"
#endif
#endif

#ifndef ARDUINO
#define NUM_CONTINUOUS_OUTS 3
#define NUM_BINARY_OUTS (6 - NUM_CONTINUOUS_OUTS)
#endif

// Forward declarations
class OutputManager;
class IOManager;

class uSEQ {
  public:
    uSEQ() : m_error_manager(), m_environment(), m_parser(&m_error_manager), 
             m_interpreter(&m_error_manager, &m_environment, &m_parser),
             m_output_manager(nullptr), m_io_manager(nullptr) {
        init();
    }
    explicit uSEQ(IClock* clk, ILogger* log, IIo* io_port = nullptr
#ifdef ENABLE_I2C_NETWORKING
                  , II2CBus* i2c_port = nullptr
#endif
#ifdef ENABLE_FLASH_STORAGE
                  , IStorage* storage_port = nullptr
#endif
                  )
        : m_error_manager(), m_environment(), m_parser(&m_error_manager),
          m_interpreter(&m_error_manager, &m_environment, &m_parser, clk, log),
          m_output_manager(nullptr), m_io_manager(nullptr), io(io_port)
#ifdef ENABLE_I2C_NETWORKING
          , i2c(i2c_port)
#endif
#ifdef ENABLE_FLASH_STORAGE
          , storage(storage_port)
#endif
          {
              init();
          }
    ~uSEQ(); // Custom destructor to manage OutputManager and IOManager lifecycle

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

    // Interrupt handlers moved to IOManager, but we need callbacks
    void handle_input1_interrupt(double ts, int value);
    void handle_input2_interrupt(double ts, int value);
#ifdef ENABLE_TEMPO_ESTIMATOR
     tempoEstimator tempoI1, tempoI2;
#endif
    void update_clock_from_external(double ts);

    double delme = 928.22234;

    static uSEQ *instance;
    
    // ModuLispInterpreter delegation methods
    Value eval(Value v) { return m_interpreter.eval(v); }
    String eval(const String &code) { return m_interpreter.eval(code); }
    Environment* get_environment() { return m_interpreter.get_environment(); }
    const Environment* get_environment() const { return m_interpreter.get_environment(); }
    uLispParser* get_parser() { return m_interpreter.get_parser(); }
    void reset_logical_time() { m_interpreter.reset_logical_time(); }
    void set_atom_currently_being_evaluated(const String& atom_name) {
        Interpreter::set_atom_currently_being_evaluated(atom_name);
    }
    bool get_attempt_expr_eval_first() { return Interpreter::get_attempt_expr_eval_first(); }
    void set_attempt_expr_eval_first(bool value) { Interpreter::set_attempt_expr_eval_first(value); }
    bool get_update_loop_evaluation() { return Interpreter::get_update_loop_evaluation(); }
    void set_update_loop_evaluation(bool value) { Interpreter::set_update_loop_evaluation(value); }
    bool get_manual_evaluation() { return Interpreter::get_manual_evaluation(); }
    void set_manual_evaluation(bool value) { Interpreter::set_manual_evaluation(value); }
    void init_interpreter() { m_interpreter.init(); }
    void set_bpm(double newBpm, double changeThreshold) { m_interpreter.set_bpm(newBpm, changeThreshold); }
    void update_time() { m_interpreter.update_time(); }
    void run_scheduled_items() { m_interpreter.run_scheduled_items(); }
    void update_Q0() { m_interpreter.update_Q0(); }
    
    // Access to public member variables and components
    int& get_ts() { return m_interpreter.ts; }
    int& get_update_speed() { return m_interpreter.updateSpeed; }
    Scheduler* get_scheduler() { return m_interpreter.get_scheduler(); }
    
    // IOManager integration
    IOManager* get_io_manager() { return m_io_manager; }

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
    // Core components (order matters for initialization)
    ErrorManager m_error_manager;
    Environment m_environment;
    uLispParser m_parser;
    ModuLispInterpreter m_interpreter;
    
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
    
    // IO management system
    IOManager* m_io_manager;

    // Optional I/O adapter for desktop tests/hardware abstraction (now managed by IOManager)
    IIo* io = nullptr;
    
    // Optional I2C and Storage adapters for desktop tests/hardware abstraction
#ifdef ENABLE_I2C_NETWORKING
    II2CBus* i2c = nullptr;
#endif
#ifdef ENABLE_FLASH_STORAGE
    IStorage* storage = nullptr;
#endif

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

    // Function declarations are now organized in module-specific headers:
    // - I/O functions: uSEQ_io.h
    // - DSP functions: uSEQ_dsp.h  
    // - LISP integration: uSEQ_lisp.h
    // - Hardware-specific: uSEQ_hardware.h

#include "uSEQ_io.h"
#include "uSEQ_lisp.h" 
#include "uSEQ_hardware.h"


    void clear_all_outputs();
#ifdef ARDUINO
    void erase_info_flash();
#endif

    void set_my_id(int num);
    
    // IO operations now delegated to IOManager
    void analog_write_with_led(int output, CONTINUOUS_OUTPUT_VALUE_TYPE val);
    void digital_write_with_led(int output, BINARY_OUTPUT_VALUE_TYPE val);
    void serial_write(int out, SERIAL_OUTPUT_VALUE_TYPE val);

#ifndef ARDUINO
  public:
    // Test hook to exercise write paths without exposing internals
    void __test_call_writes(double a0, int d0, double s0);
    
    // Test hooks for I2C functions
#ifdef ENABLE_I2C_NETWORKING
    Value __test_send_sync_trigger_i2c();
    Value __test_i2c_send_to(int addr, const String& expr_str);
#endif
    
    // Test accessors for environment
    ValueMap& __test_get_defs() { return m_environment.__test_get_defs(); }
    ValueMap& __test_get_def_exprs() { return m_environment.__test_get_def_exprs(); }
    
    // Public wrappers for storage functions (for testing)
#ifndef ARDUINO
#ifdef ENABLE_FLASH_STORAGE
    bool __test_save_env_to_storage(IStorage& s);
    bool __test_load_env_from_storage(IStorage& s);
#endif
#endif
#endif

#ifdef MIDIOUT
    void update_midi_out();
    std::map<int, Value> useqMDOMap;
#endif

    // INIT
    void init_ASTs();

    // SETUP - most setup functions moved to IOManager
    void eval_lisp_library();
    void led_animation(); // Delegates to IOManager
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

    static constexpr const char *m_flash_stamp_str = "uSEQ";
    static constexpr uint m_flash_stamp_size_bytes =
        strlen(m_flash_stamp_str) + 1;

    bool flash_has_been_written_before();
    void autoload_flash();
#endif

    // Functions needed for desktop storage (declared for all builds)
    std::pair<size_t, size_t> num_bytes_def_strs() const;
    void copy_def_strings_to_buffer(char *);

    // Desktop storage wrappers (declared for all builds, implemented only for desktop)
#ifdef ENABLE_FLASH_STORAGE
    bool save_env_to_storage(IStorage& s);
    bool load_env_from_storage(IStorage& s);
#endif

    // void clear_non_program_flash();
    static String current_output_being_processed;

#ifdef ARDUINO
    // DSP ENGINE - use types from uSEQ_dsp.h
#include "uSEQ_dsp.h"
    dsp_engine_info dspEngine;
#endif

    void init_useq_builtinfuncs();
};

#ifdef ARDUINO
USEQ_SUPPRESS_EXTERNAL_WARNINGS_POP
#endif

#endif // USEQ_H_
