#ifndef MACROS_H_
#define MACROS_H_

// Needs access to ModuLispInterpreter static helpers
#include "../modulisp_interpreter.h"

// NON-MEMBER FUNCTIONS
#define BUILTINFUNC(__name__, __body__, __numArgs__)                                \
    Value __name__(std::vector<Value>& args, Environment& env)                      \
    {                                                                               \
        ModuLispInterpreter::eval_args(args, env);                                  \
        Value ret = Value();                                                        \
        if (args.size() != __numArgs__)                                             \
        {                                                                           \
            ::println(args.size() > __numArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS);    \
            ret = Value::error();                                                   \
        }                                                                           \
        else                                                                        \
        {                                                                           \
            __body__                                                                \
        }                                                                           \
        return ret;                                                                 \
    }

#define BUILTINFUNC_VARGS(__name__, __body__, __minArgs__, __maxArgs__)             \
    Value __name__(std::vector<Value>& args, Environment& env)                      \
    {                                                                               \
        ModuLispInterpreter::eval_args(args, env);                                  \
        Value ret = Value();                                                        \
        if (args.size() < __minArgs__ || args.size() > __maxArgs__)                 \
            ::println(args.size() > __maxArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS);    \
        else                                                                        \
        {                                                                           \
            __body__                                                                \
        }                                                                           \
        return ret;                                                                 \
    }

#define BUILTINFUNC_NOEVAL(__name__, __body__, __numArgs__)                         \
    Value __name__(std::vector<Value>& args, Environment& env)                      \
    {                                                                               \
        Value ret = Value();                                                        \
        if (args.size() != __numArgs__)                                             \
            ::println(args.size() > __numArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS);    \
        else                                                                        \
        {                                                                           \
            __body__                                                                \
        }                                                                           \
        return ret;                                                                 \
    }

// uSEQ MEMBER FUNCTIONS
#define BUILTINFUNC_MEMBER(__name__, __body__, __numArgs__)                         \
    Value uSEQ::__name__(std::vector<Value>& args, Environment& env)                \
    {                                                                               \
        ModuLispInterpreter::eval_args(args, env);                                  \
        Value ret = Value();                                                        \
        if (args.size() != __numArgs__)                                             \
        {                                                                           \
            ::println(args.size() > __numArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS);    \
            ret = Value::error();                                                   \
        }                                                                           \
        else                                                                        \
        {                                                                           \
            __body__                                                                \
        }                                                                           \
        return ret;                                                                 \
    }

#define BUILTINFUNC_VARGS_MEMBER(__name__, __body__, __minArgs__, __maxArgs__)      \
    Value uSEQ::__name__(std::vector<Value>& args, Environment& env)                \
    {                                                                               \
        Interpreter::eval_args(args, env);                                          \
        Value ret = Value();                                                        \
        if (args.size() < __minArgs__ || args.size() > __maxArgs__)                 \
            ::println(args.size() > __maxArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS);    \
        else                                                                        \
        {                                                                           \
            __body__                                                                \
        }                                                                           \
        return ret;                                                                 \
    }

#define BUILTINFUNC_NOEVAL_MEMBER(__name__, __body__, __numArgs__)                  \
    Value uSEQ::__name__(std::vector<Value>& args, Environment& env)                \
    {                                                                               \
        Value ret = Value();                                                        \
        if (args.size() != __numArgs__)                                             \
            ::println(args.size() > __numArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS);    \
        else                                                                        \
        {                                                                           \
            __body__                                                                \
        }                                                                           \
        return ret;                                                                 \
    }

// ============================================================
// Composable validation macros
// ============================================================

// Arity checks
#define BUILTIN_CHECK_ARITY(lisp_name, expected) \
    if (args.size() != static_cast<size_t>(expected)) { \
        report_error_wrong_num_args(lisp_name, \
            static_cast<int>(args.size()), \
            NumArgsComparison::EqualTo, expected, -1); \
        return Value::error(); \
    }

#define BUILTIN_CHECK_ARITY_RANGE(lisp_name, min_a, max_a) \
    if (args.size() < static_cast<size_t>(min_a) || args.size() > static_cast<size_t>(max_a)) { \
        report_error_wrong_num_args(lisp_name, \
            static_cast<int>(args.size()), \
            NumArgsComparison::Between, min_a, max_a); \
        return Value::error(); \
    }

#define BUILTIN_CHECK_MIN_ARITY(lisp_name, min_a) \
    if (args.size() < static_cast<size_t>(min_a)) { \
        report_error_wrong_num_args(lisp_name, \
            static_cast<int>(args.size()), \
            NumArgsComparison::AtLeast, min_a, -1); \
        return Value::error(); \
    }

// Arg evaluation
#define BUILTIN_EVAL_ARGS() \
    ModuLispInterpreter::eval_args(args, env)

#define BUILTIN_EVAL_FIRST(n) \
    for (size_t _bi = 0; _bi < static_cast<size_t>(n) && _bi < args.size(); ++_bi) { \
        args[_bi] = args[_bi].eval(env); \
        if (args[_bi].is_error()) return Value::error(); \
    }

