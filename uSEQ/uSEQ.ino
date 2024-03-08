/*
 ------------------------------------------------------------------------------
| Copyright Dimitris Kyriakoudis and Chris Kiefer 2022.                                                    |
|                                                                              |
| This source describes Open Hardware and is licensed under the CERN-OHL-S v2. |
|                                                                              |
| You may redistribute and modify this source and make products using it under |
| the terms of the CERN-OHL-S v2 (https://ohwr.org/cern_ohl_s_v2.txt).         |
|                                                                              |
| This source is distributed WITHOUT ANY EXPRESS OR IMPLIED WARRANTY,          |
| INCLUDING OF MERCHANTABILITY, SATISFACTORY QUALITY AND FITNESS FOR A         |
| PARTICULAR PURPOSE. Please see the CERN-OHL-S v2 for applicable conditions.  |
|                                                                              |
| Source location: https://github.com/lnfiniteMonkeys/uSEQ                                      |
|                                                                              |
| As per CERN-OHL-S v2 section 4, should You produce hardware based on this    |
| source, You must where practicable maintain the Source Location visible      |
| on the external case of the Gizmo or other products you make using this      |
| source.                                                                      |
 ------------------------------------------------------------------------------

 */
// uSEQ firmware, by Dimitris Kyriakoudis and Chris Kiefer

/// LISP interpreter forked from Wisp, by Adam McDaniel


// configure the number of PWM and digital outputs (this is to reflect the hardware, each PWM out be configured with a capacitor)




// Not sure where the best place to put this is, needs to be accessible
// by all interpreter functions that may need to raise it
bool currentExprSound = false;

// firmware build options (comment out as needed)

// #define MIDIOUT  // (drum sequencer implemented using (mdo note (f t)))
//#define MIDIIN //(to be implemented)

#include "pinmap.h"

#define SERIAL_OUTS 8
#define SERIAL_INS 32

#if defined(MUSICTHING) || defined(USEQHARDWARE_1_0)
#define ANALOG_INPUTS 
#endif


// end of build options

#include "LispLibrary.h"
#include "MAFilter.hpp"
#include "tempoEstimator.h"
#include "piopwm.h"

#include "drum-model-4.h"

#ifndef NO_ETL

#define ETL_NO_STL
#include <Embedded_Template_Library.h> // Mandatory for Arduino IDE only
#include <etl/map.h>
#include <etl/unordered_map.h>
#include <etl/string.h>
String board(ARDUINO_BOARD);

#endif

////////////////////////////////////////////////////////////////////////////////
/// LISP LIBRARY /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

int ts1 = 0;
int ts_total = 0;
int timer_level = 0;
#define RESET_TIMER \
  ts_total = 0; \
  timer_level = 0;
#define START_TIMER \
  if (timer_level == 0) { ts1 = micros(); }; \
  timer_level++;
#define STOP_TIMER \
  timer_level--; \
  if (timer_level == 0) ts_total += (micros() - ts1);

enum useqInputNames {
  //signals
  USEQI1 = 0,
  USEQI2 = 1,
  //momentary
  USEQM1 = 2,
  USEQM2 = 3,
  //toggle
  USEQT1 = 4,
  USEQT2 = 5,
  //rotary enoder
  USEQRS1 = 6,  //switch
  USEQR1 = 7,    //encoder
  //analog ins
  USEQAI1 = 8,
  USEQAI2 = 9,
  //
  MTMAINKNOB = 10,
  MTXKNOB = 11,
  MTYKNOB = 12,
  MTZSWITCH = 13,
};

double useqInputValues[14];
tempoEstimator tempoI1, tempoI2;


#define NO_LIBM_SUPPORT "no libm support"


// Comment this define out to drop support for standard library functions.
// This allows the program to run without a runtime.
#define USE_STD
#ifdef USE_STD
#else
#define NO_STD "no standard library support"
#endif


////////////////////////////////////////////////////////////////////////////////
/// REQUIRED INCLUDES //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#include <map>
// #include <unordered_map>
#include <vector>
#include <algorithm>





////////////////////////////////////////////////////////////////////////////////
/// ERROR MESSAGES /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#define RO_STRING(x, y) const char PROGMEM x[] = y;
RO_STRING(TOO_FEW_ARGS, "too few arguments to function")
RO_STRING(TOO_MANY_ARGS, "too many arguments to function")
RO_STRING(INVALID_ARGUMENT, "invalid argument")
RO_STRING(MISMATCHED_TYPES, "mismatched types")
RO_STRING(CALL_NON_FUNCTION, "called non-function ")
RO_STRING(UNKNOWN_ERROR, "unknown exception")
RO_STRING(INVALID_LAMBDA, "invalid lambda")
RO_STRING(INVALID_BIN_OP, "invalid binary operation")
RO_STRING(INVALID_ORDER, "cannot order expression")
RO_STRING(BAD_CAST, "cannot cast")
RO_STRING(ATOM_NOT_DEFINED, "atom not defined ")
RO_STRING(EVAL_EMPTY_LIST, "evaluated empty list")
RO_STRING(INTERNAL_ERROR, "internal virtual machine error")
RO_STRING(INDEX_OUT_OF_RANGE, "index out of range")
RO_STRING(MALFORMED_PROGRAM, "malformed program")

//RO_STRING(LISP_LIBRARY, LispLibrary)


////////////////////////////////////////////////////////////////////////////////
/// TYPE NAMES /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#define STRING_TYPE "string"
#define INT_TYPE "int"
#define FLOAT_TYPE "float"
#define UNIT_TYPE "unit"
#define FUNCTION_TYPE "function"
#define ATOM_TYPE "atom"
#define QUOTE_TYPE "quote"
#define LIST_TYPE "list"
#define ERROR_TYPE "error"

////////////////////////////////////////////////////////////////////////////////
/// HELPER FUNCTIONS ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

String unescape(String str) {
  String result = "";
  for (int i = 0; i < str.length(); i++) {
    if (str[i] == '\\' && i + 1 < str.length()) {
      i++;
      switch (str[i]) {
        case 'n':
          result += "\n";
          break;
        case '"':
          result += "\"";
          break;
        case 'r':
          result += "\r";
          break;
        case 't':
          result += "\t";
          break;
        default:
          result += str[i];
          break;
      }
    } else {
      result += str[i];
    }
  }
  return result;
}
// Is this character a valid lisp symbol character
bool is_symbol(char ch) {
  return (isdigit(ch) || isalpha(ch) || ispunct(ch)) && ch != '(' && ch != ')' && ch != '"' && ch != '\'';
}

////////////////////////////////////////////////////////////////////////////////
/// LISP FORWARD DECL //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Forward declaration for Environment class definition
class Value;
class Environment;
Value parse(String &s, int &ptr);
Value run(String code, Environment &env);
Value runParsedCode(std::vector<Value> ast, Environment &env);


////////////////////////////////////////////////////////////////////////////////
/// USEQ DATA      /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#ifdef MIDIOUT
std::map<int, Value> useqMDOMap;
#endif

#ifndef NO_ETL

namespace etl {

  template <>
  struct hash<String>
  {
    std::size_t operator()(const String& k) const
    {
      using etl::hash;
      etl::string<3> firstThree(k.substring(0,3).c_str());
      return hash<etl::string<3> >()(firstThree);
    }
  };

}

#endif

// An instance of a function's scope.
class Environment {
public:
  // Default constructor
  Environment()
    : parent_scope(NULL) {
    // init();
  }

  Environment(const Environment &v)
    : defs(defs) {
    // init();
  }
  Environment(Environment &&v)
    : defs(std::move(v.defs)) {
    // init();
  }
  Environment &operator=(const Environment &env2) {
    this->defs = std::move(env2.defs);
    return *this;
  }

  // Does this environment, or its parent environment,
  // have this atom in scope?
  // This is only used to determine which atoms to capture when
  // creating a lambda function.
  bool has(String const &name) const;
  // Get the value associated with this name in this scope
  Value get(const String &name) const;
  // Set the value associated with this name in this scope
  void set(String name, Value value);
  void set_global(String name, Value value);

  void combine(Environment const &other);
  String toString(Environment const &e);

  void set_parent_scope(Environment *parent) {
    parent_scope = parent;
  }


  // Output this scope in readable form to a stream.
  // friend std::ostream &operator<<(std::ostream &os, Environment const &v);

#ifdef NO_ETL
 static std::map<String, Value> builtindefs;
#else
static etl::unordered_map<String, Value, 256> builtindefs;
#endif

private:

  // The definitions in the scope.
  std::map<String, Value> defs;
  // etl::unordered_map<String, Value, 1024> defs; //note can't do this yet because of forward declaration
  Environment *parent_scope;
  // mutex write_mutex;

  // void init() {
    // mutex_init(&write_mutex);
  // }
};


// The type for a builtin function, which takes a list of values,
// and the environment to run the function in.
typedef Value (*Builtin)(std::vector<Value> &, Environment &);

class Value {
public:
  ////////////////////////////////////////////////////////////////////////////////
  /// CONSTRUCTORS ///////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////

  // Constructs a unit value
  Value()
    : type(UNIT) {}

  // Constructs an integer
  Value(int i)
    : type(INT) {
    stack_data.i = i;
  }
  // Constructs a floating point value
  Value(double f)
    : type(FLOAT) {
    stack_data.f = f;
  }
  // Constructs a list
  Value(std::vector<Value> list)
    : type(LIST), list(list) {}

  // Value(const Value& v ) : stack_data(v.stack_data), str(v.str), list(v.list), lambda_scope(v.lambda_scope) {}
  // Value(Value&& v) : stack_data(std::move(v.stack_data)), str(std::move(v.str)), list(std::move(v.list)), lambda_scope(std::move(v.lambda_scope)) {}
  // Value& operator=(const Value& v) {
  //   this->stack_data = v.stack_data;
  //   this->str = v.str;
  //   this->list = v.list;
  //   this->lambda_scope = v.lambda_scope;
  //   return *this;
  // }


  static Value error() {
    Value result;
    result.type = ERROR;
    return result;
  }
  // Construct a quoted value
  static Value quote(Value quoted) {
    Value result;
    result.type = QUOTE;

    // The first position in the list is
    // used to store the quoted expression.
    result.list.push_back(quoted);
    return result;
  }

  // Construct an atom
  static Value atom(String s) {
    Value result;
    result.type = ATOM;

    // We use the `str` member to store the atom.
    result.str = s;
    return result;
  }

  // Construct a string
  static Value string(String s) {
    Value result;
    result.type = STRING;

    // We use the `str` member to store the string.
    result.str = s;
    return result;
  }

  // Construct a lambda function
  Value(std::vector<Value> params, Value ret, Environment const &env)
    : type(LAMBDA) {
    // We store the params and the result in the list member
    // instead of having dedicated members. This is to save memory.
    list.push_back(Value(params));
    list.push_back(ret);

    // Lambdas capture only variables that they know they will use.
    std::vector<String> used_atoms = ret.get_used_atoms();
    for (size_t i = 0; i < used_atoms.size(); i++) {
      // If the environment has a symbol that this lambda uses, capture it.
      if (env.has(used_atoms[i]))
        lambda_scope.set(used_atoms[i], env.get(used_atoms[i]));
    }
  }

