// 149, 169, 243, 255, 267, 278, 286, 302

#include "lisp/value.h"
#include "lisp/environment.h"
#include <cmath>

#include "lisp/configure.h"

//// CONSTRUCTORS
// LAMBDA
Value::Value(std::vector<Value> params, Value ret, Environment const& env) : type(LAMBDA)
{
    // We store the params and the result in the list member
    // instead of having dedicated members. This is to save memory.
    list.push_back(Value(params));
    list.push_back(ret);

    // Lambdas capture only variables that they know they will use.
    std::vector<String> used_atoms = ret.get_used_atoms();
    for (size_t i = 0; i < used_atoms.size(); i++)
    {
        // If the environment has a symbol that this lambda uses, capture it.
        if (env.has(used_atoms[i]))
            scope->set(used_atoms[i], env.get(used_atoms[i]));
    }
}

// METHODS
Value Value::quote(Value quoted)
{
    Value result;
    result.type = QUOTE;
    result.list.push_back(quoted);
    return result;
}

Value Value::atom(String s)
{
    Value result;
    result.type = ATOM;
    result.str  = s;
    return result;
}

Value Value::string(String s)
{
    Value result;
    result.type = STRING;
    result.str  = s;
    return result;
}

std::vector<String> Value::get_used_atoms()
{
    std::vector<String> result, tmp;
    switch (type)
    {
    case QUOTE:
        // The data for a quote is stored in the
        // first slot of the list member.
        return list[0].get_used_atoms();
    case ATOM:
        // If this is an atom, add it to the list
        // of used atoms in this expression.
        result.push_back(as_atom());
        return result;
    case LAMBDA:
        // If this is a lambda, get the list of used atoms in the body
        // of the expression.
        return list[1].get_used_atoms();
    case LIST:
        // If this is a list, add each of the atoms used in all
        // of the elements in the list.
        for (size_t i = 0; i < list.size(); i++)
        {
            // Get the atoms used in the element
            tmp = list[i].get_used_atoms();
            // Add the used atoms to the current list of used atoms
            result.insert(result.end(), tmp.begin(), tmp.end());
        }
        return result;
    default:
        return result;
    }
}

bool Value::is_builtin() { return type == BUILTIN; }

Value Value::apply(std::vector<Value>& args, Environment& env)
{
    switch (type)
    {
    case LAMBDA:
    {
        Environment e;
        std::vector<Value>* params;

        // Get the list of parameter atoms
        params = &list[0].list;
        if (params->size() != args.size())
        {
            // FIXME
            // println(args.size() > params->size() ? TOO_MANY_ARGS : TOO_FEW_ARGS);
            return Value::error();
        }

        // throw Error(Value(args), env, args.size() > params.size()?
        //     TOO_MANY_ARGS : TOO_FEW_ARGS
        // );

        // Get the captured scope from the lambda
        e = *scope;
        // And make this scope the parent scope
        e.set_parent_scope(&env);

        // Iterate through the list of parameters and
        // insert the arguments into the scope.
        for (size_t i = 0; i < params->size(); i++)
        {
            if ((*params)[i].type != ATOM)
                // FIXME
                // println(INVALID_LAMBDA);
                // throw Error(*this, env, INVALID_LAMBDA);
                // Set the parameter name into the scope.
                e.set((*params)[i].str, args[i]);
        }
        // Evaluate the function body with the function scope
        auto result = list[1].eval(e);
        return result;
    }
    case BUILTIN:
    {
        // Here, we call the builtin function with the current scope.
        // This allows us to write special forms without syntactic sugar.
        // For functions that are not special forms, we just evaluate
        // the arguments before we run the function.
        auto result = (*stack_data.b)(args, env);
        return result;
    }
    default:
        // We can only call lambdas and builtins
        // print(CALL_NON_FUNCTION);
        // println(str);
        return Value::error();
    }
}

