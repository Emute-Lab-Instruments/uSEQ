// Suppress all warnings for this file
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wpedantic"

#include "../utils.h"
#include "bytecode_vm.h"
#include "modulisp_interpreter.h"
#include "temporal_context.h"
#include "lisp/macros.h"
#include <cctype>
#include <cmath>

// Template trampoline: generates a zero-overhead static function for each
// ModuLispInterpreter member, converting a plugin-style (void* ctx) call
// into a member function call.
template <Value (ModuLispInterpreter::*Method)(std::vector<Value>&, Environment&)>
static Value method_trampoline(void* ctx, std::vector<Value>& args, Environment& env)
{
    return (static_cast<ModuLispInterpreter*>(ctx)->*Method)(args, env);
}

// Register a ModuLispInterpreter method as a plugin builtin via the
// zero-overhead template trampoline.
#define INSERT_BUILTINDEF(__name__, __func_name__)                                  \
    register_plugin_builtin(                                                        \
        __name__,                                                                   \
        &method_trampoline<&ModuLispInterpreter::__func_name__>,                    \
        this);

void ModuLispInterpreter::init_builtinfuncs()
{
    DBG("ModuLispInterpreter::init_builtinfuncs");

    // Output assignment functions (always available)
    INSERT_BUILTINDEF("a1", useq_a1);
    INSERT_BUILTINDEF("a2", useq_a2);
    INSERT_BUILTINDEF("a3", useq_a3);
    INSERT_BUILTINDEF("a4", useq_a4);
    INSERT_BUILTINDEF("a5", useq_a5);
    INSERT_BUILTINDEF("a6", useq_a6);
    INSERT_BUILTINDEF("a7", useq_a7);
    INSERT_BUILTINDEF("a8", useq_a8);

    INSERT_BUILTINDEF("d1", useq_d1);
    INSERT_BUILTINDEF("d2", useq_d2);
    INSERT_BUILTINDEF("d3", useq_d3);
    INSERT_BUILTINDEF("d4", useq_d4);
    INSERT_BUILTINDEF("d5", useq_d5);
    INSERT_BUILTINDEF("d6", useq_d6);
    INSERT_BUILTINDEF("d7", useq_d7);
    INSERT_BUILTINDEF("d8", useq_d8);

    INSERT_BUILTINDEF("s1", useq_s1);
    INSERT_BUILTINDEF("s2", useq_s2);
    INSERT_BUILTINDEF("s3", useq_s3);
    INSERT_BUILTINDEF("s4", useq_s4);
    INSERT_BUILTINDEF("s5", useq_s5);
    INSERT_BUILTINDEF("s6", useq_s6);
    INSERT_BUILTINDEF("s7", useq_s7);
    INSERT_BUILTINDEF("s8", useq_s8);

    INSERT_BUILTINDEF("eval-at-time", useq_eval_at_time);
    INSERT_BUILTINDEF("useq-play", useq_play);
    INSERT_BUILTINDEF("useq-pause", useq_pause);
    INSERT_BUILTINDEF("useq-stop", useq_stop);
    INSERT_BUILTINDEF("useq-rewind", useq_rewind_logical_time);
    INSERT_BUILTINDEF("useq-clear", useq_clear);
    INSERT_BUILTINDEF("useq-get-transport-state", useq_get_transport_state);

    // These are not class methods, so they can be inserted normally

    // These are all class methods

    INSERT_BUILTINDEF("slow", useq_slow);
    INSERT_BUILTINDEF("fast", useq_fast);
    INSERT_BUILTINDEF("offset", useq_offset_time);

    INSERT_BUILTINDEF("set-bpm", useq_setbpm);
    INSERT_BUILTINDEF("set-time-sig", useq_set_time_sig);
    INSERT_BUILTINDEF("schedule", useq_schedule);
    INSERT_BUILTINDEF("unschedule", useq_unschedule);

    INSERT_BUILTINDEF("tri", useq_tri);

    // INSERT_BUILTINDEF("looph", useq_loopPhasor);
    INSERT_BUILTINDEF("dm", useq_dm);
    INSERT_BUILTINDEF("gates", useq_gates);
    INSERT_BUILTINDEF("gatesw", useq_gatesw);
    INSERT_BUILTINDEF("trigs", useq_trigs);
    INSERT_BUILTINDEF("euclid", useq_euclidean);
    INSERT_BUILTINDEF("eu", useq_eu);
    INSERT_BUILTINDEF("rpulse", useq_ratiotrig);
    INSERT_BUILTINDEF("rstep", useq_ratiostep);
    INSERT_BUILTINDEF("ridx", useq_ratioindex);
    INSERT_BUILTINDEF("rwarp", useq_ratiowarp);
    INSERT_BUILTINDEF("shift", useq_phasor_offset);

    // "seq" is an alias for "from-list"
    INSERT_BUILTINDEF("from-list", useq_fromList);
    INSERT_BUILTINDEF("seq", useq_fromList);
    // "flatseq" is an alias for "from-flattened-list"
    INSERT_BUILTINDEF("from-flattened-list", useq_fromFlattenedList);
    INSERT_BUILTINDEF("flatseq", useq_fromFlattenedList);
    INSERT_BUILTINDEF("flatten", useq_flatten);
    INSERT_BUILTINDEF("interp", useq_interpolate);
    INSERT_BUILTINDEF("step", useq_step);

    INSERT_BUILTINDEF("random", useq_random);
    INSERT_BUILTINDEF("index-rand", useq_index_rand);
    INSERT_BUILTINDEF("loop-at", useq_loop_at_time);

    // Transport offsets
    INSERT_BUILTINDEF("useq-set-time-offset", useq_set_time_offset);
    INSERT_BUILTINDEF("useq-nudge-time", useq_nudge_time);

    // Sync functions
    // INSERT_BUILTINDEF("useq-send-sync-trigger-i2c",
    // useq_send_sync_trigger_i2c);
}