  // Construct a builtin function
  Value(String name, Builtin b)
    : type(BUILTIN) {
    // Store the name of the builtin function in the str member
    // to save memory, and use the builtin function slot in the union
    // to store the function pointer.
    str = name;
    stack_data.b = b;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// C++ INTEROP METHODS ////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////

  // Get all of the atoms used in a given Value
  std::vector<String> get_used_atoms() {
    std::vector<String> result, tmp;
    switch (type) {
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
        for (size_t i = 0; i < list.size(); i++) {
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

  // Is this a builtin function?
  bool is_builtin() {
    return type == BUILTIN;
  }

  // Apply this as a function to a list of arguments in a given environment.
  Value apply(std::vector<Value> &args, Environment &env);
  // Evaluate this value as lisp code.
  Value eval(Environment &env);

  bool is_number() const {
    return type == INT || type == FLOAT;
  }

  bool is_error() const {
    return type == ERROR;
  }

  bool is_list () const {
    return type == LIST;
  }

  // Get the "truthy" boolean value of this value.
  bool as_bool() const {
    return *this != Value(0);
  }

  // Get this item's integer value
  int as_int() const {
    return cast_to_int().stack_data.i;
  }

  // Get this item's floating point value
  double as_float() const {
    return cast_to_float().stack_data.f;
  }

  // Get this item's string value
  String as_string() const {
    // If this item is not a string, throw a cast error.
    if (type != STRING) {
      Serial.print("str: ");
      Serial.println(BAD_CAST);
      currentExprSound = false;
      return "";
    }
    return str;
  }

  // Get this item's atom value
  String as_atom() const {
    // If this item is not an atom, throw a cast error.
    if (type != ATOM) {
      Serial.print("atom: ");
      Serial.println(BAD_CAST);
      currentExprSound = false;
      return "";
    }
    return str;
  }

  // Get this item's list value
  std::vector<Value> as_list() const {
    // If this item is not a list, throw a cast error.
    if (type != LIST) {
      Serial.print("list: ");
      Serial.println(BAD_CAST);
      currentExprSound = false;
      return {Value::error()};
    }
    return list;
  }

  // Push an item to the end of this list
  void push(Value val) {
    // If this item is not a list, you cannot push to it.
    // Throw an error.
    if (type != LIST)
      Serial.println(MISMATCHED_TYPES);

    // throw Error(*this, Environment(), MISMATCHED_TYPES);

    list.push_back(val);
  }

  // Push an item from the end of this list
  Value pop() {
    // If this item is not a list, you cannot pop from it.
    // Throw an error.
    if (type != LIST)
      // throw Error(*this, Environment(), MISMATCHED_TYPES);
      Serial.println(MISMATCHED_TYPES);

    // Remember the last item in the list
    Value result = list[list.size() - 1];
    // Remove it from this instance
    list.pop_back();
    // Return the remembered value
    return result;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// TYPECASTING METHODS ////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////

  // Cast this to an integer value
  Value cast_to_int() const {
    switch (type) {
      case INT: return *this;
      case FLOAT: return Value(int(stack_data.f));
      // Only ints and floats can be cast to an int
      default:
        Serial.println("int: ");
        Serial.println(BAD_CAST);
      currentExprSound = false;
        return Value::error();
        // throw Error(*this, Environment(), BAD_CAST);
    }
  }

  // Cast this to a floating point value
  Value cast_to_float() const {
    switch (type) {
      case FLOAT: return *this;
      case INT: return Value(double(stack_data.i));
      // Only ints and floats can be cast to a float
      default:
        Serial.println("float: ");
        Serial.println(BAD_CAST);
      currentExprSound = false;
        // throw Error(*this, Environment(), BAD_CAST);
        return Value::error();
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// COMPARISON OPERATIONS //////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////

  bool operator==(Value other) const {
    // If either of these values are floats, promote the
    // other to a float, and then compare for equality.
    if (type == FLOAT && other.type == INT) return *this == other.cast_to_float();
    else if (type == INT && other.type == FLOAT) return this->cast_to_float() == other;
    // If the values types aren't equal, then they cannot be equal.
    else if (type != other.type) return false;

    switch (type) {
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

  bool operator!=(Value other) const {
    return !(*this == other);
  }

  // bool operator<(Value other) const {
  //     if (other.type != FLOAT && other.type != INT)
  //         throw Error(*this, Environment(), INVALID_BIN_OP);

  //     switch (type) {
  //     case FLOAT:
  //         return stack_data.f < other.cast_to_float().stack_data.f;
  //     case INT:
  //         if (other.type == FLOAT)
  //             return cast_to_float().stack_data.f < other.stack_data.f;
  //         else return stack_data.i < other.stack_data.i;
  //     default:
  //         throw Error(*this, Environment(), INVALID_ORDER);
  //     }
  // }

  ////////////////////////////////////////////////////////////////////////////////
  /// ORDERING OPERATIONS ////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////

  bool operator>=(Value other) const {
    return !(*this < other);
  }

  bool operator<=(Value other) const {
    return (*this == other) || (*this < other);
  }

  bool operator>(Value other) const {
    return !(*this <= other);
  }

  bool operator<(Value other) const {
    // Other type must be a float or an int
    if (other.type != FLOAT && other.type != INT)
      Serial.println(INVALID_BIN_OP);
    // throw Error(*this, Environment(), INVALID_BIN_OP);

    switch (type) {
      case FLOAT:
        // If this is a float, promote the other value to a float and compare.
        return stack_data.f < other.cast_to_float().stack_data.f;
      case INT:
        // If the other value is a float, promote this value to a float and compare.
        if (other.type == FLOAT)
          return cast_to_float().stack_data.f < other.stack_data.f;
        // Otherwise compare the integer values
        else return stack_data.i < other.stack_data.i;
      default:
        // Only allow comparisons between integers and floats
        Serial.println(INVALID_ORDER);
        return false;
        // throw Error(*this, Environment(), INVALID_ORDER);
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// ARITHMETIC OPERATIONS //////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////

  // This function adds two lisp values, and returns the lisp value result.
  Value operator+(Value other) const {
    // If the other value's type is the unit type,
    // don't even bother continuing.
    // Unit types consume all arithmetic operations.
    if (other.type == UNIT) return other;

    // Other type must be a float or an int
    if ((is_number() || other.is_number()) && !(is_number() && other.is_number()))
      Serial.println(INVALID_BIN_OP);
    // throw Error(*this, Environment(), INVALID_BIN_OP);

    switch (type) {
      case FLOAT:
        // If one is a float, promote the other by default and do
        // float addition.
        return Value(stack_data.f + other.cast_to_float().stack_data.f);
      case INT:
        // If the other type is a float, go ahead and promote this expression
        // before continuing with the addition.
        if (other.type == FLOAT)
          return Value(cast_to_float() + other.stack_data.f);
        // Otherwise, do integer addition.
        else return Value(stack_data.i + other.stack_data.i);
      case STRING:
        // If the other value is also a string, do the concat
        if (other.type == STRING)
          return Value::string(str + other.str);
        // We throw an error if we try to concat anything of non-string type
        else
          Serial.println(INVALID_BIN_OP);
        // throw Error(*this, Environment(), INVALID_BIN_OP);
      case LIST:
        // If the other value is also a list, do the concat
        if (other.type == LIST) {
          // Maintain the value that will be returned
          Value result = *this;
          // Add each item in the other list to the end of this list
          for (size_t i = 0; i < other.list.size(); i++)
            result.push(other.list[i]);
          return result;

        } else
          Serial.println(INVALID_BIN_OP);
        // throw Error(*this, Environment(), INVALID_BIN_OP);
      case UNIT:
        return *this;
      default:
        Serial.println(INVALID_BIN_OP);
        return Value();
        // throw Error(*this, Environment(), INVALID_BIN_OP);
    }
  }

  // This function subtracts two lisp values, and returns the lisp value result.
  Value operator-(Value other) const {
    // If the other value's type is the unit type,
    // don't even bother continuing.
    // Unit types consume all arithmetic operations.
    if (other.type == UNIT) return other;

    // Other type must be a float or an int
    if (other.type != FLOAT && other.type != INT)
      Serial.println(INVALID_BIN_OP);
    // throw Error(*this, Environment(), INVALID_BIN_OP);

    switch (type) {
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
        else return Value(stack_data.i - other.stack_data.i);
      case UNIT:
        // Unit types consume all arithmetic operations.
        return *this;
      default:
        // This operation was done on an unsupported type
        Serial.println(INVALID_BIN_OP);
        return Value();
        // throw Error(*this, Environment(), INVALID_BIN_OP);
    }
  }

  // This function multiplies two lisp values, and returns the lisp value result.
  Value operator*(Value other) const {
    // If the other value's type is the unit type,
    // don't even bother continuing.
    // Unit types consume all arithmetic operations.
    if (other.type == UNIT) return other;

    // Other type must be a float or an int
    if (other.type != FLOAT && other.type != INT)
      Serial.println(INVALID_BIN_OP);
    // throw Error(*this, Environment(), INVALID_BIN_OP);

    switch (type) {
      case FLOAT:
        return Value(stack_data.f * other.cast_to_float().stack_data.f);
      case INT:
        // If the other type is a float, go ahead and promote this expression
        // before continuing with the product
        if (other.type == FLOAT)
          return Value(cast_to_float().stack_data.f * other.stack_data.f);
        // Otherwise, do integer multiplication.
        else return Value(stack_data.i * other.stack_data.i);
      case UNIT:
        // Unit types consume all arithmetic operations.
        return *this;
      default:
        // This operation was done on an unsupported type
        Serial.println(INVALID_BIN_OP);
        return Value();
        // throw Error(*this, Environment(), INVALID_BIN_OP);
    }
  }

  // This function divides two lisp values, and returns the lisp value result.
  Value operator/(Value other) const {
    // If the other value's type is the unit type,
    // don't even bother continuing.
    // Unit types consume all arithmetic operations.
    if (other.type == UNIT) return other;

    // Other type must be a float or an int
    if (other.type != FLOAT && other.type != INT)
      Serial.println(INVALID_BIN_OP);
    //             throw Error(*this, Environment(), INVALID_BIN_OP);

    switch (type) {
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
          else res = Value(stack_data.i / other.stack_data.i);
          return res;
        }
      case UNIT:
        // Unit types consume all arithmetic operations.
        return *this;
      default:
        // This operation was done on an unsupported type
        Serial.println(INVALID_BIN_OP);
        return Value();
        //  throw Error(*this, Environment(), INVALID_BIN_OP);
    }
  }

  // This function finds the remainder of two lisp values, and returns the lisp value result.
  Value operator%(Value other) const {
    // If the other value's type is the unit type,
    // don't even bother continuing.
    // Unit types consume all arithmetic operations.
    if (other.type == UNIT) return other;

    // Other type must be a float or an int
    if (other.type != FLOAT && other.type != INT)
      Serial.println(INVALID_BIN_OP);
    // throw Error(*this, Environment(), INVALID_BIN_OP);

    switch (type) {
      // If we support libm, we can find the remainder of floating point values.
      case FLOAT:
        return Value(fmod(stack_data.f, other.cast_to_float().stack_data.f));
      case INT:
        if (other.type == FLOAT)
          return Value(fmod(cast_to_float().stack_data.f, other.stack_data.f));
        else return Value(stack_data.i % other.stack_data.i);

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
        Serial.println(INVALID_BIN_OP);
        return Value();
        // throw Error(*this, Environment(), INVALID_BIN_OP);
    }
  }

  // Get the name of the type of this value
  String get_type_name() {
    switch (type) {
      case QUOTE: return QUOTE_TYPE;
      case ATOM: return ATOM_TYPE;
      case INT: return INT_TYPE;
      case FLOAT: return FLOAT_TYPE;
      case LIST: return LIST_TYPE;
      case STRING: return STRING_TYPE;
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
        Serial.println(INTERNAL_ERROR);
        return "";
        // throw Error(*this, Environment(), INTERNAL_ERROR);
    }
  }

  String display() const {
    String result;
    switch (type) {
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
        for (size_t i = 0; i < list.size(); i++) {
          result += list[i].debug();
          if (i < list.size() - 1) result += " ";
        }
        return "(lambda " + result + ")";
      case LIST:
        for (size_t i = 0; i < list.size(); i++) {
          result += list[i].debug();
          if (i < list.size() - 1) result += " ";
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
        Serial.println(INTERNAL_ERROR);
        // throw Error(*this, Environment(), INTERNAL_ERROR);
        return "";
    }
  }

  String debug() const {
    String result;
    // string result;
    switch (type) {
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
        for (size_t i = 0; i < str.length(); i++) {
          if (str[i] == '"') result += "\\\"";
          else result += str[i];
        }
        return "\"" + result + "\"";
      case LAMBDA:
        for (size_t i = 0; i < list.size(); i++) {
          result += list[i].debug();
          if (i < list.size() - 1) result += " ";
        }
        return "(lambda " + result + ")";
      case LIST:
        for (size_t i = 0; i < list.size(); i++) {
          result += list[i].debug();
          if (i < list.size() - 1) result += " ";
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
        Serial.println(INTERNAL_ERROR);
        // throw Error(*this, Environment(), INTERNAL_ERROR);
        return "";
    }
  }

  // friend std::ostream &operator<<(std::ostream &os, Value const &v) {
  //   return os << v.display();
  // }

private:
  enum {
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

  union {
    int i;
    double f;
    Builtin b;
  } stack_data;

  String str;
  std::vector<Value> list;
  Environment lambda_scope;
};

// end of class Value

// Error::Error(Value v, Environment const &env, const char *msg) : env(env), msg(msg) {
//     cause = new Value;
//     *cause = v;
// }

// Error::Error(Error const &other) : env(other.env), msg(other.msg) {
//     cause = new Value(*other.cause);
// }

// Error::~Error() {
//     delete cause;
// }

// std::string Error::description() {
//     return "error: the expression `" + cause->debug() + "` failed in scope " + to_string(env) + " with message \"" + msg + "\"";
// }

void Environment::combine(Environment const &other) {
  // Normally, I would use the `insert` method of the `map` class,
  // but it doesn't overwrite previously declared values for keys.
  auto itr = other.defs.begin();
  for (; itr != other.defs.end(); itr++) {
    // Iterate through the keys and assign each value.
    defs[itr->first] = itr->second;
  }
}

// std::ostream &operator<<(std::ostream &os, Environment const &e) {
//   auto itr = e.defs.begin();
//   os << "{ ";
//   for (; itr != e.defs.end(); itr++) {
//     os << '\'' << itr->first << "' : " << itr->second.debug() << ", ";
//   }
//   return os << "}";
// }

String Environment::toString(Environment const &e) {
  auto itr = e.defs.begin();
  String os = "{ ";
  for (; itr != e.defs.end(); itr++) {

    os += '\'';
    os += itr->first;
    os += "' : ";
    os += itr->second.debug();
    os += ", ";
  }
  os += "}";
  return os;
}

void Environment::set(String name, Value value) {
  //for multicore
  // mutex_enter_blocking(&write_mutex);
  defs[name] = value;
  // mutex_exit(&write_mutex);
}

void Environment::set_global(String name, Value value) {
  set(name, value);
  if (parent_scope) {
    parent_scope->set_global(name, value);
  }
}

Value Value::apply(std::vector<Value> &args, Environment &env) {

  switch (type) {
    case LAMBDA:
      {
        Environment e;
        std::vector<Value> *params;

        // Get the list of parameter atoms
        params = &list[0].list;
        if (params->size() != args.size()) {
          Serial.println(args.size() > params->size() ? TOO_MANY_ARGS : TOO_FEW_ARGS);
          return Value::error();
        }

        // throw Error(Value(args), env, args.size() > params.size()?
        //     TOO_MANY_ARGS : TOO_FEW_ARGS
        // );

        // Get the captured scope from the lambda
        e = lambda_scope;
        // And make this scope the parent scope
        e.set_parent_scope(&env);

        // Iterate through the list of parameters and
        // insert the arguments into the scope.
        for (size_t i = 0; i < params->size(); i++) {
          if ((*params)[i].type != ATOM)
            Serial.println(INVALID_LAMBDA);
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
        auto result = (stack_data.b)(args, env);
        return result;
      }
    default:
      // We can only call lambdas and builtins
      Serial.print(CALL_NON_FUNCTION);
      Serial.println(str);
      return Value::error();
  }
}

int ts_get = 0;
int get_time = 0;
Value Value::eval(Environment &env) {
  std::vector<Value> args;
  Value function;
  // Environment e;
  switch (type) {
    case QUOTE:
      return list[0];
    case ATOM:
      {
        ts_get = micros();
        auto atomdata = env.get(str);
        if (atomdata == Value::error()) {
          Serial.print("Get error: ");
          Serial.println(str);
        }
        ts_get = micros() - ts_get;
        get_time += ts_get;
        return atomdata;
      }
    case LIST:
      {
        if (list.size() < 1)
          Serial.println(EVAL_EMPTY_LIST);
        // throw Error(*this, env, EVAL_EMPTY_LIST);
        //note: this needs to be a copy?  so original remains unevaluated?  or if not, use std::span to avoid the copy?
        args = std::vector<Value>(list.begin() + 1, list.end());
        // Only evaluate our arguments if it's not builtin!
        // Builtin functions can be special forms, so we
        // leave them to evaluate their arguments.
        function = list[0].eval(env);
        if (function.is_error()) {
          Serial.print("err: function is error");
          return Value::error();
        } else {
          //lambda?
          bool evalError=false;
          if (!function.is_builtin()) {
            for (size_t i = 0; i < args.size(); i++) {
              args[i] = args[i].eval(env);
              if (args[i] == Value::error()) {
                evalError=true;
                break;
              }
            }
          }

          if (evalError) {
            return Value::error();
          }else{
            auto functionResult = function.apply(
              args,
              env);
            return functionResult;
          }
        }
      }

    default:
      return *this;
  }
}

void skip_whitespace(String &s, int &ptr) {
  while (isspace(s[ptr])) { ptr++; }
}

// Value v = Value();
// Value ftest() {
//   return Value();
// }

// Parse a single value and increment the pointer
// to the beginning of the next value to parse.
Value parse(String &s, int &ptr) {
  skip_whitespace(s, ptr);

  // Skip comments
  while (s[ptr] == ';') {
    // If this is a comment
    int work_ptr = ptr;
    // Skip to the end of the line
    while (s[work_ptr] != '\n' && work_ptr < int(s.length())) { work_ptr++; }
    ptr = work_ptr;
    skip_whitespace(s, ptr);

    // If we're at the end of the string, return an empty value
    if (s.substring(ptr, ptr + s.length() - ptr - 1) == "")
      return Value();
  }


  // Parse the value
  if (s == "") {
    return Value();
  } else if (s[ptr] == '\'') {
    // If this is a quote
    ptr++;
    return Value::quote(parse(s, ptr));

  } else if (s[ptr] == '(') {
    // If this is a list
    skip_whitespace(s, ++ptr);

    Value result = Value(std::vector<Value>());

    while (s[ptr] != ')') {
        Value res = parse(s, ptr);
        if (res == Value::error()) {
            result = Value::error();
            break;
        }else {
            result.push(res);
        }
    }

    skip_whitespace(s, ++ptr);
    return result;

  } else if (isdigit(s[ptr]) || (s[ptr] == '-' && isdigit(s[ptr + 1]))) {
    // If this is a number
    bool negate = s[ptr] == '-';
    if (negate) ptr++;

    int save_ptr = ptr;
    while (isdigit(s[ptr]) || s[ptr] == '.') ptr++;
    String n = s.substring(save_ptr, save_ptr + ptr);
    skip_whitespace(s, ptr);

    if (n.indexOf('.') != -1)
      // return Value((negate? -1 : 1) * atof(n.c_str()));
      return Value((negate ? -1 : 1) * atof(n.c_str()));
    else return Value((negate ? -1 : 1) * atoi(n.c_str()));

  } else if (s[ptr] == '\"') {
    // If this is a string
    int n = 1;
    while (s[ptr + n] != '\"') {
      if (ptr + n >= int(s.length())) {
        Serial.print(MALFORMED_PROGRAM);
        Serial.println(" 1");
        return Value::error();
        // throw std::runtime_error(MALFORMED_PROGRAM);
      }

      if (s[ptr + n] == '\\') n++;
      n++;
    }

    String x = s.substring(ptr + 1, ptr + 1 + n - 1);
    ptr += n + 1;
    skip_whitespace(s, ptr);

    // Iterate over the characters in the string, and
    // replace escaped characters with their intended values.
    x = unescape(x);
    return Value::string(x);
  } else if (s[ptr] == '@') {
    ptr++;
    skip_whitespace(s, ptr);
    return Value();

  } else if (is_symbol(s[ptr])) {
    // If this is a string
    int n = 0;
    while (is_symbol(s[ptr + n])) {
      n++;
    }

    String x = s.substring(ptr, ptr + n);
    ptr += n;
    skip_whitespace(s, ptr);
    return Value::atom(x);
  } else {
    Serial.println(MALFORMED_PROGRAM);
    return Value::error();
    // throw std::runtime_error(MALFORMED_PROGRAM);
  }
}

// Parse an entire program and get its list of expressions.
std::vector<Value> parse(String s) {
  int i = 0, last_i = -1;
  std::vector<Value> result;
  bool error = false;
  // While the parser is making progress (while the pointer is moving right)
  // and the pointer hasn't reached the end of the string,
  while (last_i != i && i <= int(s.length() - 1)) {
    // Parse another expression and add it to the list.
    last_i = i;
    Value token = parse(s, i);
    if (token.is_error()) {
      error = true;
      break;
    }
    result.push_back(token);
  }

  // If the whole string wasn't parsed, the program must be bad.
  if (i < int(s.length())) {
    Serial.print("parse: ");
    Serial.println(MALFORMED_PROGRAM);
    error = true;
  }
  if (error) {
    result.clear();
  }
  // Return the list of values parsed.
  return result;
}

int ts_parse = 0, ts_run = 0;
int parse_time = 0, run_time = 0;
// Execute code in an environment
Value run(String code, Environment &env) {
  ts_parse = micros();
  // Parse the code
  std::vector<Value> parsed = parse(code);
  parse_time += (micros() - ts_parse);
  if (parsed.size() > 0) {
    // Iterate over the expressions and evaluate them
    // in this environment.
    ts_run = micros();
    for (size_t i = 0; i < parsed.size() - 1; i++)
      parsed[i].eval(env);

    // Return the result of the last expression.
    auto result = parsed[parsed.size() - 1].eval(env);
    run_time += (micros() - ts_run);
    return result;
  } else {
    return Value::error();
  }
}

Value runParsedCode(std::vector<Value> ast, Environment &env) {
  if (ast.size() > 0) {
    // Iterate over the expressions and evaluate them
    // in this environment.
    ts_run = micros();
    Value result;
    for (size_t i = 0; i < ast.size(); i++) {
      result = ast[i].eval(env);
    }

    // Return the result of the last expression.
    // auto result = ast[ast.size() - 1].eval(env);
    run_time += (micros() - ts_run);
    return result;
  } else {
//    return Value::error();
    return Value();
  }
}


// This namespace contains all the definitions of builtin functions
namespace builtin {
// This function is NOT a builtin function, but it is used
// by almost all of them.
//
// Special forms are just builtin functions that don't evaluate
// their arguments. To make a regular builtin that evaluates its
// arguments, we just call this function in our builtin definition.
Value eval_args(std::vector<Value> &args, Environment &env) {
  Value ret = Value();
  for (size_t i = 0; i < args.size(); i++) {
    args[i] = args[i].eval(env);
    if (args[i] == Value::error()) {
      Serial.println("eval args error");
      ret = Value::error();
    }
  }
  return ret;
}

// Create a lambda function (SPECIAL FORM)
Value lambda(std::vector<Value> &args, Environment &env) {
  if (args.size() < 2)
    Serial.println(TOO_FEW_ARGS);

  // throw Error(Value("lambda", lambda), env, TOO_FEW_ARGS);

  if (args[0].get_type_name() != LIST_TYPE)
    Serial.println(INVALID_LAMBDA);

  // throw Error(Value("lambda", lambda), env, INVALID_LAMBDA);

  return Value(args[0].as_list(), args[1], env);
}

// if-else (SPECIAL FORM)
Value if_then_else(std::vector<Value> &args, Environment &env) {
  if (args.size() != 3)
    Serial.println(args.size() > 3 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  if (args[0].eval(env).as_bool())
    return args[1].eval(env);
  else return args[2].eval(env);
}

// Define a variable with a value (SPECIAL FORM)
Value define(std::vector<Value> &args, Environment &env) {
  if (args.size() != 2)
    Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);

  Value result = args[1].eval(env);
  env.set(args[0].display(), result);
  return result;
}

Value set(std::vector<Value> &args, Environment &env) {
  if (args.size() != 2)
    Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);

  Value result = args[1].eval(env);
  env.set_global(args[0].display(), result);
  return result;
}

// Define a function with parameters and a result expression (SPECIAL FORM)
Value defun(std::vector<Value> &args, Environment &env) {
  if (args.size() != 3)
    Serial.println(args.size() > 3 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("defun", defun), env, args.size() > 3? TOO_MANY_ARGS : TOO_FEW_ARGS);

  if (args[1].get_type_name() != LIST_TYPE)
    Serial.println(INVALID_LAMBDA);
  // throw Error(Value("defun", defun), env, INVALID_LAMBDA);

  Value f = Value(args[1].as_list(), args[2], env);
  env.set(args[0].display(), f);
  return f;
}

// Loop over a list of expressions with a condition (SPECIAL FORM)
Value while_loop(std::vector<Value> &args, Environment &env) {
  Value acc;
  while (args[0].eval(env).as_bool()) {
    for (size_t i = 1; i < args.size() - 1; i++)
      args[i].eval(env);
    acc = args[args.size() - 1].eval(env);
  }
  return acc;
}

// Iterate through a list of values in a list (SPECIAL FORM)
Value for_loop(std::vector<Value> &args, Environment &env) {
  Value acc;
  std::vector<Value> list = args[1].eval(env).as_list();

  for (size_t i = 0; i < list.size(); i++) {
    env.set(args[0].as_atom(), list[i]);

    for (size_t j = 1; j < args.size() - 1; j++)
      args[j].eval(env);
    acc = args[args.size() - 1].eval(env);
  }

  return acc;
}

// Evaluate a block of expressions in the current environment (SPECIAL FORM)
Value do_block(std::vector<Value> &args, Environment &env) {
  Value acc;
  for (size_t i = 0; i < args.size(); i++)
    acc = args[i].eval(env);
  return acc;
}

// Evaluate a block of expressions in a new environment (SPECIAL FORM)
Value scope(std::vector<Value> &args, Environment &env) {
  Environment e = env;
  Value acc;
  for (size_t i = 0; i < args.size(); i++)
    acc = args[i].eval(e);
  return acc;
}

// Quote an expression (SPECIAL FORM)
Value quote(std::vector<Value> &args, Environment &) {
  // std::vector<Value> v(args);
  // for (size_t i = 0; i < args.size(); i++)
  //   v.push_back(args[i]);
  return Value(args);
}

#ifdef USE_STD
// Exit the program with an integer code
// Value exit(std::vector<Value> &args, Environment &env) {
//   // Is not a special form, so we can evaluate our args.
//   eval_args(args, env);

//   std::exit(args.size() < 1 ? 0 : args[0].cast_to_int().as_int());
//   return Value();
// }

// Print several values and return the last one
Value print(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() < 1)
    Serial.println(TOO_FEW_ARGS);
  // throw Error(Value("print", print), env, TOO_FEW_ARGS);

  Value acc;
  for (size_t i = 0; i < args.size(); i++) {
    acc = args[i];
    Serial.print(acc.display().c_str());
    // std::cout << acc.display();
    if (i < args.size() - 1)
      // std::cout << " ";
      Serial.print(" ");
  }
  // std::cout << std::endl;
  Serial.println();
  return acc;
}


// Get a random number between two numbers inclusively
Value gen_random(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("random", random), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);

  int low = args[0].as_int(), high = args[1].as_int();
  return Value((int)random(low, high));
}

// // Get the contents of a file
// Value read_file(std::vector<Value> &args, Environment &env) {
//     // Is not a special form, so we can evaluate our args.
//     eval_args(args, env);

//     if (args.size() != 1)
//         throw Error(Value("read-file", read_file), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

//     // return Value::string(content);
//     return Value::string(read_file_contents(args[0].as_string()));
// }

// // Write a string to a file
// Value write_file(std::vector<Value> &args, Environment &env) {
//     // Is not a special form, so we can evaluate our args.
//     eval_args(args, env);

//     if (args.size() != 2)
//         throw Error(Value("write-file", write_file), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

//     std::ofstream f;
//     // The first argument is the file name
//     f.open(args[0].as_string().c_str());
//     // The second argument is the contents of the file to write
//     Value result = Value((f << args[1].as_string())? 1 : 0);
//     f.close();
//     return result;
// }

// Read a file and execute its code
// Value include(std::vector<Value> &args, Environment &env) {
//     // Import is technically not a special form, it's more of a macro.
//     // We can evaluate our arguments.
//     eval_args(args, env);

//     if (args.size() != 1)
//         throw Error(Value("include", include), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

//     Environment e;
//     Value result = run(read_file_contents(args[0].as_string()), e);
//     env.combine(e);
//     return result;
// }
#endif

// Evaluate a value as code
Value eval(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  Value ret = eval_args(args, env);
  if (ret == Value::error()) {
    Serial.println("eval err");
    return Value::error();
  }
  if (args.size() != 1) {
    Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    return Value();
  }
  // throw Error(Value("eval", eval), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
  else {
    return args[0].eval(env);
  }
}

// Create a list of values
Value list(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  return Value(args);
}

// Sum multiple values
Value sum(std::vector<Value> &args, Environment &env) {
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
Value subtract(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("-", subtract), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
  return args[0] - args[1];
}

// Multiply several values
Value product(std::vector<Value> &args, Environment &env) {
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
Value divide(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("/", divide), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
  auto result = args[0] / args[1];
  return result;
}

// Get the remainder of values
Value remainder(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("%", remainder), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
  auto result = args[0] % args[1];
  return result;
}

// Are two values equal?
Value eq(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("=", eq), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
  return Value(int(args[0] == args[1]));
}

// Are two values not equal?
Value neq(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("!=", neq), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
  return Value(int(args[0] != args[1]));
}

// Is one number greater than another?
Value greater(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value(">", greater), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
  return Value(int(args[0] > args[1]));
}

// Is one number less than another?
Value less(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("<", less), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
  return Value(int(args[0] < args[1]));
}

// Is one number greater than or equal to another?
Value greater_eq(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value(">=", greater_eq), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
  return Value(int(args[0] >= args[1]));
}

// Is one number less than or equal to another?
Value less_eq(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("<=", less_eq), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
  return Value(int(args[0] <= args[1]));
}

// Get the type name of a value
Value get_type_name(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 1)
    Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("type", get_type_name), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

  return Value::string(args[0].get_type_name());
}

// Cast an item to a float
Value cast_to_float(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 1)
    Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value(FLOAT_TYPE, cast_to_float), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
  return args[0].cast_to_float();
}

// Cast an item to an int
Value cast_to_int(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 1)
    Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value(INT_TYPE, cast_to_int), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
  return args[0].cast_to_int();
}

// Index a list
Value index(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("index", index), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);

  std::vector<Value> list = args[0].as_list();
  int i = args[1].as_int();
  if (list.empty() || i >= (int)list.size()) {
    Serial.println(INDEX_OUT_OF_RANGE);
    return Value::error();
  }
  // throw Error(list, env, INDEX_OUT_OF_RANGE);

  return list[i];
}

// Insert a value into a list
Value insert(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 3)
    Serial.println(args.size() > 3 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("insert", insert), env, args.size() > 3? TOO_MANY_ARGS : TOO_FEW_ARGS);

  std::vector<Value> list = args[0].as_list();
  int i = args[1].as_int();
  if (i > (int)list.size())
    Serial.println(INDEX_OUT_OF_RANGE);
  else
      list.insert(list.begin() + args[1].as_int(), args[2].as_int());
  return Value(list);
}

// Remove a value at an index from a list
Value remove(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    Serial.println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("remove", remove), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);

  std::vector<Value> list = args[0].as_list();
  int i = args[1].as_int();
  if (list.empty() || i >= (int)list.size())
    Serial.println(INDEX_OUT_OF_RANGE);
  else
    list.erase(list.begin() + i);
  return Value(list);
}

// Get the length of a list
Value len(std::vector<Value> &args, Environment &env) {
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
Value push(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() == 0)
    Serial.println(TOO_FEW_ARGS);
  // throw Error(Value("push", push), env, TOO_FEW_ARGS);
  for (size_t i = 1; i < args.size(); i++)
    args[0].push(args[i]);
  return args[0];
}

Value pop(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 1)
    Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("pop", pop), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
  return args[0].pop();
}

Value head(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 1)
    Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("head", head), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
  std::vector<Value> list = args[0].as_list();
  if (list.empty())
    Serial.println(INDEX_OUT_OF_RANGE);
  // throw Error(Value("head", head), env, INDEX_OUT_OF_RANGE);

  return list[0];
}

Value tail(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 1)
    Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("tail", tail), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

  std::vector<Value> result, list = args[0].as_list();

  for (size_t i = 1; i < list.size(); i++)
    result.push_back(list[i]);

  return Value(result);
}

Value flatten(Value &val, Environment &env) {
    std::vector<Value> flattened;
    if (!val.is_list()) {
      flattened.push_back(val);
    }else{
      auto valList = val.as_list();
      for(size_t i=0; i < valList.size(); i++) {
        Value evaluatedElement = valList[i].eval(env);
        if(evaluatedElement.is_list()) {
          auto flattenedElement = flatten(evaluatedElement, env).as_list();
          flattened.insert(flattened.end(), flattenedElement.begin(), flattenedElement.end());
        }else{
          flattened.push_back(evaluatedElement);
        }
      }
    }
    return Value(flattened);
  }


Value parse(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 1)
    Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("parse", parse), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
  if (args[0].get_type_name() != STRING_TYPE)
    Serial.println(INVALID_ARGUMENT);
  // throw Error(args[0], env, INVALID_ARGUMENT);
  std::vector<Value> parsed = ::parse(args[0].as_string());

  // if (parsed.size() == 1)
  //     return parsed[0];
  // else return Value(parsed);
  return Value(parsed);
}

Value replace(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 3)
    Serial.println(args.size() > 3 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("replace", replace), env, args.size() > 3? TOO_MANY_ARGS : TOO_FEW_ARGS);

  String src = args[0].as_string();
  src.replace(args[1].as_string(), args[2].as_string());
  return Value::string(src);
}

Value display(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 1)
    Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("display", display), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

  return Value::string(args[0].display());
}

Value debug(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 1)
    Serial.println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("debug", debug), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

  return Value::string(args[0].debug());
}

Value map_list(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  std::vector<Value> result, l = args[1].as_list(), tmp;
  for (size_t i = 0; i < l.size(); i++) {
    tmp.push_back(l[i]);
    result.push_back(args[0].apply(tmp, env));
    tmp.clear();
  }
  return Value(result);
}

Value filter_list(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  std::vector<Value> result, l = args[1].as_list(), tmp;
  for (size_t i = 0; i < l.size(); i++) {
    tmp.push_back(l[i]);
    if (args[0].apply(tmp, env).as_bool())
      result.push_back(l[i]);
    tmp.clear();
  }
  return Value(result);
}

Value reduce_list(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  std::vector<Value> l = args[2].as_list(), tmp;
  Value acc = args[1];
  for (size_t i = 0; i < l.size(); i++) {
    tmp.push_back(acc);
    tmp.push_back(l[i]);
    acc = args[0].apply(tmp, env);
    tmp.clear();
  }
  return acc;
}

Value range(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  std::vector<Value> result;
  Value low = args[0], high = args[1];
  if (low.get_type_name() != INT_TYPE && low.get_type_name() != FLOAT_TYPE)
    Serial.println(MISMATCHED_TYPES);
  // throw Error(low, env, MISMATCHED_TYPES);
  if (high.get_type_name() != INT_TYPE && high.get_type_name() != FLOAT_TYPE)
    Serial.println(MISMATCHED_TYPES);
  // throw Error(high, env, MISMATCHED_TYPES);

  if (low >= high) return Value(result);

  while (low < high) {
    result.push_back(low);
    low = low + Value(1);
  }
  return Value(result);
}

// uSEQ-specific builtins

}
//end of builtin namespace

