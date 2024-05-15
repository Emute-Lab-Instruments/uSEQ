Value eval(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    if (args.size() != 1)
    {
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return Value();
    }
    // throw Error(Value("eval", eval), env, args.size() > 1? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    else
    {
        return args[0].eval(env);
    }
}

// Create a list of values
Value list(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    return Value(args);
}

// Sum multiple values
Value sum(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() < 2)
        Serial.println(TOO_FEW_ARGS);
    // throw Error(Value("+", sum), env, TOO_FEW_ARGS);

    Value acc = args[0];
    for (size_t i = 1; i < args.size(); i++)
        acc = acc + args[i];
    return acc;
}

// Subtract two values
Value subtract(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("-", subtract), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    return args[0] - args[1];
}

// Multiply several values
Value product(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() < 2)
        Serial.println(TOO_FEW_ARGS);
    // throw Error(Value("*", product), env, TOO_FEW_ARGS);

    Value acc = args[0];
    for (size_t i = 1; i < args.size(); i++)
        acc = acc * args[i];
    return acc;
}

// Divide two values
Value divide(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("/", divide), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    auto result = args[0] / args[1];
    return result;
}

// Get the remainder of values
Value remainder(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("%", remainder), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    auto result = args[0] % args[1];
    return result;
}

// Are two values equal?
Value eq(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("=", eq), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    return Value(int(args[0] == args[1]));
}

// Are two values not equal?
Value neq(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("!=", neq), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    return Value(int(args[0] != args[1]));
}

// Is one number greater than another?
Value greater(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value(">", greater), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    return Value(int(args[0] > args[1]));
}

// Is one number less than another?
Value less(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("<", less), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    return Value(int(args[0] < args[1]));
}

// Is one number greater than or equal to another?
Value greater_eq(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value(">=", greater_eq), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    return Value(int(args[0] >= args[1]));
}

// Is one number less than or equal to another?
Value less_eq(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("<=", less_eq), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    return Value(int(args[0] <= args[1]));
}

// Get the type name of a value
Value get_type_name(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("type", get_type_name), env, args.size() > 1? TOO_MANY_ARGS
    // : TOO_FEW_ARGS);

    return Value::string(args[0].get_type_name());
}

// Cast an item to a float
Value cast_to_float(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value(FLOAT_TYPE, cast_to_float), env, args.size() > 1?
    // TOO_MANY_ARGS : TOO_FEW_ARGS);
    return args[0].cast_to_float();
}

// Cast an item to an int
Value cast_to_int(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value(INT_TYPE, cast_to_int), env, args.size() > 1? TOO_MANY_ARGS
    // : TOO_FEW_ARGS);
    return args[0].cast_to_int();
}

// Index a list
Value index(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("index", index), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);

    std::vector<Value> list = args[0].as_list();
    int i                   = args[1].as_int();
    if (i < list.size())
    {
        return = list[i];
    }
    else
    {
        error("(list) Index should be smaller than the size of the list.");
    }

    return list[i];
}

// Insert a value into a list
Value insert(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 3)
        Serial.println(args.size() > 3 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("insert", insert), env, args.size() > 3? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);

    std::vector<Value> list = args[0].as_list();
    int i                   = args[1].as_int();
    if (i > (int)list.size())
        Serial.println(INDEX_OUT_OF_RANGE);
    else
        list.insert(list.begin() + args[1].as_int(), args[2].as_int());
    return Value(list);
}

// Remove a value at an index from a list
Value remove(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 2)
        Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("remove", remove), env, args.size() > 2? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);

    std::vector<Value> list = args[0].as_list();
    int i                   = args[1].as_int();
    if (list.empty() || i >= (int)list.size())
        Serial.println(INDEX_OUT_OF_RANGE);
    else
        list.erase(list.begin() + i);
    return Value(list);
}

// Get the length of a list
Value len(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("len", len), env, args.size() > 1?
    // TOO_MANY_ARGS : TOO_FEW_ARGS
    // );

    return Value(int(args[0].as_list().size()));
}

// Add an item to the end of a list
Value push(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() == 0)
        Serial.println(TOO_FEW_ARGS);
    // throw Error(Value("push", push), env, TOO_FEW_ARGS);
    for (size_t i = 1; i < args.size(); i++)
        args[0].push(args[i]);
    return args[0];
}

Value pop(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("pop", pop), env, args.size() > 1? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    return args[0].pop();
}

Value head(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("head", head), env, args.size() > 1? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);
    std::vector<Value> list = args[0].as_list();
    if (list.empty())
        Serial.println(INDEX_OUT_OF_RANGE);
    // throw Error(Value("head", head), env, INDEX_OUT_OF_RANGE);

    return list[0];
}

Value tail(std::vector<Value>& args, Environment& env)
{
    // Is not a special form, so we can evaluate our args.
    eval_args(args, env);

    if (args.size() != 1)
        Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    // throw Error(Value("tail", tail), env, args.size() > 1? TOO_MANY_ARGS :
    // TOO_FEW_ARGS);

    std::vector<Value> result, list = args[0].as_list();

    for (size_t i = 1; i < list.size(); i++)
        result.push_back(list[i]);

    return Value(result);
}