Value Value::eval(Environment& env)
{
    std::vector<Value> args;
    Value function;
    // Environment e;
    switch (type)
    {
    case QUOTE:
        dbg("quote!");
        return list[0];
    case ATOM:
    {
        dbg("atom!");
        // ts_get = micros();
        auto atomdata = env.get(str);
        if (atomdata == Value::error())
        {
            print("Get error: ");
            println(str);
        }
        // ts_get = micros() - ts_get;
        // get_time += ts_get;
        return atomdata;
    }
    case LIST:
    {
        dbg("list!");
        if (list.size() < 1)
            println(EVAL_EMPTY_LIST);
        // throw Error(*this, env, EVAL_EMPTY_LIST);
        // note: this needs to be a copy?  so original remains unevaluated?  or if not, use std::span to avoid
        // the copy?
        args = std::vector<Value>(list.begin() + 1, list.end());
        // Only evaluate our arguments if it's not builtin!
        // Builtin functions can be special forms, so we
        // leave them to evaluate their arguments.
        function = list[0].eval(env);
        if (function.is_error())
        {
            print("err: function is error");
            return Value::error();
        }
        else
        {
            // lambda?
            bool evalError = false;
            if (!function.is_builtin())
            {
                for (size_t i = 0; i < args.size(); i++)
                {
                    args[i] = args[i].eval(env);
                    if (args[i] == Value::error())
                    {
                        evalError = true;
                        break;
                    }
                }
            }

            if (evalError)
            {
                return Value::error();
            }
            else
            {
                auto functionResult = function.apply(args, env);
                return functionResult;
            }
        }
    }

    default:
        return *this;
    }
}

bool Value::is_number() const { return type == INT || type == FLOAT; }

bool Value::is_error() const { return type == ERROR; }

bool Value::is_list() const { return type == LIST; }

bool Value::as_bool() const { return *this != Value(0); }

int Value::as_int() const { return cast_to_int().stack_data.i; }

double Value::as_float() const { return cast_to_float().stack_data.f; }

String Value::as_string() const
{
    if (type != STRING)
    {
        print("str: ");
        println(BAD_CAST);
        currentExprSound = false;
        return "";
    }
    return str;
}

String Value::as_atom() const
{
    if (type != ATOM)
    {
        print("atom: ");
        // FIXME
        // println(BAD_CAST);
        currentExprSound = false;
        return "";
    }
    return str;
}

std::vector<Value> Value::as_list() const
{
    if (type != LIST)
    {
        print("list: ");
        // FIXME
        // println(BAD_CAST);
        currentExprSound = false;
        return { Value::error() };
    }
    return list;
}

void Value::push(Value val)
{
    if (type != LIST)
    {
        // FIXME
        // println(MISMATCHED_TYPES);
    }

    list.push_back(val);
}

Value Value::pop()
{
    if (type != LIST)
    {
        // FIXME
        // println(MISMATCHED_TYPES);
    }

    Value result = list[list.size() - 1];
    list.pop_back();
    return result;
}

Value Value::cast_to_int() const
{
    switch (type)
    {
    case INT:
        return *this;
    case FLOAT:
        return Value(int(stack_data.f));
    default:
        println("int: ");
        // FIXME
        // println(BAD_CAST);
        currentExprSound = false;
        return Value::error();
    }
}

Value Value::cast_to_float() const
{
    switch (type)
    {
    case FLOAT:
        return *this;
    case INT:
        return Value(double(stack_data.i));
    default:
        println("float: ");
        println(BAD_CAST);
        currentExprSound = false;
        return Value::error();
    }
}

bool Value::operator==(Value other) const
{
    // If either of these values are floats, promote the
    // other to a float, and then compare for equality.
    if (type == FLOAT && other.type == INT)
        return *this == other.cast_to_float();
    else if (type == INT && other.type == FLOAT)
        return this->cast_to_float() == other;
    // If the values types aren't equal, then they cannot be equal.
    else if (type != other.type)
        return false;

    switch (type)
    {
    case FLOAT:
        return stack_data.f == other.stack_data.f;
    case INT:
        return stack_data.i == other.stack_data.i;
    case BUILTIN:
        return stack_data.b == other.stack_data.b;
    case STRING:
    case ATOM:
        // Both atoms and strings store their
        // data in the str member.
        return str == other.str;
    case LAMBDA:
    case LIST:
        // Both lambdas and lists store their
        // data in the list member.
        return list == other.list;
    case QUOTE:
        // The values for quotes are stored in the
        // first slot of the list member.
        return list[0] == other.list[0];
    default:
        return true;
    }
}

