// Suppress all warnings for this file (consistent with modulisp_api.cpp)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wpedantic"

#include "../utils.h"
#include "modulisp_interpreter.h"
#include "temporal_context.h"
#include "lisp/macros.h"
#include <cmath>

// ============================================================================
// Timing builtins: fast, slow, offset, set-bpm, play, pause, stop,
// set-time-offset, nudge-time, set-time-sig, clear, rewind, loop-at-time
// ============================================================================

BUILTIN_NUMS(useq_set_time_offset, "useq-set-time-offset", 1, {
    m_time_manager->set_transport_offset(args[0].as_float());
    get_environment()->set("useq-time-offset", args[0].as_float());
    return args[0];
})

BUILTIN_NUMS(useq_nudge_time, "useq-nudge-time", 1, {
    double new_offset = m_time_manager->get_transport_offset() + args[0].as_float();
    m_time_manager->set_transport_offset(new_offset);
    get_environment()->set("useq-time-offset", new_offset);
    return args[0];
})

BUILTIN_ZEROARG(useq_play, "useq-play", {
    m_is_playing = true;
    if (m_time_manager)
    {
        m_time_manager->play_transport();
    }
    return Value::string(get_transport_state_string());
})

BUILTIN_ZEROARG(useq_pause, "useq-pause", {
    m_is_playing = false;
    if (m_time_manager)
    {
        m_time_manager->pause_transport();
    }
    return Value::string(get_transport_state_string());
})

BUILTIN_ZEROARG(useq_stop, "useq-stop", {
    m_is_playing = false;
    if (m_time_manager)
    {
        m_time_manager->pause_transport();
    }
    reset_logical_time();
    return Value::string(get_transport_state_string());
})

BUILTIN_ZEROARG(useq_clear, "useq-clear", {
    clear_all_outputs();
    return Value::string("cleared");
})

BUILTIN_ZEROARG(useq_get_transport_state, "useq-get-transport-state", {
    return Value::string(get_transport_state_string());
})

BUILTIN_ZEROARG(useq_rewind_logical_time, "useq-rewind", {
    reset_logical_time();
    return Value::string(get_transport_state_string());
})

Value ModuLispInterpreter::useq_fast(std::vector<Value>& args, Environment& env)
{
    DBG("ModuLispInterpreter::fast");
    constexpr const char* user_facing_name = "fast";

    BUILTIN_CHECK_ARITY("fast", 2)
    BUILTIN_EVAL_FIRST(1)
    BUILTIN_CHECK_ARG_NUM("fast", 0)

    // BODY
    double current_t = 0;
    auto t_val = env.get("t");
    if (t_val) current_t = t_val->as_float();

    double factor = args[0].as_float();
    double new_time_micros = (current_t * factor) * 1e+6;

    TemporalContext ctx;
    ctx.t = new_time_micros * 1e-6;
    ctx.beat = beat_at_time(new_time_micros);
    ctx.bar = bar_at_time(new_time_micros);
    ctx.phrase = phrase_at_time(new_time_micros);
    ctx.section = section_at_time(new_time_micros);
    ctx.beatDur = m_beat_length / 1000000.0 / factor;
    ctx.barDur = m_bar_length / 1000000.0 / factor;
    ctx.phraseDur = m_phrase_length / 1000000.0 / factor;
    ctx.sectionDur = m_section_length / 1000000.0 / factor;

    Environment new_env;
    new_env.set_temporal_context(&ctx);
    new_env.set_parent_scope(&env);
    return eval_in(args[1], new_env);
}

Value ModuLispInterpreter::useq_slow(std::vector<Value>& args, Environment& env)
{
    DBG("ModuLispInterpreter::useq_slow");
    constexpr const char* user_facing_name = "slow";

    BUILTIN_CHECK_ARITY("slow", 2)
    BUILTIN_EVAL_FIRST(1)
    BUILTIN_CHECK_ARG_NUM("slow", 0)

    // BODY
    double current_t = 0;
    auto t_val = env.get("t");
    if (t_val) current_t = t_val->as_float();

    double factor = args[0].as_float();
    double new_time_micros = (current_t / factor) * 1e+6;

    TemporalContext ctx;
    ctx.t = new_time_micros * 1e-6;
    ctx.beat = beat_at_time(new_time_micros);
    ctx.bar = bar_at_time(new_time_micros);
    ctx.phrase = phrase_at_time(new_time_micros);
    ctx.section = section_at_time(new_time_micros);
    ctx.beatDur = m_beat_length / 1000000.0 * factor;
    ctx.barDur = m_bar_length / 1000000.0 * factor;
    ctx.phraseDur = m_phrase_length / 1000000.0 * factor;
    ctx.sectionDur = m_section_length / 1000000.0 * factor;

    Environment new_env;
    new_env.set_temporal_context(&ctx);
    new_env.set_parent_scope(&env);
    return eval_in(args[1], new_env);
}