inline int digital_out_pin(int out) {
  int res=-1;
  int pindex = PWM_OUTS + out;
  if ( pindex <= 6)
    res = useq_output_pins[pindex-1];
  return res;
}

inline int digital_out_LED_pin(int out) {
  int res=-1;
  int pindex = PWM_OUTS + out;
  if ( pindex <= 6)
    res = useq_output_led_pins[pindex-1];
  return res;
}

inline int analog_out_pin(int out) {
  int res=-1;
  if (out <= PWM_OUTS)
    res = useq_output_pins[out-1];
  return res;
}

inline int analog_out_LED_pin(int out) {
  int res=-1;
  if (out <= PWM_OUTS)
    res = useq_output_led_pins[out-1];
  return res;
}


#define BUILTINFUNC(__name__, __body__, __numArgs__) \
  Value __name__(std::vector<Value> &args, Environment &env) { \
    eval_args(args, env); \
    Value ret = Value(); \
    if (args.size() != __numArgs__) {\
      Serial.println(args.size() > __numArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS); \
      ret = Value::error(); \
    } else { \
      __body__ \
    } \
    return ret; \
  }


#define BUILTINFUNC_VARGS(__name__, __body__, __minArgs__, __maxArgs__) \
  Value __name__(std::vector<Value> &args, Environment &env) { \
    eval_args(args, env); \
    Value ret = Value(); \
    if (args.size() < __minArgs__ || args.size() > __maxArgs__) \
      Serial.println(args.size() > __maxArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS); \
    else { \
      __body__ \
    } \
    return ret; \
  }


