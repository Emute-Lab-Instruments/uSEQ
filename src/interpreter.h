#ifndef INTERPRETER_H_
#define INTERPRETER_H_

bool currentExprSound = true;

#include "string.h"
#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "interpreter/environment.h"
#include "interpreter/value.h"

int micros() {
  auto current_time =
      std::chrono::high_resolution_clock::now().time_since_epoch();
  auto microseconds =
      std::chrono::duration_cast<std::chrono::microseconds>(current_time)
          .count();
  return microseconds;
}

template <typename T> void print(const T &value) {
  std::cout << value << std::endl;
}

template <typename T> void println(const T &value) {
  std::cout << value << std::endl;
}

////////////////////////////////////////////////////////////////////////////////
/// ERROR MESSAGES /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#define RO_STRING(x, y) const char x[] = y;

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
/// HELPER FUNCTIONS
/// ///////////////////////////////////////////////////////////
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
  return (isdigit(ch) || isalpha(ch) || ispunct(ch)) && ch != '(' &&
         ch != ')' && ch != '"' && ch != '\'';
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

// namespace etl {

// template <> struct hash<String> {
//   std::size_t operator()(const String &k) const {
//     using etl::hash;
//     etl::string<3> firstThree(k.substring(0, 3).c_str());
//     return hash<etl::string<3>>()(firstThree);
//   }
// };

//} // namespace etl

#define BUILTINFUNC(__name__, __body__, __numArgs__)                           \
  Value __name__(std::vector<Value> &args, Environment &env) {                 \
    eval_args(args, env);                                                      \
    Value ret = Value();                                                       \
    if (args.size() != __numArgs__) {                                          \
      println(args.size() > __numArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS);       \
      ret = Value::error();                                                    \
    } else {                                                                   \
      __body__                                                                 \
    }                                                                          \
    return ret;                                                                \
  }

#define BUILTINFUNC_VARGS(__name__, __body__, __minArgs__, __maxArgs__)        \
  Value __name__(std::vector<Value> &args, Environment &env) {                 \
    eval_args(args, env);                                                      \
    Value ret = Value();                                                       \
    if (args.size() < __minArgs__ || args.size() > __maxArgs__)                \
      println(args.size() > __maxArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS);       \
    else {                                                                     \
      __body__                                                                 \
    }                                                                          \
    return ret;                                                                \
  }

#define BUILTINFUNC_NOEVAL(__name__, __body__, __numArgs__)                    \
  Value __name__(std::vector<Value> &args, Environment &env) {                 \
    Value ret = Value();                                                       \
    if (args.size() != __numArgs__)                                            \
      println(args.size() > __numArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS);       \
    else {                                                                     \
      __body__                                                                 \
    }                                                                          \
    return ret;                                                                \
  }

namespace builtin {

double fast(double speed, double phasor) {
  phasor *= speed;
  double phase = fmod(phasor, 1.0);
  return phase;
}

// Value fromList(std::vector<Value> &lst, double phasor, Environment &env) {
//   if (phasor < 0.0) {
//     phasor = 0;
//   } else if (phasor > 1) {
//     phasor = 1;
//   }
//   double scaled_phasor = lst.size() * phasor;
//   size_t idx = floor(scaled_phasor);
//   if (idx == lst.size())
//     idx--;
//   return lst[idx].eval(env);
// }

} // namespace builtin

// The type for a builtin function, which takes a list of values,
// and the environment to run the function in.
typedef Value (*Builtin)(std::vector<Value> &, Environment &);

// Error::Error(Value v, Environment const &env, const char *msg) : env(env),
// msg(msg) {
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
//     return "error: the expression `" + cause->debug() + "` failed in scope
//     "
//     + to_string(env) + " with message \"" + msg + "\"";
// }

Value apply(Value &f, std::vector<Value> &args, Environment &env) {
  return f.apply(args, env);
}