////////////////////
// USEQ API

// Helper function: get default value for output type
double ModuLispInterpreter::default_output_value(OutputType type) const
{
    switch (type)
    {
    case OutputType::ANALOG:
        return 0.5;
    case OutputType::DIGITAL:
    case OutputType::SERIAL:
    default:
        return 0.0;
    }
}

void ModuLispInterpreter::reset_output_slot(StoredOutput& slot, OutputType type)
{
    slot.expr            = Value::nil();
    slot.lastTimeSeconds = std::numeric_limits<double>::quiet_NaN();
    slot.lastValue       = default_output_value(type);
    slot.hasExpr         = false;
    slot.activeProgram = {};
    slot.lkgProgram = {};
    slot.numericProgramAttempted = false;
    slot.numericProgramSucceeded = false;
    slot.numericProgramDirty = false;
    slot.fallbackToLkg = false;
    slot.lastDiagnostic = "";
    slot.lastDiagnosticCategory = DiagnosticCategory::Runtime;
    slot.diagnosticFrameStamp = 0;
}

void ModuLispInterpreter::clear_all_outputs()
{
    const auto clearOutputs = [this](auto& outputs, OutputType type, const char prefix) {
        const double defaultValue = default_output_value(type);
        for (size_t index = 0; index < outputs.size(); ++index)
        {
            String outputName(prefix);
            outputName += String(static_cast<int>(index) + 1);

            Value defaultExpr(defaultValue);
            get_environment()->set(outputName, defaultExpr);
            get_environment()->set_expr(outputName, defaultExpr);

            outputs[index].expr            = defaultExpr;
            outputs[index].lastTimeSeconds = std::numeric_limits<double>::quiet_NaN();
            outputs[index].lastValue       = defaultValue;
            outputs[index].hasExpr         = true;
            outputs[index].activeProgram = {};
            outputs[index].lkgProgram = {};
            outputs[index].numericProgramAttempted = false;
            outputs[index].numericProgramSucceeded = false;
            outputs[index].numericProgramDirty = false;
            outputs[index].fallbackToLkg = false;
            outputs[index].lastDiagnostic = "";
            outputs[index].lastDiagnosticCategory = DiagnosticCategory::Runtime;
            outputs[index].diagnosticFrameStamp = 0;
        }
    };

    clearOutputs(m_analog_outputs, OutputType::ANALOG, 'a');
    clearOutputs(m_digital_outputs, OutputType::DIGITAL, 'd');
    clearOutputs(m_serial_outputs, OutputType::SERIAL, 's');
}