#define BUILTINFUNC_NOEVAL(__name__, __body__, __numArgs__) \
  Value __name__(std::vector<Value> &args, Environment &env) { \
    Value ret = Value(); \
    if (args.size() != __numArgs__) \
      Serial.println(args.size() > __numArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS); \
    else { \
      __body__ \
    } \
    return ret; \
  }


///////////////////////////
// USEQ NAMESPACE
///////////////////////////

Environment env;

void pio_pwm_set_period(PIO pio, uint sm, uint32_t period) {
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_put_blocking(pio, sm, period);
    pio_sm_exec(pio, sm, pio_encode_pull(false, false));
    pio_sm_exec(pio, sm, pio_encode_out(pio_isr, 32));
    pio_sm_set_enabled(pio, sm, true);
}

// Write `level` to TX FIFO. State machine will copy this into X.
void pio_pwm_set_level(PIO pio, uint sm, uint32_t level) {
    pio_sm_put_blocking(pio, sm, level);
}

namespace useq {



// Meter
double meter_numerator = 4;
double meter_denominator = 4;

//TODO: better to have custom specified long phasors e.g. (addPhasor phasorName (lambda () (beats * 17))) - to be stored in std::map<String, Double[2]>
double barsPerPhrase = 16;
double phrasesPerSection = 16;

// BPM
double defaultBpm = 130.0;
double bpm = 130.0;
double bps = 0.0;

//TODO: builtin functions to set phasor lengths and other timing
//phasor lengths
double beatDur = 0.0;
double barDur = 0.0;
double phraseDur = 0.0;
double sectionDur = 0.0;

// Timing
double lastResetTime = millis();
double time = 0;
double t = 0;       //time since last reset
double last_t = 0;  //timestamp of the previous time update (since reset)
double beat = 0.0;
double bar = 0.0;
double phrase = 0.0;
double section = 0.0;

std::vector<Value> analogASTs[PWM_OUTS];
std::vector<Value> digitalASTs[DIGI_OUTS];
std::vector<Value> serialASTs[SERIAL_OUTS];
std::vector<Value> q0AST;
std::vector<double> serialInputStreams(SERIAL_INS,0);

struct scheduledItem {
//    Value statement;
    std::vector<Value> ast;
    size_t period;
    size_t lastRun;
    String id;
};

std::vector<scheduledItem> scheduledItems;


void updateBpmVariables() {
  env.set("bpm", Value(bpm));
  env.set("bps", Value(bps));
  env.set("beatDur", Value(beatDur));
  env.set("barDur", Value(barDur));
  env.set("phraseDur", Value(phraseDur));
  env.set("sectionDur", Value(sectionDur));
}

void setBpm(double newBpm, double changeThreshold = 0) {
  if (fabs(newBpm - bpm) >= changeThreshold) {
    bpm = newBpm;
    bps = bpm / 60.0;

    beatDur = 1000000.0 / bps;
    barDur = beatDur * (4.0/meter_denominator) * meter_numerator;
    phraseDur = barDur * barsPerPhrase;
    sectionDur = phraseDur * phrasesPerSection;


    updateBpmVariables();
  }
}

void setTimeSignature(double numerator, double denominator) {
  meter_denominator = denominator;
  meter_numerator = numerator;
  setBpm(bpm);
}

void updateTimeVariables() {
  env.set("time", Value(time));
  env.set("t", Value(t));

  //phasors
  env.set("beat", Value(beat));
  env.set("bar", Value(bar));
  env.set("phrase", Value(phrase));
  env.set("section", Value(section));
}

// Set the module's "transport" to a specified value in microseconds
// and update all derrivative variables
void setTime(size_t newTimeMicros) {
  time = newTimeMicros;
  // last_t = t;
  t = newTimeMicros - lastResetTime;
  beat = fmod(t, beatDur) / beatDur;
  bar = fmod(t, barDur) / barDur;
  phrase = fmod(t, phraseDur) / phraseDur;
  section = fmod(t, sectionDur) / sectionDur;
  updateTimeVariables();
}


// Update time to the current value of `micros()`
// and update each variable that's derrived from it
void updateTime() {
  setTime(micros());
}

void resetTime() {
  lastResetTime = micros();
  updateTime();
}

//just temp clearing these to make testing easier
const Value defaultForm_digital = parse("0")[0];
const Value defaultForm_analog  = parse("0")[0];
// const Value defaultForm_digital = parse("(fast 16 (sqr beat))")[0];
// const Value defaultForm_analog  = parse("(fast 16 beat)")[0];

void updateDigitalOutputs() {
  for (size_t i=0; i < DIGI_OUTS; i++) {
    currentExprSound = true;
    Value result = runParsedCode(digitalASTs[i], env);

    if (result == Value::error() || !currentExprSound) {
      Serial.println("Error in digital output function, clearing");
      digitalASTs[i] = {defaultForm_digital};
      currentExprSound = true;
    }
// Write
    else {
      int pin = digital_out_pin(i + 1);
      int led_pin = digital_out_LED_pin(i + 1);
      int val = result.as_int();
      //TODO: this is repeat of ard_useqdw, should rationalise
#ifdef DIGI_OUT_INVERT
      digitalWrite(pin, 1-val);
#else
      digitalWrite(pin, val);
#endif
      digitalWrite(led_pin, val);
    }
  }
}

void updateAnalogOutputs() {

  // for (size_t i=0; i < 1; i++) {
  for (size_t i=0; i < PWM_OUTS; i++) {
    currentExprSound = true;
    Value result = runParsedCode(analogASTs[i], env);

    if (result == Value::error() || !currentExprSound) {
      Serial.println("Error in analog output function, clearing");
      analogASTs[i] = {defaultForm_analog};
      currentExprSound = true;
    }
    // Write
    else {
      //PWM out
      const double maxpwm = 8191.0;
      int sigval = result.as_float() * maxpwm;
      if (sigval > maxpwm)
        sigval = maxpwm;
      pio_pwm_set_level(i < 4 ? pio0 : pio1, i % 4, sigval);

      //led out
      int led_pin = analog_out_LED_pin(i + 1);  
      int ledsigval = sigval>>2; //shift to 11 bit range for the LED
      ledsigval = (ledsigval * ledsigval) >> 11; //cheap way to square and get a exp curve
      analogWrite(led_pin, ledsigval); 
    }
  }

}

void updateQ0() {
  currentExprSound = true;
  Value result = runParsedCode(q0AST, env);

  if (result == Value::error() || !currentExprSound) {
    Serial.println("Error in q0 output function, clearing");
    q0AST = {};
    currentExprSound = true;
  }
}

void updateSerialOutputs() {
    for (size_t i=0; i < SERIAL_OUTS; i++) {
        if (serialASTs[i].size() > 0) {
            currentExprSound = true;
            Value result = runParsedCode(serialASTs[i], env);

            if (result == Value::error() || !currentExprSound) {
                Serial.println("Error in serial output function, clearing");
                serialASTs[i] = {};
                currentExprSound = true;
            }
                // Write
            else {
                double val = result.as_float();
                Serial.write((u_int8_t )31);
                Serial.write((u_int8_t )(i + 1));
                char *byteArray = reinterpret_cast<char *>(&val);
                for (size_t b = 0; b < 8; b++) {
                    Serial.write(byteArray[b]);
                }
            }
        }
    }
}


#ifdef MIDIOUT
double last_midi_t = 0;
void updateMidiOut() {
  const double midiRes = 48 * meter_numerator * 1;
  const double timeUnitMillis = (barDur / midiRes);

  const double timeDeltaMillis = t - last_midi_t;
  size_t steps = floor(timeDeltaMillis / timeUnitMillis);
  double initValPhase = bar - (timeDeltaMillis / barDur);

  if (steps > 0) {
    const double timeUnitBar = 1.0 / midiRes;

    auto itr = useqMDOMap.begin();
    for (; itr != useqMDOMap.end(); itr++) {
      // Iterate through the keys process MIDI events
      Value midiFunction = itr->second;
      if (initValPhase < 0) initValPhase++;
      std::vector<Value> mdoArgs = { Value(initValPhase) };
      Value prev = midiFunction.apply(mdoArgs, env);
      for (size_t step = 0; step < steps; step++) {
        double t_step = bar - ((steps - (step + 1)) * timeUnitBar);
        //wrap phasor
        if (t_step < 0) t_step += 1.0;
        // Serial.println(t_step);
        mdoArgs[0] = Value(t_step);
        Value val = midiFunction.apply(mdoArgs, env);

        // Serial.println(val.as_float());
        if (val > prev) {
          Serial1.write(0x99);
          Serial1.write(itr->first);
          Serial1.write(val.as_int() * 14);
        } else if (val < prev) {
          Serial1.write(0x89);
          Serial1.write(itr->first);
          Serial1.write((byte)0);
        }
        prev = val;
      }
    }
    last_midi_t = t;
  }
}
#endif // end of MIDI OUT SECTION

MovingAverageFilter cqpMA(3); //code quantising phasor
double lastCQP = 0;
String cqpCode = "(define cqp 'bar)";
std::vector<Value> cqpAST;

std::vector< std::vector<Value>> runQueue;

void initASTs() {
    for(int i = 0; i < DIGI_OUTS; i++)
      //  digitalASTs[i] = {parse("(sqr beat)")[0]};
       digitalASTs[i] = {0};

    for(int i = 0; i < PWM_OUTS; i++)
      //  analogASTs[i] = {parse("bar")[0]};
       analogASTs[i] = {0};

    for(int i = 0; i < SERIAL_OUTS; i++)
        serialASTs[i] = {};

}

void setup() {
  setBpm(defaultBpm);
  updateTime();
  // env.set_global("cqp", ::parse(cqpCode));
  // run(cqpCode, env);
  cqpAST = ::parse("(eval 'bar)");
  initASTs();
}

void runScheduledItems() {
    for(size_t i=0; i < scheduledItems.size(); i++) {
        //run the statement once every period
        size_t run = static_cast<size_t>(bar * scheduledItems[i].period);
//        size_t run_norm = run > scheduledItems[i].lastRun ? run : run + scheduledItems[i].period;
        size_t numRuns = run >= scheduledItems[i].lastRun ? run - scheduledItems[i].lastRun : scheduledItems[i].period - scheduledItems[i].lastRun;
        for(size_t j=0; j < numRuns; j++) {
            //run the statement
//            Serial.println(scheduledItems[i].id);
            runParsedCode(scheduledItems[i].ast, env);
        }
        scheduledItems[i].lastRun = run;
    }
}

int ts_inputs = 0, ts_time = 0, ts_outputs = 0;

void update() {
  

  ts_inputs = millis();
  readInputs();
  env.set("perf_in", Value(int(millis() - ts_inputs)));
  ts_time = millis();
  updateTime();
  env.set("perf_time", Value(int(millis() - ts_time)));
  ts_outputs = millis();

  //check code quant phasor
  double newCqpVal = runParsedCode(cqpAST, env).as_float();
  // double cqpAvgTime = cqpMA.process(newCqpVal - lastCQP);
  if (newCqpVal < lastCQP) {
    updateQ0();
    for(size_t q=0; q < runQueue.size(); q++) {
      Value res;
      int cmdts = micros();
      res = runParsedCode(runQueue[q], env);
      cmdts = micros() - cmdts;
      Serial.println(res.debug());
    }
    runQueue.clear();
  }
  lastCQP = newCqpVal;

  runScheduledItems();

  /* run("(eval q-form)", env);   */
  updateAnalogOutputs();
  updateDigitalOutputs();
  updateSerialOutputs();
#ifdef MIDIOUT
  updateMidiOut();
#endif

  env.set("perf_out", Value(int(millis() - ts_outputs)));
}


}  //end of useq namespace