Value Value::apply(std::vector<Value> &args, Environment &env) {

  switch (type) {
  case LAMBDA: {
    Environment e;
    std::vector<Value> *params;

    // Get the list of parameter atoms
    params = &list[0].list;
    if (params->size() != args.size()) {
      println(args.size() > params->size() ? TOO_MANY_ARGS : TOO_FEW_ARGS);
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
        println(INVALID_LAMBDA);
      // throw Error(*this, env, INVALID_LAMBDA);
      // Set the parameter name into the scope.
      e.set((*params)[i].str, args[i]);
    }
    // Evaluate the function body with the function scope
    auto result = list[1].eval(e);
    return result;
  }
  case BUILTIN: {
    // Here, we call the builtin function with the current scope.
    // This allows us to write special forms without syntactic sugar.
    // For functions that are not special forms, we just evaluate
    // the arguments before we run the function.
    auto result = (stack_data.b)(args, env);
    return result;
  }
  default:
    // We can only call lambdas and builtins
    print(CALL_NON_FUNCTION);
    println(str);
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
  case ATOM: {
    ts_get = micros();
    auto atomdata = env.get(str);
    if (atomdata == Value::error()) {
      print("Get error: ");
      println(str);
    }
    ts_get = micros() - ts_get;
    get_time += ts_get;
    return atomdata;
  }
  case LIST: {
    if (list.size() < 1)
      println(EVAL_EMPTY_LIST);
    // throw Error(*this, env, EVAL_EMPTY_LIST);
    // note: this needs to be a copy?  so original remains unevaluated?  or if
    // not, use std::span to avoid the copy?
    args = std::vector<Value>(list.begin() + 1, list.end());
    // Only evaluate our arguments if it's not builtin!
    // Builtin functions can be special forms, so we
    // leave them to evaluate their arguments.
    function = list[0].eval(env);
    if (function.is_error()) {
      print("err: function is error");
      return Value::error();
    } else {
      // lambda?
      bool evalError = false;
      if (!function.is_builtin()) {
        for (size_t i = 0; i < args.size(); i++) {
          args[i] = args[i].eval(env);
          if (args[i] == Value::error()) {
            evalError = true;
            break;
          }
        }
      }

      if (evalError) {
        return Value::error();
      } else {
        auto functionResult = function.apply(args, env);
        return functionResult;
      }
    }
  }

  default:
    return *this;
  }
}

void skip_whitespace(String &s, int &ptr) {
  while (isspace(s[ptr])) {
    ptr++;
  }
}

// Value v = Value();
// Value ftest() {
//   return Value();
// }

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
    // return Value::error();
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
      println("eval args error");
      ret = Value::error();
    }
  }
  return ret;
}

// Create a lambda function (SPECIAL FORM)
Value lambda(std::vector<Value> &args, Environment &env) {
  if (args.size() < 2)
    println(TOO_FEW_ARGS);

  // throw Error(Value("lambda", lambda), env, TOO_FEW_ARGS);

  if (args[0].get_type_name() != LIST_TYPE)
    println(INVALID_LAMBDA);

  // throw Error(Value("lambda", lambda), env, INVALID_LAMBDA);

  return Value(args[0].as_list(), args[1], env);
}

// if-else (SPECIAL FORM)
Value if_then_else(std::vector<Value> &args, Environment &env) {
  if (args.size() != 3)
    println(args.size() > 3 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  if (args[0].eval(env).as_bool())
    return args[1].eval(env);
  else
    return args[2].eval(env);
}

// Define a variable with a value (SPECIAL FORM)
Value define(std::vector<Value> &args, Environment &env) {
  if (args.size() != 2)
    println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);

  Value result = args[1].eval(env);
  env.set(args[0].display(), result);
  return result;
}

Value set(std::vector<Value> &args, Environment &env) {
  if (args.size() != 2)
    println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);

  Value result = args[1].eval(env);
  env.set_global(args[0].display(), result);
  return result;
}