Value ModuLispInterpreter::useq_offset_time(std::vector<Value>& args,
                                            Environment& env)
{
    DBG("ModuLispInterpreter::offset");
    constexpr const char* user_facing_name = "offset";

    BUILTIN_CHECK_ARITY("offset", 2)
    BUILTIN_EVAL_FIRST(1)
    BUILTIN_CHECK_ARG_NUM("offset", 0)

    // BODY
    double current_t = 0;
    auto t_val = env.get("t");
    if (t_val) current_t = t_val->as_float();

    double amt             = args[0].as_float();
    double new_time_micros = (current_t + amt) * 1e+6;
    return eval_at_time(args[1], env, new_time_micros);
}

Value ModuLispInterpreter::useq_setbpm(std::vector<Value>& args, Environment& env)
{
    DBG("ModuLispInterpreter::useq_setbpm");
    constexpr const char* user_facing_name = "set-bpm";

    BUILTIN_CHECK_ARITY_RANGE("set-bpm", 1, 2)
    BUILTIN_EVAL_ARGS();
    BUILTIN_CHECK_ARG_NUM("set-bpm", 0)

    double thresh = 0.0;
    double newBpm = args[0].as_float();

    if (args.size() == 2)
    {
        BUILTIN_CHECK_ARG_NUM("set-bpm", 1)
        thresh = args[1].as_float();
    }

    set_bpm(newBpm, thresh);
    return args[0];
}

BUILTIN_NUMS(useq_set_time_sig, "set-time-sig", 2, {
    set_time_sig(args[0].as_float(), args[1].as_float());
    return Value::nil();
})

Value ModuLispInterpreter::useq_loop_at_time(std::vector<Value>& args,
                                             Environment& env)
{
    constexpr const char* user_facing_name = "loop-at-time";

    BUILTIN_CHECK_ARITY("loop-at-time", 2)
    BUILTIN_EVAL_FIRST(1)
    BUILTIN_CHECK_ARG_NUM("loop-at-time", 0)

    // BODY
    double current_time_s   = env.get("t").value().as_float();
    double modulo_time_s    = args[0].as_float();
    double new_time_seconds = fmod(current_time_s, modulo_time_s);
    double new_time_micros  = new_time_seconds * 1e+6;

    return eval_at_time(args[1], env, new_time_micros);
}

Value ModuLispInterpreter::useq_schedule(std::vector<Value>& args, Environment& env)
{
    DBG("uSEQ::lisp_schedule");
    constexpr const char* user_facing_name = "schedule";

    BUILTIN_CHECK_ARITY("schedule", 3)
    BUILTIN_EVAL_FIRST(2)
    BUILTIN_CHECK_ARG_STR("schedule", 0)
    BUILTIN_CHECK_ARG_NUM("schedule", 1)

    // BODY
    const auto itemName = args[0].as_string();
    const auto period   = static_cast<size_t>(args[1].as_float());
    const auto ast      = args[2];

    m_scheduler->schedule(itemName, ast, period);
    return Value::nil();
}

Value ModuLispInterpreter::useq_unschedule(std::vector<Value>& args,
                                           Environment& env)
{
    DBG("ModuLispInterpreter::useq_unschedule");
    constexpr const char* user_facing_name = "unschedule";

    BUILTIN_CHECK_ARITY("unschedule", 1)
    BUILTIN_EVAL_FIRST(1)
    BUILTIN_CHECK_ARG_STR("unschedule", 0)

    const String id = args[0].as_string();

    if (m_scheduler->unschedule(id))
    {
        println("- (unschedule) Item " + args[0].str + " removed successfully.");
    }
    else
    {
        println("- (unschedule) Item " + args[0].str + " not found; ignoring.");
    }
    return Value::nil();
}

#pragma GCC diagnostic pop