//extra arduino api functions
namespace builtin {

double fast(double speed, double phasor) {
  phasor *= speed;
  double phase = fmod(phasor, 1.0);
  return phase;
}

Value fromList(std::vector<Value> &lst, double phasor, Environment &env) {
  if (phasor < 0.0) {
    phasor = 0;
  } else if (phasor > 1.0) {
    phasor = 1.0;
  }
  double scaled_phasor = lst.size() * phasor;
  size_t idx = floor(scaled_phasor);
  if (idx == lst.size()) idx--;
  return lst[idx].eval(env);
}

BUILTINFUNC_NOEVAL(useq_q0, 
  env.set_global("q-form", args[0]);
  useq::q0AST = {args[0]};
  ,1)

// ANALOG OUTS
BUILTINFUNC_NOEVAL(a1,
  if (PWM_OUTS>=1) {
    env.set_global("a1-form", args[0]);
    useq::analogASTs[0] = {args[0]};
  }
, 1)
BUILTINFUNC_NOEVAL(a2,
  if (PWM_OUTS>=2) {
    env.set_global("a2-form", args[0]);
    useq::analogASTs[1] = {args[0]};
  }
, 1)
BUILTINFUNC_NOEVAL(a3,
  if (PWM_OUTS>=3) {
    env.set_global("a3-form", args[0]);
    useq::analogASTs[2] = {args[0]};
  }
, 1)
BUILTINFUNC_NOEVAL(a4,
  if (PWM_OUTS>=4) {
    env.set_global("a4-form", args[0]);
    useq::analogASTs[3] = {args[0]};
  }
, 1)
BUILTINFUNC_NOEVAL(a5,
  if (PWM_OUTS>=5) {
    env.set_global("a5-form", args[0]);
    useq::analogASTs[4] = {args[0]};
  }
, 1)
BUILTINFUNC_NOEVAL(a6,
  if (PWM_OUTS>=6) {
    env.set_global("a6-form", args[0]);
    useq::analogASTs[5] = {args[0]};
  }
, 1)

// DIGITAL OUTS
BUILTINFUNC_NOEVAL(d1,
  if (DIGI_OUTS>=1) {
    env.set_global("d1-form", args[0]);
    useq::digitalASTs[0] = {args[0]};
  }
, 1)
BUILTINFUNC_NOEVAL(d2,
  if (DIGI_OUTS>=2) {
    env.set_global("d2-form", args[0]);
    useq::digitalASTs[1] = {args[0]};
  }
  , 1)
BUILTINFUNC_NOEVAL(d3,
  if (DIGI_OUTS>=3) {
    env.set_global("d3-form", args[0]);
    useq::digitalASTs[2] = {args[0]};
  }
, 1)
BUILTINFUNC_NOEVAL(d4,
  if (DIGI_OUTS>=4) {
    env.set_global("d4-form", args[0]);
    useq::digitalASTs[3] = {args[0]};
  }
, 1)
BUILTINFUNC_NOEVAL(d5,
  if (DIGI_OUTS>=5) {
    env.set_global("d5-form", args[0]);
    useq::digitalASTs[4] = {args[0]};
  }
, 1)
BUILTINFUNC_NOEVAL(d6,
  if (DIGI_OUTS>=6) {
    env.set_global("d6-form", args[0]);
    useq::digitalASTs[5] = {args[0]};
  }
, 1)


BUILTINFUNC_NOEVAL(s1,
   env.set_global("s1-form", args[0]);
   useq::serialASTs[0] = {args[0]};
, 1)
BUILTINFUNC_NOEVAL(s2,
   env.set_global("s2-form", args[0]);
   useq::serialASTs[1] = {args[0]};
, 1)
BUILTINFUNC_NOEVAL(s3,
   env.set_global("s3-form", args[0]);
   useq::serialASTs[2] = {args[0]};
, 1)
BUILTINFUNC_NOEVAL(s4,
   env.set_global("s4-form", args[0]);
   useq::serialASTs[3] = {args[0]};
, 1)
BUILTINFUNC_NOEVAL(s5,
   env.set_global("s5-form", args[0]);
   useq::serialASTs[4] = {args[0]};
, 1)
BUILTINFUNC_NOEVAL(s6,
   env.set_global("s6-form", args[0]);
   useq::serialASTs[5] = {args[0]};
, 1)
BUILTINFUNC_NOEVAL(s7,
   env.set_global("s7-form", args[0]);
   useq::serialASTs[6] = {args[0]};
, 1)
BUILTINFUNC_NOEVAL(s8,
   env.set_global("s8-form", args[0]);
   useq::serialASTs[7] = {args[0]};
, 1)

#ifdef MIDIOUT
//midi drum out
BUILTINFUNC(useq_mdo,
            int midiNote = args[0].as_int();
            if (args[1] != 0) {
              useqMDOMap[midiNote] = args[1];
            }else{
              useqMDOMap.erase(midiNote);
            }
            , 2)
#endif

BUILTINFUNC(ard_pinMode,
            int pinNumber = args[0].as_int();
            int onOff = args[1].as_int();
            pinMode(pinNumber, onOff);
            , 2)

BUILTINFUNC(ard_useqdw,
            if (args[1] == Value::error()) {
              Serial.println("useqdw arg err");
              ret = args[1];
            }else{
              int pin = digital_out_pin(args[0].as_int());
              int led_pin = digital_out_LED_pin(args[0].as_int());
              int val = args[1].as_int();
#ifdef DIGI_OUT_INVERT
      digitalWrite(pin, 1-val);
#else
      digitalWrite(pin, val);
#endif
              digitalWrite(led_pin, val);
            }
            , 2)

BUILTINFUNC(ard_useqaw,
            if (args[1] == Value::error()) {
              Serial.println("useqaw arg err");
              ret = args[1];
            }else{
              int pin = analog_out_pin(args[0].as_int());
              int led_pin = analog_out_LED_pin(args[0].as_int());
              int val = args[1].as_float() * 2047.0;
              analogWrite(pin, val);
              analogWrite(led_pin, val);
            }
            , 2)



BUILTINFUNC(ard_digitalWrite,
            int pinNumber = args[0].as_int();
            int onOff = args[1].as_int();
            digitalWrite(pinNumber, onOff);
            ret = args[0];
            , 2)

BUILTINFUNC(ard_digitalRead,
            int pinNumber = args[0].as_int();
            int val = digitalRead(pinNumber);
            ret = Value(val);
            , 1)

BUILTINFUNC(ard_delay,
            int delaytime = args[0].as_int();
            delay(delaytime);
            ret = args[0];
            , 1)

BUILTINFUNC(ard_delaymicros,
            int delaytime = args[0].as_int();
            delayMicroseconds(delaytime);
            ret = args[0];
            , 1)

BUILTINFUNC(ard_millis,
            int m = millis();
            ret = Value(m);
            , 0)

BUILTINFUNC(ard_micros,
            int m = micros();
            ret = Value(m);
            , 0)

BUILTINFUNC(ard_sin,
            float m = sin(args[0].as_float());
            ret = Value(m);
            , 1)
BUILTINFUNC(ard_cos,
            float m = cos(args[0].as_float());
            ret = Value(m);
            , 1)
BUILTINFUNC(ard_tan,
            float m = tan(args[0].as_float());
            ret = Value(m);
            , 1)

BUILTINFUNC(ard_abs,
            float m = abs(args[0].as_float());
            ret = Value(m);
            , 1)
BUILTINFUNC(ard_min,
            float m = min(args[0].as_float(), args[1].as_float());
            ret = Value(m);
            , 2)
BUILTINFUNC(ard_max,
            float m = max(args[0].as_float(), args[1].as_float());
            ret = Value(m);
            , 2)
BUILTINFUNC(ard_pow,
            float m = pow(args[0].as_float(), args[1].as_float());
            ret = Value(m);
            , 2)
BUILTINFUNC(ard_sqrt,
            float m = sqrt(args[0].as_float());
            ret = Value(m);
            , 1)
BUILTINFUNC(ard_map,
            float m = map(args[0].as_float(), args[1].as_float(), args[2].as_float(), args[3].as_float(), args[4].as_float());
            ret = Value(m);
            , 5)
BUILTINFUNC(ard_floor,
            double m = floor(args[0].as_float());
            ret = Value(m);
            , 1)
BUILTINFUNC(ard_ceil,
            double m = ceil(args[0].as_float());
            ret = Value(m);
            , 1)

BUILTINFUNC(useq_pulse,
            //args: pulse width, phasor
            double pulseWidth = args[0].as_float();
            double phasor = args[1].as_float();
            ret = Value(pulseWidth < phasor ? 1.0 : 0.0);
            , 2)
BUILTINFUNC(useq_sqr,
            ret = Value(args[0].as_float() < 0.5 ? 1.0 : 0.0);
            , 1)