// Define a function with parameters and a result expression (SPECIAL FORM)
Value defun(std::vector<Value> &args, Environment &env) {
  if (args.size() != 3)
    println(args.size() > 3 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("defun", defun), env, args.size() > 3? TOO_MANY_ARGS :
  // TOO_FEW_ARGS);

  if (args[1].get_type_name() != LIST_TYPE)
    println(INVALID_LAMBDA);
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
    println(TOO_FEW_ARGS);
  // throw Error(Value("print", print), env, TOO_FEW_ARGS);

  Value acc;
  for (size_t i = 0; i < args.size(); i++) {
    acc = args[i];
    print(acc.display().c_str());
    // std::cout << acc.display();
    if (i < args.size() - 1)
      // std::cout << " ";
      print(" ");
  }
  // std::cout << std::endl;
  println();
  return acc;
}

// Get a random number between two numbers inclusively
Value gen_random(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("random", random), env, args.size() > 2? TOO_MANY_ARGS
  // : TOO_FEW_ARGS);

  int low = args[0].as_int(), high = args[1].as_int();
  return Value((int)random(low, high));
}

// // Get the contents of a file
// Value read_file(std::vector<Value> &args, Environment &env) {
//     // Is not a special form, so we can evaluate our args.
//     eval_args(args, env);

//     if (args.size() != 1)
//         throw Error(Value("read-file", read_file), env, args.size() > 1?
//         TOO_MANY_ARGS : TOO_FEW_ARGS);

//     // return Value::string(content);
//     return Value::string(read_file_contents(args[0].as_string()));
// }

// // Write a string to a file
// Value write_file(std::vector<Value> &args, Environment &env) {
//     // Is not a special form, so we can evaluate our args.
//     eval_args(args, env);

//     if (args.size() != 2)
//         throw Error(Value("write-file", write_file), env, args.size() > 1?
//         TOO_MANY_ARGS : TOO_FEW_ARGS);

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
//         throw Error(Value("include", include), env, args.size() > 1?
//         TOO_MANY_ARGS : TOO_FEW_ARGS);

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
    println("eval err");
    return Value::error();
  }
  if (args.size() != 1) {
    println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
    return Value();
  }
  // throw Error(Value("eval", eval), env, args.size() > 1? TOO_MANY_ARGS :
  // TOO_FEW_ARGS);
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
    println(TOO_FEW_ARGS);
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
    println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("-", subtract), env, args.size() > 2? TOO_MANY_ARGS :
  // TOO_FEW_ARGS);
  return args[0] - args[1];
}

// Multiply several values
Value product(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() < 2)
    println(TOO_FEW_ARGS);
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
    println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("/", divide), env, args.size() > 2? TOO_MANY_ARGS :
  // TOO_FEW_ARGS);
  auto result = args[0] / args[1];
  return result;
}

// Get the remainder of values
Value remainder(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("%", remainder), env, args.size() > 2? TOO_MANY_ARGS :
  // TOO_FEW_ARGS);
  auto result = args[0] % args[1];
  return result;
}

// Are two values equal?
Value eq(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("=", eq), env, args.size() > 2? TOO_MANY_ARGS :
  // TOO_FEW_ARGS);
  return Value(int(args[0] == args[1]));
}

// Are two values not equal?
Value neq(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("!=", neq), env, args.size() > 2? TOO_MANY_ARGS :
  // TOO_FEW_ARGS);
  return Value(int(args[0] != args[1]));
}

// Is one number greater than another?
Value greater(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value(">", greater), env, args.size() > 2? TOO_MANY_ARGS :
  // TOO_FEW_ARGS);
  return Value(int(args[0] > args[1]));
}

// Is one number less than another?
Value less(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("<", less), env, args.size() > 2? TOO_MANY_ARGS :
  // TOO_FEW_ARGS);
  return Value(int(args[0] < args[1]));
}

// Is one number greater than or equal to another?
Value greater_eq(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value(">=", greater_eq), env, args.size() > 2? TOO_MANY_ARGS
  // : TOO_FEW_ARGS);
  return Value(int(args[0] >= args[1]));
}

// Is one number less than or equal to another?
Value less_eq(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("<=", less_eq), env, args.size() > 2? TOO_MANY_ARGS :
  // TOO_FEW_ARGS);
  return Value(int(args[0] <= args[1]));
}

// Get the type name of a value
Value get_type_name(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 1)
    println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("type", get_type_name), env, args.size() > 1?
  // TOO_MANY_ARGS : TOO_FEW_ARGS);

  return Value::string(args[0].get_type_name());
}