void ModuLispInterpreter::collect_active_diagnostics(
    std::vector<std::pair<String, std::vector<Diagnostic>>>& out) const
{
    const auto scan = [&out](const std::vector<StoredOutput>& slots, char prefix) {
        for (size_t i = 0; i < slots.size(); ++i)
        {
            const auto& slot = slots[i];
            if (slot.lastDiagnostic.length() == 0)
                continue;

            // Build output name, e.g. "a1"
            String name(prefix);
            name += String(static_cast<int>(i) + 1);

            // Use stored category when available; fall back to Syntax for
            // compile failures that predate the category field.
            DiagnosticCategory cat = slot.lastDiagnosticCategory;
            if (!slot.numericProgramSucceeded && slot.numericProgramAttempted)
                cat = DiagnosticCategory::Syntax; // compile failure overrides

            Diagnostic d;
            d.severity = DiagnosticSeverity::Error;
            d.category = cat;
            d.span     = {0, 0};
            d.message  = slot.lastDiagnostic;
            d.triggered_by = slot.lastInvalidatedBy;

            std::vector<Diagnostic> diags;
            diags.push_back(d);
            out.push_back({name, std::move(diags)});
        }
    };

    scan(m_analog_outputs, 'a');
    scan(m_digital_outputs, 'd');
    scan(m_serial_outputs, 's');
}

String ModuLispInterpreter::get_transport_state_string() const
{
    if (m_is_playing)
    {
        return "playing";
    }

    constexpr TimeValue stopped_epsilon = 1e-9;
    if (m_time_manager &&
        std::fabs(m_time_manager->get_transport_time()) <= stopped_epsilon)
    {
        return "stopped";
    }

    return "paused";
}

std::optional<Value> ModuLispInterpreter::lookup_expr_without_error(
    const Environment& env,
    const String& symbol) const
{
    if (auto expr = env.get_def_exprs().get(symbol))
    {
        return expr;
    }

    if (const Environment* parent = env.get_parent_scope())
    {
        return lookup_expr_without_error(*parent, symbol);
    }

    return std::nullopt;
}

std::optional<Value> ModuLispInterpreter::lookup_value_without_error(
    const Environment& env,
    const String& symbol) const
{
    if (auto value = env.get_defs().get(symbol))
    {
        return value;
    }

    if (auto builtin = Environment::builtindefs().get(symbol))
    {
        return builtin;
    }

    if (const Environment* parent = env.get_parent_scope())
    {
        return lookup_value_without_error(*parent, symbol);
    }

    return std::nullopt;
}

String ModuLispInterpreter::snapshot_binding_state(const Environment& env,
                                                   const String& symbol) const
{
    if (auto expr = lookup_expr_without_error(env, symbol))
    {
        return String("expr:") + expr->to_lisp_src();
    }

    if (auto value = lookup_value_without_error(env, symbol))
    {
        return String("value:") + value->to_lisp_src();
    }

    return String("missing");
}

bool ModuLispInterpreter::compiled_program_is_dirty(const CompiledOutputProgram& program,
                                                    const Environment& env) const
{
    if (!program.program)
    {
        return false;
    }

    if (program.dependencies.size() != program.dependencySnapshots.size())
    {
        return true;
    }

    for (size_t i = 0; i < program.dependencies.size(); ++i)
    {
        if (snapshot_binding_state(env, program.dependencies[i]) !=
            program.dependencySnapshots[i])
        {
            return true;
        }
    }

    return false;
}

String ModuLispInterpreter::find_dirty_dependency(const CompiledOutputProgram& program,
                                                  const Environment& env) const
{
    if (!program.program)
    {
        return "";
    }

    if (program.dependencies.size() != program.dependencySnapshots.size())
    {
        return "";
    }

    for (size_t i = 0; i < program.dependencies.size(); ++i)
    {
        if (snapshot_binding_state(env, program.dependencies[i]) !=
            program.dependencySnapshots[i])
        {
            return program.dependencies[i];
        }
    }

    return "";
}

bool ModuLispInterpreter::output_program_is_dirty(const StoredOutput& slot,
                                                  const Environment& env) const
{
    if (slot.numericProgramDirty)
    {
        return true;
    }

    return compiled_program_is_dirty(slot.activeProgram, env);
}