  // TODO D-R-Y
  //this version doesn't work properly with phasors - freezes at >2x speedup
// BUILTINFUNC_NOEVAL(useq_fast,
//             double factor = args[0].eval(env).as_float();
//             Value expr = args[1];
//             // store the current time to reset later
//             double currentTime = env.get("time").as_float();
//             // update the interpreter's time just for this expr
//             double newTime = currentTime * factor;
//             useq::setTime((size_t)newTime);
//             double evaled_expr = expr.eval(env).as_float();
//             ret = Value(evaled_expr);
//             // restore the interpreter's time
//             useq::setTime((size_t)currentTime);
//             , 2)
BUILTINFUNC(useq_fast,
            double speed = args[0].as_float();
            double phasor = args[1].as_float();
            double fastPhasor = fast(speed, phasor);
            ret = Value(fastPhasor);
            , 2)

BUILTINFUNC_NOEVAL(useq_slow,
            double factor = args[0].eval(env).as_float();
            Value expr = args[1];
            // store the current time to reset later
            double currentTime = env.get("time").as_float();
            // update the interpreter's time just for this expr
            double newTime = currentTime / factor;
            useq::setTime((size_t)newTime);
            double evaled_expr = expr.eval(env).as_float();
            ret = Value(evaled_expr);
            // restore the interpreter's time
            useq::setTime((size_t)currentTime);
            , 2)
BUILTINFUNC_VARGS(useq_fromList,
            auto lst = args[0].as_list();
            const double phasor = args[1].as_float();
            ret = fromList(lst, phasor, env);
            if (args.size() == 3) {
              double scale = args[2].as_float();
              if (scale != 0) {
                ret = Value(ret / scale);
              }
            }
            , 2,3)
BUILTINFUNC(useq_fromFlattenedList,
            auto lst = flatten(args[0], env).as_list();
            double phasor = args[1].as_float();
            ret = fromList(lst, phasor, env);
            , 2)
BUILTINFUNC(useq_flatten,
            ret = flatten(args[0], env);
            , 1)
BUILTINFUNC(useq_interpolate,
            auto lst = args[0].as_list();
            double phasor = args[1].as_float();
            if (phasor < 0.0) {
              phasor = 0;
            } else if (phasor > 1) {
              phasor = 1;
            }
            float a;
            double index = phasor * (lst.size()-1);
            size_t pos0 = static_cast<size_t>(index);
            if (pos0==(lst.size()-1)) pos0--;
            a = (index - pos0) ;
            double v2 = lst[pos0+1].eval(env).as_float();
            double v1 = lst[pos0].eval(env).as_float();
            ret = Value(((v2 - v1) * a) + v1);
            , 2)

// (step <phasor> <count> (<offset>))
BUILTINFUNC_VARGS(useq_step,
      const double phasor = args[0].as_float();
      const int count = args[1].as_int();
      const double offset = (args.size() == 3) ? args[2].as_float() : 0;
      double val = static_cast<int>(phasor * abs(count));
      if (val == count) val--;
      ret = Value((count > 0 ? val : count - 1 -val) + offset);
, 2, 3)

// (euclid <phasor> <n> <k> (<offset>) (<pulsewidth>)
BUILTINFUNC_VARGS(useq_euclidean,
  const double phasor = args[0].as_float();
  const int n = args[1].as_int();
  const int k = args[2].as_int();
  const int offset = (args.size() >= 4) ? args[3].as_int() : 0;
  const float pulseWidth = (args.size() == 5) ? args[4].as_float() : 0.5;
  const float fi = phasor * n;
  int i = static_cast<int>(fi);
  const float rem = fi - i;
  if (i == n) { i--; }
  const int idx =((i+n-offset) * k) % n;
  ret = Value(idx < k && rem < pulseWidth ? 1 : 0);
, 3, 5)


// (schedule <name> <statement> <period>)
BUILTINFUNC_NOEVAL(useq_schedule,
    const auto itemName = args[0].as_string();
    const auto ast = args[1];
    const auto period = args[2].as_float();
    useq::scheduledItem v;
    v.id = itemName;
    v.period = period;
    v.lastRun = 0;
//    v.statement = statement;
    v.ast = {ast};
    useq::scheduledItems.push_back(v);
    ret = Value(0);
, 3)

BUILTINFUNC(useq_unschedule,
    const String id = args[0].as_string();
    auto is_item = [id](useq::scheduledItem &v) { return v.id==id;};

    if (auto it = std::find_if(begin(useq::scheduledItems), end(useq::scheduledItems), is_item); it != std::end(useq::scheduledItems)) {
        useq::scheduledItems.erase(it);
        Serial.println("Item removed");
    }
    else {
        Serial.println("Item not found");
    }
, 1 )

//(timeit <function>) - returns time take to run in microseconds
BUILTINFUNC_NOEVAL(timeit,
  unsigned long ts = micros();
  args[0].eval(env);
  ts = micros() - ts;
  ret = Value(static_cast<int>(ts));
, 1)


//(drum-predict <input-pattern>) -> list
BUILTINFUNC(useq_drumpredict,
    const std::vector<Value> inputs = args[0].as_list();
    std::vector<char> invec(32,1);
    for(size_t i=0; i < 32; i++) {
        invec[i] = inputs[i].as_int();
    }
    std::vector<int> outvec(14,0);
    apply_logic_gate_net_singleval(invec.data(), outvec.data());
    std::vector<Value> result(14);
    for(size_t i=0; i < 14; i++) {
      result.at(i) = Value(outvec.at(i));
    }
    ret = Value(result);
, 1 )


BUILTINFUNC(useq_dm,
            auto index = args[0].as_int();
            auto v1 = args[1].as_float();
            auto v2 = args[2].as_float();
            ret = Value(index > 0 ? v2 : v1);
            , 3)

BUILTINFUNC_VARGS(useq_gates,
                  auto lst = args[0].as_list();
                  const double phasor = args[1].as_float();
                  const double speed = args[2].as_float();
                  const double pulseWidth = args.size() == 4 ? args[3].as_float() : 0.5;
                  const double val = fromList(lst, fast(speed, phasor), env).as_int();
                  const double gates = fast(speed * lst.size(), phasor) < pulseWidth ? 1.0 : 0.0;
                  ret = Value(val * gates);
                  , 3, 4)

BUILTINFUNC_VARGS(useq_gatesw,
                  auto lst = args[0].as_list();
                  const double phasor = args[1].as_float();
                  const double speed = args.size() == 3 ? args[2].as_float() : 1.0;
                  const double val = fromList(lst, fast(speed, phasor), env).as_int();
                  const double pulseWidth = val / 9.0;
                  const double gate = fast(speed * lst.size(), phasor) < pulseWidth ? 1.0 : 0.0;
                  ret = Value((val > 0 ? 1.0 : 0.0) * gate);
                  , 2, 3)

BUILTINFUNC_VARGS(useq_trigs,
                  auto lst = args[0].as_list();
                  const double phasor = args[1].as_float();
                  const double speed = args.size() == 3 ? args[2].as_float() : 1.0;
                  const double val = fromList(lst, fast(speed, phasor), env).as_int();
                  const double amp = val / 9.0;
                  const double pulseWidth = args.size() == 4 ? args[3].as_float() : 0.1;
                  const double gate = fast(speed * lst.size(), phasor) < pulseWidth ? 1.0 : 0.0;
                  ret = Value((val > 0 ? 1.0 : 0.0) * gate * amp);
                  , 2, 4)

BUILTINFUNC(useq_loopPhasor,
            auto phasor = args[0].as_float();
            auto loopPoint = args[1].as_float();
            if (loopPoint == 0) loopPoint = 1; //avoid infinity
            double spedupPhasor = fast(1.0/loopPoint, phasor);
            ret = spedupPhasor * loopPoint;
            , 2)

BUILTINFUNC_VARGS(useq_setbpm,
          double newBpm = args[0].as_float();
          double thresh = args.size() == 2 ? args[1].as_float() : 0;
          useq::setBpm(newBpm, thresh);
          ret = args[0];
          , 1, 2)

BUILTINFUNC(useq_getbpm,
          int index = args[0].as_int();
          if (index==1) {
            ret = tempoI1.avgBPM;
          }
          else if (index==1) {
            ret = tempoI2.avgBPM;
          }else{
            ret = 0;            
          }
         , 1)

BUILTINFUNC(useq_settimesig,
          useq::setTimeSignature(args[0].as_float(), args[1].as_float());
          ret = Value(1);
          , 2)

BUILTINFUNC(useq_in1,
          ret = Value(useqInputValues[USEQI1]);
          , 0)
BUILTINFUNC(useq_in2,
          ret = Value(useqInputValues[USEQI2]);
          , 0)

#ifdef MUSICTHING
BUILTINFUNC(useq_mt_knob,
          ret = Value(useqInputValues[MTMAINKNOB]);
          , 0)
BUILTINFUNC(useq_mt_knobx,
          ret = Value(useqInputValues[MTXKNOB]);
          , 0)
BUILTINFUNC(useq_mt_knoby,
          ret = Value(useqInputValues[MTYKNOB]);
          , 0)
BUILTINFUNC(useq_mt_swz,
          ret = Value(useqInputValues[MTZSWITCH]);
          , 0)
#endif          


BUILTINFUNC(useq_ssin,
    int index = args[0].as_int();
    if(index >0 && index <= SERIAL_INS) {
        ret = Value(useq::serialInputStreams[index-1]);
        }
, 1)

BUILTINFUNC(useq_swm,
            int index = args[0].as_int();
                    if (index == 1) {
                        ret = Value(useqInputValues[USEQM1]);
                    }
                    else {
                        ret = Value(useqInputValues[USEQM2]);
                    }
, 1)

BUILTINFUNC(useq_swt,
          int index = args[0].as_int();
          if (index == 1) {
            ret = Value(useqInputValues[USEQT1]);
          }
          else {
            ret = Value(useqInputValues[USEQT2]);
          }
          , 1)

BUILTINFUNC(useq_swr,
          ret = Value(useqInputValues[USEQRS1]);
          , 0)

BUILTINFUNC(useq_rot,
          ret = Value(useqInputValues[USEQR1]);
          , 0)

BUILTINFUNC(perf,

            String report = "fps0: ";
            report += env.get("fps").as_float();
            // report += ", fps1: ";
            // report += env.get("perf_fps1").as_int();
            report += ", qt: ";
            report += env.get("qt").as_float();
            report += ", in: ";
            report += env.get("perf_in").as_int();
            report += ", upd_tm: ";
            report += env.get("perf_time").as_int();
            report += ", out: ";
            report += env.get("perf_out").as_int();
            report += ", get: ";
            report += env.get("perf_get").as_float();
            report += ", parse: ";
            report += env.get("perf_parse").as_float();
            report += ", run: ";
            report += env.get("perf_run").as_float();
            report += ", ts1: ";
            report += env.get("perf_ts1").as_float();
            report += ", heap free: ";
            report += rp2040.getFreeHeap() / 1024;
            Serial.println(report);
            ret = Value();
            , 0)

    BUILTINFUNC(zeros,
                int length = args[0].as_int();
                        std::vector<Value> zeroList(length, Value(0));
                        ret = Value(zeroList);
    , 1)
}




// Does this environment, or its parent environment, have a variable?
bool Environment::has(String const &name) const {
  // Find the value in the map
  auto itr = defs.find(name);
  if (itr != defs.end())
    // If it was found
    return true;
  else if (parent_scope != NULL)
    // If it was not found in the current environment,
    // try to find it in the parent environment
    return parent_scope->has(name);
  else return false;
}

// std::map<String, Value> Environment::builtindefs;
// etl::unordered_map<String, Value, 256> Environment::builtindefs;

#ifdef NO_ETL
std::map<String, Value> Environment::builtindefs;
#else
etl::unordered_map<String, Value, 256> Environment::builtindefs;
#endif


