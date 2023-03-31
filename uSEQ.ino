/// A microlisp named Wisp, by Adam McDaniel

////////////////////////////////////////////////////////////////////////////////
/// LANGUAGE OPTIONS ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


#define NO_LIBM_SUPPORT "no libm support"


// Comment this define out to drop support for standard library functions.
// This allows the program to run without a runtime.
#define USE_STD
#ifdef USE_STD
// #include <cstdlib>
// #include <iostream>
// #include <fstream>
// #include <ctime>



#else
#define NO_STD "no standard library support"
#endif


////////////////////////////////////////////////////////////////////////////////
/// REQUIRED INCLUDES //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#include <map>
#include <vector>
#include <sstream>

////////////////////////////////////////////////////////////////////////////////
/// ERROR MESSAGES /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#define RO_STRING(x,y) const char PROGMEM x[] = y;
RO_STRING(TOO_FEW_ARGS,"too few arguments to function")
RO_STRING(TOO_MANY_ARGS,"too many arguments to function")
RO_STRING(INVALID_ARGUMENT,"invalid argument")
RO_STRING(MISMATCHED_TYPES,"mismatched types")
RO_STRING(CALL_NON_FUNCTION,"called non-function")
RO_STRING(UNKNOWN_ERROR,"unknown exception")
RO_STRING(INVALID_LAMBDA,"invalid lambda")
RO_STRING(INVALID_BIN_OP,"invalid binary operation")
RO_STRING(INVALID_ORDER,"cannot order expression")
RO_STRING(BAD_CAST,"cannot cast")
RO_STRING(ATOM_NOT_DEFINED,"atom not defined")
RO_STRING(EVAL_EMPTY_LIST,"evaluated empty list")
RO_STRING(INTERNAL_ERROR,"interal virtual machine error")
RO_STRING(INDEX_OUT_OF_RANGE,"index out of range")
RO_STRING(MALFORMED_PROGRAM,"malformed program")


////////////////////////////////////////////////////////////////////////////////
/// LISP LIBRARY /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#include "LispLibrary.h"
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
    return (isalpha(ch) || ispunct(ch)) && ch != '(' && ch != ')' && ch != '"' && ch != '\'';
}

////////////////////////////////////////////////////////////////////////////////
/// LISP CONSTRUCTS ////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Forward declaration for Environment class definition
class Value;
class Environment;
Value parse(String &s, int &ptr);
Value run(String code, Environment &env);

// An instance of a function's scope.
class Environment {
public:
    // Default constructor
    Environment() : parent_scope(NULL) {}

    // Does this environment, or its parent environment,
    // have this atom in scope?
    // This is only used to determine which atoms to capture when
    // creating a lambda function.
    bool has(String name) const;
    // Get the value associated with this name in this scope
    Value get(String name) const;
    // Set the value associated with this name in this scope
    void set(String name, Value value);

    void combine(Environment const &other);

    void set_parent_scope(Environment *parent) {
        parent_scope = parent;
    }
    
    // Output this scope in readable form to a stream.
    friend std::ostream &operator<<(std::ostream &os, Environment const &v);
private:

    // The definitions in the scope.
    std::map<String, Value> defs;
    Environment *parent_scope;
};

// The type for a builtin function, which takes a list of values,
// and the environment to run the function in.
typedef Value (*Builtin)(std::vector<Value>, Environment &);