bool ModuLispInterpreter::compile_output_program(const Value& expr,
                                                 Environment& env,
                                                 CompiledOutputProgram& out,
                                                 String* error) const
{
    out = {};
    out.exprSource = expr.to_lisp_src();

    const NumericVmCompileResult compiled =
        compile_numeric_program(expr, env, true);
    if (!compiled.ok)
    {
        if (error)
        {
            *error = compiled.error;
        }
        return false;
    }

    out.program = std::make_shared<NumericVmProgram>(compiled.program);
    out.dependencies = compiled.program.dependencies;
    out.dependencySnapshots.reserve(out.dependencies.size());
    for (const String& dependency : out.dependencies)
    {
        out.dependencySnapshots.push_back(snapshot_binding_state(env, dependency));
    }
    return true;
}

bool ModuLispInterpreter::refresh_output_program(StoredOutput& slot, Environment& env)
{
    slot.numericProgramAttempted = true;
    slot.numericProgramDirty = false;

    if (!slot.hasExpr)
    {
        slot.numericProgramSucceeded = false;
        slot.fallbackToLkg = false;
        return false;
    }

    CompiledOutputProgram compiled;
    String compileError;
    if (!compile_output_program(slot.expr, env, compiled, &compileError))
    {
        slot.numericProgramSucceeded = false;
        slot.lastDiagnostic = compileError;
        return false;
    }

    if (slot.activeProgram.program && slot.activeProgram.observedGood)
    {
        slot.lkgProgram = slot.activeProgram;
    }

    slot.activeProgram = compiled;
    slot.numericProgramSucceeded = true;
    slot.fallbackToLkg = false;
    slot.lastDiagnostic = "";
    slot.lastInvalidatedBy = "";  // successful recompile clears blame

    return true;
}

TemporalContext ModuLispInterpreter::make_temporal_context(double time_seconds) const
{
    const double time_micros = time_seconds * 1e6;

    TemporalContext ctx;
    ctx.t = time_seconds;
    ctx.time_since_boot = m_time_manager ? m_time_manager->get_time_seconds() : 0;
    ctx.beat = const_cast<ModuLispInterpreter*>(this)->beat_at_time(time_micros);
    ctx.bar = const_cast<ModuLispInterpreter*>(this)->bar_at_time(time_micros);
    ctx.phrase = const_cast<ModuLispInterpreter*>(this)->phrase_at_time(time_micros);
    ctx.section =
        const_cast<ModuLispInterpreter*>(this)->section_at_time(time_micros);
    ctx.beatNum =
        static_cast<int>(const_cast<ModuLispInterpreter*>(this)->beat_num_at_time(
            time_micros));
    ctx.barNum =
        static_cast<int>(const_cast<ModuLispInterpreter*>(this)->bar_num_at_time(
            time_micros));
    ctx.beatDur = m_beat_length / 1000000.0;
    ctx.barDur = m_bar_length / 1000000.0;
    ctx.phraseDur = m_phrase_length / 1000000.0;
    ctx.sectionDur = m_section_length / 1000000.0;
    return ctx;
}

Value ModuLispInterpreter::handle_output_assignment(const char* name,
                                                    size_t index,
                                                    OutputType type,
                                                    std::vector<Value>& args,
                                                    Environment& env)
{
    if (args.empty())
    {
        return Value::error();
    }

    // Store the expression as-is (even if it's an atom/symbol).
    // We'll look up expression bindings dynamically during evaluation,
    // so that redefined variables update the output.
    // Example: (define foo (usin bar)) (a1 foo) (define foo (usin beat))
    // The second define will update a1's output.
    Value expr = args[0];

    String lispName(name);

    // Persist expression without evaluating it so it can be replayed later.
    get_environment()->set_expr(lispName, expr);
    (void)env;

    StoredOutput* slot = nullptr;
    switch (type)
    {
    case OutputType::ANALOG:
        if (index < m_analog_outputs.size())
            slot = &m_analog_outputs[index];
        break;
    case OutputType::DIGITAL:
        if (index < m_digital_outputs.size())
            slot = &m_digital_outputs[index];
        break;
    case OutputType::SERIAL:
        if (index < m_serial_outputs.size())
            slot = &m_serial_outputs[index];
        break;
    }

    if (!slot)
    {
        return Value::error();
    }

    slot->expr            = expr;
    slot->hasExpr         = !expr.is_nil();
    slot->lastTimeSeconds = std::numeric_limits<double>::quiet_NaN();
    slot->lastValue       = default_output_value(type);
    slot->numericProgramAttempted = false;
    slot->numericProgramDirty = false;
    slot->fallbackToLkg = false;
    slot->lastDiagnostic = "";
    slot->lastDiagnosticCategory = DiagnosticCategory::Runtime;
    slot->diagnosticFrameStamp = 0;
    slot->lastInvalidatedBy = "";  // fresh assignment, not dependency-triggered
    refresh_output_program(*slot, *get_environment());

    return Value::atom(lispName);
}

