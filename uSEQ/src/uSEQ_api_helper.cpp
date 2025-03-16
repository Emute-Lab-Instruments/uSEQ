#include "uSEQ.h"



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
    return Interpreter::eval_in(lst[idx], env);
}

Value flatten_impl(const Value& val, Environment& env)
{
    std::vector<Value> flattened;

    // int original_type = val.type;

    if (!val.is_sequential())
    {
        flattened.push_back(val);
    }
    else
    {
        auto valList = val.as_sequential();
        for (size_t i = 0; i < valList.size(); i++)
        {
            Value evaluatedElement = Interpreter::eval_in(valList[i], env);
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

    Value result;

    // Only return a list if the input was a list,
    // otherwise a vec
    // if (original_type == Value::LIST)
    // {
    //     result = Value(flattened);
    // }
    // else
    // {
    //     result = Value::vector(flattened);
    // }

    result = Value::vector(flattened);
    return result;
}