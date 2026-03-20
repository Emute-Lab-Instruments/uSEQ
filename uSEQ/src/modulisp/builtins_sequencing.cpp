// Suppress all warnings for this file (consistent with modulisp_api.cpp)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wpedantic"

#include "../utils.h"
#include "modulisp_interpreter.h"
#include "lisp/macros.h"
#include <cmath>

// ============================================================================
// Sequencing builtins: euclidean, eu, gates, gatesw, trigs, ratiotrig,
// ratiostep, ratioindex, ratiowarp, dm, shift (phasor_offset), tri
// ============================================================================

// Forward declaration for fromList helper (defined in builtins_interpolation.cpp)
Value fromList(std::vector<Value>& lst, double phasor, Environment& env);

BUILTIN_NUMS(useq_dm, "dm", 3, {
    int index = args[0].as_int();
    double v1 = args[1].as_float();
    double v2 = args[2].as_float();
    return Value(index > 0 ? v2 : v1);
})

Value ModuLispInterpreter::useq_tri(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "tri";

    BUILTIN_CHECK_ARITY("tri", 2)
    // tri does NOT eval its args (they're raw numbers)
    BUILTIN_CHECK_ARG_NUM("tri", 0)
    BUILTIN_CHECK_ARG_NUM("tri", 1)

    double duty = args[0].as_float();
    if (duty < 0.01)
    {
        duty = 0.01;
    }
    if (duty > 0.99)
    {
        duty = 0.99;
    }
    double phase = args[1].as_float();
    if (phase > duty)
    {
        phase = (duty - ((phase - duty) * (duty / (1 - duty))));
    }
    return Value(phase / duty);
}

Value ModuLispInterpreter::useq_gates(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "gates";

    BUILTIN_CHECK_ARITY_RANGE("gates", 2, 3)
    BUILTIN_EVAL_ARGS();
    BUILTIN_CHECK_ARG_SEQ("gates", 0)

    // Remaining args must be numbers
    for (size_t i = 1; i < args.size(); ++i)
    {
        BUILTIN_CHECK_ARG_NUM("gates", i)
    }

    // BODY
    bool pulse_width_specified = args.size() == 3;
    auto gates_vec             = args[0].as_sequential();

    double pulseWidth;
    double phasor;
    if (pulse_width_specified)
    {
        pulseWidth = args[1].as_float();
        phasor     = args[2].as_float();
    }
    else
    {
        pulseWidth = 0.5;
        phasor     = args[1].as_float();
    }

    const double val = fromList(gates_vec, phasor, env).as_int();
    const double gates =
        (fmod(phasor * gates_vec.size(), 1.0)) < pulseWidth ? 1.0 : 0.0;
    return Value(val * gates);
}

Value ModuLispInterpreter::useq_gatesw(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "gatesw";

    BUILTIN_CHECK_ARITY("gatesw", 2)
    BUILTIN_EVAL_ARGS();
    BUILTIN_CHECK_ARG_SEQ("gatesw", 0)
    BUILTIN_CHECK_ARG_NUM("gatesw", 1)

    // BODY
    auto gates_vec               = args[0].as_sequential();
    const double phasor          = args[1].as_float();
    const double val             = fromList(gates_vec, phasor, env).as_int();
    const double pulseWidth      = val / 9.0;
    const double relative_phasor = fmod(phasor * gates_vec.size(), 1.0);
    const double gate            = relative_phasor < pulseWidth ? 1.0 : 0.0;

    return Value((val > 0 ? 1.0 : 0.0) * gate);
}

Value ModuLispInterpreter::useq_trigs(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "trigs";

    BUILTIN_CHECK_ARITY_RANGE("trigs", 2, 3)
    BUILTIN_EVAL_ARGS();
    BUILTIN_CHECK_ARG_SEQ("trigs", 0)

    if (!(args.back().is_number()))
    {
        report_error_wrong_all_pred(user_facing_name,
            static_cast<int>(args.size()) + 1, "a number", args[1].display());
        return Value::error();
    }

    // BODY
    auto lst = args[0].as_sequential();
    const double phasor     = args.back().as_float();
    const double val        = fromList(lst, phasor, env).as_int();
    const double amp        = std::clamp(val / 9.0, 0.0, 1.0);
    const double pulseWidth = args.size() == 3 ? args[1].as_float() : 0.1;
    const double gate = fmod(phasor * lst.size(), 1.0) < pulseWidth ? 1.0 : 0.0;
    return Value((val > 0 ? 1.0 : 0.0) * gate * amp);
}

BUILTIN_VARGS_NUMS(useq_euclidean, "euclid", 3, 5, {
    const double phasor = args.back().as_float();
    const int n         = args[0].as_int();
    const int k         = args[1].as_int();

    float pulseWidth = 0.5f;
    int offset = 0;

    if (args.size() == 4) {
        pulseWidth = args[2].as_float();
    } else if (args.size() == 5) {
        pulseWidth = args[2].as_float();
        offset = args[3].as_int();
    }

    const float fi = phasor * n;
    int i = static_cast<int>(fi);
    const float rem = fi - i;
    if (i == n)
    {
        i--;
    }

    const int idx = ((i + n - offset) * k) % n;
    return Value(idx < k && rem < pulseWidth ? 1 : 0);
})