bool ModuLispInterpreter::resolve_output(const char* name,
                                         OutputType& type,
                                         size_t& index) const
{
    if (!name || !name[0])
    {
        return false;
    }

    const char prefix = name[0];
    int numeric       = 0;
    for (size_t i = 1; name[i] != '\0'; ++i)
    {
        unsigned char c = static_cast<unsigned char>(name[i]);
        if (!std::isdigit(c))
        {
            return false;
        }
        numeric = numeric * 10 + (c - '0');
    }

    if (numeric < 1)
    {
        return false;
    }

    index = static_cast<size_t>(numeric - 1);

    switch (prefix)
    {
    case 'a':
    case 'A':
        type = OutputType::ANALOG;
        return index < m_analog_outputs.size();
    case 'd':
    case 'D':
        type = OutputType::DIGITAL;
        return index < m_digital_outputs.size();
    case 's':
    case 'S':
        type = OutputType::SERIAL;
        return index < m_serial_outputs.size();
    default:
        break;
    }

    return false;
}

double ModuLispInterpreter::eval_output_internal(OutputType type,
                                                 size_t index,
                                                 double time_seconds,
                                                 bool* ok)
{
    ++m_diagnosticFrameCounter;

    const double defaultValue = default_output_value(type);
    if (ok)
    {
        *ok = false;
    }

    if (!std::isfinite(time_seconds))
    {
        return defaultValue;
    }

    StoredOutput* slot = nullptr;
    switch (type)
    {
    case OutputType::ANALOG:
        if (index < m_analog_outputs.size())
            slot = &m_analog_outputs[index];
        break;
    case OutputType::DIGITAL:
        if (index < m_digital_outputs.size())
            slot = &m_digital_outputs[index];
        break;
    case OutputType::SERIAL:
        if (index < m_serial_outputs.size())
            slot = &m_serial_outputs[index];
        break;
    }

    if (!slot)
    {
        return defaultValue;
    }

    if (!slot->hasExpr)
    {
        slot->lastValue       = defaultValue;
        slot->lastTimeSeconds = time_seconds;
        if (ok)
        {
            *ok = true;
        }
        return defaultValue;
    }

    const double time_micros = time_seconds * 1e6;

    char prefixChar = 'a';
    switch (type)
    {
    case OutputType::ANALOG:
        prefixChar = 'a';
        break;
    case OutputType::DIGITAL:
        prefixChar = 'd';
        break;
    case OutputType::SERIAL:
        prefixChar = 's';
        break;
    }

    String exprName(prefixChar);
    exprName += String(static_cast<int>(index) + 1);

    bool program_refreshed = false;
    bool permit_tree_fallback = true;
    if (!slot->numericProgramAttempted ||
        slot->activeProgram.exprSource != slot->expr.to_lisp_src() ||
        output_program_is_dirty(*slot, *get_environment()))
    {
        // Before recompiling, identify which dependency changed (if any).
        // This is empty for first-time compilation or source-change triggers.
        String culprit = find_dirty_dependency(slot->activeProgram, *get_environment());
        slot->lastInvalidatedBy = culprit;

        refresh_output_program(*slot, *get_environment());
        program_refreshed = true;
        if (!slot->numericProgramSucceeded && slot->lastDiagnostic.length() > 0)
        {
            report_generic_error("Output " + exprName + " compile failed: " +
                                 slot->lastDiagnostic);
            if (slot->lastDiagnostic.startsWith("Signal VM rejects"))
            {
                permit_tree_fallback = false;
            }
        }
    }

    constexpr double epsilon = 1e-9;
    if (std::isfinite(slot->lastTimeSeconds) &&
        std::fabs(slot->lastTimeSeconds - time_seconds) < epsilon &&
        !program_refreshed &&
        !output_program_is_dirty(*slot, *get_environment()))
    {
        if (ok)
        {
            *ok = true;
        }
        return slot->lastValue;
    }

    CompiledOutputProgram* program_to_run = nullptr;
    if (slot->fallbackToLkg && slot->lkgProgram.program)
    {
        program_to_run = &slot->lkgProgram;
    }
    else if (slot->activeProgram.program)
    {
        program_to_run = &slot->activeProgram;
    }

    if (program_to_run && program_to_run->program)
    {
        const TemporalContext ctx = make_temporal_context(time_seconds);
        const NumericVmExecutionResult vm_result =
            execute_numeric_program(*program_to_run->program, ctx);
        if (vm_result.ok && std::isfinite(vm_result.value))
        {
            if (program_to_run == &slot->activeProgram)
            {
                slot->activeProgram.observedGood = true;
            }
            slot->lastDiagnostic = "";
            slot->lastValue = vm_result.value;
            slot->lastTimeSeconds = time_seconds;
            if (ok)
            {
                *ok = true;
            }
            return vm_result.value;
        }

        // Deduplicate: only update and report if message changed or enough
        // frames have elapsed since the last report (~100 frames ≈ 100ms at 1kHz).
        constexpr uint32_t DIAG_DEDUP_FRAMES = 100;
        const bool message_changed = (vm_result.error != slot->lastDiagnostic);
        const bool cooldown_elapsed =
            (m_diagnosticFrameCounter - slot->diagnosticFrameStamp) >= DIAG_DEDUP_FRAMES;
        if (message_changed || cooldown_elapsed)
        {
            slot->lastDiagnostic = vm_result.error;
            slot->lastDiagnosticCategory = vm_result.error_category;
            slot->diagnosticFrameStamp = m_diagnosticFrameCounter;
            report_generic_error("Output " + exprName + " runtime failed: " +
                                 vm_result.error);
        }
        permit_tree_fallback = false;

        if (program_to_run == &slot->activeProgram && slot->lkgProgram.program)
        {
            slot->fallbackToLkg = true;
            const NumericVmExecutionResult lkg_result =
                execute_numeric_program(*slot->lkgProgram.program, ctx);
            if (lkg_result.ok && std::isfinite(lkg_result.value))
            {
                slot->lastValue = lkg_result.value;
                slot->lastTimeSeconds = time_seconds;
                if (ok)
                {
                    *ok = true;
                }
                return lkg_result.value;
            }
        }

        if (std::isfinite(slot->lastTimeSeconds))
        {
            if (ok)
            {
                *ok = true;
            }
            return slot->lastValue;
        }
    }

    // TEMPORARY: force VM-only mode — disable tree-walker fallback
    // to verify all expressions are handled by the bytecode VM.
    // Revert this block to restore fallback behavior.
    permit_tree_fallback = false;

    if (!permit_tree_fallback)
    {
        slot->lastValue = defaultValue;
        slot->lastTimeSeconds = time_seconds;
        return std::isfinite(slot->lastValue) ? slot->lastValue : defaultValue;
    }

    Value expr_to_eval = slot->expr;

    set_atom_currently_being_evaluated(exprName);
    set_attempt_expr_eval_first(true);
    Value result = eval_at_time(expr_to_eval, *get_environment(), time_micros);
    set_attempt_expr_eval_first(false);
    set_atom_currently_being_evaluated(String(""));

    if (!result.is_number())
    {
        slot->lastValue       = defaultValue;
        slot->lastTimeSeconds = time_seconds;
        if (ok)
        {
            *ok = false;
        }
        return defaultValue;
    }

    const double numeric = result.as_float();
    slot->lastValue       = numeric;
    slot->lastTimeSeconds = time_seconds;
    if (ok)
    {
        *ok = true;
    }
    return numeric;
}

