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
// Interpolation / list-based sequence builtins:
// seq/fromList, flatseq/fromFlattenedList, interp, step, flatten
// ============================================================================

// NOTE: doesn't eval its arguments until they're selected by the phasor
Value fromList(std::vector<Value>& lst, double phasor, Environment& env)
{
    if (phasor < 0.0)
    {
        phasor = 0;
    }
    else if (phasor > 1.0)
    {
        phasor = 1.0;
    }
    double scaled_phasor = lst.size() * phasor;
    size_t idx           = floor(scaled_phasor);
    // keep index in bounds
    if (idx == lst.size())
        idx--;
    return ModuLispInterpreter::eval_in(lst[idx], env);
}

static Value flatten_impl(const Value& val, Environment& env)
{
    std::vector<Value> flattened;

    if (!val.is_sequential())
    {
        flattened.push_back(val);
    }
    else
    {
        auto valList = val.as_sequential();
        for (size_t i = 0; static_cast<size_t>(i) < valList.size(); i++)
        {
            Value evaluatedElement = ModuLispInterpreter::eval_in(valList[i], env);
            if (evaluatedElement.is_sequential())
            {
                auto flattenedElement =
                    flatten_impl(evaluatedElement, env).as_list();
                flattened.insert(flattened.end(), flattenedElement.begin(),
                                 flattenedElement.end());
            }
            else
            {
                flattened.push_back(evaluatedElement);
            }
        }
    }

    return val.is_vector() ? Value::vector(flattened) : Value(flattened);
}

// NOTE: fromList evals both of its args, including the list,
// to cover for cases where the user passes a symbol that points to a list
Value ModuLispInterpreter::useq_fromList(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "from-list";

    BUILTIN_CHECK_ARITY("from-list", 2)
    BUILTIN_EVAL_ARGS();
    BUILTIN_CHECK_ARG_SEQ("from-list", 0)
    BUILTIN_CHECK_ARG_NUM("from-list", 1)

    // BODY
    auto lst            = args[0].as_sequential();
    const double phasor = args[1].as_float();
    return fromList(lst, phasor, env);
}

Value ModuLispInterpreter::useq_flatten(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "flatten";

    BUILTIN_CHECK_ARITY("flatten", 1)
    BUILTIN_EVAL_FIRST(1)

    // BODY
    return flatten_impl(args[0], env);
}

Value ModuLispInterpreter::useq_fromFlattenedList(std::vector<Value>& args,
                                                  Environment& env)
{
    constexpr const char* user_facing_name = "from-flat-list";

    BUILTIN_CHECK_ARITY("from-flat-list", 2)
    BUILTIN_EVAL_ARGS();
    BUILTIN_CHECK_ARG_SEQ("from-flat-list", 0)
    BUILTIN_CHECK_ARG_NUM("from-flat-list", 1)

    // BODY
    auto lst      = flatten_impl(args[0], env).as_sequential();
    double phasor = args[1].as_float();
    return fromList(lst, phasor, env);
}

Value ModuLispInterpreter::useq_interpolate(std::vector<Value>& args,
                                            Environment& env)
{
    constexpr const char* user_facing_name = "interp";

    BUILTIN_CHECK_ARITY("interp", 2)
    BUILTIN_EVAL_ARGS();

    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(user_facing_name, 0, "a list or a vector",
                                         args[0].display());
        return Value::error();
    }
    BUILTIN_CHECK_ARG_NUM("interp", 1)

    // BODY
    auto lst      = args[0].as_list();
    double phasor = args[1].as_float();
    if (phasor < 0.0)
    {
        phasor = 0;
    }
    else if (phasor > 1)
    {
        phasor = 1;
    }
    float a;
    double index = phasor * (lst.size() - 1);
    size_t pos0  = static_cast<size_t>(index);
    if (pos0 == (lst.size() - 1))
        pos0--;
    a         = (index - pos0);
    double v2 = lst[pos0 + 1].as_float();
    double v1 = lst[pos0].as_float();
    return Value(((v2 - v1) * a) + v1);
}

BUILTIN_VARGS_NUMS(useq_step, "step", 2, 3, {
    const int count      = args[0].as_int();
    bool offset_provided = args.size() == 3;
    double phasor;
    double offset;

    if (offset_provided)
    {
        offset = args[1].as_float();
        phasor = args[2].as_float();
    }
    else
    {
        offset = 0;
        phasor = args[1].as_float();
    }

    double val = static_cast<int>(phasor * abs(count));
    if (val == count)
        val--;
    return Value((count > 0 ? val : count - 1 - val) + offset);
})

#pragma GCC diagnostic pop
