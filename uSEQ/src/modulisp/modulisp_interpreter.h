#ifndef MODULISP_INTERPRETER_H_
#define MODULISP_INTERPRETER_H_

#include "../ports/IClock.h"
#include "../ports/ILogger.h"
#include "lisp/environment.h"
#include "lisp/error_context.h"
#include "lisp/parser.h"
#include "lisp/value.h"
#include "random_generator.h"
#include "scheduler.h"
#include "time_manager.h"
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <vector>

#define LISP_FUNC_ARGS_TYPE std::vector<Value>&, Environment&
#define LISP_FUNC_ARGS std::vector<Value>&args, Environment &env
#define LISP_FUNC_RETURN_TYPE Value
#define LISP_FUNC_TYPE LISP_FUNC_RETURN_TYPE(LISP_FUNC_ARGS_TYPE)
#define LISP_FUNC_DECL(__name__) LISP_FUNC_RETURN_TYPE __name__(LISP_FUNC_ARGS)

using TimeValue  = double;
using PhaseValue = double;

/**
 * @brief Result of code execution - platform-agnostic execution semantics
 *
 * This struct encapsulates the result of executing ModuLisp code, allowing
 * the transport layer (Serial, I2C, WASM, etc.) to handle output routing
 * without being coupled to execution logic.
 *
 * Design rationale:
 * - result_text: Contains either the evaluation result OR the echoed input
 * - had_errors: Quick boolean check for error state
 * - errors: Full error messages for detailed reporting
 * - printed: Controls whether output should be shown to user (vs silent execution)
 */
struct ExecutionResult
{
    String result_text;              ///< Evaluation result or echo of scheduled code
    bool had_errors = false;         ///< True if error_msg_q was non-empty after execution
    std::vector<String> errors;      ///< Copy of error_msg_q (empty if no errors)
    bool printed = true;             ///< True if result should be output to user
};