double ModuLispInterpreter::eval_output_at_time(const char* name,
                                                 double time_seconds,
                                                 bool* ok)
{
    OutputType type;
    size_t index = 0;
    if (!resolve_output(name, type, index))
    {
        if (ok)
        {
            *ok = false;
        }
        return std::numeric_limits<double>::quiet_NaN();
    }

    return eval_output_internal(type, index, time_seconds, ok);
}

// Evaluate all outputs at current time
std::map<String, double> ModuLispInterpreter::eval_outputs()
{
    double current_time_seconds = get_time_manager()->get_transport_time() / 1e6;
    return eval_outputs(current_time_seconds);
}

// Evaluate all outputs at specific time
std::map<String, double> ModuLispInterpreter::eval_outputs(double time_seconds)
{
    std::vector<String> all_outputs;

    // Build list of all active outputs
    for (size_t i = 0; i < m_num_analog_outs; ++i)
    {
        all_outputs.push_back(String("a") + String(static_cast<int>(i + 1)));
    }
    for (size_t i = 0; i < m_num_digital_outs; ++i)
    {
        all_outputs.push_back(String("d") + String(static_cast<int>(i + 1)));
    }
    for (size_t i = 0; i < m_num_serial_outs; ++i)
    {
        all_outputs.push_back(String("s") + String(static_cast<int>(i + 1)));
    }

    return eval_outputs(all_outputs, time_seconds);
}

