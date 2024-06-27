#include "value.h"
#include "../utils.h"
#include "../utils/log.h"
#include "environment.h"
#include <cmath>

#include "configure.h"

// TODO do we care to support this some_value.apply() and .eval()
// syntax and, if so, is there a better way?
class Interpreter;
#include "interpreter.h"

////DESTRUCTOR
Value::~Value() {}

//// CONSTRUCTORS
// static Value Value::error()
Value Value::error()
{
    Value result;
    result.type = ERROR;
    return result;
}

Value Value::nil()
{
    Value result;
    result.type = NIL;
    return result;
}

// LAMBDA
Value::Value(std::vector<Value> params, Value ret, Environment const& env)
    : type(LAMBDA)
{
    DBG("Value::Value (LAMBDA)");

    lambda_scope = std::make_shared<Environment>();

    // We store the params and the result in the list member
    // instead of having dedicated members. This is to save memory.
    list.push_back(Value::vector(params));
    list.push_back(ret);

    // Lambdas capture only variables that they know they will use.
    std::set<String> used_atoms = ret.get_used_atoms();

    for (const String& atom : used_atoms)
    {
        // Don't capture the current value of time variables
        // TODO: there should be some globally-accessible set
        // of the special time-varying values

        if (atom == "time" || atom == "t" || atom == "bar" || atom == "beat" ||
            atom == "section" || atom == "phrase")
        {
            continue;
        }

        // Ignore atoms that are known to be args
        if (std::find(params.begin(), params.end(), atom) != params.end())
        {
            continue;
        }

        // Expr
        std::optional<Value> def_expr = env.get_expr(atom);
        // If the environment has a symbol that this lambda uses, capture it.
        if (def_expr)
        {
            lambda_scope->set_expr(atom, *def_expr);
        }

        // Static def
        std::optional<Value> def = env.get(atom);
        // If the environment has a symbol that this lambda uses, capture it.
        if (def)
        {
            lambda_scope->set(atom, *def);
        }
    }
}

// BUILTIN
Value::Value(String name, BuiltinFuncRawPtr ptr) : type(BUILTIN)
{
    // Store the name of the builtin function in the str member
    // to save memory, and use the builtin function slot in the union
    // to store the function pointer.
    str                = name;
    stack_data.builtin = ptr;
}

// BUILTIN_METHOD
Value::Value(String name, uSEQ_Method_Ptr ptr) : type(BUILTIN_METHOD)
{
    str                       = name;
    stack_data.builtin_method = ptr;
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

Value Value::vector(std::vector<Value> vec)
{
    Value result;
    result.type = VECTOR;
    result.list = vec;
    return result;
}

std::set<String> Value::get_used_atoms() const
{
    std::set<String> result, tmp;
    switch (type)
    {
    case QUOTE:
        // The data for a quote is stored in the
        // first slot of the list member.
        return list[0].get_used_atoms();
    case ATOM:
        // If this is an atom, add it to the list
        // of used atoms in this expression.
        result.insert(as_atom());
        return result;
    case LAMBDA:
        // If this is a lambda, get the list of used atoms in the body
        // of the expression.
        return list[1].get_used_atoms();
    case LIST:
    case VECTOR:
        // If this is a list, add each of the atoms used in all
        // of the elements in the list.
        for (size_t i = 0; i < list.size(); i++)
        {
            // Get the atoms used in the element
            tmp = list[i].get_used_atoms();
            // Add the used atoms to the current list of used atoms
            result = set_union(result, tmp);
        }
        return result;
    default:
        return result;
    }
}

bool Value::is_nil() const { return type == NIL; }
bool Value::is_builtin() const { return type == BUILTIN || type == BUILTIN_METHOD; }

Value Value::apply(std::vector<Value>& args, Environment& env)
{
    return Interpreter::apply(*this, args, env);
}

Value Value::eval(Environment& env) { return Interpreter::eval_in(*this, env); }

bool Value::is_number() const { return type == INT || type == FLOAT; }
// FIXME
bool Value::is_negative_number() const { return is_number() && *this < 0.0; }
bool Value::is_positive_number() const { return is_number() && *this > 0.0; }
bool Value::is_non_zero_number() const
{
    return is_number() && (*this < 0.0 || *this > 0.0);
}

bool Value::is_error() const { return type == ERROR; }

bool Value::is_list() const { return type == LIST; }
bool Value::is_vector() const { return type == VECTOR; }
bool Value::is_sequential() const { return is_list() || is_vector(); }

bool Value::is_signal() const
{
    // TODO
    return false;
    // return ::is_signal(*this);
    // NOTE: would this be more accurate than checking used atoms?
    // switch (type)
    // {
    // case ATOM:
    //     // If it's an atom, it's a signal only if it can be found
    //     // in our set of known registered signals
    //     return known_signal_vars.find(str) != known_signal_vars.end();
    // case LIST:
    // case VECTOR:
    //     auto return;
    // default:
    //     return false;
    // }
}

bool Value::is_empty() const { return list.empty(); }

bool Value::is_list_and_empty() const { return type == LIST && list.empty(); }

bool Value::is_string() const { return type == STRING; }

bool Value::is_symbol() const { return type == ATOM; }

bool Value::as_bool() const { return *this != Value(0); }

int Value::as_int() const { return cast_to_int().stack_data.i; }

double Value::as_float() const { return cast_to_float().stack_data.f; }

// FIXME @correctness these should probably change to std::optionals
String Value::as_string() const
{
    if (type != STRING)
    {
        println("str: " + BAD_CAST);
        return "std::nullopt";
    }
    return str;
}

String Value::as_atom() const
{
    if (type != ATOM)
    {
        println("atom: " + BAD_CAST);
        return "std::nullopt";
    }
    return str;
}

std::vector<Value> Value::as_list() const
{
    if (type != LIST && type != VECTOR)
    {
        println("list: " + BAD_CAST);
        return {};
    }
    return list;
}

std::vector<Value> Value::as_vector() const
{
    if (type != VECTOR)
    {
        println("vector: " + BAD_CAST);
        return {};
    }
    return list;
}

std::vector<Value> Value::as_sequential() const
{
    if (type != VECTOR && type != LIST)
    {
        println(BAD_CAST);
        return {};
    }
    return list;
}

void Value::push(Value val)
{
    if (type == LIST || type == VECTOR)
    {
        list.push_back(val);
    }
    else
    {
        println(MISMATCHED_TYPES);
    }
}

Value Value::pop()
{
    Value result = Value::nil();

    if (type == LIST || type == VECTOR)
    {
        result = list[list.size() - 1];
        list.pop_back();
    }
    else
    {
        println(MISMATCHED_TYPES);
    }

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
        println(BAD_CAST + " (int)");
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
        println(BAD_CAST + " (float)");
        return Value::error();
    }
}

