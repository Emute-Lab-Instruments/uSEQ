#ifndef VALUE_H_
#define VALUE_H_

#include "utils/error_messages.h"
#include "utils/flags.h"
#include "utils/log.h"
#include "utils/string.h"
#include <functional>
#include <memory>
#include <vector>

class Environment;
class Value;

// A builtin method is just a native C(++) function
// TODO should this be a vector to (shared?) pointers?
using ValuePtr    = std::shared_ptr<Value>;
using BuiltinFunc = std::function<Value(std::vector<Value>&, Environment&)>;
// using BuiltinFunc = Value (*)(std::vector<Value>&, Environment&);
// using NativeProcedure = Value (*)(std::vector<Value>&, Environment&);

class Value
{
public:
    ////////////////////////////////////////////////////////////////////////////////
    /// CONSTRUCTORS ///////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

    // Constructs a unit value
    Value() : type(UNIT) {}

    // Constructs an integer
    Value(int i) : type(INT) { stack_data.i = i; }
    // Constructs a floating point value
    Value(double f) : type(FLOAT) { stack_data.f = f; }
    // Constructs a list
    Value(std::vector<Value> list) : type(LIST), list(list) {}
    // Constructs a named function that corresponds to a native C/C++ Lisp function
    Value(String name, BuiltinFunc b);

    // Copy etc constructors
    Value(const Value& v) : stack_data(v.stack_data), str(v.str), list(v.list), scope(v.scope) {}
    Value(Value&& v)
        : stack_data(std::move(v.stack_data)), str(std::move(v.str)), list(std::move(v.list)),
          scope(std::move(v.scope))
    {
    }

    Value& operator=(const Value& v)
    {
        this->stack_data = v.stack_data;
        this->str        = v.str;
        this->list       = v.list;
        this->scope      = v.scope;
        return *this;
    }

    static Value error();

    static Value quote(Value quoted);
    static Value atom(String s);
    static Value string(String s);

    Value(std::vector<Value> params, Value ret, const Environment& env);

    std::vector<String> get_used_atoms();
    bool is_builtin();
    Value apply(std::vector<Value>& args, Environment& env);
    Value eval(Environment& env);

    bool is_number() const;
    bool is_error() const;
    bool is_list() const;
    bool as_bool() const;
    int as_int() const;
    double as_float() const;
    String as_string() const;
    String as_atom() const;
    std::vector<Value> as_list() const;
    void push(Value val);
    Value pop();

    ////////////////////////////////////////////////////////////////////////////////
    /// TYPECASTING METHODS ////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

    Value cast_to_int() const;
    Value cast_to_float() const;
    bool operator==(Value other) const;
    bool operator!=(Value other) const;
    bool operator>=(Value other) const;
    bool operator<=(Value other) const;
    bool operator>(Value other) const;
    bool operator<(Value other) const;

    ////////////////////////////////////////////////////////////////////////////////
    /// ARITHMETIC OPERATIONS //////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

    Value operator+(Value other) const;
    Value operator-(Value other) const;
    Value operator*(Value other) const;
    Value operator/(Value other) const;
    Value operator%(Value other) const;

    // Get the name of the type of this value
    String get_type_name() const;
    int get_type_enum() const;
    String display() const;
    String debug() const;

    // friend std::ostream &operator<<(std::ostream &os, Value const &v) {
    //   return os << v.display();
    // }

private:
    enum
    {
        QUOTE,
        ATOM,
        INT,
        FLOAT,
        LIST,
        STRING,
        LAMBDA,
        BUILTIN,
        UNIT,
        ERROR
    } type;

    union
    {
        int i;
        double f;
        BuiltinFunc* b;
    } stack_data;

    String str;
    std::vector<Value> list;
    std::shared_ptr<Environment> scope;
};

// end of class Value

#endif // VALUE_H_