bool Value::operator!=(Value other) const { return !(*this == other); }

bool Value::operator>=(Value other) const { return !(*this < other); }

bool Value::operator<=(Value other) const { return (*this == other) || (*this < other); }

bool Value::operator>(Value other) const { return !(*this <= other); }

bool Value::operator<(Value other) const
{
    // Other type must be a float or an int
    if (other.type != FLOAT && other.type != INT)
        println(INVALID_BIN_OP);
    // throw Error(*this, Environment(), INVALID_BIN_OP);

    switch (type)
    {
    case FLOAT:
        // If this is a float, promote the other value to a float and compare.
        return stack_data.f < other.cast_to_float().stack_data.f;
    case INT:
        // If the other value is a float, promote this value to a float and compare.
        if (other.type == FLOAT)
            return cast_to_float().stack_data.f < other.stack_data.f;
        // Otherwise compare the integer values
        else
            return stack_data.i < other.stack_data.i;
    default:
        // Only allow comparisons between integers and floats
        println(INVALID_ORDER);
        return false;
        // throw Error(*this, Environment(), INVALID_ORDER);
    }
}

// static Value Value::error()
Value Value::error()
{
    Value result;
    result.type = ERROR;
    return result;
}

Value Value::operator+(Value other) const
{
    if (other.type == UNIT)
        return other;

    if ((is_number() || other.is_number()) && !(is_number() && other.is_number()))
        println(INVALID_BIN_OP);

    switch (type)
    {
    case FLOAT:
        return Value(stack_data.f + other.cast_to_float().stack_data.f);
    case INT:
        if (other.type == FLOAT)
            return Value(cast_to_float() + other.stack_data.f);
        else if (other.type == STRING)
            return Value::string(as_string() + other.as_string());
        else
            return Value(stack_data.i + other.stack_data.i);
    case STRING:
        if (other.type == STRING)
            return Value::string(str + other.str);
        else
            println(INVALID_BIN_OP);
    case LIST:
        if (other.type == LIST)
        {
            Value result = *this;
            for (size_t i = 0; i < other.list.size(); i++)
                result.push(other.list[i]);
            return result;
        }
        else
            println(INVALID_BIN_OP);
    case UNIT:
        return *this;
    default:
        println(INVALID_BIN_OP);
        return Value();
    }
}

Value Value::operator-(Value other) const
{
    // If the other value's type is the unit type,
    // don't even bother continuing.
    // Unit types consume all arithmetic operations.
    if (other.type == UNIT)
        return other;

    // Other type must be a float or an int
    if (other.type != FLOAT && other.type != INT)
        println(INVALID_BIN_OP);
    // throw Error(*this, Environment(), INVALID_BIN_OP);

    switch (type)
    {
    case FLOAT:
        // If one is a float, promote the other by default and do
        // float subtraction.
        return Value(stack_data.f - other.cast_to_float().stack_data.f);
    case INT:
        // If the other type is a float, go ahead and promote this expression
        // before continuing with the subtraction
        if (other.type == FLOAT)
            return Value(cast_to_float().stack_data.f - other.stack_data.f);
        // Otherwise, do integer subtraction.
        else
            return Value(stack_data.i - other.stack_data.i);
    case UNIT:
        // Unit types consume all arithmetic operations.
        return *this;
    default:
        // This operation was done on an unsupported type
        println(INVALID_BIN_OP);
        return Value();
        // throw Error(*this, Environment(), INVALID_BIN_OP);
    }
}

Value Value::operator*(Value other) const
{

    // If the other value's type is the unit type,
    // don't even bother continuing.
    // Unit types consume all arithmetic operations.
    if (other.type == UNIT)
        return other;

    // Other type must be a float or an int
    if (other.type != FLOAT && other.type != INT)
        println(INVALID_BIN_OP);
    // throw Error(*this, Environment(), INVALID_BIN_OP);

    switch (type)
    {
    case FLOAT:
        return Value(stack_data.f * other.cast_to_float().stack_data.f);
    case INT:
        // If the other type is a float, go ahead and promote this expression
        // before continuing with the product
        if (other.type == FLOAT)
            return Value(cast_to_float().stack_data.f * other.stack_data.f);
        // Otherwise, do integer multiplication.
        else
            return Value(stack_data.i * other.stack_data.i);
    case UNIT:
        // Unit types consume all arithmetic operations.
        return *this;
    default:
        // This operation was done on an unsupported type
        println(INVALID_BIN_OP);
        return Value();
        // throw Error(*this, Environment(), INVALID_BIN_OP);
    }
    // Definition...
}

