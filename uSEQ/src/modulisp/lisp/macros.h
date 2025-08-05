#ifndef MACROS_H_
#define MACROS_H_

// NON-MEMBER FUNCTIONS
#define BUILTINFUNC(__name__, __body__, __numArgs__)                                \
    Value __name__(std::vector<Value>& args, Environment& env)                      \
    {                                                                               \
        Interpreter::eval_args(args, env);                                          \
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
        eval_args(args, env);                                                       \
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
        eval_args(args, env);                                                       \
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

#endif // MACROS_H_