void loadBuiltinDefs() {
  Environment::builtindefs["useqdw"] = Value("useqdw", builtin::ard_useqdw);
  Environment::builtindefs["useqaw"] = Value("useqaw", builtin::ard_useqaw);
  Environment::builtindefs["a1"] = Value("a1", builtin::a1);
  Environment::builtindefs["a2"] = Value("a2", builtin::a2);
  Environment::builtindefs["a3"] = Value("a3", builtin::a3);
  Environment::builtindefs["a4"] = Value("a4", builtin::a4);
  Environment::builtindefs["a5"] = Value("a5", builtin::a5);
  Environment::builtindefs["a6"] = Value("a6", builtin::a6);
  Environment::builtindefs["d1"] = Value("d1", builtin::d1);
  Environment::builtindefs["d2"] = Value("d2", builtin::d2);
  Environment::builtindefs["d3"] = Value("d3", builtin::d3);
  Environment::builtindefs["d4"] = Value("d4", builtin::d4);
  Environment::builtindefs["d5"] = Value("d5", builtin::d5);
  Environment::builtindefs["d6"] = Value("d6", builtin::d6);
  Environment::builtindefs["q0"] = Value("q0", builtin::useq_q0);

  Environment::builtindefs["s1"] = Value("s1", builtin::s1);
  Environment::builtindefs["s2"] = Value("s2", builtin::s2);
  Environment::builtindefs["s3"] = Value("s3", builtin::s3);
  Environment::builtindefs["s4"] = Value("s4", builtin::s4);
  Environment::builtindefs["s5"] = Value("s5", builtin::s5);
  Environment::builtindefs["s6"] = Value("s6", builtin::s6);
  Environment::builtindefs["s7"] = Value("s7", builtin::s7);
  Environment::builtindefs["s8"] = Value("s8", builtin::s8);

  Environment::builtindefs["pm"] = Value("pm", builtin::ard_pinMode);
  Environment::builtindefs["dw"] = Value("dw", builtin::ard_digitalWrite);
  Environment::builtindefs["dr"] = Value("dr", builtin::ard_digitalRead);
  Environment::builtindefs["delay"] = Value("delay", builtin::ard_delay);
  Environment::builtindefs["delaym"] = Value("delaym", builtin::ard_delaymicros);
  Environment::builtindefs["millis"] = Value("millis", builtin::ard_millis);
  Environment::builtindefs["micros"] = Value("micros", builtin::ard_micros);
  Environment::builtindefs["perf"] = Value("perf", builtin::perf);
  Environment::builtindefs["in1"] = Value("in1", builtin::useq_in1);
  Environment::builtindefs["in2"] = Value("in2", builtin::useq_in2);
  Environment::builtindefs["swm"] = Value("swm", builtin::useq_swm);
  Environment::builtindefs["swt"] = Value("swt", builtin::useq_swt);
  Environment::builtindefs["swr"] = Value("swr", builtin::useq_swr);
  Environment::builtindefs["rot"] = Value("rot", builtin::useq_rot);
  Environment::builtindefs["ssin"] = Value("ssin", builtin::useq_ssin);

#ifdef MUSICTHING
  Environment::builtindefs["knob"] = Value("knob", builtin::useq_mt_knob);
  Environment::builtindefs["knobx"] = Value("knobx", builtin::useq_mt_knobx);
  Environment::builtindefs["knoby"] = Value("knoby", builtin::useq_mt_knoby);
  Environment::builtindefs["swz"] = Value("swz", builtin::useq_mt_swz);
#endif

  //sequencing
  Environment::builtindefs["pulse"] = Value("pulse", builtin::useq_pulse);
  Environment::builtindefs["sqr"] = Value("sqr", builtin::useq_sqr);
  Environment::builtindefs["fast"] = Value("fast", builtin::useq_fast);
  Environment::builtindefs["slow"] = Value("slow", builtin::useq_slow);
  Environment::builtindefs["fromList"] = Value("fromList", builtin::useq_fromList);
  Environment::builtindefs["seq"] = Value("fromList", builtin::useq_fromList);
  Environment::builtindefs["flatIdx"] = Value("flatIdx", builtin::useq_fromFlattenedList);
  Environment::builtindefs["flat"] = Value("flat", builtin::useq_flatten);
  Environment::builtindefs["looph"] = Value("looph", builtin::useq_loopPhasor);
  Environment::builtindefs["dm"] = Value("dm", builtin::useq_dm);
  Environment::builtindefs["gates"] = Value("gates", builtin::useq_gates);
  Environment::builtindefs["gatesw"] = Value("gatesw", builtin::useq_gatesw);
  Environment::builtindefs["trigs"] = Value("trigs", builtin::useq_trigs);
  Environment::builtindefs["setbpm"] = Value("setbpm", builtin::useq_setbpm);
  Environment::builtindefs["getbpm"] = Value("getbpm", builtin::useq_getbpm);
  Environment::builtindefs["settimesig"] = Value("settimesig", builtin::useq_settimesig);
  Environment::builtindefs["interp"] = Value("interp", builtin::useq_interpolate);
  Environment::builtindefs["step"] = Value("step", builtin::useq_step);
  Environment::builtindefs["euclid"] = Value("euclid", builtin::useq_euclidean);
  Environment::builtindefs["schedule"] = Value("schedule", builtin::useq_schedule);
  Environment::builtindefs["unschedule"] = Value("unschedule", builtin::useq_unschedule);


  Environment::builtindefs["drum-predict"] = Value("drum-predict", builtin::useq_drumpredict);


#ifdef MIDIOUT
  Environment::builtindefs["mdo"] = Value("mdo", builtin::useq_mdo);
#endif
  //arduino math
  Environment::builtindefs["sin"] = Value("sin", builtin::ard_sin);
  Environment::builtindefs["cos"] = Value("cos", builtin::ard_cos);
  Environment::builtindefs["tan"] = Value("tan", builtin::ard_tan);
  Environment::builtindefs["abs"] = Value("abs", builtin::ard_abs);

  Environment::builtindefs["min"] = Value("min", builtin::ard_min);
  Environment::builtindefs["max"] = Value("max", builtin::ard_max);
  Environment::builtindefs["pow"] = Value("pow", builtin::ard_pow);
  Environment::builtindefs["sqrt"] = Value("sqrt", builtin::ard_sqrt);
  Environment::builtindefs["scale"] = Value("scale", builtin::ard_map);

  // Meta operations
  Environment::builtindefs["eval"] = Value("eval", builtin::eval);
  Environment::builtindefs["type"] = Value("type", builtin::get_type_name);
  Environment::builtindefs["parse"] = Value("parse", builtin::parse);

  // Special forms
  Environment::builtindefs["do"] = Value("do", builtin::do_block);
  Environment::builtindefs["if"] = Value("if", builtin::if_then_else);
  Environment::builtindefs["for"] = Value("for", builtin::for_loop);
  Environment::builtindefs["while"] = Value("while", builtin::while_loop);
  Environment::builtindefs["scope"] = Value("scope", builtin::scope);
  Environment::builtindefs["quote"] = Value("quote", builtin::quote);
  Environment::builtindefs["defun"] = Value("defun", builtin::defun);
  Environment::builtindefs["define"] = Value("define", builtin::define);
  Environment::builtindefs["set"] = Value("set", builtin::set);
  Environment::builtindefs["lambda"] = Value("lambda", builtin::lambda);

  // Comparison operations
  Environment::builtindefs["="] = Value("=", builtin::eq);
  Environment::builtindefs["!="] = Value("!=", builtin::neq);
  Environment::builtindefs[">"] = Value(">", builtin::greater);
  Environment::builtindefs["<"] = Value("<", builtin::less);
  Environment::builtindefs[">="] = Value(">=", builtin::greater_eq);
  Environment::builtindefs["<="] = Value("<=", builtin::less_eq);

  // Arithmetic operations
  Environment::builtindefs["+"] = Value("+", builtin::sum);
  Environment::builtindefs["-"] = Value("-", builtin::subtract);
  Environment::builtindefs["*"] = Value("*", builtin::product);
  Environment::builtindefs["/"] = Value("/", builtin::divide);
  Environment::builtindefs["%"] = Value("%", builtin::remainder);
  Environment::builtindefs["floor"] = Value("floor", builtin::ard_floor);
  Environment::builtindefs["ceil"] = Value("ceil", builtin::ard_ceil);

  // List operations
  Environment::builtindefs["list"] = Value("list", builtin::list);
  Environment::builtindefs["insert"] = Value("insert", builtin::insert);
  Environment::builtindefs["index"] = Value("index", builtin::index);
  Environment::builtindefs["remove"] = Value("remove", builtin::remove);

  Environment::builtindefs["len"] = Value("len", builtin::len);

  Environment::builtindefs["push"] = Value("push", builtin::push);
  Environment::builtindefs["pop"] = Value("pop", builtin::pop);
  Environment::builtindefs["head"] = Value("head", builtin::head);
  Environment::builtindefs["tail"] = Value("tail", builtin::tail);
  Environment::builtindefs["first"] = Value("first", builtin::head);
  Environment::builtindefs["last"] = Value("last", builtin::pop);
  Environment::builtindefs["range"] = Value("range", builtin::range);

  // List generators

  Environment::builtindefs["zeros"] = Value("zeros", builtin::zeros);


  // Functional operations
  Environment::builtindefs["map"] = Value("map", builtin::map_list);
  Environment::builtindefs["filter"] = Value("filter", builtin::filter_list);
  Environment::builtindefs["reduce"] = Value("reduce", builtin::reduce_list);

  //utility
  Environment::builtindefs["timeit"] = Value("timeit", builtin::timeit);


// IO operations
#ifdef USE_STD
  // if (name == "exit") return Value("exit", builtin::exit);
  // if (name == "quit") return Value("quit", builtin::exit);
  Environment::builtindefs["print"] = Value("print", builtin::print);
  // if (name == "input") return Value("input", builtin::input);
  Environment::builtindefs["random"] = Value("random", builtin::gen_random);
#endif

  // String operations
  Environment::builtindefs["debug"] = Value("debug", builtin::debug);
  Environment::builtindefs["replace"] = Value("replace", builtin::replace);
  Environment::builtindefs["display"] = Value("display", builtin::display);

  // Casting operations
  Environment::builtindefs["int"] = Value("int", builtin::cast_to_int);
  Environment::builtindefs["float"] = Value("float", builtin::cast_to_float);

  // Constants
  Environment::builtindefs["endl"] = Value::string("\n");
}
// Get the value associated with this name in this scope
Value Environment::get(const String &name) const {
  // Serial.print("Env get ");
  // Serial.println(name);


  auto b_itr = Environment::builtindefs.find(name);
  if (b_itr != Environment::builtindefs.end()) return b_itr->second;

  auto itr = defs.find(name);
  if (itr != defs.end()) return itr->second;
  else if (parent_scope != NULL) {
    itr = parent_scope->defs.find(name);
    if (itr != parent_scope->defs.end()) return itr->second;
    else return parent_scope->get(name);
  }
  Serial.print(ATOM_NOT_DEFINED);
  Serial.print(": ");
  Serial.println(name);
  return Value::error();
}



void flash_builtin_led(int num, int amt) {
  for (int i = 0; i < num; i++) {
    digitalWrite(LED_BUILTIN, 1);
    delay(amt);
    digitalWrite(LED_BUILTIN, 0);
    delay(amt);
  }
}

void setup_outs() {
  for (int i=0; i < PWM_OUTS + DIGI_OUTS; i++) {
    pinMode(useq_output_pins[i], OUTPUT);
  }

#ifdef MUSICTHING
  pinMode(MUX_LOGIC_A, OUTPUT);
  pinMode(MUX_LOGIC_B, OUTPUT);
#endif

}



void setup_analog_outs() {
  //PWM outputs
  analogWriteFreq(30000);     //out of hearing range
  analogWriteResolution(11);  // about the best we can get for 30kHz

  //set PIO PWM state machines to run PWM outputs
  uint offset = pio_add_program(pio0, &pwm_program);
  uint offset2 = pio_add_program(pio1, &pwm_program);
  // printf("Loaded program at %d\n", offset);

  for(int i=0; i < PWM_OUTS; i++) {
    auto pioInstance = i < 4 ? pio0 : pio1;
    uint pioOffset = i < 4 ? offset : offset2;
    auto smIdx = i % 4;
    pwm_program_init(pioInstance, smIdx, pioOffset, useq_output_pins[i]);
    pio_pwm_set_period(pioInstance, smIdx, (1u << 13) - 1);  

  }

}

void setup_digital_ins() {
  pinMode(USEQ_PIN_I1, INPUT_PULLUP);
  pinMode(USEQ_PIN_I2, INPUT_PULLUP);
}

void setup_leds() {

#ifndef MUSICTHING
  pinMode(LED_BOARD, OUTPUT);  //test LED
  digitalWrite(LED_BOARD, 1);
  pinMode(USEQ_PIN_LED_I1, OUTPUT);
  pinMode(USEQ_PIN_LED_I2, OUTPUT);
#endif


#ifdef USEQHARDWARE_1_0
  pinMode(USEQ_PIN_LED_AI1, OUTPUT);
  pinMode(USEQ_PIN_LED_AI2, OUTPUT);
#endif

  for (int i=0; i < 6; i++) {
    pinMode(useq_output_led_pins[i], OUTPUT);
  }
}

void setup_switches() {
#ifdef USEQHARDWARE_1_0
  pinMode(USEQ_PIN_SWITCH_M1, INPUT_PULLUP);

  pinMode(USEQ_PIN_SWITCH_T1, INPUT_PULLUP);
#endif
#ifdef USEQHARDWARE_0_2
  pinMode(USEQ_PIN_SWITCH_M1, INPUT_PULLUP);
  pinMode(USEQ_PIN_SWITCH_M2, INPUT_PULLUP);

  pinMode(USEQ_PIN_SWITCH_T1, INPUT_PULLUP);
  pinMode(USEQ_PIN_SWITCH_T2, INPUT_PULLUP);
#endif

}

#ifdef USEQHARDWARE_0_2
void setup_rotary_encoder() {
  pinMode(USEQ_PIN_SWITCH_R1, INPUT_PULLUP);
  pinMode(USEQ_PIN_ROTARYENC_A, INPUT_PULLUP);
  pinMode(USEQ_PIN_ROTARYENC_B, INPUT_PULLUP);
  useqInputValues[USEQR1] = 0;
}
#endif