// Cast an item to a float
Value cast_to_float(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 1)
    println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value(FLOAT_TYPE, cast_to_float), env, args.size() > 1?
  // TOO_MANY_ARGS : TOO_FEW_ARGS);
  return args[0].cast_to_float();
}

// Cast an item to an int
Value cast_to_int(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 1)
    println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value(INT_TYPE, cast_to_int), env, args.size() > 1?
  // TOO_MANY_ARGS : TOO_FEW_ARGS);
  return args[0].cast_to_int();
}

// Index a list
Value index(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("index", index), env, args.size() > 2? TOO_MANY_ARGS :
  // TOO_FEW_ARGS);

  std::vector<Value> list = args[0].as_list();
  int i = args[1].as_int();
  if (list.empty() || i >= (int)list.size()) {
    println(INDEX_OUT_OF_RANGE);
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
    println(args.size() > 3 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("insert", insert), env, args.size() > 3? TOO_MANY_ARGS
  // : TOO_FEW_ARGS);

  std::vector<Value> list = args[0].as_list();
  int i = args[1].as_int();
  if (i > (int)list.size())
    println(INDEX_OUT_OF_RANGE);
  // throw Error(list, env, INDEX_OUT_OF_RANGE);

  list.insert(list.begin() + args[1].as_int(), args[2].as_int());
  return Value(list);
}

// Remove a value at an index from a list
Value remove(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 2)
    println(args.size() > 2 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("remove", remove), env, args.size() > 2? TOO_MANY_ARGS
  // : TOO_FEW_ARGS);

  std::vector<Value> list = args[0].as_list();
  int i = args[1].as_int();
  if (list.empty() || i >= (int)list.size())
    println(INDEX_OUT_OF_RANGE);
  //  throw Error(list, env, INDEX_OUT_OF_RANGE);

  list.erase(list.begin() + i);
  return Value(list);
}

// Get the length of a list
Value len(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 1)
    println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
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
    println(TOO_FEW_ARGS);
  // throw Error(Value("push", push), env, TOO_FEW_ARGS);
  for (size_t i = 1; i < args.size(); i++)
    args[0].push(args[i]);
  return args[0];
}

Value pop(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 1)
    println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("pop", pop), env, args.size() > 1? TOO_MANY_ARGS :
  // TOO_FEW_ARGS);
  return args[0].pop();
}

Value head(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 1)
    println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("head", head), env, args.size() > 1? TOO_MANY_ARGS :
  // TOO_FEW_ARGS);
  std::vector<Value> list = args[0].as_list();
  if (list.empty())
    println(INDEX_OUT_OF_RANGE);
  // throw Error(Value("head", head), env, INDEX_OUT_OF_RANGE);

  return list[0];
}

Value tail(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 1)
    println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("tail", tail), env, args.size() > 1? TOO_MANY_ARGS :
  // TOO_FEW_ARGS);

  std::vector<Value> result, list = args[0].as_list();

  for (size_t i = 1; i < list.size(); i++)
    result.push_back(list[i]);

  return Value(result);
}