class ModuLispInterpreter
{
public:
    // Constructor with dependency injection for testability
    // NOTE: unused for now
    explicit ModuLispInterpreter(ErrorManager* error_mgr, Environment* env = nullptr,
                                 uLispParser* parser = nullptr,
                                 IClock* clk = nullptr, ILogger* log = nullptr,
                                 IRandomGenerator* rng = nullptr,
                                 size_t num_analog_outs = 8,
                                 size_t num_digital_outs = 8,
                                 size_t num_serial_outs = 8);

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
        if (m_beat_length <= 0)
            return 0;
        return static_cast<uint32_t>(time / m_beat_length);
    }
    PhaseValue bar_at_time(TimeValue time)
    {
        return std::fmod(time, m_bar_length) / m_bar_length;
    }
    uint32_t bar_num_at_time(TimeValue time)
    {
        if (m_bar_length <= 0)
            return 0;
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
    double get_meter_numerator() const { return m_meter_numerator; }
    double get_meter_denominator() const { return m_meter_denominator; }

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

    // --- Execution API: Platform-agnostic code execution ---
    /**
     * @brief Execute code immediately with manual evaluation semantics
     *
     * This method provides immediate execution similar to the '@' marker in serial input.
     * It temporarily sets manual_evaluation mode, evaluates the code, captures any errors,
     * and returns a structured result for the transport layer to route.
     *
     * Execution semantics:
     * - Sets manual_evaluation = true (distinguishes user input from automated eval)
     * - Clears error queue before evaluation
     * - Evaluates code synchronously
     * - Captures errors if any occur
     * - Restores manual_evaluation = false
     * - Always marks result as printed (user expects immediate feedback)
     *
     * @param code ModuLisp code to execute immediately
     * @return ExecutionResult containing result text, errors, and output flags
     *
     * @note This is the platform-agnostic implementation of immediate execution
     *       that was previously embedded in uSEQ::check_and_handle_user_input()
     */
    ExecutionResult execute_now(const String& code);

    /**
     * @brief Schedule code for deferred execution at next quantum boundary
     *
     * This method queues code for later execution, typically at the next bar/beat boundary.
     * It parses the code, adds it to the scheduler's run queue, and echoes the input back.
     *
     * Execution semantics:
     * - Sets manual_evaluation = true
     * - Parses code into AST
     * - Adds to scheduler run queue (executed during check_code_quant_phasor)
     * - Echoes the scheduled code as confirmation
     * - Restores manual_evaluation = false
     * - Marks result as printed (echo confirms scheduling)
     *
     * @param code ModuLisp code to schedule for later execution
     * @return ExecutionResult containing echoed code and status
     *
     * @note This is the platform-agnostic implementation of scheduled execution
     *       that was previously embedded in uSEQ::check_and_handle_user_input()
     */
    ExecutionResult schedule_code(const String& code);

    /**
     * @brief Update a serial input stream value
     *
     * This method updates one of the serial input stream channels with a new value.
     * Serial streams (sin1, sin2, etc.) are exposed as LISP variables that can be
     * referenced in expressions. This allows external controllers to inject values.
     *
     * Channel numbering:
     * - Channels are 1-indexed externally (1-32)
     * - Stored as 0-indexed internally (0-31)
     * - Invalid channels are silently ignored (matches current behavior)
     *
     * @param channel Serial input channel (1-indexed, range 1-32)
     * @param value New value for the channel
     *
     * @note Currently a no-op stub - serial stream storage remains in uSEQ layer.
     *       This method exists to complete the execution API contract. Full implementation
     *       will be added when serial stream ownership migrates to interpreter (Phase 2).
     *
     * @todo Move serial input stream storage to ModuLispInterpreter (Phase 2)
     */
    void update_stream_value(size_t channel, double value);

    // --- Absorbed Interpreter API ---
    // Evaluation API
    String eval(const String& code);
    Value eval(Value v);
    Value eval_v(const String& code);

    // Static evaluation helpers
    static String eval_in(const String& code, Environment& env);
    static Value eval_in(Value& v, Environment& env);
    static Value apply(Value& f, LispFuncArgsVec& args, Environment& env);
    static void eval_args(std::vector<Value>& args, Environment& env);
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
    static void set_atom_currently_being_evaluated(const String& atom_name)
    {
        m_atom_currently_being_evaluated = atom_name;
    }
    static bool get_attempt_expr_eval_first() { return m_attempt_expr_eval_first; }
    static void set_attempt_expr_eval_first(bool value)
    {
        m_attempt_expr_eval_first = value;
    }
    static bool get_update_loop_evaluation() { return m_update_loop_evaluation; }
    static void set_update_loop_evaluation(bool value)
    {
        m_update_loop_evaluation = value;
    }

    // Plugin builtin registration API: allows the system layer to register
    // hardware-specific builtins without the interpreter knowing about the system.
    void register_plugin_builtin(const String& name, PluginBuiltinFunc func, void* ctx);

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
    LISP_FUNC_DECL(useq_play);
    LISP_FUNC_DECL(useq_pause);
    LISP_FUNC_DECL(useq_stop);
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
    LISP_FUNC_DECL(useq_interpolate);
    LISP_FUNC_DECL(useq_rewind_logical_time);
    LISP_FUNC_DECL(useq_clear);
    LISP_FUNC_DECL(useq_get_transport_state);
    LISP_FUNC_DECL(useq_q0);

    // Output assignment functions (a1-a8 for analog, d1-d8 for digital, s1-s8 for serial)
    LISP_FUNC_DECL(useq_a1);
    LISP_FUNC_DECL(useq_a2);
    LISP_FUNC_DECL(useq_a3);
    LISP_FUNC_DECL(useq_a4);
    LISP_FUNC_DECL(useq_a5);
    LISP_FUNC_DECL(useq_a6);
    LISP_FUNC_DECL(useq_a7);
    LISP_FUNC_DECL(useq_a8);

    LISP_FUNC_DECL(useq_d1);
    LISP_FUNC_DECL(useq_d2);
    LISP_FUNC_DECL(useq_d3);
    LISP_FUNC_DECL(useq_d4);
    LISP_FUNC_DECL(useq_d5);
    LISP_FUNC_DECL(useq_d6);
    LISP_FUNC_DECL(useq_d7);
    LISP_FUNC_DECL(useq_d8);

    LISP_FUNC_DECL(useq_s1);
    LISP_FUNC_DECL(useq_s2);
    LISP_FUNC_DECL(useq_s3);
    LISP_FUNC_DECL(useq_s4);
    LISP_FUNC_DECL(useq_s5);
    LISP_FUNC_DECL(useq_s6);
    LISP_FUNC_DECL(useq_s7);
    LISP_FUNC_DECL(useq_s8);

    // Output evaluation API - core interpreter functionality
    // Evaluate outputs at specific or current time
    std::map<String, double> eval_outputs();                                    // current time, all outputs
    std::map<String, double> eval_outputs(double time_seconds);                 // specific time, all outputs
    std::map<String, double> eval_outputs(const std::vector<String>& outputs);  // current time, subset
    std::map<String, double> eval_outputs(const std::vector<String>& outputs,
                                         double time_seconds);                  // specific time, subset

    // Sample outputs across a time window at specified resolution
    // Returns a map of output names to their time-series vectors
    // Each vector contains num_samples values uniformly distributed from start to end time
    // If outputs vector is empty, samples all outputs (a1-a8, d1-d8, s1-s8)
    // Format: {a1: [0.5, 0.6, ...], a2: [0.3, 0.4, ...]}
    std::map<String, std::vector<double>> eval_outputs(
        double start_time_seconds,
        double end_time_seconds,
        size_t num_samples,
        const std::vector<String>& outputs = {});

    // Convenience method for single output evaluation (backward compatibility)
    double eval_output_at_time(const char* name, double time_seconds,
                               bool* ok = nullptr);

    // Time control
    void set_time_from_external_source(TimeValue actual_time);

    // Output type enumeration
    enum class OutputType
    {
        ANALOG,
        DIGITAL,
        SERIAL
    };

    // Stored output structure
    struct StoredOutput
    {
        Value expr;
        double lastTimeSeconds = std::numeric_limits<double>::quiet_NaN();
        double lastValue       = 0.0;
        bool hasExpr           = false;
    };

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
    double m_beat_length     = 500000.0;   // microseconds per beat (120 BPM default)
    double m_bar_length      = 2000000.0;  // microseconds per bar
    double m_phrase_length   = 8000000.0;  // microseconds per phrase
    double m_section_length  = 32000000.0; // microseconds per section
    double m_bpm             = 120.0;
    double m_bars_per_phrase = 4.0;
    double m_phrases_per_section = 4.0;
    double m_meter_numerator     = 4.0; // Time signature numerator (default 4/4)
    double m_meter_denominator   = 4.0; // Time signature denominator (default 4/4)

    // Output storage and configuration
    size_t m_num_analog_outs;
    size_t m_num_digital_outs;
    size_t m_num_serial_outs;

    std::vector<StoredOutput> m_analog_outputs;
    std::vector<StoredOutput> m_digital_outputs;
    std::vector<StoredOutput> m_serial_outputs;
    bool m_is_playing = true;

protected:

    // Helper methods for output handling
    Value handle_output_assignment(const char* name, size_t index, OutputType type,
                                  std::vector<Value>& args, Environment& env);
    bool resolve_output(const char* name, OutputType& type, size_t& index) const;
    double eval_output_internal(OutputType type, size_t index, double time_seconds, bool* ok);
    double default_output_value(OutputType type) const;
    void reset_output_slot(StoredOutput& slot, OutputType type);
    void clear_all_outputs();
    String get_transport_state_string() const;

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
    static bool m_update_loop_evaluation;
    static String m_atom_currently_being_evaluated;

public:
    // Make instance pointer public for builtin access
    static ModuLispInterpreter* modulisp_instance_ptr;
};

#endif // MODULISP_INTERPRETER_H_