bool Value::operator==(const String& other) const { return str == other; }

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
        // FIXME how do we compare functions? compare if they point to the same
        // thing?
        return str == other.str;
    case STRING:
    case ATOM:
        // Both atoms and strings store their
        // data in the str member.
        return str == other.str;
    case LAMBDA:
    case LIST:
    case VECTOR:
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

bool Value::operator<=(Value other) const
{
    return (*this == other) || (*this < other);
}

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
        // //            // If we do not support libm, we have to throw errors for
        // floating point values.
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
    case NIL:
        return "nil";
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
    case VECTOR:
        return VECTOR_TYPE;
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
        ::report_generic_error("(get_type_name) UNKNOWN TYPE");
        return "";
        // throw Error(*this, Environment(), INTERNAL_ERROR);
    }
}

String Value::display() const
{
    // DBG("Value::display");
    // dbg("type: " + String(type));

    String result;
    switch (type)
    {
    case STRING:
        return str;
    case LAMBDA:
        for (size_t i = 0; i < list.size(); i++)
        {
            result += list[i].display();
            if (i < list.size() - 1)
                result += " ";
        }
        return "(lambda " + result + ")";
    case LIST:
        for (size_t i = 0; i < list.size(); i++)
        {
            result += list[i].display();
            if (i < list.size() - 1)
                result += " ";
        }
        return "(" + result + ")";
    case VECTOR:
        for (size_t i = 0; i < list.size(); i++)
        {
            result += list[i].display();
            if (i < list.size() - 1)
                result += " ";
        }
        return "[" + result + "]";
    case BUILTIN:
        // NOTE: should this print the address of the unique
        // pointer or of the thing it's pointing to?
        return "{builtin " + str + " at " + String(size_t(&stack_data.builtin)) +
               "}";
    case BUILTIN_METHOD:
        // NOTE: should this print the address of the unique
        // pointer or of the thing it's pointing to?
        return "{builtin method " + str + " at " +
               String(size_t(&stack_data.builtin_method)) + "}";
    case UNIT:
        return "";
    case ERROR:
        return "{error}";
    default:
        return to_lisp_src();
    }
}

String Value::to_lisp_src() const
{
    String result;
    switch (type)
    {
    case NIL:
        return "nil";
    case QUOTE:
        return "'" + list[0].to_lisp_src();
    case ATOM:
        return str;
    case INT:
        return String(stack_data.i);
    case FLOAT:
        return String(stack_data.f);
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
            result += list[i].to_lisp_src();
            if (i < list.size() - 1)
                result += " ";
        }
        return "(lambda " + result + ")";
    case LIST:
        for (size_t i = 0; i < list.size(); i++)
        {
            result += list[i].to_lisp_src();
            if (i < list.size() - 1)
                result += " ";
        }
        return "(" + result + ")";
    case VECTOR:
        for (size_t i = 0; i < list.size(); i++)
        {
            result += list[i].to_lisp_src();
            if (i < list.size() - 1)
                result += " ";
        }
        return "[" + result + "]";
    case UNIT:
        return "@";
        // TODO @correctness this needs to be lexically sound,
        // i.e. referring to the variable that the original did
        // and not to any potential shadowings in the current scope
    case BUILTIN:
    case BUILTIN_METHOD:
        return str;
    default:
        // We don't know how to display whatever type this is.
        // This isn't the users fault, this is just unhandled.
        return "";
    }
}