class Value {
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
    Value(std::vector<Value> params, Value ret, Environment const &env) : type(LAMBDA) {
        // We store the params and the result in the list member
        // instead of having dedicated members. This is to save memory.
        list.push_back(Value(params));
        list.push_back(ret);

        // Lambdas capture only variables that they know they will use.
        std::vector<String> used_atoms = ret.get_used_atoms();
        for (size_t i=0; i<used_atoms.size(); i++) {
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
            for (size_t i=0; i<list.size(); i++) {
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
    Value apply(std::vector<Value> args, Environment &env);
    // Evaluate this value as lisp code.
    Value eval(Environment &env);

    bool is_number() const {
        return type == INT || type == FLOAT;
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
        return cast_to_int().stack_data.f;
    }

    // Get this item's string value
    String as_string() const {
        // If this item is not a string, throw a cast error.
        if (type != STRING)
//            throw Error(*this, Environment(), BAD_CAST);
          Serial.println(BAD_CAST);
        return str;
    }

    // Get this item's atom value
    String as_atom() const {
        // If this item is not an atom, throw a cast error.
        if (type != ATOM)
          Serial.println(BAD_CAST);
            // throw Error(*this, Environment(), BAD_CAST);
        return str;
    }

    // Get this item's list value
    std::vector<Value> as_list() const {
        // If this item is not a list, throw a cast error.
        if (type != LIST)
          Serial.println(BAD_CAST);
            // throw Error(*this, Environment(), BAD_CAST);
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
        Value result = list[list.size()-1];
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
          Serial.println(BAD_CAST);
          return Value();
            // throw Error(*this, Environment(), BAD_CAST);
        }
    }

    // Cast this to a floating point value
    Value cast_to_float() const {
        switch (type) {
        case FLOAT: return *this;
        case INT: return Value(float(stack_data.i));
        // Only ints and floats can be cast to a float
        default:
          Serial.println(BAD_CAST);
            // throw Error(*this, Environment(), BAD_CAST);
            return Value();
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
        if ((is_number() || other.is_number()) &&
            !(is_number() && other.is_number()))
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
                for (size_t i=0; i<other.list.size(); i++)
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
            return Value(stack_data.f / other.cast_to_float().stack_data.f);
        case INT:
            // If the other type is a float, go ahead and promote this expression
            // before continuing with the product
            if (other.type == FLOAT)
                return Value(cast_to_float().stack_data.f / other.stack_data.f);
            // Otherwise, do integer multiplication.
            else return Value(stack_data.i / other.stack_data.i);
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
        #ifdef HAS_LIBM
        case FLOAT:
            return Value(fmod(stack_data.f, other.cast_to_float().stack_data.f));
        case INT:
            if (other.type == FLOAT)
                return Value(fmod(cast_to_float().stack_data.f, other.stack_data.f));
            else return Value(stack_data.i % other.stack_data.i);

        #else
        case INT:
            // If we do not support libm, we have to throw errors for floating point values.
            if (other.type != INT)
               Serial.println(NO_LIBM_SUPPORT);
                // throw Error(other, Environment(), NO_LIBM_SUPPORT);
            return Value(stack_data.i % other.stack_data.i);
        #endif

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
            for (size_t i=0; i<list.size(); i++) {
                result += list[i].debug();
                if (i < list.size()-1) result += " ";
            }
            return "(lambda " + result + ")";
        case LIST:
            for (size_t i=0; i<list.size(); i++) {
                result += list[i].debug();
                if (i < list.size()-1) result += " ";
            }
            return "(" + result + ")";
        case BUILTIN:
            return "<" + str + " at " + String(long(stack_data.b)) + ">";
        case UNIT:
            return "@";
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
            return String(stack_data.i);
        case FLOAT:
            return String(stack_data.f);
        case STRING:
            for (size_t i=0; i<str.length(); i++) {
                if (str[i] == '"') result += "\\\"";
                else result += str[i];
            }
            return "\"" + result + "\"";
        case LAMBDA:
            for (size_t i=0; i<list.size(); i++) {
                result += list[i].debug();
                if (i < list.size()-1) result += " ";
            }
            return "(lambda " + result + ")";
        case LIST:
            for (size_t i=0; i<list.size(); i++) {
                result += list[i].debug();
                if (i < list.size()-1) result += " ";
            }
            return "(" + result + ")";
        case BUILTIN:
            return "<" + str + " at " + String(long(stack_data.b)) + ">";
        case UNIT:
            return "@";
        default:
            // We don't know how to debug whatever type this is.
            // This isn't the users fault, this is just unhandled.
            // This should never be reached.
            Serial.println(INTERNAL_ERROR);
            // throw Error(*this, Environment(), INTERNAL_ERROR);
            return "";
        }
    }

    friend std::ostream &operator<<(std::ostream &os, Value const &v) {
        return os << v.display();
    }

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
        UNIT
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
    std::map<String, Value>::const_iterator itr = other.defs.begin();
    for (; itr!=other.defs.end(); itr++) {
        // Iterate through the keys and assign each value.
        defs[itr->first] = itr->second;
    }
}

std::ostream &operator<<(std::ostream &os, Environment const &e) {
    std::map<String, Value>::const_iterator itr = e.defs.begin();
    os << "{ ";
    for (; itr != e.defs.end(); itr++) {
        os << '\'' << itr->first << "' : " << itr->second.debug() << ", ";
    }
    return os << "}";
}

void Environment::set(String name, Value value) {
    defs[name] = value;
}


Value Value::apply(std::vector<Value> args, Environment &env) {
    Environment e;
    std::vector<Value> params;
    switch (type) {
    case LAMBDA:
        // Get the list of parameter atoms
        params = list[0].list;
        if (params.size() != args.size())
          Serial.println(args.size() > params.size() ? TOO_MANY_ARGS : TOO_FEW_ARGS);

            // throw Error(Value(args), env, args.size() > params.size()?
            //     TOO_MANY_ARGS : TOO_FEW_ARGS
            // );

        // Get the captured scope from the lambda
        e = lambda_scope;
        // And make this scope the parent scope
        e.set_parent_scope(&env);

        // Iterate through the list of parameters and
        // insert the arguments into the scope.
        for (size_t i=0; i<params.size(); i++) {
            if (params[i].type != ATOM) 
              Serial.println(INVALID_LAMBDA);
                // throw Error(*this, env, INVALID_LAMBDA);
            // Set the parameter name into the scope.
            e.set(params[i].str, args[i]);
        }

        // Evaluate the function body with the function scope
        return list[1].eval(e);
    case BUILTIN:
        // Here, we call the builtin function with the current scope.
        // This allows us to write special forms without syntactic sugar.
        // For functions that are not special forms, we just evaluate
        // the arguments before we run the function.
        return (stack_data.b)(args, env);
    default:
        // We can only call lambdas and builtins
      Serial.println(CALL_NON_FUNCTION);
        // throw Error(*this, env, CALL_NON_FUNCTION);
        return Value(args);
    }
}



Value Value::eval(Environment &env) {
    std::vector<Value> args;
    Value function;
    Environment e;
    switch (type) {
    case QUOTE:
        return list[0];
    case ATOM:
        return env.get(str);
    case LIST:
        if (list.size() < 1)
          Serial.println(EVAL_EMPTY_LIST);
            // throw Error(*this, env, EVAL_EMPTY_LIST);

        args = std::vector<Value>(list.begin() + 1, list.end());
        
        // Only evaluate our arguments if it's not builtin!
        // Builtin functions can be special forms, so we
        // leave them to evaluate their arguments.
        function = list[0].eval(env);

        if (!function.is_builtin())
            for (size_t i=0; i<args.size(); i++)
                args[i] = args[i].eval(env);

        return function.apply(
            args,
            env
        );

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
        if (s.substring(ptr, ptr + s.length()-ptr-1) == "")
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

        while (s[ptr] != ')')
            result.push(parse(s, ptr));
        
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
            return Value((negate? -1 : 1) * atof(n.c_str()));
        else return Value((negate? -1 : 1) * atoi(n.c_str()));

    } else if (s[ptr] == '\"') {
        // If this is a string
        int n = 1;
        while (s[ptr + n] != '\"') {
            if (ptr + n >= int(s.length()))
              Serial.println(MALFORMED_PROGRAM);
                // throw std::runtime_error(MALFORMED_PROGRAM);
                
            if (s[ptr + n] == '\\') n++;
            n++;
        }

        String x = s.substring(ptr+1, ptr + 1 + n-1);
        ptr += n+1;
        skip_whitespace(s, ptr);

        // Iterate over the characters in the string, and
        // replace escaped characters with their intended values.
        x = unescape(x);        
        // for (size_t i=0; i<x.length(); i++) {
        //     if (x[i] == '\\' && x[i+1] == '\\')
        //         x.replace(i, 2, "\\");
        //     else if (x[i] == '\\' && x[i+1] == '"')
        //         x.replace(i, 2, "\"");
        //     else if (x[i] == '\\' && x[i+1] == 'n')
        //         x.replace(i, 2, "\n");
        //     else if (x[i] == '\\' && x[i+1] == 't')
        //         x.replace(i, 2, "\t");
        // }

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

        String x = s.substring(ptr, ptr+n);
        ptr += n;
        skip_whitespace(s, ptr);
        return Value::atom(x);
    } else {
      Serial.println(MALFORMED_PROGRAM);
      return Value();
        // throw std::runtime_error(MALFORMED_PROGRAM);
    }
}

// Parse an entire program and get its list of expressions.
std::vector<Value> parse(String s) {
    int i=0, last_i=-1;
    std::vector<Value> result;
    // While the parser is making progress (while the pointer is moving right)
    // and the pointer hasn't reached the end of the string,
    while (last_i != i && i <= int(s.length()-1)) {
        // Parse another expression and add it to the list.
        last_i = i;
        result.push_back(parse(s, i));
    }

    // If the whole string wasn't parsed, the program must be bad.
    if (i < int(s.length()))
      Serial.println(MALFORMED_PROGRAM);
        // throw std::runtime_error(MALFORMED_PROGRAM);

    // Return the list of values parsed.
    return result;
}

// Execute code in an environment
Value run(String code, Environment &env) {
    // Parse the code
    std::vector<Value> parsed = parse(code);
    // Iterate over the expressions and evaluate them
    // in this environment.
    for (size_t i=0; i<parsed.size()-1; i++)
        parsed[i].eval(env);

    // Return the result of the last expression.
    return parsed[parsed.size()-1].eval(env);
}


// This namespace contains all the definitions of builtin functions
namespace builtin {
    // This function is NOT a builtin function, but it is used
    // by almost all of them.
    //
    // Special forms are just builtin functions that don't evaluate
    // their arguments. To make a regular builtin that evaluates its
    // arguments, we just call this function in our builtin definition.
    void eval_args(std::vector<Value> &args, Environment &env) {
        for (size_t i=0; i<args.size(); i++)
            args[i] = args[i].eval(env);
    }

    // Create a lambda function (SPECIAL FORM)
    Value lambda(std::vector<Value> args, Environment &env) {
        if (args.size() < 2)
          Serial.println(TOO_FEW_ARGS);

            // throw Error(Value("lambda", lambda), env, TOO_FEW_ARGS);

        if (args[0].get_type_name() != LIST_TYPE)
          Serial.println(INVALID_LAMBDA);

            // throw Error(Value("lambda", lambda), env, INVALID_LAMBDA);

        return Value(args[0].as_list(), args[1], env);
    }

    // if-else (SPECIAL FORM)
    Value if_then_else(std::vector<Value> args, Environment &env) {
        if (args.size() != 3)

            // throw Error(Value("if", if_then_else), env, args.size() > 3? TOO_MANY_ARGS : TOO_FEW_ARGS);
            Serial.println(args.size() > 3? TOO_MANY_ARGS : TOO_FEW_ARGS);
        if (args[0].eval(env).as_bool())
            return args[1].eval(env);
        else return args[2].eval(env);
    }

    // Define a variable with a value (SPECIAL FORM)
    Value define(std::vector<Value> args, Environment &env) {
        if (args.size() != 2)
            Serial.println(args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value("define", define), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
            
        Value result = args[1].eval(env);
        env.set(args[0].display(), result);
        return result;
    }

    // Define a function with parameters and a result expression (SPECIAL FORM)
    Value defun(std::vector<Value> args, Environment &env) {
        if (args.size() != 3)
            Serial.println(args.size() > 3? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value("defun", defun), env, args.size() > 3? TOO_MANY_ARGS : TOO_FEW_ARGS);

        if (args[1].get_type_name() != LIST_TYPE)
            Serial.println(INVALID_LAMBDA);
            // throw Error(Value("defun", defun), env, INVALID_LAMBDA);

        Value f = Value(args[1].as_list(), args[2], env);
        env.set(args[0].display(), f);
        return f;
    }

    // Loop over a list of expressions with a condition (SPECIAL FORM)
    Value while_loop(std::vector<Value> args, Environment &env) {
        Value acc;
        while (args[0].eval(env).as_bool()) {
            for (size_t i=1; i<args.size()-1; i++)
                args[i].eval(env);
            acc = args[args.size()-1].eval(env);
        }
        return acc;
    }

    // Iterate through a list of values in a list (SPECIAL FORM)
    Value for_loop(std::vector<Value> args, Environment &env) {
        Value acc;
        std::vector<Value> list = args[1].eval(env).as_list();

        for (size_t i=0; i<list.size(); i++) {
            env.set(args[0].as_atom(), list[i]);

            for (size_t j=1; j<args.size()-1; j++)
                args[j].eval(env);
            acc = args[args.size()-1].eval(env);
        }

        return acc;
    }

    // Evaluate a block of expressions in the current environment (SPECIAL FORM)
    Value do_block(std::vector<Value> args, Environment &env) {
        Value acc;
        for (size_t i=0; i<args.size(); i++)
            acc = args[i].eval(env);
        return acc;
    }

    // Evaluate a block of expressions in a new environment (SPECIAL FORM)
    Value scope(std::vector<Value> args, Environment &env) {
        Environment e = env;
        Value acc;
        for (size_t i=0; i<args.size(); i++)
            acc = args[i].eval(e);
        return acc;
    }

    // Quote an expression (SPECIAL FORM)
    Value quote(std::vector<Value> args, Environment &) {
        std::vector<Value> v;
        for (size_t i=0; i<args.size(); i++)
            v.push_back(args[i]);
        return Value(v);
    }

    #ifdef USE_STD
    // Exit the program with an integer code
    Value exit(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        std::exit(args.size() < 1? 0 : args[0].cast_to_int().as_int());
        return Value();
    }

    // Print several values and return the last one
    Value print(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() < 1)
            Serial.println(TOO_FEW_ARGS);
            // throw Error(Value("print", print), env, TOO_FEW_ARGS);

        Value acc;
        for (size_t i=0; i<args.size(); i++) {
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

    // Get user input with an optional prompt
    Value input(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() > 1)
            Serial.println(TOO_MANY_ARGS);
            // throw Error(Value("input", input), env, TOO_MANY_ARGS);

        if (!args.empty())
            Serial.println(args[0].as_string());

        String s = Serial.readString();
        return Value::string(s);
    }

    // Get a random number between two numbers inclusively
    Value gen_random(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
            Serial.println(args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value("random", random), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);

        int low = args[0].as_int(), high = args[1].as_int();
        return Value((int)random(low,high));
    }

    // // Get the contents of a file
    // Value read_file(std::vector<Value> args, Environment &env) {
    //     // Is not a special form, so we can evaluate our args.
    //     eval_args(args, env);

    //     if (args.size() != 1)
    //         throw Error(Value("read-file", read_file), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

    //     // return Value::string(content);
    //     return Value::string(read_file_contents(args[0].as_string()));
    // }

    // // Write a string to a file
    // Value write_file(std::vector<Value> args, Environment &env) {
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
    // Value include(std::vector<Value> args, Environment &env) {
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
    Value eval(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1){
            Serial.println(args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
            return Value();
        }
            // throw Error(Value("eval", eval), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
        else return args[0].eval(env);
    }

    // Create a list of values
    Value list(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);
        
        return Value(args);
    }

    // Sum multiple values
    Value sum(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);
        
        if (args.size() < 2)
            Serial.println(TOO_FEW_ARGS);
            // throw Error(Value("+", sum), env, TOO_FEW_ARGS);
        
        Value acc = args[0];
        for (size_t i=1; i<args.size(); i++)
            acc = acc + args[i];
        return acc;
    }

    // Subtract two values
    Value subtract(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
            Serial.println(args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value("-", subtract), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return args[0] - args[1];
    }

    // Multiply several values
    Value product(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() < 2)
            Serial.println(TOO_FEW_ARGS);
            // throw Error(Value("*", product), env, TOO_FEW_ARGS);

        Value acc = args[0];
        for (size_t i=1; i<args.size(); i++)
            acc = acc * args[i];
        return acc;
    }

    // Divide two values
    Value divide(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
            Serial.println(args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value("/", divide), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return args[0] / args[1];
    }

    // Get the remainder of values
    Value remainder(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
            Serial.println(args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value("%", remainder), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return args[0] % args[1];
    }

    // Are two values equal?
    Value eq(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
            Serial.println(args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value("=", eq), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return Value(int(args[0] == args[1]));
    }

    // Are two values not equal?
    Value neq(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
            Serial.println(args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value("!=", neq), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return Value(int(args[0] != args[1]));
    }

    // Is one number greater than another?
    Value greater(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
            Serial.println(args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value(">", greater), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return Value(int(args[0] > args[1]));
    }

    // Is one number less than another?
    Value less(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
            Serial.println(args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value("<", less), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return Value(int(args[0] < args[1]));
    }

    // Is one number greater than or equal to another?
    Value greater_eq(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
            Serial.println(args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value(">=", greater_eq), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return Value(int(args[0] >= args[1]));
    }

    // Is one number less than or equal to another?
    Value less_eq(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
            Serial.println(args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value("<=", less_eq), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return Value(int(args[0] <= args[1]));
    }

    // Get the type name of a value
    Value get_type_name(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1)
            Serial.println(args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value("type", get_type_name), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

        return Value::string(args[0].get_type_name());
    }

    // Cast an item to a float
    Value cast_to_float(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1)
            Serial.println(args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value(FLOAT_TYPE, cast_to_float), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return args[0].cast_to_float();
    }

    // Cast an item to an int
    Value cast_to_int(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1)
            Serial.println(args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value(INT_TYPE, cast_to_int), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return args[0].cast_to_int();
    }

    // Index a list
    Value index(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
          Serial.println(args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
      // throw Error(Value("index", index), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);

        std::vector<Value> list = args[0].as_list();
        int i = args[1].as_int();
        if (list.empty() || i >= (int)list.size())
            Serial.println(INDEX_OUT_OF_RANGE);
            // throw Error(list, env, INDEX_OUT_OF_RANGE);

        return list[i];
    }

    // Insert a value into a list
    Value insert(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 3)
             Serial.println(args.size() > 3? TOO_MANY_ARGS : TOO_FEW_ARGS);
           // throw Error(Value("insert", insert), env, args.size() > 3? TOO_MANY_ARGS : TOO_FEW_ARGS);

        std::vector<Value> list = args[0].as_list();
        int i = args[1].as_int();
        if (i > (int)list.size())
            Serial.println(INDEX_OUT_OF_RANGE);
            // throw Error(list, env, INDEX_OUT_OF_RANGE);

        list.insert(list.begin() + args[1].as_int(), args[2]);
        return Value(list);
    }

    // Remove a value at an index from a list
    Value remove(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 2)
            Serial.println(args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);
        // throw Error(Value("remove", remove), env, args.size() > 2? TOO_MANY_ARGS : TOO_FEW_ARGS);

        std::vector<Value> list = args[0].as_list();
        int i = args[1].as_int();
        if (list.empty() || i >= (int)list.size())
            Serial.println(INDEX_OUT_OF_RANGE);
          //  throw Error(list, env, INDEX_OUT_OF_RANGE);

        list.erase(list.begin() + i);
        return Value(list);
    }

    // Get the length of a list
    Value len(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1)
            Serial.println(args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value("len", len), env, args.size() > 1?
                // TOO_MANY_ARGS : TOO_FEW_ARGS
            // );
        
        return Value(int(args[0].as_list().size()));
    }

    // Add an item to the end of a list
    Value push(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() == 0)
            Serial.println(TOO_FEW_ARGS);
            // throw Error(Value("push", push), env, TOO_FEW_ARGS);
        for (size_t i=1; i<args.size(); i++)
            args[0].push(args[i]);
        return args[0];
    }

    Value pop(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1)
            Serial.println(args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value("pop", pop), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
        return args[0].pop();
    }

    Value head(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1)
            Serial.println(args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value("head", head), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
        std::vector<Value> list = args[0].as_list();
        if (list.empty())
            Serial.println(INDEX_OUT_OF_RANGE);
            // throw Error(Value("head", head), env, INDEX_OUT_OF_RANGE);

        return list[0];
    }

    Value tail(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1)
            Serial.println(args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value("tail", tail), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

        std::vector<Value> result, list = args[0].as_list();

        for (size_t i = 1; i<list.size(); i++)
            result.push_back(list[i]);
        
        return Value(result);
    }

    Value parse(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1)
            Serial.println(args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
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

    Value replace(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 3)
            Serial.println(args.size() > 3? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value("replace", replace), env, args.size() > 3? TOO_MANY_ARGS : TOO_FEW_ARGS);

        String src = args[0].as_string();
        src.replace(args[1].as_string(), args[2].as_string());
        return Value::string(src);
    }

    Value display(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1)
            Serial.println(args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value("display", display), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

        return Value::string(args[0].display());
    }

    Value debug(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        if (args.size() != 1)        
            Serial.println(args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);
            // throw Error(Value("debug", debug), env, args.size() > 1? TOO_MANY_ARGS : TOO_FEW_ARGS);

        return Value::string(args[0].debug());
    }

    Value map_list(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        std::vector<Value> result, l=args[1].as_list(), tmp;
        for (size_t i=0; i<l.size(); i++) {
            tmp.push_back(l[i]);
            result.push_back(args[0].apply(tmp, env));
            tmp.clear();
        }
        return Value(result);
    }

    Value filter_list(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        std::vector<Value> result, l=args[1].as_list(), tmp;
        for (size_t i=0; i<l.size(); i++) {
            tmp.push_back(l[i]);
            if (args[0].apply(tmp, env).as_bool())
                result.push_back(l[i]);
            tmp.clear();
        }
        return Value(result);
    }

    Value reduce_list(std::vector<Value> args, Environment &env) {
        // Is not a special form, so we can evaluate our args.
        eval_args(args, env);

        std::vector<Value> l=args[2].as_list(), tmp;
        Value acc = args[1];
        for (size_t i=0; i<l.size(); i++) {
            tmp.push_back(acc);
            tmp.push_back(l[i]);
            acc = args[0].apply(tmp, env);
            tmp.clear();
        }
        return acc;
    }

    Value range(std::vector<Value> args, Environment &env) {
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

#define BUILTINFUNC(__name__, __body__, __numArgs__) Value __name__(std::vector<Value> args, Environment &env) { \
          eval_args(args, env); \
          Value ret = Value(); \
          if (args.size() != __numArgs__) \
              Serial.println(args.size() > __numArgs__? TOO_MANY_ARGS : TOO_FEW_ARGS); \
          else { \
            __body__ \
          } \
          return ret; \
  } 

#define BUILTINFUNC_NOEVAL(__name__, __body__, __numArgs__) Value __name__(std::vector<Value> args, Environment &env) { \
          eval_args(args, env); \
          Value ret = Value(); \
          if (args.size() != __numArgs__) \
              Serial.println(args.size() > __numArgs__? TOO_MANY_ARGS : TOO_FEW_ARGS); \
          else { \
            __body__ \
          } \
          return ret; \
  }


//extra arduino api functions
namespace builtin {
    // TODO add faux-macro API functions
/*     BUILTINFUNC_NOEVAL( */
/*         d1, */
/*         env.set("d1-form", ) */

/* ) */

    BUILTINFUNC(ard_pinMode, 
          int pinNumber = args[0].as_int();     
          int onOff = args[1].as_int();   
          pinMode(pinNumber, onOff);
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
          int  m = millis();     
          ret = Value(m);
    , 0)

    BUILTINFUNC(ard_micros, 
          int  m = micros();     
          ret = Value(m);
    , 0)

    BUILTINFUNC(ard_sin, 
          float  m = sin(args[0].as_float());     
          ret = Value(m);
    , 1)
    BUILTINFUNC(ard_cos, 
          float  m = cos(args[0].as_float());     
          ret = Value(m);
    , 1)
    BUILTINFUNC(ard_tan, 
          float  m = tan(args[0].as_float());     
          ret = Value(m);
    , 1)

    BUILTINFUNC(ard_abs, 
          float  m = abs(args[0].as_float());     
          ret = Value(m);
    , 1)
    BUILTINFUNC(ard_min, 
          float  m = min(args[0].as_float(), args[1].as_float());     
          ret = Value(m);
    , 2)
    BUILTINFUNC(ard_max, 
          float  m = max(args[0].as_float(), args[1].as_float());     
          ret = Value(m);
    , 2)
    BUILTINFUNC(ard_pow, 
          float  m = pow(args[0].as_float(), args[1].as_float());     
          ret = Value(m);
    , 2)
    BUILTINFUNC(ard_sqrt, 
          float  m = sqrt(args[0].as_float());     
          ret = Value(m);
    , 1)
    BUILTINFUNC(ard_map, 
          float  m = map(args[0].as_float(), args[1].as_float(), args[2].as_float(), args[3].as_float(), args[4].as_float());     
          ret = Value(m);
    , 5)

}


// Does this environment, or its parent environment, have a variable?
bool Environment::has(String name) const {
    // Find the value in the map
    std::map<String, Value>::const_iterator itr = defs.find(name);
    if (itr != defs.end())
        // If it was found
        return true;
    else if (parent_scope != NULL)
        // If it was not found in the current environment,
        // try to find it in the parent environment
        return parent_scope->has(name);
    else return false;
}

// Get the value associated with this name in this scope
Value Environment::get(String name) const {
    //arduino
    if (name == "pm") return Value("pm",  builtin::ard_pinMode);
    if (name == "dw") return Value("dw",  builtin::ard_digitalWrite);
    if (name == "dr") return Value("dr",  builtin::ard_digitalRead);
    if (name == "delay") return Value("delay",  builtin::ard_delay);
    if (name == "delaymicros") return Value("delaymicros",  builtin::ard_delaymicros);
    if (name == "millis") return Value("millis",  builtin::ard_millis);
    if (name == "micros") return Value("micros",  builtin::ard_micros);

    //arduino math
    if (name == "sin") return Value("sin",  builtin::ard_sin);
    if (name == "cos") return Value("cos",  builtin::ard_cos);
    if (name == "tan") return Value("tan",  builtin::ard_tan);
    if (name == "abs") return Value("abs",  builtin::ard_abs);
    if (name == "min") return Value("min",  builtin::ard_min);
    if (name == "max") return Value("max",  builtin::ard_max);
    if (name == "pow") return Value("pow",  builtin::ard_pow);
    if (name == "sqrt") return Value("sqrt",  builtin::ard_sqrt);
    if (name == "map") return Value("map",  builtin::ard_map);


    // Meta operations
    if (name == "eval")  return Value("eval",  builtin::eval);
    if (name == "type")  return Value("type",  builtin::get_type_name);
    if (name == "parse") return Value("parse", builtin::parse);

    // Special forms
    if (name == "do")     return Value("do",     builtin::do_block);
    if (name == "if")     return Value("if",     builtin::if_then_else);
    if (name == "for")    return Value("for",    builtin::for_loop);
    if (name == "while")  return Value("while",  builtin::while_loop);
    if (name == "scope")  return Value("scope",  builtin::scope);
    if (name == "quote")  return Value("quote",  builtin::quote);
    if (name == "defun")  return Value("defun",  builtin::defun);
    if (name == "define") return Value("define", builtin::define);
    if (name == "lambda") return Value("lambda", builtin::lambda);

    // Comparison operations
    if (name == "=")  return Value("=",  builtin::eq);
    if (name == "!=") return Value("!=", builtin::neq);
    if (name == ">")  return Value(">",  builtin::greater);
    if (name == "<")  return Value("<",  builtin::less);
    if (name == ">=") return Value(">=", builtin::greater_eq);
    if (name == "<=") return Value("<=", builtin::less_eq);

    // Arithmetic operations
    if (name == "+") return Value("+", builtin::sum);
    if (name == "-") return Value("-", builtin::subtract);
    if (name == "*") return Value("*", builtin::product);
    if (name == "/") return Value("/", builtin::divide);
    if (name == "%") return Value("%", builtin::remainder);

    // List operations
    if (name == "list")   return Value("list",   builtin::list);
    if (name == "insert") return Value("insert", builtin::insert);
    if (name == "index")  return Value("index",  builtin::index);
    if (name == "remove") return Value("remove", builtin::remove);

    if (name == "len")    return Value("len",   builtin::len);

    if (name == "push")   return Value("push",  builtin::push);
    if (name == "pop")    return Value("pop",   builtin::pop);
    if (name == "head")   return Value("head",  builtin::head);
    if (name == "tail")   return Value("tail",  builtin::tail);
    if (name == "first")  return Value("first", builtin::head);
    if (name == "last")   return Value("last",  builtin::pop);
    if (name == "range")  return Value("range", builtin::range);

    // Functional operations
    if (name == "map")    return Value("map",    builtin::map_list);
    if (name == "filter") return Value("filter", builtin::filter_list);
    if (name == "reduce") return Value("reduce", builtin::reduce_list);

    // IO operations
    #ifdef USE_STD
    if (name == "exit")       return Value("exit",       builtin::exit);
    if (name == "quit")       return Value("quit",       builtin::exit);
    if (name == "print")      return Value("print",      builtin::print);
    if (name == "input")      return Value("input",      builtin::input);
    if (name == "random")     return Value("random",     builtin::gen_random);
    #endif

    // String operations
    if (name == "debug")   return Value("debug",   builtin::debug);
    if (name == "replace") return Value("replace", builtin::replace);
    if (name == "display") return Value("display", builtin::display);
    
    // Casting operations
    if (name == "int")   return Value("int",   builtin::cast_to_int);
    if (name == "float") return Value("float", builtin::cast_to_float);

    // Constants
    if (name == "endl") return Value::string("\n");
    
    std::map<String, Value>::const_iterator itr = defs.find(name);
    if (itr != defs.end()) return itr->second;
    else if (parent_scope != NULL) {
        itr = parent_scope->defs.find(name);
        if (itr != parent_scope->defs.end()) return itr->second;
        else return parent_scope->get(name);
    }
    Serial.println(ATOM_NOT_DEFINED);  
    return Value();
}


Environment env;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.setTimeout(0);
  randomSeed(analogRead (0));
}

void loop() {
  // put your main code here, to run repeatedly:
  if (Serial.available()) {
    String cmd = Serial.readString();
    Serial.println(cmd);
    Value res; 
    res = run(cmd, env);  
    Serial.println(res.debug());
  }
}