BUILTIN_VARGS_NUMS(useq_eu, "eu", 3, 5, {
    const double phasor = args.back().as_float();
    const int n         = args[0].as_int();
    const int k         = args[1].as_int();

    float pulseWidth = 0.5f;
    int offset = 0;

    if (args.size() == 4) {
        pulseWidth = args[2].as_float();
    } else if (args.size() == 5) {
        pulseWidth = args[2].as_float();
        offset = args[3].as_int();
    }

    const float fi = phasor * n;
    int i = static_cast<int>(fi);
    const float rem = fi - i;
    if (i == n)
    {
        i--;
    }

    const int idx = ((i + n - offset) * k) % n;
    return Value(idx < k && rem < pulseWidth ? 1 : 0);
})

Value ModuLispInterpreter::useq_ratiotrig(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "rpulse";

    BUILTIN_CHECK_ARITY("rpulse", 3)
    BUILTIN_EVAL_ARGS();
    BUILTIN_CHECK_ARG_SEQ("rpulse", 0)
    BUILTIN_CHECK_ARG_NUM("rpulse", 1)
    BUILTIN_CHECK_ARG_NUM("rpulse", 2)

    // BODY
    auto ratios           = args[0].as_sequential();
    const auto pulseWidth = args[1].as_float();
    const auto phase      = args[2].as_float();

    double trig     = 0;
    double ratioSum = 0;
    for (Value v : ratios)
    {
        ratioSum += v.as_float();
    }
    double phaseAdj           = ratioSum * phase;
    double accumulatedSum     = 0;
    double lastAccumulatedSum = 0;
    for (Value v : ratios)
    {
        accumulatedSum += v.as_float();
        if (phaseAdj <= accumulatedSum)
        {
            double beatPhase = (phaseAdj - lastAccumulatedSum) /
                               (accumulatedSum - lastAccumulatedSum);
            trig = beatPhase <= pulseWidth;
            break;
        }
        lastAccumulatedSum = accumulatedSum;
    }
    return Value(trig);
}

Value ModuLispInterpreter::useq_ratiostep(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "rstep";

    BUILTIN_CHECK_ARITY("rstep", 2)
    BUILTIN_EVAL_ARGS();
    BUILTIN_CHECK_ARG_SEQ("rstep", 0)
    BUILTIN_CHECK_ARG_NUM("rstep", 1)

    // BODY
    auto ratios      = args[0].as_sequential();
    const auto phase = args[1].as_float();

    double phaseOut = 0;
    double ratioSum = 0;
    for (Value v : ratios)
    {
        ratioSum += v.as_float();
    }
    double phaseAdj           = ratioSum * phase;
    double accumulatedSum     = 0;
    double lastAccumulatedSum = 0;
    for (Value v : ratios)
    {
        accumulatedSum += v.as_float();
        if (phaseAdj <= accumulatedSum)
        {
            phaseOut = lastAccumulatedSum;
            break;
        }
        lastAccumulatedSum = accumulatedSum;
    }
    return Value(phaseOut / ratioSum);
}

Value ModuLispInterpreter::useq_ratioindex(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "ridx";

    BUILTIN_CHECK_ARITY("ridx", 2)
    BUILTIN_EVAL_ARGS();
    BUILTIN_CHECK_ARG_SEQ("ridx", 0)
    BUILTIN_CHECK_ARG_NUM("ridx", 1)

    // BODY
    auto ratios      = args[0].as_sequential();
    const auto phase = args[1].as_float();

    double index    = 0;
    double ratioSum = 0;
    for (Value v : ratios)
    {
        ratioSum += v.as_float();
    }
    double phaseAdj       = ratioSum * phase;
    double accumulatedSum = 0;
    for (Value v : ratios)
    {
        accumulatedSum += v.as_float();
        if (phaseAdj <= accumulatedSum)
        {
            break;
        }
        index++;
    }
    index /= static_cast<double>(ratios.size());
    return Value(index);
}

Value ModuLispInterpreter::useq_ratiowarp(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "rwarp";

    BUILTIN_CHECK_ARITY("rwarp", 2)
    BUILTIN_EVAL_ARGS();
    BUILTIN_CHECK_ARG_SEQ("rwarp", 0)
    BUILTIN_CHECK_ARG_NUM("rwarp", 1)

    // BODY
    auto ratios      = args[0].as_sequential();
    const auto phase = args[1].as_float();

    double output = 0;

    if (ratios.size() > 0)
    {
        double index      = 0;
        double indexWidth = 1.0 / ratios.size();

        double ratioSum = 0;
        for (Value v : ratios)
        {
            ratioSum += v.as_float();
        }

        double phaseAdj           = ratioSum * phase;
        double accumulatedSum     = 0;
        double lastAccumulatedSum = 0;
        for (Value v : ratios)
        {
            accumulatedSum += v.as_float();
            if (phaseAdj <= accumulatedSum)
            {
                double beatPhase = (phaseAdj - lastAccumulatedSum) /
                                   (accumulatedSum - lastAccumulatedSum);
                output = (index * indexWidth) + (beatPhase * indexWidth);
                break;
            }
            lastAccumulatedSum = accumulatedSum;
            index++;
        }
    }

    return Value(output);
}

BUILTIN_NUMS(useq_phasor_offset, "shift", 2, {
    const auto offset = args[0].as_float();
    auto phase        = args[1].as_float();
    phase  = std::fmod(phase + offset, 1.0);
    return Value(phase);
})

#pragma GCC diagnostic pop