Value Value::operator/(Value other) const
{

    // If the other value's type is the unit type,
    // don't even bother continuing.
    // Unit types consume all arithmetic operations.
    if (other.type == UNIT)
        return other;

    // Other type must be a float or an int
    if (other.type != FLOAT && other.type != INT)
        println(INVALID_BIN_OP);
    //             throw Error(*this, Environment(), INVALID_BIN_OP);

    switch (type)
    {
    case FLOAT:
    {
        auto res = Value(stack_data.f / other.cast_to_float().stack_data.f);
        return res;
    }
    case INT:
    {
        // If the other type is a float, go ahead and promote this expression
        // before continuing with the product
        Value res;
        if (other.type == FLOAT)
            res = Value(cast_to_float().stack_data.f / other.stack_data.f);
        // Otherwise, do integer multiplication.
        else
            res = Value(stack_data.i / other.stack_data.i);
        return res;
    }
    case UNIT:
        // Unit types consume all arithmetic operations.
        return *this;
    default:
        // This operation was done on an unsupported type
        println(INVALID_BIN_OP);
        return Value();
        //  throw Error(*this, Environment(), INVALID_BIN_OP);
    }
    // Definition...
}

Value Value::operator%(Value other) const
{
    // If the other value's type is the unit type,
    // don't even bother continuing.
    // Unit types consume all arithmetic operations.
    if (other.type == UNIT)
        return other;

    // Other type must be a float or an int
    if (other.type != FLOAT && other.type != INT)
        println(INVALID_BIN_OP);
    // throw Error(*this, Environment(), INVALID_BIN_OP);

    switch (type)
    {
    // If we support libm, we can find the remainder of floating point values.
    case FLOAT:
        return Value(fmod(stack_data.f, other.cast_to_float().stack_data.f));
    case INT:
        if (other.type == FLOAT)
            return Value(fmod(cast_to_float().stack_data.f, other.stack_data.f));
        else
            return Value(stack_data.i % other.stack_data.i);

        //         #else
        //         case INT:
        // //            // If we do not support libm, we have to throw errors for floating point values.
        //             return Value(stack_data.i % other.stack_data.i);
        //         #endif

    case UNIT:
        // Unit types consume all arithmetic operations.
        return *this;
    default:
        // This operation was done on an unsupported type
        println(INVALID_BIN_OP);
        return Value();
        // throw Error(*this, Environment(), INVALID_BIN_OP);
    }
    // Definition...
}

int Value::get_type_enum() const { return type; }

// Get the name of the type of this value
String Value::get_type_name() const
{
    switch (type)
    {
    case QUOTE:
        return QUOTE_TYPE;
    case ATOM:
        return ATOM_TYPE;
    case INT:
        return INT_TYPE;
    case FLOAT:
        return FLOAT_TYPE;
    case LIST:
        return LIST_TYPE;
    case STRING:
        return STRING_TYPE;
    case BUILTIN:
    case LAMBDA:
        // Instead of differentiating between
        // lambda and builtin types, we group them together.
        // This is because they are both callable.
        return FUNCTION_TYPE;
    case UNIT:
        return UNIT_TYPE;
    case ERROR:
        return ERROR_TYPE;
    default:
        // We don't know the name of this type.
        // This isn't the users fault, this is just unhandled.
        // This should never be reached.
        println(INTERNAL_ERROR);
        return "";
        // throw Error(*this, Environment(), INTERNAL_ERROR);
    }
}

