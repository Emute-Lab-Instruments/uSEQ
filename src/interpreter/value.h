#ifndef VALUE_H_
#define VALUE_H_

#include "string.h"

class Value {
public:
  ////////////////////////////////////////////////////////////////////////////////
  /// CONSTRUCTORS
  /// ///////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////

  // Constructs a unit value
  Value() : type(UNIT) {}

  // Constructs an integer
  Value(int i) : type(INT) { stack_data.i = i; }
  // Constructs a floating point value
  Value(double f) : type(FLOAT) { stack_data.f = f; }
  // Constructs a list
  Value(std::vector<Value> list) : type(LIST), list(list) {}

  // Value(const Value& v ) : stack_data(v.stack_data), str(v.str),
  // list(v.list), lambda_scope(v.lambda_scope) {} Value(Value&& v) :
  // stack_data(std::move(v.stack_data)), str(std::move(v.str)),
  // list(std::move(v.list)), lambda_scope(std::move(v.lambda_scope)) {}
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
  Value(String name, Builtin b) : type(BUILTIN) {
    // Store the name of the builtin function in the str member
    // to save memory, and use the builtin function slot in the union
    // to store the function pointer.
    str = name;
    stack_data.b = b;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// C++ INTEROP METHODS
  /// ////////////////////////////////////////////////////////
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
  bool is_builtin() { return type == BUILTIN; }

  // Apply this as a function to a list of arguments in a given environment.
  Value apply(std::vector<Value> &args, Environment &env);
  // Evaluate this value as lisp code.
  Value eval(Environment &env);

  bool is_number() const { return type == INT || type == FLOAT; }

  bool is_error() const { return type == ERROR; }

  bool is_list() const { return type == LIST; }

  // Get the "truthy" boolean value of this value.
  bool as_bool() const { return *this != Value(0); }

  // Get this item's integer value
  int as_int() const { return cast_to_int().stack_data.i; }

  // Get this item's floating point value
  double as_float() const { return cast_to_float().stack_data.f; }

  // Get this item's string value
  String as_string() const {
    // If this item is not a string, throw a cast error.
    if (type != STRING) {
      print("str: ");
      println(BAD_CAST);
      currentExprSound = false;
      return "";
    }
    return str;
  }

  // Get this item's atom value
  String as_atom() const {
    // If this item is not an atom, throw a cast error.
    if (type != ATOM) {
      print("atom: ");
      println(BAD_CAST);
      currentExprSound = false;
      return "";
    }
    return str;
  }

  // Get this item's list value
  std::vector<Value> as_list() const {
    // If this item is not a list, throw a cast error.
    if (type != LIST) {
      print("list: ");
      println(BAD_CAST);
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
      println(MISMATCHED_TYPES);

    // throw Error(*this, Environment(), MISMATCHED_TYPES);

    list.push_back(val);
  }

  // Push an item from the end of this list
  Value pop() {
    // If this item is not a list, you cannot pop from it.
    // Throw an error.
    if (type != LIST)
      // throw Error(*this, Environment(), MISMATCHED_TYPES);
      println(MISMATCHED_TYPES);

    // Remember the last item in the list
    Value result = list[list.size() - 1];
    // Remove it from this instance
    list.pop_back();
    // Return the remembered value
    return result;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// TYPECASTING METHODS
  /// ////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////

  // Cast this to an integer value
  Value cast_to_int() const {
    switch (type) {
    case INT:
      return *this;
    case FLOAT:
      return Value(int(stack_data.f));
    // Only ints and floats can be cast to an int
    default:
      println("int: ");
      println(BAD_CAST);
      currentExprSound = false;
      return Value::error();
      // throw Error(*this, Environment(), BAD_CAST);
    }
  }

  // Cast this to a floating point value
  Value cast_to_float() const {
    switch (type) {
    case FLOAT:
      return *this;
    case INT:
      return Value(double(stack_data.i));
    // Only ints and floats can be cast to a float
    default:
      println("float: ");
      println(BAD_CAST);
      currentExprSound = false;
      // throw Error(*this, Environment(), BAD_CAST);
      return Value::error();
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// COMPARISON OPERATIONS
  /// //////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////

  bool operator==(Value other) const {
    // If either of these values are floats, promote the
    // other to a float, and then compare for equality.
    if (type == FLOAT && other.type == INT)
      return *this == other.cast_to_float();
    else if (type == INT && other.type == FLOAT)
      return this->cast_to_float() == other;
    // If the values types aren't equal, then they cannot be equal.
    else if (type != other.type)
      return false;

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

  bool operator!=(Value other) const { return !(*this == other); }

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
  /// ORDERING OPERATIONS
  /// ////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////

  bool operator>=(Value other) const { return !(*this < other); }

  bool operator<=(Value other) const {
    return (*this == other) || (*this < other);
  }

  bool operator>(Value other) const { return !(*this <= other); }

  bool operator<(Value other) const {
    // Other type must be a float or an int
    if (other.type != FLOAT && other.type != INT)
      println(INVALID_BIN_OP);
    // throw Error(*this, Environment(), INVALID_BIN_OP);

    switch (type) {
    case FLOAT:
      // If this is a float, promote the other value to a float and compare.
      return stack_data.f < other.cast_to_float().stack_data.f;
    case INT:
      // If the other value is a float, promote this value to a float and
      // compare.
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

  ////////////////////////////////////////////////////////////////////////////////
  /// ARITHMETIC OPERATIONS
  /// //////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////

  // This function adds two lisp values, and returns the lisp value result.
  Value operator+(Value other) const {
    // If the other value's type is the unit type,
    // don't even bother continuing.
    // Unit types consume all arithmetic operations.
    if (other.type == UNIT)
      return other;

    // Other type must be a float or an int
    if ((is_number() || other.is_number()) &&
        !(is_number() && other.is_number()))
      println(INVALID_BIN_OP);
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
      else
        return Value(stack_data.i + other.stack_data.i);
    case STRING:
      // If the other value is also a string, do the concat
      if (other.type == STRING)
        return Value::string(str + other.str);
      // We throw an error if we try to concat anything of non-string type
      else
        println(INVALID_BIN_OP);
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
        println(INVALID_BIN_OP);
      // throw Error(*this, Environment(), INVALID_BIN_OP);
    case UNIT:
      return *this;
    default:
      println(INVALID_BIN_OP);
      return Value();
      // throw Error(*this, Environment(), INVALID_BIN_OP);
    }
  }

  // This function subtracts two lisp values, and returns the lisp value
  // result.
  Value operator-(Value other) const {
    // If the other value's type is the unit type,
    // don't even bother continuing.
    // Unit types consume all arithmetic operations.
    if (other.type == UNIT)
      return other;

    // Other type must be a float or an int
    if (other.type != FLOAT && other.type != INT)
      println(INVALID_BIN_OP);
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

  // This function multiplies two lisp values, and returns the lisp value
  // result.
  Value operator*(Value other) const {
    // If the other value's type is the unit type,
    // don't even bother continuing.
    // Unit types consume all arithmetic operations.
    if (other.type == UNIT)
      return other;

    // Other type must be a float or an int
    if (other.type != FLOAT && other.type != INT)
      println(INVALID_BIN_OP);
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
  }

  // This function divides two lisp values, and returns the lisp value result.
  Value operator/(Value other) const {
    // If the other value's type is the unit type,
    // don't even bother continuing.
    // Unit types consume all arithmetic operations.
    if (other.type == UNIT)
      return other;

    // Other type must be a float or an int
    if (other.type != FLOAT && other.type != INT)
      println(INVALID_BIN_OP);
    //             throw Error(*this, Environment(), INVALID_BIN_OP);

    switch (type) {
    case FLOAT: {
      auto res = Value(stack_data.f / other.cast_to_float().stack_data.f);
      return res;
    }
    case INT: {
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
  }

  Value operator[](int i) {
    if (type == LIST && 0 <= i < list.size()) {
      return list[i];
    } else {
      println("BAD OPERATOR[]");
      currentExprSound = false;
      return Value::error();
    }
  }
  // This function finds the remainder of two lisp values, and returns the
  // lisp value result.
  Value operator%(Value other) const {
    // If the other value's type is the unit type,
    // don't even bother continuing.
    // Unit types consume all arithmetic operations.
    if (other.type == UNIT)
      return other;

    // Other type must be a float or an int
    if (other.type != FLOAT && other.type != INT)
      println(INVALID_BIN_OP);
    // throw Error(*this, Environment(), INVALID_BIN_OP);

    switch (type) {
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
      // //            // If we do not support libm, we have to throw errors
      // for floating point values.
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
  }

  // Get the name of the type of this value
  String get_type_name() {
    switch (type) {
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
        if (i < list.size() - 1)
          result += " ";
      }
      return "(lambda " + result + ")";
    case LIST:
      for (size_t i = 0; i < list.size(); i++) {
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

  String debug() const {
    String result;
    // string result;
    switch (type) {
    case QUOTE:
      return "'" + list[0].debug();
    case ATOM:
      return str;
    case INT: {
      auto val = String(stack_data.i);
      return val;
    }
    case FLOAT: {
      auto val = String(stack_data.f);
      return val;
    }
    case STRING:
      for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '"')
          result += "\\\"";
        else
          result += str[i];
      }
      return "\"" + result + "\"";
    case LAMBDA:
      for (size_t i = 0; i < list.size(); i++) {
        result += list[i].debug();
        if (i < list.size() - 1)
          result += " ";
      }
      return "(lambda " + result + ")";
    case LIST:
      for (size_t i = 0; i < list.size(); i++) {
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

#endif // VALUE_H_