#ifdef ANALOG_INPUTS
void setup_analog_ins() {
  analogReadResolution(12);  
#ifdef USEQHARDWARE_1_0
  pinMode(USEQ_PIN_AI1, INPUT);
  pinMode(USEQ_PIN_AI2, INPUT);
#endif
#ifdef MUSICTHING
  pinMode(MUX_IN_1, INPUT);
  pinMode(MUX_IN_2, INPUT);
  pinMode(AUDIO_IN_1, INPUT);
  pinMode(AUDIO_IN_2, INPUT);
#endif
}
#endif

void led_animation() {
  int ledDelay = 30;
#ifdef MUSICTHING
  ledDelay = 40;
  for (int i = 0; i < 8; i++) {
    digitalWrite(useq_output_led_pins[0], 1);
    delay(ledDelay);
    digitalWrite(useq_output_led_pins[2], 1);
    delay(ledDelay);
    digitalWrite(useq_output_led_pins[4], 1);
    digitalWrite(useq_output_led_pins[0], 0);
    delay(ledDelay);
    digitalWrite(useq_output_led_pins[5], 1);
    digitalWrite(useq_output_led_pins[2], 0);
    delay(ledDelay);
    digitalWrite(useq_output_led_pins[3], 1);
    digitalWrite(useq_output_led_pins[4], 0);
    delay(ledDelay);
    digitalWrite(useq_output_led_pins[1], 1);
    digitalWrite(useq_output_led_pins[5], 0);
    delay(ledDelay);
    digitalWrite(useq_output_led_pins[3], 0);
    delay(ledDelay);
    digitalWrite(useq_output_led_pins[1], 0);
    ledDelay -= 3;
  }
#endif
#ifdef USEQHARDWARE_0_2
  for (int i = 0; i < 8; i++) {
    digitalWrite(USEQ_PIN_LED_I1, 1);
    delay(ledDelay);
    digitalWrite(USEQ_PIN_LED_A1, 1);
    delay(ledDelay);
    digitalWrite(USEQ_PIN_LED_D1, 1);
    digitalWrite(USEQ_PIN_LED_I1, 0);
    delay(ledDelay);
    digitalWrite(USEQ_PIN_LED_D3, 1);
    digitalWrite(USEQ_PIN_LED_A1, 0);
    delay(ledDelay);
    digitalWrite(USEQ_PIN_LED_D4, 1);
    digitalWrite(USEQ_PIN_LED_D1, 0);
    delay(ledDelay);
    digitalWrite(USEQ_PIN_LED_D2, 1);
    digitalWrite(USEQ_PIN_LED_D3, 0);
    delay(ledDelay);
    digitalWrite(USEQ_PIN_LED_A2, 1);
    digitalWrite(USEQ_PIN_LED_D4, 0);
    delay(ledDelay);
    digitalWrite(USEQ_PIN_LED_I2, 1);
    digitalWrite(USEQ_PIN_LED_D2, 0);
    delay(ledDelay);
    digitalWrite(USEQ_PIN_LED_A2, 0);
    delay(ledDelay);
    digitalWrite(USEQ_PIN_LED_I2, 0);
    delay(ledDelay);
    ledDelay -= 3;
  }
#endif
#ifdef USEQHARDWARE_1_0
  for (int i = 0; i < 8; i++) {
    digitalWrite(USEQ_PIN_LED_AI1, 1);
    delay(ledDelay);
    digitalWrite(USEQ_PIN_LED_AI2, 1);
    delay(ledDelay);
    digitalWrite(USEQ_PIN_LED_A1, 1);
    digitalWrite(USEQ_PIN_LED_AI1, 0);
    delay(ledDelay);
    digitalWrite(USEQ_PIN_LED_A2, 1);
    digitalWrite(USEQ_PIN_LED_AI2, 0);
    delay(ledDelay);
    digitalWrite(USEQ_PIN_LED_A3, 1);
    digitalWrite(USEQ_PIN_LED_A1, 0);
    delay(ledDelay);
    digitalWrite(USEQ_PIN_LED_D3, 1);
    digitalWrite(USEQ_PIN_LED_A2, 0);
    delay(ledDelay);
    digitalWrite(USEQ_PIN_LED_D2, 1);
    digitalWrite(USEQ_PIN_LED_A3, 0);
    delay(ledDelay);
    digitalWrite(USEQ_PIN_LED_D1, 1);
    digitalWrite(USEQ_PIN_LED_D3, 0);
    delay(ledDelay);
    digitalWrite(USEQ_PIN_LED_I2, 1);
    digitalWrite(USEQ_PIN_LED_D2, 0);
    delay(ledDelay);
    digitalWrite(USEQ_PIN_LED_I1, 1);
    digitalWrite(USEQ_PIN_LED_D1, 0);
    delay(ledDelay);
    digitalWrite(USEQ_PIN_LED_I2, 0);
    delay(ledDelay);
    digitalWrite(USEQ_PIN_LED_I1, 0);
    delay(ledDelay);
    ledDelay -= 3;
  }
#endif
}


void setup_IO() {
  setup_outs();
  setup_analog_outs();
  setup_digital_ins();
  setup_switches();
#ifdef USEQHARDWARE_0_2  
  setup_rotary_encoder();
#endif
#ifdef ANALOG_INPUTS
  setup_analog_ins();  
#endif

#ifdef MIDIOUT
  Serial1.setRX(1);
  Serial1.setTX(0);
  Serial1.begin(31250);
#endif
}

void module_setup() {
  setup_IO();
}

static uint8_t prevNextCode = 0;
static uint16_t store = 0;

#ifdef USEQHARDWARE_0_2
int8_t read_rotary() {
  static int8_t rot_enc_table[] = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };

  prevNextCode <<= 2;
  if (digitalRead(USEQ_PIN_ROTARYENC_B)) prevNextCode |= 0x02;
  if (digitalRead(USEQ_PIN_ROTARYENC_A)) prevNextCode |= 0x01;
  prevNextCode &= 0x0f;

  // If valid then store as 16 bit data.
  if (rot_enc_table[prevNextCode]) {
    store <<= 4;
    store |= prevNextCode;
    //if (store==0xd42b) return 1;
    //if (store==0xe817) return -1;
    if ((store & 0xff) == 0x2b) return -1;
    if ((store & 0xff) == 0x17) return 1;
  }
  return 0;
}

void readRotaryEnc() {
  static int32_t c, val;

  if (val = read_rotary()) {
    useqInputValues[USEQR1] += val;
    // Serial.print(c);Serial.print(" ");
  }
}
#endif //useq 0.2 rotary

void readInputs() {
  //inputs are input_pullup, so invert
  auto now=micros();
  const double recp4096 = 0.000244141; //1/4096
#ifdef MUSICTHING
  const size_t muxdelay=2;
  //unroll loop for efficiency
  digitalWrite(MUX_LOGIC_A,0);
  digitalWrite(MUX_LOGIC_B,0);
  delayMicroseconds(muxdelay);
  useqInputValues[MTMAINKNOB] = analogRead(MUX_IN_1) * recp4096;
  useqInputValues[USEQAI1] = analogRead(MUX_IN_2) * recp4096;
  digitalWrite(MUX_LOGIC_A,0);
  digitalWrite(MUX_LOGIC_B,1);
  delayMicroseconds(muxdelay);
  useqInputValues[MTYKNOB] = analogRead(MUX_IN_1) * recp4096;
  digitalWrite(MUX_LOGIC_A,1);
  digitalWrite(MUX_LOGIC_B,0);
  delayMicroseconds(muxdelay);
  useqInputValues[MTXKNOB] = analogRead(MUX_IN_1) * recp4096;
  useqInputValues[USEQAI2] = analogRead(MUX_IN_2) * recp4096;
  digitalWrite(MUX_LOGIC_A,1);
  digitalWrite(MUX_LOGIC_B,1);
  delayMicroseconds(muxdelay);
  int switchVal = analogRead(MUX_IN_1);
  if (switchVal < 100) {
    switchVal = 0;
  }else if (switchVal > 3500) {
    switchVal = 2;
  }else {
    switchVal = 1;
  }
  useqInputValues[MTZSWITCH] = switchVal;

  // Serial.print(useqInputValues[MTMAINKNOB]);
  // Serial.print("\t");
  // Serial.print(useqInputValues[MTXKNOB]);
  // Serial.print("\t");
  // Serial.print(useqInputValues[MTYKNOB]);
  // Serial.print("\t");
  // Serial.println(useqInputValues[MTZSWITCH]);

  const int input1 = 1 - digitalRead(USEQ_PIN_I1);
  const int input2 = 1 - digitalRead(USEQ_PIN_I2);
  digitalWrite(useq_output_led_pins[4], input1);
  digitalWrite(useq_output_led_pins[5], input2);
  useqInputValues[USEQI1] = input1;
  useqInputValues[USEQI2] = input2;

#else  
  const auto input1 = 1 - digitalRead(USEQ_PIN_I1);
  const auto input2 = 1 - digitalRead(USEQ_PIN_I2);
  useqInputValues[USEQI1] = input1;
  useqInputValues[USEQI2] = input2;

  digitalWrite(USEQ_PIN_LED_I1, input1);
  digitalWrite(USEQ_PIN_LED_I2, input2);

  //tempo estimates
  tempoI1.averageBPM(input1, now);
  tempoI2.averageBPM(input2, now);
  

  useqInputValues[USEQM1] = 1 - digitalRead(USEQ_PIN_SWITCH_M1);
  useqInputValues[USEQT1] = 1 - digitalRead(USEQ_PIN_SWITCH_T1);
#endif

#ifdef USEQHARDWARE_0_2
  useqInputValues[USEQRS1] = 1 - digitalRead(USEQ_PIN_SWITCH_R1);
  useqInputValues[USEQM2] = 1 - digitalRead(USEQ_PIN_SWITCH_M2);
  useqInputValues[USEQT2] = 1 - digitalRead(USEQ_PIN_SWITCH_T2);
#endif

#ifdef USEQHARDWARE_1_0
  auto v_ai1 = analogRead(USEQ_PIN_AI1);
  auto v_ai1_11  = v_ai1 >> 1; //scale from 12 bit to 11 bit range
  v_ai1_11 = (v_ai1_11 * v_ai1_11) >> 11; //sqr to get exp curve
  analogWrite(USEQ_PIN_LED_AI1, v_ai1_11); 
  // useqInputValues[USEQAI1] = v_ai1
  auto v_ai2 = analogRead(USEQ_PIN_AI2);
  auto v_ai2_11  = v_ai2 >> 1;
  v_ai2_11 = (v_ai2_11 * v_ai2_11) >> 11;
  analogWrite(USEQ_PIN_LED_AI2, v_ai2_11 >> 1); 
#endif
}


int test = 0;


bool setupComplete = false;

void setup() {
  //doesn't work properly 
  // vreg_set_voltage(VREG_VOLTAGE_1_30);
  // delay(1000);
  // set_sys_clock_khz(360000, true);


  Serial.begin(115200);
  Serial.setTimeout(2);
  randomSeed(analogRead(0));

  loadBuiltinDefs();

  setup_leds();
  led_animation();
  module_setup();

  for (int i = 0; i < LispLibrarySize; i++)
    run(LispLibrary[i], env);
  Serial.println("Library loaded");
  useq::setup();
  setupComplete = true;
  // multicore_launch_core1(loop_core1);
}

int ts = 0;
int updateSpeed = 0;


void loop() {

  updateSpeed = micros() - ts;
  env.set("fps", Value(1000000.0 / updateSpeed));
  env.set("qt", Value(updateSpeed * 0.001));
  ts = micros();

  get_time = 0;
  parse_time = 0;
  run_time = 0;

  RESET_TIMER

  useq::update();

  env.set("perf_get", Value(float(get_time * 0.001)));
  env.set("perf_parse", Value(float(parse_time * 0.001)));
  env.set("perf_run", Value(float(run_time * 0.001)));
  env.set("perf_ts1", Value(float(ts_total * 0.001)));

  if (Serial.available()) {
      int b = Serial.read();
      if (b==31) {
          //incoming serial stream
          size_t channel = Serial.read();
          char buffer[8];
          Serial.readBytes(buffer,8);
          if (channel > 0 && channel <= SERIAL_INS) {
              double v = 0;
              memcpy(&v, buffer, 8);
              useq::serialInputStreams.at(channel - 1) = v;
          }
      }else if (b == '@') {
          //run now
        String cmd = Serial.readStringUntil('\n');
        // for(size_t k=0; k < cmd.length(); k++) {
        //   Serial.println((int)cmd.charAt(k));
        // }
        // Serial.println(cmd);
        auto res = run(cmd, env);
        Serial.println(res.debug());
      }else{
          String cmd = Serial.readStringUntil('\n');
          cmd = String((char)b) + cmd;
          Serial.println(cmd);
          auto parsedCode = ::parse(cmd);
          useq::runQueue.push_back(parsedCode);
      }
//    String cmd = Serial.readString();
//    //queue or run now
////    Serial.println((int)cmd.charAt(0));
//    Serial.println(cmd);
//    if (cmd.charAt(0) == '@') {
//        //clear the token
//        cmd.setCharAt(0, ' ');
//        auto res = run(cmd, env);
//        Serial.println(res.debug());
//
//    }
//    else {
//      auto parsedCode = ::parse(cmd);
//      useq::runQueue.push_back(parsedCode);
//    }
  }

#ifdef USEQHARDWARE_0_2
  readRotaryEnc();
#endif

  //slow down for testing?
  // delay(50);
}


//test - make white noise on core 1, output 
// double a;

// void setup1() {
//   a=0;
// }

// //https://arduino-pico.readthedocs.io/en/latest/multicore.html
// void loop1() {
//   // rp2040.idleOtherCore();
//   a = a + 0.01;
//   if (a>=1.0) {
//     a =0;
//   }
//   analogWrite(useq_output_pins[1], rand() % 2000);
//   // rp2040.resumeOtherCore();
//   // delay(1);
// }