String Value::display() const
{
    String result;
    switch (type)
    {
    case QUOTE:
        return "'" + list[0].debug();
    case ATOM:
        return str;
    case INT:
        return String(stack_data.i);
    case FLOAT:
        return String(stack_data.f);
    case STRING:
        return str;
    case LAMBDA:
        for (size_t i = 0; i < list.size(); i++)
        {
            result += list[i].debug();
            if (i < list.size() - 1)
                result += " ";
        }
        return "(lambda " + result + ")";
    case LIST:
        for (size_t i = 0; i < list.size(); i++)
        {
            result += list[i].debug();
            if (i < list.size() - 1)
                result += " ";
        }
        return "(" + result + ")";
    case BUILTIN:
        return "<" + str + " at " + String(size_t(stack_data.b)) + ">";
    case UNIT:
        return "@";
    case ERROR:
        return "error";
    default:
        // We don't know how to display whatever type this is.
        // This isn't the users fault, this is just unhandled.
        // This should never be reached.
        println(INTERNAL_ERROR);
        // throw Error(*this, Environment(), INTERNAL_ERROR);
        return "";
    }
}

String Value::debug() const
{
    String result;
    switch (type)
    {
    case QUOTE:
        return "'" + list[0].debug();
    case ATOM:
        return str;
    case INT:
    {
        auto val = String(stack_data.i);
        return val;
    }
    case FLOAT:
    {
        auto val = String(stack_data.f);
        return val;
    }
    case STRING:
        for (size_t i = 0; i < str.length(); i++)
        {
            if (str[i] == '"')
                result += "\\\"";
            else
                result += str[i];
        }
        return "\"" + result + "\"";
    case LAMBDA:
        for (size_t i = 0; i < list.size(); i++)
        {
            result += list[i].debug();
            if (i < list.size() - 1)
                result += " ";
        }
        return "(lambda " + result + ")";
    case LIST:
        for (size_t i = 0; i < list.size(); i++)
        {
            result += list[i].debug();
            if (i < list.size() - 1)
                result += " ";
        }
        return "(" + result + ")";
    case BUILTIN:
        return "<" + str + " at " + String(long(stack_data.b)) + ">";
    case UNIT:
        return "@";
    case ERROR:
        return "error";
    default:
        // We don't know how to debug whatever type this is.
        // This isn't the users fault, this is just unhandled.
        // This should never be reached.
        println(INTERNAL_ERROR);
        // throw Error(*this, Environment(), INTERNAL_ERROR);
        return "";
    }
}

// BUILTIN
Value::Value(String name, BuiltinFunc b) : type(BUILTIN)
{
    // Store the name of the builtin function in the str member
    // to save memory, and use the builtin function slot in the union
    // to store the function pointer.
    str          = name;
    stack_data.b = &b;
}

int ts_get   = 0;
int get_time = 0;
// Value Value::eval(Environment& env)
// {
//     std::vector<Value> args;
//     Value function;
//     // Environment e;
//     switch (type)
//     {
//     case QUOTE:
//         return list[0];
//     case ATOM:
//     {
//         // ts_get        = micros();
//         auto atomdata = env.get(str);
//         if (atomdata == Value::error())
//         {
//             print("Get error: ");
//             println(str);
//         }
//         // ts_get = micros() - ts_get;
//         // get_time += ts_get;
//         return atomdata;
//     }
//     case LIST:
//     {
//         if (list.size() < 1)
//             println(EVAL_EMPTY_LIST);
//         // throw Error(*this, env, EVAL_EMPTY_LIST);
//         // note: this needs to be a copy?  so original remains unevaluated?  or if not, use std::span to
//         avoid
//         // the copy?
//         args = std::vector<Value>(list.begin() + 1, list.end());
//         // Only evaluate our arguments if it's not builtin!
//         // BuiltinMethod functions can be special forms, so we
//         // leave them to evaluate their arguments.
//         function = list[0].eval(env);
//         if (function.is_error())
//         {
//             print("err: function is error");
//             return Value::error();
//         }
//         else
//         {
//             // lambda?
//             bool evalError = false;
//             if (!function.is_builtin())
//             {
//                 for (size_t i = 0; i < args.size(); i++)
//                 {
//                     args[i] = args[i].eval(env);
//                     if (args[i] == Value::error())
//                     {
//                         evalError = true;
//                         break;
//                     }
//                 }
//             }

//             if (evalError)
//             {
//                 return Value::error();
//             }
//             else
//             {
//                 auto functionResult = function.apply(args, env);
//                 return functionResult;
//             }
//         }
//     }

//     default:
//         return *this;
//     }
// }