Value flatten(Value &val, Environment &env) {
  std::vector<Value> flattened;
  if (!val.is_list()) {
    flattened.push_back(val);
  } else {
    auto valList = val.as_list();
    for (size_t i = 0; i < valList.size(); i++) {
      Value evaluatedElement = valList[i].eval(env);
      if (evaluatedElement.is_list()) {
        auto flattenedElement = flatten(evaluatedElement, env).as_list();
        flattened.insert(flattened.end(), flattenedElement.begin(),
                         flattenedElement.end());
      } else {
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
    println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("parse", parse), env, args.size() > 1? TOO_MANY_ARGS :
  // TOO_FEW_ARGS);
  if (args[0].get_type_name() != STRING_TYPE)
    println(INVALID_ARGUMENT);
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
    println(args.size() > 3 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("replace", replace), env, args.size() > 3?
  // TOO_MANY_ARGS : TOO_FEW_ARGS);

  String src = args[0].as_string();
  src.replace(args[1].as_string(), args[2].as_string());
  return Value::string(src);
}

Value display(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 1)
    println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("display", display), env, args.size() > 1?
  // TOO_MANY_ARGS : TOO_FEW_ARGS);

  return Value::string(args[0].display());
}

Value debug(std::vector<Value> &args, Environment &env) {
  // Is not a special form, so we can evaluate our args.
  eval_args(args, env);

  if (args.size() != 1)
    println(args.size() > 1 ? TOO_MANY_ARGS : TOO_FEW_ARGS);
  // throw Error(Value("debug", debug), env, args.size() > 1? TOO_MANY_ARGS :
  // TOO_FEW_ARGS);

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
    println(MISMATCHED_TYPES);
  // throw Error(low, env, MISMATCHED_TYPES);
  if (high.get_type_name() != INT_TYPE && high.get_type_name() != FLOAT_TYPE)
    println(MISMATCHED_TYPES);
  // throw Error(high, env, MISMATCHED_TYPES);

  if (low >= high)
    return Value(result);

  while (low < high) {
    result.push_back(low);
    low = low + Value(1);
  }
  return Value(result);
}
} // namespace builtin

#define BUILTINFUNC(__name__, __body__, __numArgs__)                           \
  Value __name__(std::vector<Value> &args, Environment &env) {                 \
    eval_args(args, env);                                                      \
    Value ret = Value();                                                       \
    if (args.size() != __numArgs__) {                                          \
      println(args.size() > __numArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS);       \
      ret = Value::error();                                                    \
    } else {                                                                   \
      __body__                                                                 \
    }                                                                          \
    return ret;                                                                \
  }

#define BUILTINFUNC_VARGS(__name__, __body__, __minArgs__, __maxArgs__)        \
  Value __name__(std::vector<Value> &args, Environment &env) {                 \
    eval_args(args, env);                                                      \
    Value ret = Value();                                                       \
    if (args.size() < __minArgs__ || args.size() > __maxArgs__)                \
      println(args.size() > __maxArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS);       \
    else {                                                                     \
      __body__                                                                 \
    }                                                                          \
    return ret;                                                                \
  }

#define BUILTINFUNC_NOEVAL(__name__, __body__, __numArgs__)                    \
  Value __name__(std::vector<Value> &args, Environment &env) {                 \
    Value ret = Value();                                                       \
    if (args.size() != __numArgs__)                                            \
      println(args.size() > __numArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS);       \
    else {                                                                     \
      __body__                                                                 \
    }                                                                          \
    return ret;                                                                \
  }

void load_builtindefs() {
  // arduino math
  // Environment::builtindefs["sin"] = Value("sin", builtin::ard_sin);
  // Environment::builtindefs["cos"] = Value("cos", builtin::ard_cos);
  // Environment::builtindefs["tan"] = Value("tan", builtin::ard_tan);
  // Environment::builtindefs["abs"] = Value("abs", builtin::ard_abs);

  // Environment::builtindefs["min"] = Value("min", builtin::ard_min);
  // Environment::builtindefs["max"] = Value("max", builtin::ard_max);
  // Environment::builtindefs["pow"] = Value("pow", builtin::ard_pow);
  // Environment::builtindefs["sqrt"] = Value("sqrt", builtin::ard_sqrt);
  // Environment::builtindefs["scale"] = Value("scale", builtin::ard_map);

  // // Meta operations
  // Environment::builtindefs["eval"] = Value("eval", builtin::eval);
  // Environment::builtindefs["type"] = Value("type", builtin::get_type_name);
  // Environment::builtindefs["parse"] = Value("parse", builtin::parse);

  // // Special forms
  // Environment::builtindefs["do"] = Value("do", builtin::do_block);
  // Environment::builtindefs["if"] = Value("if", builtin::if_then_else);
  // Environment::builtindefs["for"] = Value("for", builtin::for_loop);
  // Environment::builtindefs["while"] = Value("while", builtin::while_loop);
  // Environment::builtindefs["scope"] = Value("scope", builtin::scope);
  // Environment::builtindefs["quote"] = Value("quote", builtin::quote);
  // Environment::builtindefs["defun"] = Value("defun", builtin::defun);
  // Environment::builtindefs["define"] = Value("define", builtin::define);
  // Environment::builtindefs["set"] = Value("set", builtin::set);
  // Environment::builtindefs["lambda"] = Value("lambda", builtin::lambda);

  // // Comparison operations
  // Environment::builtindefs["="] = Value("=", builtin::eq);
  // Environment::builtindefs["!="] = Value("!=", builtin::neq);
  // Environment::builtindefs[">"] = Value(">", builtin::greater);
  // Environment::builtindefs["<"] = Value("<", builtin::less);
  // Environment::builtindefs[">="] = Value(">=", builtin::greater_eq);
  // Environment::builtindefs["<="] = Value("<=", builtin::less_eq);

  // Arithmetic operations
  Environment::builtindefs["+"] = Value("+", builtin::sum);
  Environment::builtindefs["-"] = Value("-", builtin::subtract);
  Environment::builtindefs["*"] = Value("*", builtin::product);
  Environment::builtindefs["/"] = Value("/", builtin::divide);
  // Environment::builtindefs["%"] = Value("%", builtin::remainder);
  // Environment::builtindefs["floor"] = Value("floor", builtin::ard_floor);
  // Environment::builtindefs["ceil"] = Value("ceil", builtin::ard_ceil);

  // // List operations
  // Environment::builtindefs["list"] = Value("list", builtin::list);
  // Environment::builtindefs["insert"] = Value("insert", builtin::insert);
  // Environment::builtindefs["index"] = Value("index", builtin::index);
  // Environment::builtindefs["remove"] = Value("remove", builtin::remove);

  // Environment::builtindefs["len"] = Value("len", builtin::len);

  // Environment::builtindefs["push"] = Value("push", builtin::push);
  // Environment::builtindefs["pop"] = Value("pop", builtin::pop);
  // Environment::builtindefs["head"] = Value("head", builtin::head);
  // Environment::builtindefs["tail"] = Value("tail", builtin::tail);
  // Environment::builtindefs["first"] = Value("first", builtin::head);
  // Environment::builtindefs["last"] = Value("last", builtin::pop);
  // Environment::builtindefs["range"] = Value("range", builtin::range);

  // Functional operations
  // Environment::builtindefs["map"] = Value("map", builtin::map_list);
  // Environment::builtindefs["filter"] = Value("filter", builtin::filter_list);
  // Environment::builtindefs["reduce"] = Value("reduce", builtin::reduce_list);

// IO operations
#ifdef USE_STD
  // if (name == "exit") return Value("exit", builtin::exit);
  // if (name == "quit") return Value("quit", builtin::exit);
  Environment::builtindefs["print"] = Value("print", builtin::print);
  // if (name == "input") return Value("input", builtin::input);
  Environment::builtindefs["random"] = Value("random", builtin::gen_random);
#endif

  // String operations
  // Environment::builtindefs["debug"] = Value("debug", builtin::debug);
  // Environment::builtindefs["replace"] = Value("replace", builtin::replace);
  // Environment::builtindefs["display"] = Value("display", builtin::display);

  // Casting operations
  // Environment::builtindefs["int"] = Value("int", builtin::cast_to_int);
  // Environment::builtindefs["float"] = Value("float", builtin::cast_to_float);

  // Environment::builtindefs["endl"] = Value::string("\n");
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
  else
    return false;
}

// Get the value associated with this name in this scope
Value Environment::get(const String &name) const {
  // print("Env get ");
  // println(name);

  // auto b_itr = Environment::builtindefs.find(name);
  // if (b_itr != Environment::builtindefs.end())
  //   return b_itr->second;

  // auto itr = defs.find(name);
  // if (itr != defs.end())
  //   return itr->second;
  // else if (parent_scope != NULL) {
  //   itr = parent_scope->defs.find(name);
  //   if (itr != parent_scope->defs.end())
  //     return itr->second;
  //   else
  //     return parent_scope->get(name);
  // }
  // print(ATOM_NOT_DEFINED);
  // print(": ");
  // println(name);
  return Value::error();
}

// etl::unordered_map<String, Value, 256> Environment::builtindefs;

#endif // INTERPRETER_H_
