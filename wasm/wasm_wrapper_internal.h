#pragma once

#include "../uSEQ/src/signal_engine/signal_engine.h"
#include "../uSEQ/src/utils/json_builder.h"
#include "../uSEQ/src/utils/json_cursor.h"
#include "../uSEQ/src/modulisp/lisp/symbol_intern.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

extern sig::SignalEngine* g_engine;
extern double g_hw_inputs[32];
extern double g_current_time;
extern double g_prev_tick_time;
extern double g_authoritative_wall_frontier;
extern bool g_has_authoritative_wall_frontier;
extern sig::Diagnostic g_last_diagnostics[16];
extern uint8_t g_last_diagnostic_count;
extern String s_last_error;
extern uint32_t g_cell_revision;

void reset_host_session_state();

char* alloc_cstr(const char* text);
char* alloc_string(const String& text);
bool parse_output_names(const char* json, std::vector<String>& outputs);
uint16_t resolve_output_name(const char* name);
bool format_output_name(uint16_t index, char (&name)[3]);

enum class ExecutionOwnership : uint8_t {
    Observational,
    Authoritative,
};

class PreserveRuntimeHealth {
public:
    explicit PreserveRuntimeHealth(sig::NodePool& pool)
        : pool_(pool), saved_fallback_(pool.runtime_fallback_mask),
          saved_state_(pool.state_update_failure_mask) {}

    ~PreserveRuntimeHealth() {
        pool_.runtime_fallback_mask = saved_fallback_;
        pool_.state_update_failure_mask = saved_state_;
    }

    PreserveRuntimeHealth(const PreserveRuntimeHealth&) = delete;
    PreserveRuntimeHealth& operator=(const PreserveRuntimeHealth&) = delete;

private:
    sig::NodePool& pool_;
    uint64_t saved_fallback_;
    uint64_t saved_state_;
};

double eval_expression_at_time(const char* expression, double time);
bool has_active_state();
bool needs_sequential_batch();
bool validate_execution_time(double wall_time,
                             ExecutionOwnership ownership,
                             double& logical_time);
void execute_batch_sequential(const double* times, int sample_count,
                              const double* cell_values,
                              uint16_t active_output_count,
                              double* batch_buffer);
bool execute_at_time(double wall_time, double* output_values,
                     double* synth_control_values = nullptr,
                     uint16_t synth_control_capacity = 0,
                     ExecutionOwnership ownership =
                         ExecutionOwnership::Observational);

void reset_projection_session();
void invalidate_projection();
void reset_probe_session();
