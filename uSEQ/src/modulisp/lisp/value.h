#ifndef VALUE_H_
#define VALUE_H_

#include "../../utils/error_messages.h"
#include "../../utils/log.h"
#include "../../utils/string.h"
#include "symbol_intern.h"
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

class Environment;
class Value;

// A builtin method is just a native C(++) function
// TODO should this be a vector to (shared?) pointers?
using ValueVec        = std::vector<Value>;
using LispFuncArgsVec = ValueVec;
using ValuePtr        = std::shared_ptr<Value>;
using BuiltinFunc     = std::function<Value(LispFuncArgsVec&, Environment&)>;
// using BuiltinFuncSharedPtr = std::shared_ptr<BuiltinFunc>;
// using BuiltinFuncSharedPtr = std::shared_ptr<BuiltinFunc>;
// using BuiltinFunc = Value (*)(std::vector<Value>&, Environment&);
using BuiltinFuncRawPtr = Value (*)(std::vector<Value>&, Environment&);

// Plugin builtin: function pointer + opaque context for zero-overhead extension.
// The system layer (e.g. uSEQ) registers hardware builtins through this type,
// allowing the interpreter to call them without knowing anything about the system.
using PluginBuiltinFunc = Value(*)(void* ctx, std::vector<Value>& args, Environment& env);

// using LambdaScopeEnv = Environment<32>;

#ifdef ARDUINO
#define VALUE_FAST_MEM __not_in_flash("VALUEDATA")
#else
#define VALUE_FAST_MEM // Empty for desktop builds
#endif

#include "../diagnostic.h"  // SourceSpan

class Value
{
public:
    ////////////////////////////////////////////////////////////////////////////////
    /// CONSTRUCTORS
    /// ///////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

    // Constructs a unit value
    Value() : type(UNIT) {}

    ~Value();

    // Constructs an integer
    Value(int i) : type(INT) { stack_data.i = i; }
    // Constructs a floating point value
    Value(double f) : type(FLOAT) { stack_data.f = f; }
    // Constructs a list
    Value(std::vector<Value> list_param) : type(LIST), list(list_param) {}
    // Constructs a named function that corresponds to a native C/C++ Lisp
    // function
    Value(String name, BuiltinFuncRawPtr ptr);
    Value(String name, PluginBuiltinFunc func, void* ctx);
    // Value(String name, BuiltinFunc f);
    // Value(String name, RawBuiltinFuncPtr ptr);

    // Copy etc constructors
    // Value(const Value& v)
    //     : type(v.type), stack_data(v.stack_data), b(v.b), str(v.str),
    //     list(v.list),
    //       scope(v.scope)
    // {
    // }

    // Value(Value&& v)
    //     : stack_data(std::move(v.stack_data)), str(std::move(v.str)),
    //     list(std::move(v.list)),
    //       scope(std::move(v.scope))
    // {
    // }

    // Value& operator=(const Value& v)
    // {
    //     this->stack_data = v.stack_data;
    //     this->str        = v.str;
    //     this->list       = v.list;
    //     this->scope      = v.scope;
    //     this->type       = v.type;
    //     this->b          = v.b;
    //     return *this;
    // }

    static Value nil();
    static Value error();
    static Value quote(Value quoted);
    static Value atom(String s);
    static Value string(String s);
    // static Value list(std::vector<Value> lst);
    static Value vector(std::vector<Value> vec);

    // Static time parameter - higher-order signal that updates automatically
    static Value t;

    Value(std::vector<Value> params, Value ret, const Environment& env);

    std::set<String> get_used_atoms() const;

    Value apply(std::vector<Value>& args, Environment& env);
    Value eval(Environment& env);

    bool is_builtin() const;
    bool is_nil() const;
    bool is_number() const;
    bool is_int() const;
    bool is_float() const;
    bool is_negative_number() const;
    bool is_positive_number() const;
    bool is_non_zero_number() const;

    bool is_error() const;
    bool is_list() const;
    bool is_vector() const;
    bool is_sequential() const;
    bool is_empty() const;
    bool is_list_and_empty() const;
    bool is_string() const;
    bool is_symbol() const;
    bool as_bool() const;
    int as_int() const;
    double as_float() const;
    String as_string() const;
    String as_atom() const;
    std::vector<Value> as_list() const;
    std::vector<Value> as_vector() const;
    std::vector<Value> as_sequential() const;
    void push(Value val);
    Value pop();

    ////////////////////////////////////////////////////////////////////////////////
    /// TYPECASTING METHODS
    /// ////////////////////////////////////////////////////////
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
    /// ARITHMETIC OPERATIONS
    /// //////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

    Value operator+(Value other) const;
    Value operator-(Value other) const;
    Value operator*(Value other) const;
    Value operator/(Value other) const;
    Value operator%(Value other) const;

    ////////////////////////////////////////////////////////////////////////////////
    /// TRANSCENDENTAL AND MATHEMATICAL FUNCTIONS
    /// ////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

    // Trigonometric functions
    Value sin() const;
    Value cos() const;

    // Exponential and logarithmic functions
    Value exp() const;
    Value log() const;

    // Root functions
    Value sqrt() const;

    // Other mathematical functions
    Value abs() const;

    bool operator==(const String& str) const;

    // Get the name of the type of this value
    String get_type_name() const;
    int get_type_enum() const;
    String display() const;
    String to_lisp_src() const;

    // friend std::ostream &operator<<(std::ostream &os, Value const &v) {
    //   return os << v.display();
    // }

    enum
    {
        QUOTE,
        ATOM,
        INT,
        FLOAT,
        LIST,
        VECTOR,
        STRING,
        LAMBDA,
        BUILTIN,
        BUILTIN_PLUGIN,
        UNIT,
        NIL,
        ERROR
    } type;

    // private:
    union
    {
        int i;
        double f;
        BuiltinFuncRawPtr builtin;
        PluginBuiltinFunc plugin_builtin;
    } stack_data;

    // Opaque context for plugin builtins (set only for BUILTIN_PLUGIN type)
    void* plugin_context = nullptr;

    String str;
    SymbolIntern::SymbolID symbol_id = SymbolIntern::INVALID_ID;  // For fast ATOM comparison
    SourceSpan span;  // Source location, placed in padding gap before list
    std::vector<Value> list;

    std::shared_ptr<Environment> lambda_scope;
};

// end of class Value

static_assert(sizeof(SourceSpan) == 4, "SourceSpan must be 4 bytes");
static_assert(sizeof(Value) == 88, "Value size must not change — SourceSpan fits in padding gap");

// Global time constant accessible outside class namespace
extern const Value& t;

#endif // VALUE_H_