// Type checks
#define BUILTIN_CHECK_ALL_NUMS(lisp_name) \
    for (size_t _bi = 0; _bi < args.size(); ++_bi) { \
        if (!args[_bi].is_number()) { \
            report_error_wrong_all_pred(lisp_name, \
                static_cast<int>(_bi) + 1, "a number", args[_bi].display()); \
            return Value::error(); \
        } \
    }

#define BUILTIN_CHECK_ARG_NUM(lisp_name, idx) \
    if (!args[idx].is_number()) { \
        report_error_wrong_specific_pred(lisp_name, \
            (idx) + 1, "a number", args[idx].display()); \
        return Value::error(); \
    }

#define BUILTIN_CHECK_ARG_SEQ(lisp_name, idx) \
    if (!args[idx].is_sequential()) { \
        report_error_wrong_specific_pred(lisp_name, \
            (idx) + 1, "a list or vector", args[idx].display()); \
        return Value::error(); \
    }

#define BUILTIN_CHECK_ARG_STR(lisp_name, idx) \
    if (!args[idx].is_string()) { \
        report_error_wrong_specific_pred(lisp_name, \
            (idx) + 1, "a string", args[idx].display()); \
        return Value::error(); \
    }

// ============================================================
// Convenience wrapper macros (ModuLispInterpreter methods)
// ============================================================

#define BUILTIN_NUMS(name, lisp_name, numArgs, body) \
    Value ModuLispInterpreter::name(std::vector<Value>& args, Environment& env) \
    { \
        constexpr const char* user_facing_name = lisp_name; \
        (void)user_facing_name; \
        BUILTIN_CHECK_ARITY(lisp_name, numArgs) \
        BUILTIN_EVAL_ARGS(); \
        BUILTIN_CHECK_ALL_NUMS(lisp_name) \
        body \
    }

#define BUILTIN_ANY(name, lisp_name, numArgs, body) \
    Value ModuLispInterpreter::name(std::vector<Value>& args, Environment& env) \
    { \
        constexpr const char* user_facing_name = lisp_name; \
        (void)user_facing_name; \
        BUILTIN_CHECK_ARITY(lisp_name, numArgs) \
        BUILTIN_EVAL_ARGS(); \
        body \
    }

#define BUILTIN_VARGS(name, lisp_name, minArgs, maxArgs, body) \
    Value ModuLispInterpreter::name(std::vector<Value>& args, Environment& env) \
    { \
        constexpr const char* user_facing_name = lisp_name; \
        (void)user_facing_name; \
        BUILTIN_CHECK_ARITY_RANGE(lisp_name, minArgs, maxArgs) \
        BUILTIN_EVAL_ARGS(); \
        body \
    }

#define BUILTIN_VARGS_NUMS(name, lisp_name, minArgs, maxArgs, body) \
    Value ModuLispInterpreter::name(std::vector<Value>& args, Environment& env) \
    { \
        constexpr const char* user_facing_name = lisp_name; \
        (void)user_facing_name; \
        BUILTIN_CHECK_ARITY_RANGE(lisp_name, minArgs, maxArgs) \
        BUILTIN_EVAL_ARGS(); \
        BUILTIN_CHECK_ALL_NUMS(lisp_name) \
        body \
    }

#define BUILTIN_PARTIAL(name, lisp_name, numArgs, numEval, body) \
    Value ModuLispInterpreter::name(std::vector<Value>& args, Environment& env) \
    { \
        constexpr const char* user_facing_name = lisp_name; \
        (void)user_facing_name; \
        BUILTIN_CHECK_ARITY(lisp_name, numArgs) \
        BUILTIN_EVAL_FIRST(numEval) \
        body \
    }

#define BUILTIN_NOEVAL(name, lisp_name, numArgs, body) \
    Value ModuLispInterpreter::name(std::vector<Value>& args, Environment& env) \
    { \
        constexpr const char* user_facing_name = lisp_name; \
        (void)user_facing_name; \
        BUILTIN_CHECK_ARITY(lisp_name, numArgs) \
        body \
    }

#define BUILTIN_ZEROARG(name, lisp_name, body) \
    Value ModuLispInterpreter::name(std::vector<Value>& args, Environment& env) \
    { \
        constexpr const char* user_facing_name = lisp_name; \
        (void)user_facing_name; (void)args; (void)env; \
        body \
    }

// ============================================================
// uSEQ output macros
// ============================================================

#define DEFINE_USEQ_OUTPUT_SETTER(name, index, output_type) \
    Value uSEQ::useq_##name(std::vector<Value>& args, Environment& env) \
    { \
        return m_output_manager->handle_output_setter( \
            index, OutputManager::OutputType::output_type, args, env); \
    }

#define DEFINE_USEQ_OUTPUT_GETTER(name, index, output_type) \
    Value uSEQ::useq_get_##name(std::vector<Value>& args, Environment& env) \
    { \
        return m_output_manager->handle_output_getter( \
            index, OutputManager::OutputType::output_type, args, env); \
    }

#define DEFINE_USEQ_INPUT_GETTER(name, input_id) \
    Value uSEQ::useq_##name(std::vector<Value>& args, Environment& env) \
    { \
        (void)args; (void)env; \
        return m_io_manager ? Value(m_io_manager->get_input_value(input_id)) \
                            : Value::nil(); \
    }

#endif // MACROS_H_