// Evaluate subset of outputs at current time
std::map<String, double> ModuLispInterpreter::eval_outputs(const std::vector<String>& outputs)
{
    double current_time_seconds = get_time_manager()->get_transport_time() / 1e6;
    return eval_outputs(outputs, current_time_seconds);
}

// Evaluate subset of outputs at specific time
std::map<String, double> ModuLispInterpreter::eval_outputs(
    const std::vector<String>& outputs,
    double time_seconds)
{
    std::map<String, double> results;

    for (const String& name : outputs)
    {
        OutputType type;
        size_t index;

        if (resolve_output(name.c_str(), type, index))
        {
            bool ok = false;
            double value = eval_output_internal(type, index, time_seconds, &ok);
            // Always include the value — eval_output_internal returns a usable
            // fallback (LKG, lastValue, or default) even when ok is false.
            // Dropping failed outputs here would cause the batch time-window
            // path to substitute 0.0, losing the proper per-output fallback.
            results[name] = value;
        }
    }

    return results;
}

// Sample outputs across a time window with specified resolution
// Returns channel-series format: map of output names to vectors of time samples
std::map<String, std::vector<double>> ModuLispInterpreter::eval_outputs(
    double start_time_seconds,
    double end_time_seconds,
    size_t num_samples,
    const std::vector<String>& outputs)
{
    std::map<String, std::vector<double>> results;

    // Handle edge case: no samples requested
    if (num_samples == 0)
    {
        return results;
    }

    // Determine which outputs to evaluate
    std::vector<String> outputs_to_eval = outputs;
    if (outputs_to_eval.empty())
    {
        // Build list of all active outputs
        for (size_t i = 0; i < m_num_analog_outs; ++i)
        {
            outputs_to_eval.push_back(String("a") + String(static_cast<int>(i + 1)));
        }
        for (size_t i = 0; i < m_num_digital_outs; ++i)
        {
            outputs_to_eval.push_back(String("d") + String(static_cast<int>(i + 1)));
        }
        for (size_t i = 0; i < m_num_serial_outs; ++i)
        {
            outputs_to_eval.push_back(String("s") + String(static_cast<int>(i + 1)));
        }
    }

    // Initialize result vectors for each output
    for (const auto& output_name : outputs_to_eval)
    {
        results[output_name] = std::vector<double>();
        results[output_name].reserve(num_samples);
    }

    // Generate time points linearly spaced from start to end
    double time_step = (num_samples == 1) ? 0.0 : (end_time_seconds - start_time_seconds) / (num_samples - 1);

    // Sample each time point and populate channel vectors
    for (size_t i = 0; i < num_samples; ++i)
    {
        double sample_time = start_time_seconds + (i * time_step);

        // Evaluate all outputs at this time point
        auto time_point_values = eval_outputs(outputs_to_eval, sample_time);

        // Distribute values to their respective channel vectors
        for (const auto& output_name : outputs_to_eval)
        {
            auto it = time_point_values.find(output_name);
            if (it != time_point_values.end())
            {
                results[output_name].push_back(it->second);
            }
            else
            {
                // If output not found, push default value
                results[output_name].push_back(0.0);
            }
        }
    }

    return results;
}

