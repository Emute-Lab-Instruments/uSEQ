#ifndef VALUE_H_
#define VALUE_H_

#include "../../utils/error_messages.h"
#include "../../utils/flags.h"
#include "../../utils/log.h"
#include "../../utils/string.h"
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include <cmath>

class Environment;
class Value;

// A builtin method is just a native C(++) function
// TODO should this be a vector to (shared?) pointers?
using ValueVec = std::vector<Value>;
using LispFuncArgsVec = ValueVec;
using ValuePtr = std::shared_ptr<Value>;
using BuiltinFunc = std::function<Value(LispFuncArgsVec &, Environment &)>;
// using BuiltinFuncSharedPtr = std::shared_ptr<BuiltinFunc>;
// using BuiltinFuncSharedPtr = std::shared_ptr<BuiltinFunc>;
// using BuiltinFunc = Value (*)(std::vector<Value>&, Environment&);
using BuiltinFuncRawPtr = Value (*)(std::vector<Value> &, Environment &);

class uSEQ;
using uSEQ_Method_Ptr = Value (uSEQ::*)(std::vector<Value> &, Environment &);

class ModuLispInterpreter;
using ModuLispInterpreter_Method_Ptr = Value (ModuLispInterpreter::*)(std::vector<Value> &, Environment &);

// using LambdaScopeEnv = Environment<32>;

#ifdef ARDUINO
#define VALUE_FAST_MEM __not_in_flash("VALUEDATA")
#else
#define VALUE_FAST_MEM // Empty for desktop builds
#endif

#include "signal_metadata.h"

class Value {
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
    Value(String name, uSEQ_Method_Ptr);
    Value(String name, ModuLispInterpreter_Method_Ptr);
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

    static Value VALUE_FAST_MEM nil();
    static Value VALUE_FAST_MEM error();
    static Value VALUE_FAST_MEM quote(Value quoted);
    static Value VALUE_FAST_MEM atom(String s);
    static Value VALUE_FAST_MEM string(String s);
    // static Value list(std::vector<Value> lst);
    static Value VALUE_FAST_MEM vector(std::vector<Value> vec);

    // Static time parameter - higher-order signal that updates automatically
    static Value VALUE_FAST_MEM t;

    Value(std::vector<Value> params, Value ret, const Environment &env);

    std::set<String> get_used_atoms() const;

    Value apply(std::vector<Value> &args, Environment &env);
    Value eval(Environment &env);

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
    bool is_signal() const;
    
    // Core automation signal metadata query methods
    double get_min() const;
    double get_max() const;
    bool is_periodic() const;
    double get_period() const;
    bool is_monotonic() const;
    bool is_monotonic_increasing() const;
    bool is_monotonic_decreasing() const;
    bool is_constant() const;
    
    // Continuity and smoothness metadata methods
    bool is_continuous() const;
    bool is_smooth() const;
    bool is_stepped() const;
    bool is_linear_segments() const;
    
    // Time-based properties
    double get_time_start() const;
    double get_time_end() const;
    bool is_causal() const;
    bool is_memoryless() const;
    double get_memory_length() const;
    
    // Zero crossing analysis methods
    bool has_zero_crossings() const;
    double get_zero_crossing_rate() const;
    bool are_zero_crossing_locations_known() const;
    
    // Threshold and range analysis methods
    bool supports_threshold_queries() const;
    double get_min_threshold_resolution() const;
    bool supports_range_queries() const;
    double get_range_query_resolution() const;
    
    // Extrema analysis methods
    bool has_local_extrema() const;
    bool are_extrema_locations_known() const;
    double get_extrema_detection_threshold() const;
    
    // Interpolation properties
    int get_interpolation_type() const;
    
    // Periodicity details
    double get_phase_offset() const;
    bool is_phase_locked() const;
    
    // Quantization properties (for discrete automation)
    bool is_quantized() const;
    bool is_integer_valued() const;
    double get_quantum_step() const;

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
    
    // Power operation with metadata support
    Value pow(const Value& exponent) const;
    
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
    Value floor() const;
    Value round() const;
    Value sign() const;
    Value step() const;
    
    ////////////////////////////////////////////////////////////////////////////////
    /// SIGNAL PROCESSING FUNCTIONS
    /// ////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    
    // Signal processing operations
    Value integrate() const;
    Value delay(const Value& time) const;
    Value moving_average(const Value& window) const;

    ////////////////////////////////////////////////////////////////////////////////
    /// AUTOMATION ANALYSIS METHODS
    /// ////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    
    // Zero Crossing Analysis
    std::vector<double> find_zero_crossings(double start_time, double end_time) const;
    double get_next_zero_crossing(double from_time) const;
    bool has_zero_crossings_in_range(double start_time, double end_time) const;
    
    // Threshold Analysis
    std::vector<double> find_threshold_crossings(double threshold, double start_time, double end_time) const;
    double get_next_threshold_crossing(double threshold, double from_time, bool rising_edge = true) const;
    
    // Range Analysis
    std::vector<std::pair<double, double>> find_value_ranges(double min_val, double max_val, double start_time, double end_time) const;
    bool is_in_range(double min_val, double max_val, double at_time) const;
    double get_time_in_range(double min_val, double max_val, double start_time, double end_time) const;
    
    // Extrema Analysis
    std::vector<double> find_local_maxima(double start_time, double end_time) const;
    std::vector<double> find_local_minima(double start_time, double end_time) const;
    double get_global_maximum_time(double start_time, double end_time) const;
    double get_global_minimum_time(double start_time, double end_time) const;
    
    // Monotonicity Analysis
    bool is_monotonic_in_range(double start_time, double end_time) const;
    bool is_increasing_in_range(double start_time, double end_time) const;
    bool is_decreasing_in_range(double start_time, double end_time) const;

    bool operator==(const String &str) const;

    // Get the name of the type of this value
    String get_type_name() const;
    int get_type_enum() const;
    String display() const;
    String to_lisp_src() const;

    // friend std::ostream &operator<<(std::ostream &os, Value const &v) {
    //   return os << v.display();
    // }

    enum {
        QUOTE,
        ATOM,
        INT,
        FLOAT,
        LIST,
        VECTOR,
        STRING,
        LAMBDA,
        BUILTIN,
        BUILTIN_METHOD,
        BUILTIN_MODULISP_METHOD,
        UNIT,
        NIL,
        SIGNAL,
        ERROR
    } type;

    // private:
    union {
        int i;
        double f;
        BuiltinFuncRawPtr builtin;
        uSEQ_Method_Ptr builtin_method;
        ModuLispInterpreter_Method_Ptr builtin_modulisp_method;
    } stack_data;

    String str;
    std::vector<Value> list;

    std::shared_ptr<Environment> lambda_scope;
    
    // Signal metadata (only present for SIGNAL type values)
    std::optional<SignalMetadata> signal_metadata;
};

// end of class Value

// Global time constant accessible outside class namespace
extern const Value& t;

#endif // VALUE_H_
