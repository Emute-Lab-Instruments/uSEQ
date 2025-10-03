#ifndef USEQ_CORE_STATE_H
#define USEQ_CORE_STATE_H

#include "../../uSEQ/src/modulisp/modulisp_interpreter.h"
#include "../../uSEQ/src/modulisp/lisp/value.h"
#include "../../uSEQ/src/modulisp/lisp/environment.h"
#include "../../uSEQ/src/modulisp/lisp/parser.h"
#include "../../uSEQ/src/modulisp/lisp/error_context.h"
#include <vector>
#include <optional>
#include <cstddef>

/**
 * USEQState - Core platform-agnostic state for uSEQ
 *
 * This structure contains all the state needed to run the uSEQ interpreter
 * and signal processing logic, without any platform-specific dependencies.
 *
 * Philosophy:
 * - Pure data structure (no virtual methods)
 * - Composable (contains interpreter, not inherits from it)
 * - Platform-agnostic (no #ifdef ARDUINO in this file)
 */
struct USEQState {
    // === Core Interpreter ===
    ErrorManager error_manager;
    Environment environment;
    uLispParser parser;
    ModuLispInterpreter interpreter;

    // === Output Signal Storage ===
    // ASTs (abstract syntax trees) - the expressions to evaluate
    std::vector<Value> continuous_ASTs;
    std::vector<Value> binary_ASTs;
    std::vector<Value> serial_ASTs;

    // Cached values - results of evaluating the ASTs
    std::vector<float> continuous_vals;
    std::vector<float> binary_vals;
    std::vector<std::optional<float>> serial_vals;

    // === Input Signal Storage ===
    std::vector<float> input_vals;
    std::vector<float> serial_input_vals;

    // === Default Expressions ===
    Value default_continuous_expr = Value::nil();
    Value default_binary_expr = Value::nil();
    Value default_serial_expr = Value::nil();

    // === State Flags ===
    bool is_initialized = false;
    bool should_quit = false;
    bool is_playing = true;
    bool waiting_for_sync_trigger = false;
    bool current_expr_sound = true;

    // === Timing ===
    int ts = 0;  // Timestamp for frame rate calculation
    int update_speed = 0;
    unsigned long serial_out_timestamp = 0;

    // === External Clock Tracking ===
    enum ClockSource {
        INTERNAL = 0,
        EXTERNAL_I1,
        EXTERNAL_I2
    };

    ClockSource clock_source = ClockSource::INTERNAL;

    struct ExtClockTracker {
        size_t beat_count = 0;
        size_t bar_count = 0;
        size_t count = 0;
        size_t div = 1;
    } ext_clock_tracker;

    // === REPL State ===
    String last_received_code = "";

    // === Constructor ===
    USEQState()
        : error_manager(),
          environment(),
          parser(&error_manager),
          interpreter(&error_manager, &environment, &parser)
    {
    }

    // === Helper Methods ===
    void reset_ext_clock_tracking() {
        ext_clock_tracker.beat_count = 0;
        ext_clock_tracker.bar_count = 0;
        ext_clock_tracker.count = 0;
    }

    void set_ext_clock_div(size_t val) {
        ext_clock_tracker.div = val;
        reset_ext_clock_tracking();
    }
};

/**
 * HardwareTraits - Compile-time configuration for different hardware platforms
 *
 * Each platform should define a struct that provides these constants and types.
 * This allows the core logic to be parameterized by platform at compile time
 * without runtime overhead.
 *
 * Example:
 *   struct HardwareV1_0_Traits {
 *       static constexpr size_t NUM_CONTINUOUS_OUTS = 3;
 *       static constexpr size_t NUM_BINARY_OUTS = 3;
 *       static constexpr bool HAS_TEMPO_DETECTION = true;
 *       ...
 *   };
 */
template<typename T>
struct HardwareTraits {
    // These should be provided by the concrete platform config:
    // static constexpr size_t NUM_CONTINUOUS_OUTS;
    // static constexpr size_t NUM_BINARY_OUTS;
    // static constexpr size_t NUM_SERIAL_OUTS;
    // static constexpr size_t NUM_SERIAL_INS;
    // static constexpr size_t NUM_INPUTS;
    // static constexpr bool HAS_TEMPO_DETECTION;
    // static constexpr bool HAS_I2C;
    // static constexpr bool HAS_FLASH_STORAGE;
    // static constexpr bool HAS_DSP_ENGINE;
    // static constexpr bool SUPPORTS_DYNAMIC_IO;
};

#endif // USEQ_CORE_STATE_H