// Output assignment functions with bounds checking
#define DEFINE_OUTPUT_FUNCTION(suffix, index, outputType, enumValue)                            \
    Value ModuLispInterpreter::useq_##suffix(std::vector<Value>& args, Environment& env)        \
    {                                                                                            \
        if (index >= m_num_##outputType##_outs)                                                 \
        {                                                                                        \
            if (logger)                                                                          \
            {                                                                                    \
                logger->warn("Warning: " #suffix " called but only " +                          \
                           String(static_cast<int>(m_num_##outputType##_outs)) +                \
                           " " #outputType " outputs available");                               \
            }                                                                                    \
            return Value::nil();                                                                 \
        }                                                                                        \
        return handle_output_assignment("" #suffix "", index, OutputType::enumValue,            \
                                       args, env);                                               \
    }

DEFINE_OUTPUT_FUNCTION(a1, 0, analog, ANALOG)
DEFINE_OUTPUT_FUNCTION(a2, 1, analog, ANALOG)
DEFINE_OUTPUT_FUNCTION(a3, 2, analog, ANALOG)
DEFINE_OUTPUT_FUNCTION(a4, 3, analog, ANALOG)
DEFINE_OUTPUT_FUNCTION(a5, 4, analog, ANALOG)
DEFINE_OUTPUT_FUNCTION(a6, 5, analog, ANALOG)
DEFINE_OUTPUT_FUNCTION(a7, 6, analog, ANALOG)
DEFINE_OUTPUT_FUNCTION(a8, 7, analog, ANALOG)

DEFINE_OUTPUT_FUNCTION(d1, 0, digital, DIGITAL)
DEFINE_OUTPUT_FUNCTION(d2, 1, digital, DIGITAL)
DEFINE_OUTPUT_FUNCTION(d3, 2, digital, DIGITAL)
DEFINE_OUTPUT_FUNCTION(d4, 3, digital, DIGITAL)
DEFINE_OUTPUT_FUNCTION(d5, 4, digital, DIGITAL)
DEFINE_OUTPUT_FUNCTION(d6, 5, digital, DIGITAL)
DEFINE_OUTPUT_FUNCTION(d7, 6, digital, DIGITAL)
DEFINE_OUTPUT_FUNCTION(d8, 7, digital, DIGITAL)

DEFINE_OUTPUT_FUNCTION(s1, 0, serial, SERIAL)
DEFINE_OUTPUT_FUNCTION(s2, 1, serial, SERIAL)
DEFINE_OUTPUT_FUNCTION(s3, 2, serial, SERIAL)
DEFINE_OUTPUT_FUNCTION(s4, 3, serial, SERIAL)
DEFINE_OUTPUT_FUNCTION(s5, 4, serial, SERIAL)
DEFINE_OUTPUT_FUNCTION(s6, 5, serial, SERIAL)
DEFINE_OUTPUT_FUNCTION(s7, 6, serial, SERIAL)
DEFINE_OUTPUT_FUNCTION(s8, 7, serial, SERIAL)

#undef DEFINE_OUTPUT_FUNCTION

// simple_hashing_function is now delegated to RandomGenerator via inline function in
// header

BUILTIN_VARGS_NUMS(useq_index_rand, "index-rand", 1, 3,
    bool scale                = args.size() > 1;
    bool lower_bound_provided = args.size() == 3;

    double index = args[args.size() - 1].as_float();
    double lo    = (scale && lower_bound_provided) ? args[0].as_float() : 0.0;
    double hi    = scale ? args[args.size() - 1].as_float() : 1.0;

    double rand_val = simple_hashing_function(index);
    rand_val        = lo + (rand_val * (hi - lo));
    return Value(rand_val);
)

BUILTIN_VARGS_NUMS(useq_random, "random", 0, 2,
    bool scale                = args.size() > 0;
    bool lower_bound_provided = args.size() == 2;

    uint32_t current_beat_num =
        static_cast<uint32_t>(env.get("beat-num")
                                  .value_or(Value(static_cast<int>(beat_num_at_time(
                                      get_time_manager()->get_transport_time()))))
                                  .as_int());

    double rand_val = simple_hashing_function(current_beat_num);

    if (scale)
    {
        double low  = lower_bound_provided ? args[0].as_float() : 0.0;
        double high = args[1].as_float();
        rand_val    = low + (rand_val * (high - low));
    }

    return Value(rand_val);
)

#pragma GCC diagnostic pop
