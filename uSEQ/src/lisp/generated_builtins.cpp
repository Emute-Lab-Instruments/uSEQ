#include "generated_builtins.h"

#include "value.h"

#include "environment.h"

#include "interpreter.h"

namespace builtin

{

Value tail(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(tail) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    
    
    // BODY
    Value result = Value::nil();
    std::vector<Value> acc, list = args[0].as_list();
    for (size_t i = 1; i < list.size(); i++)
        acc.push_back(list[i]);
    result = Value(acc);
    return result;
}

Value zeros(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(zeros) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(zeros) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    // Checking individual args
    if (!(args[0].is_number()))
    {
        error("(zeros) Argument #0 should evaluate to a number, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    int length = args[0].as_int();
    std::vector<Value> zeroList(length, Value(0));
    result = Value(zeroList);
    return result;
}

Value list(std::vector<Value>& args, Environment& env)
{
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(list) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    
    // BODY
    Value result = Value::nil();
    result = Value(args);
    return result;
}

Value ard_floor(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(ard_floor) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(ard_floor) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    // Checking individual args
    if (!(args[0].is_number()))
    {
        error("(ard_floor) Argument #0 should evaluate to a number, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    result = Value(floor(args[0].as_float()));
    return result;
}

Value ard_ceil(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(ard_ceil) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(ard_ceil) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    // Checking individual args
    if (!(args[0].is_number()))
    {
        error("(ard_ceil) Argument #0 should evaluate to a number, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    result = Value(ceil(args[0].as_float()));
    return result;
}

Value do_block(std::vector<Value>& args, Environment& env)
{
    
    
    
    // BODY
    Value result = Value::nil();
    Value acc;
    for (size_t i = 0; i < args.size(); i++)
        acc = args[i].eval(env);
    return acc;
    result = acc;
    return result;
}

Value neq(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 2))
    {
        error("(neq) Expected == 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(neq) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    
    // BODY
    Value result = Value::nil();
    result = Value(int(args[0] != args[1]));
    return result;
}

Value index(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 2))
    {
        error("(index) Expected == 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(index) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    // Checking individual args
    if (!(args[0].is_list()))
    {
        error("(index) Argument #0 should evaluate to a list, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    if (!(!args[0].is_empty()))
    {
        error("(index) Argument #0 should evaluate to a non-empty list, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    if (!(args[1].is_number()))
    {
        error("(index) Argument #1 should evaluate to a number, instead it evaluates to a " + args[1].get_type_name() + ":\n" + args[1].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    std::vector<Value> list = args[0].as_list();
    int i                   = args[1].as_int();
    if (i < list.size())
    {
        result = list[i];
    }
    else
    {
        error("(list) Index should be smaller than the size of the list.");
    }
    return result;
}

Value ard_cos(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(ard_cos) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(ard_cos) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    // Checking individual args
    if (!(args[0].is_number()))
    {
        error("(ard_cos) Argument #0 should evaluate to a number, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    result = Value(cos(args[0].as_float()));
    return result;
}

Value eq(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 2))
    {
        error("(eq) Expected == 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(eq) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    
    // BODY
    Value result = Value::nil();
    result = Value(int (args [0] == args [1]));
    return result;
}

Value ard_abs(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(ard_abs) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(ard_abs) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    // Checking individual args
    if (!(args[0].is_number()))
    {
        error("(ard_abs) Argument #0 should evaluate to a number, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    result = Value(abs(args[0].as_float()));
    return result;
}

Value ard_millis(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 0))
    {
        error("(ard_millis) Expected == 0 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(ard_millis) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    
    // BODY
    Value result = Value::nil();
    int m = millis();
    result = Value(m);
    return result;
}

Value sum(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() >= 2))
    {
        error("(sum) Expected >= 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(sum) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
        // Check all-pred(s)
        if (!(args[i].is_number()))
        {
            error("(sum) All arguments should evaluate to a number, but argument #" + String(i + 1) + " is a " + args[i].get_type_name() + " instead:\n" + args[i].display() + "\n");
            return Value::error();
        }
    }
    
    
    // BODY
    Value result = Value::nil();
    Value acc = args[0];
    for (size_t i = 1; i < args.size(); i++)
    {
    acc = acc + args[i];
    }
    result = acc;
    return result;
}

Value for_loop(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() >= 2))
    {
        error("(for_loop) Expected >= 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    
    // Checking individual args
    if (!(args[0].is_symbol()))
    {
        error("(for_loop) Argument #0 should evaluate to a null, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    if (!(args[1].is_list()))
    {
        error("(for_loop) Argument #1 should evaluate to a list, instead it evaluates to a " + args[1].get_type_name() + ":\n" + args[1].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    Value acc;
    std::vector<Value> list = args[1].eval(env).as_list();
    for (size_t i = 0; i < list.size(); i++)
    {
        env.set(args[0].as_atom(), list[i]);
        for (size_t j = 1; j < args.size() - 1; j++)
            args[j].eval(env);
        acc = args[args.size() - 1].eval(env);
    }
    result = acc;
    return result;
}

Value pop(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(pop) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(pop) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    
    // BODY
    Value result = Value::nil();
    result = args[0].pop();
    return result;
}

Value ard_min(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 2))
    {
        error("(ard_min) Expected == 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(ard_min) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
        // Check all-pred(s)
        if (!(args[i].is_number()))
        {
            error("(ard_min) All arguments should evaluate to a number, but argument #" + String(i + 1) + " is a " + args[i].get_type_name() + " instead:\n" + args[i].display() + "\n");
            return Value::error();
        }
    }
    
    
    // BODY
    Value result = Value::nil();
    result = Value(min(args[0].as_float(), args[1].as_float()));
    return result;
}

Value push(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 2))
    {
        error("(push) Expected == 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(push) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    
    // BODY
    Value result = Value::nil();
    for (size_t i = 1; i < args.size(); i++)
        args[0].push(args[i]);
    result = args[0];
    return result;
}

Value greater(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 2))
    {
        error("(greater) Expected == 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(greater) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    
    // BODY
    Value result = Value::nil();
    result = Value(int(args[0] > args[1]));
    return result;
}

Value product(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() >= 2))
    {
        error("(product) Expected >= 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(product) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
        // Check all-pred(s)
        if (!(args[i].is_number()))
        {
            error("(product) All arguments should evaluate to a number, but argument #" + String(i + 1) + " is a " + args[i].get_type_name() + " instead:\n" + args[i].display() + "\n");
            return Value::error();
        }
    }
    
    
    // BODY
    Value result = Value::nil();
    Value acc = args[0];
    for (size_t i = 1; i < args.size(); i++)
        acc = acc * args[i];
    result = acc;
    return result;
}

Value replace(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 3))
    {
        error("(replace) Expected == 3 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(replace) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
        // Check all-pred(s)
        if (!(args[i].is_string()))
        {
            error("(replace) All arguments should evaluate to a null, but argument #" + String(i + 1) + " is a " + args[i].get_type_name() + " instead:\n" + args[i].display() + "\n");
            return Value::error();
        }
    }
    
    
    // BODY
    Value result = Value::nil();
    String src = args[0].as_string();
    src.replace(args[1].as_string(), args[2].as_string());
    return Value::string(src);
    return result;
}

Value ard_sin(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(ard_sin) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(ard_sin) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    // Checking individual args
    if (!(args[0].is_number()))
    {
        error("(ard_sin) Argument #0 should evaluate to a number, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    result = Value(sin(args[0].as_float()));
    return result;
}

Value eval(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(eval) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    
    
    // BODY
    Value result = Value::nil();
    result = args[0].eval(env);
    return result;
}

Value cast_to_int(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(cast_to_int) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    
    // Checking individual args
    if (!(args[0].is_number()))
    {
        error("(cast_to_int) Argument #0 should evaluate to a number, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    result = args[0].cast_to_int();
    return result;
}

Value remainder(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 2))
    {
        error("(remainder) Expected == 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(remainder) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
        // Check all-pred(s)
        if (!(args[i].is_number()))
        {
            error("(remainder) All arguments should evaluate to a number, but argument #" + String(i + 1) + " is a " + args[i].get_type_name() + " instead:\n" + args[i].display() + "\n");
            return Value::error();
        }
    }
    
    
    // BODY
    Value result = Value::nil();
    result = args[0] % args[1];
    return result;
}

Value subtract(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 2))
    {
        error("(subtract) Expected == 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(subtract) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
        // Check all-pred(s)
        if (!(args[i].is_number()))
        {
            error("(subtract) All arguments should evaluate to a number, but argument #" + String(i + 1) + " is a " + args[i].get_type_name() + " instead:\n" + args[i].display() + "\n");
            return Value::error();
        }
    }
    
    
    // BODY
    Value result = Value::nil();
    result = args[0] - args[1];
    return result;
}

Value define(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 2))
    {
        error("(define) Expected == 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    
    // Checking individual args
    if (!(args[0].is_symbol()))
    {
        error("(define) Argument #0 should evaluate to a null, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    result = args[1].eval(env);
    env.set(args[0].display(), result);
    return result;
}

Value ard_pow(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 2))
    {
        error("(ard_pow) Expected == 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(ard_pow) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
        // Check all-pred(s)
        if (!(args[i].is_number()))
        {
            error("(ard_pow) All arguments should evaluate to a number, but argument #" + String(i + 1) + " is a " + args[i].get_type_name() + " instead:\n" + args[i].display() + "\n");
            return Value::error();
        }
    }
    
    
    // BODY
    Value result = Value::nil();
    result = Value(pow(args[0].as_float(), args[1].as_float()));
    return result;
}

Value while_loop(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() >= 1))
    {
        error("(while_loop) Expected >= 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    
    
    // BODY
    Value result = Value::nil();
    Value acc;
    while (args[0].eval(env).as_bool())
    {
        for (size_t i = 1; i < args.size() - 1; i++)
            args[i].eval(env);
        acc = args[args.size() - 1].eval(env);
    }
    result = acc;
    return result;
}

Value remove(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 2))
    {
        error("(remove) Expected == 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(remove) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    // Checking individual args
    if (!(args[0].is_list()))
    {
        error("(remove) Argument #0 should evaluate to a list, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    if (!(args[1].is_number()))
    {
        error("(remove) Argument #1 should evaluate to a number, instead it evaluates to a " + args[1].get_type_name() + ":\n" + args[1].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    std::vector<Value> list = args[0].as_list();
        int i                   = args[1].as_int();
        if (list.empty() || i >= (int)list.size())
            error(INDEX_OUT_OF_RANGE);
        else
            list.erase(list.begin() + i);
    result = Value(list);
    return result;
}

Value scope(std::vector<Value>& args, Environment& env)
{
    
    
    
    // BODY
    Value result = Value::nil();
    Environment e = env;
    Value acc;
    for (size_t i = 0; i < args.size(); i++)
        acc = args[i].eval(e);
    result = acc;
    return result;
}

Value debug(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(debug) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(debug) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    
    // BODY
    Value result = Value::nil();
    result = Value::string(args[0].debug());
    return result;
}

Value less_eq(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 2))
    {
        error("(less_eq) Expected == 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(less_eq) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    
    // BODY
    Value result = Value::nil();
    result = Value(int(args[0] <= args[1]));
    return result;
}

Value ard_max(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 2))
    {
        error("(ard_max) Expected == 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(ard_max) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
        // Check all-pred(s)
        if (!(args[i].is_number()))
        {
            error("(ard_max) All arguments should evaluate to a number, but argument #" + String(i + 1) + " is a " + args[i].get_type_name() + " instead:\n" + args[i].display() + "\n");
            return Value::error();
        }
    }
    
    
    // BODY
    Value result = Value::nil();
    result = Value(max(args[0].as_float(), args[1].as_float()));
    return result;
}

Value ard_delay(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(ard_delay) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(ard_delay) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    // Checking individual args
    if (!(args[0].is_number()))
    {
        error("(ard_delay) Argument #0 should evaluate to a number, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    int delaytime = args[0].as_int();
    delay(delaytime);
    result = args[0];
    return result;
}

Value timeit(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(timeit) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(timeit) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    
    // BODY
    Value result = Value::nil();
    unsigned long ts = micros();
    args[0].eval(env);
    ts = micros() - ts;
    result = Value(static_cast<int>(ts));
    return result;
}

Value ard_map(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 5))
    {
        error("(ard_map) Expected == 5 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(ard_map) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    
    // BODY
    Value result = Value::nil();
    float m = map(args[0].as_float(), args[1].as_float(),
                          args[2].as_float(), args[3].as_float(),
                          args[4].as_float());
    result = Value(m);
    return result;
}

Value head(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(head) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    
    // Checking individual args
    if (!(args[0].is_list()))
    {
        error("(head) Argument #0 should evaluate to a list, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    std::vector<Value> list = args[0].as_list();
    if (list.empty())
    {
        error(INDEX_OUT_OF_RANGE);
    }
    else
    {
        result = list[0];
    }
    return result;
}

Value get_type_name(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(get_type_name) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(get_type_name) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    
    // BODY
    Value result = Value::nil();
    result = Value::string(args[0].get_type_name());
    return result;
}

Value less(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 2))
    {
        error("(less) Expected == 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(less) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    
    // BODY
    Value result = Value::nil();
    result = Value(int(args[0] > args[1]));
    return result;
}

Value ard_micros(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 0))
    {
        error("(ard_micros) Expected == 0 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(ard_micros) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    
    // BODY
    Value result = Value::nil();
    int m = micros();
    result = Value(m);
    return result;
}

Value divide(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 2))
    {
        error("(divide) Expected == 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(divide) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
        // Check all-pred(s)
        if (!(args[i].is_number()))
        {
            error("(divide) All arguments should evaluate to a number, but argument #" + String(i + 1) + " is a " + args[i].get_type_name() + " instead:\n" + args[i].display() + "\n");
            return Value::error();
        }
    }
    
    
    // BODY
    Value result = Value::nil();
    result = args[0] / args[1];
    return result;
}

Value ard_delaymicros(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(ard_delaymicros) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(ard_delaymicros) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    // Checking individual args
    if (!(args[0].is_number()))
    {
        error("(ard_delaymicros) Argument #0 should evaluate to a number, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    int  delaytime = args[0].as_int();
    delayMicroseconds(delaytime);
    result = args[0];
    return result;
}

Value insert(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 3))
    {
        error("(insert) Expected == 3 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(insert) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    // Checking individual args
    if (!(args[0].is_list()))
    {
        error("(insert) Argument #0 should evaluate to a list, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    if (!(args[1].is_number()))
    {
        error("(insert) Argument #1 should evaluate to a number, instead it evaluates to a " + args[1].get_type_name() + ":\n" + args[1].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    std::vector<Value> list = args[0].as_list();
    int i                   = args[1].as_int();
    if (i < list.size())
        Serial.println(INDEX_OUT_OF_RANGE);
    else
        list.insert(list.begin() + args[1].as_int(), args[2].as_int());
    result = Value(list);
    return result;
}

Value ard_sqrt(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(ard_sqrt) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(ard_sqrt) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    // Checking individual args
    if (!(args[0].is_number()))
    {
        error("(ard_sqrt) Argument #0 should evaluate to a number, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    result = Value(sqrt(args[0].as_float()));
    return result;
}

Value display(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(display) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(display) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    
    // BODY
    Value result = Value::nil();
    result = Value::string(args[0].display());
    return result;
}

Value greater_eq(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 2))
    {
        error("(greater_eq) Expected == 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(greater_eq) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    
    // BODY
    Value result = Value::nil();
    result = Value(int(args[0] >= args[1]));
    return result;
}

Value quote(std::vector<Value>& args, Environment& env)
{
    
    
    
    // BODY
    Value result = Value::nil();
    result = Value(args);
    return result;
}

Value cast_to_float(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(cast_to_float) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    
    // Checking individual args
    if (!(args[0].is_number()))
    {
        error("(cast_to_float) Argument #0 should evaluate to a number, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    result = Value(args[0].as_float());
    return result;
}

Value len(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(len) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(len) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    // Checking individual args
    if (!(args[0].is_list()))
    {
        error("(len) Argument #0 should evaluate to a list, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    result = Value((int)args[0].as_list().size());
    return result;
}

Value set(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 2))
    {
        error("(set) Expected == 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    
    // Checking individual args
    if (!(args[0].is_symbol()))
    {
        error("(set) Argument #0 should evaluate to a null, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    result = args[1].eval(env);
    env.set_global(args[0].display(), result);
    return result;
}

Value defun(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 3))
    {
        error("(defun) Expected == 3 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    
    // Checking individual args
    if (!(args[0].is_symbol()))
    {
        error("(defun) Argument #0 should evaluate to a null, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    if (!(args[1].is_list()))
    {
        error("(defun) Argument #1 should evaluate to a list, instead it evaluates to a " + args[1].get_type_name() + ":\n" + args[1].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    String f_name             = args[0].display();
    std::vector<Value> params = args[1].as_list();
    Value body                = args[2];
    result                    = Value(params, body, env);
    env.set(f_name, result);
    return result;
}

Value useq_sqr(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(useq_sqr) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(useq_sqr) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    // Checking individual args
    if (!(args[0].is_number()))
    {
        error("(useq_sqr) Argument #0 should evaluate to a number, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    result = Value(fmod(args[0].as_float(), 1.0) < 0.5 ? 1.0 : 0.0);
    return result;
}

Value lambda(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 2))
    {
        error("(lambda) Expected == 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    
    // Checking individual args
    if (!(args[0].is_list()))
    {
        error("(lambda) Argument #0 should evaluate to a list, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    result = Value(args[0].as_list(), args[1], env);
    return result;
}

Value useq_pulse(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 2))
    {
        error("(useq_pulse) Expected == 2 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(useq_pulse) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    // Checking individual args
    if (!(args[0].is_number()))
    {
        error("(useq_pulse) Argument #0 should evaluate to a number, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    if (!(args[1].is_number()))
    {
        error("(useq_pulse) Argument #1 should evaluate to a number, instead it evaluates to a " + args[1].get_type_name() + ":\n" + args[1].display());
        return Value::error();
    }
    if (!(args[2].is_number()))
    {
        error("(useq_pulse) Argument #2 should evaluate to a number, instead it evaluates to a " + args[2].get_type_name() + ":\n" + args[2].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    // args: pulse width, phasor
    double pulseWidth = args[0].as_float();
    double phasor = args[1].as_float();
    result = Value(pulseWidth < phasor ? 1.0 : 0.0);
    return result;
}

Value ard_tan(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 1))
    {
        error("(ard_tan) Expected == 1 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            error("(ard_tan) Argument #" + String(i + 1) + " evaluates to an error:\n" + pre_eval.display());
            return Value::error();
        }
        
    }
    
    // Checking individual args
    if (!(args[0].is_number()))
    {
        error("(ard_tan) Argument #0 should evaluate to a number, instead it evaluates to a " + args[0].get_type_name() + ":\n" + args[0].display());
        return Value::error();
    }
    
    // BODY
    Value result = Value::nil();
    result = Value(tan(args[0].as_float()));
    return result;
}

Value if_then_else(std::vector<Value>& args, Environment& env)
{
    // Checking number of args
    if (!(args.size() == 3))
    {
        error("(if_then_else) Expected == 3 args, received " + String(args.size()) + " instead.");
        return Value::error();
    }
    
    
    
    // BODY
    Value result = Value::nil();
    if (args[0].eval(env).as_bool())
            result = args[1].eval(env);
        else
            result = args[2].eval(env);
    return result;
}

} // namespace builtin