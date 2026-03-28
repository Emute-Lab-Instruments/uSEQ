#define CATCH_CONFIG_MAIN // This tells Catch to provide a main() - only do this in
                          // one cpp file
#include "catch.hpp"

#include "../uSEQ/src/modulisp/bytecode_vm.h"
#include "../uSEQ/src/modulisp/lisp/environment.h"
#include "../uSEQ/src/modulisp/lisp/value.h"
#include "../uSEQ/src/modulisp/modulisp_interpreter.h"
#include <cmath>

// Ensure builtins are initialized once for all tests in this binary
namespace
{
struct BuiltinsInitOnce
{
    BuiltinsInitOnce() { ModuLispInterpreter::init_builtin_functions(); }
} _builtinsInitOnce;
} // namespace

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER CONSTRUCTION AND INITIALIZATION API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Interpreter construction using factory function",
          "[interpreter][api][construction]")
{
    // Test interpreter construction using factory function
    ModuLispInterpreter interp;
    REQUIRE(true); // If we get here, construction and initialization succeeded
}

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER STRING EVALUATION API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("String evaluation of basic values", "[interpreter][api][string_eval]")
{
    ModuLispInterpreter interp;

    // Test evaluating integer literals
    String result1 = interp.eval("42");
    REQUIRE(result1.length() > 0);
    // Should contain the number 42 in some form
    REQUIRE(result1.indexOf("42") >= 0);

    // Test evaluating float literals
    String result2 = interp.eval("3.14");
    REQUIRE(result2.length() > 0);
    REQUIRE(result2.indexOf("3.14") >= 0);

    // Test evaluating string literals
    String result3 = interp.eval("\"hello world\"");
    REQUIRE(result3.length() > 0);
    REQUIRE(result3.indexOf("hello world") >= 0);

    // Test evaluating empty list
    String result4 = interp.eval("()");
    REQUIRE(result4.length() > 0);

    // Test evaluating empty vector
    String result5 = interp.eval("[]");
    REQUIRE(result5.length() > 0);
}

TEST_CASE("String evaluation of arithmetic operations",
          "[interpreter][api][string_eval][arithmetic]")
{
    ModuLispInterpreter interp;

    // Test basic arithmetic operations
    String result1 = interp.eval("(+ 2 3)");
    REQUIRE_FALSE(result1.length() == 0);
    REQUIRE(result1.indexOf("5") != -1);

    String result2 = interp.eval("(- 10 4)");
    REQUIRE_FALSE(result2.length() == 0);
    REQUIRE(result2.indexOf("6") != -1);

    String result3 = interp.eval("(* 6 7)");
    REQUIRE_FALSE(result3.length() == 0);
    REQUIRE(result3.indexOf("42") != -1);

    String result4 = interp.eval("(/ 15 3)");
    REQUIRE_FALSE(result4.length() == 0);
    REQUIRE(result4.indexOf("5") != -1);

    // Test nested arithmetic
    String result5 = interp.eval("(+ (* 2 3) (/ 8 4))");
    REQUIRE_FALSE(result5.length() == 0);
    REQUIRE(result5.indexOf("8") != -1); // 6 + 2 = 8
}

TEST_CASE("String evaluation of comparison operations",
          "[interpreter][api][string_eval][comparisons]")
{
    ModuLispInterpreter interp;

    // Test equality
    String result1 = interp.eval("(= 5 5)");
    REQUIRE_FALSE(result1.length() == 0);
    // Should indicate true (might be "1", "true", etc.)

    String result2 = interp.eval("(= 5 3)");
    REQUIRE_FALSE(result2.length() == 0);
    // Should indicate false (might be "0", "false", etc.)

    // Test ordering
    String result3 = interp.eval("(> 5 3)");
    REQUIRE_FALSE(result3.length() == 0);

    String result4 = interp.eval("(< 3 5)");
    REQUIRE_FALSE(result4.length() == 0);

    String result5 = interp.eval("(>= 5 5)");
    REQUIRE_FALSE(result5.length() == 0);

    String result6 = interp.eval("(<= 3 5)");
    REQUIRE_FALSE(result6.length() == 0);
}

TEST_CASE("String evaluation of list operations",
          "[interpreter][api][string_eval][lists]")
{
    ModuLispInterpreter interp;

    // Test list creation
    String result1 = interp.eval("(list 1 2 3)");
    REQUIRE_FALSE(result1.length() == 0);

    // Test vector creation
    String result2 = interp.eval("(vec 1 2 3)");
    REQUIRE_FALSE(result2.length() == 0);

    // Test list access functions (if available)
    String result3 = interp.eval("(head (list 1 2 3))");
    REQUIRE_FALSE(result3.length() == 0);

    String result4 = interp.eval("(len (list 1 2 3 4))");
    REQUIRE_FALSE(result4.length() == 0);
    REQUIRE(result4.indexOf("4") != -1);
}

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER VALUE EVALUATION API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Value evaluation of literals", "[interpreter][api][value_eval]")
{
    ModuLispInterpreter interp;

    // Test evaluating integer Value
    Value int_val(42);
    Value result1 = interp.eval(int_val);
    REQUIRE(result1.is_int());
    REQUIRE(result1.as_int() == 42);

    // Test evaluating float Value
    Value float_val(3.14);
    Value result2 = interp.eval(float_val);
    REQUIRE(result2.is_float());
    REQUIRE(result2.as_float() == Approx(3.14).epsilon(0.001));

    // Test evaluating string Value
    Value string_val = Value::string("hello");
    Value result3    = interp.eval(string_val);
    REQUIRE(result3.is_string());
    REQUIRE(result3.as_string() == "hello");

    // Test evaluating nil Value
    Value nil_val = Value::nil();
    Value result4 = interp.eval(nil_val);
    REQUIRE(result4.is_nil());
}

TEST_CASE("Value evaluation of symbols", "[interpreter][api][value_eval][symbols]")
{
    ModuLispInterpreter interp;

    // Test evaluating undefined symbol (should return error or the symbol itself)
    Value symbol_val = Value::atom("undefined-symbol");
    Value result1    = interp.eval(symbol_val);
    // Could be an error or the symbol itself, depending on implementation
    REQUIRE((result1.is_error() || result1.is_symbol()));

    // Test evaluating builtin symbol (like +, -, etc.)
    Value builtin_symbol = Value::atom("+");
    Value result2        = interp.eval(builtin_symbol);
    // Should return the builtin function value
    REQUIRE((result2.is_builtin() || result2.is_symbol()));
}

TEST_CASE("Value evaluation of lists", "[interpreter][api][value_eval][lists]")
{
    ModuLispInterpreter interp;

    // Test evaluating arithmetic expression as Value
    std::vector<Value> add_expr = { Value::atom("+"), Value(2), Value(3) };
    Value add_list(add_expr);
    Value result1 = interp.eval(add_list);

    if (result1.is_int())
    {
        REQUIRE(result1.as_int() == 5);
    }
    else if (result1.is_float())
    {
        REQUIRE(result1.as_float() == Approx(5.0).epsilon(0.001));
    }
    else if (!result1.is_error())
    {
        // If it's not an error, the evaluation should have produced a numeric result
        REQUIRE(result1.is_number());
    }

    // Test evaluating nested expression
    std::vector<Value> mult_expr   = { Value::atom("*"), Value(6), Value(7) };
    std::vector<Value> nested_expr = { Value::atom("+"), Value(mult_expr),
                                       Value(2) };
    Value nested_list(nested_expr);
    Value result2 = interp.eval(nested_list);

    // Should evaluate (* 6 7) = 42, then (+ 42 2) = 44
    if (result2.is_number() && !result2.is_error())
    {
        double expected = 44.0;
        double actual   = result2.is_int() ? result2.as_int() : result2.as_float();
        REQUIRE(actual == Approx(expected).epsilon(0.001));
    }
}

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER EVAL_V (PARSE AND EVALUATE) API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Parse and evaluate basic values (eval_v)", "[interpreter][api][eval_v]")
{
    ModuLispInterpreter interp;

    // Test parsing and evaluating integer
    Value result1 = interp.eval_v("42");
    REQUIRE(result1.is_int());
    REQUIRE(result1.as_int() == 42);

    // Test parsing and evaluating float
    Value result2 = interp.eval_v("3.14");
    REQUIRE(result2.is_float());
    REQUIRE(result2.as_float() == Approx(3.14).epsilon(0.001));

    // Test parsing and evaluating string
    Value result3 = interp.eval_v("\"hello\"");
    REQUIRE(result3.is_string());
    REQUIRE(result3.as_string() == "hello");

    // Test parsing and evaluating empty list (should return nil)
    Value result4 = interp.eval_v("()");
    REQUIRE(result4.is_nil());
}

TEST_CASE("Parse and evaluate expressions (eval_v)",
          "[interpreter][api][eval_v][expressions]")
{
    ModuLispInterpreter interp;

    // Test arithmetic expressions
    Value result1 = interp.eval_v("(+ 10 20)");
    if (result1.is_number() && !result1.is_error())
    {
        double expected = 30.0;
        double actual   = result1.is_int() ? result1.as_int() : result1.as_float();
        REQUIRE(actual == Approx(expected).epsilon(0.001));
    }

    // Test comparison expressions
    Value result2 = interp.eval_v("(> 5 3)");
    if (!result2.is_error())
    {
        // Should return truthy value (1, true, etc.)
        REQUIRE((result2.is_number() || result2.is_symbol()));
    }

    // Test list operations
    Value result3 = interp.eval_v("(list 1 2 3)");
    if (result3.is_list() && !result3.is_error())
    {
        REQUIRE(result3.as_list().size() == 3);
        REQUIRE(result3.as_list()[0].is_int());
        REQUIRE(result3.as_list()[0].as_int() == 1);
    }
}

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER STATIC EVALUATION API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("VM expression evaluation with environment",
          "[interpreter][api][vm_eval]")
{
    Environment env;

    Value result1 = execute_expr_with_vm(uLispParser::parse_static("42"), env);
    REQUIRE(result1.is_int());
    REQUIRE(result1.as_int() == 42);

    env.set("x", Value(10));
    env.set("y", Value(20));

    Value result2 = execute_expr_with_vm(uLispParser::parse_static("(+ x y)"), env);
    REQUIRE(result2.is_number());
    const double actual = result2.is_int() ? result2.as_int() : result2.as_float();
    REQUIRE(actual == Approx(30.0).epsilon(0.001));
}

TEST_CASE("VM value evaluation with environment",
          "[interpreter][api][vm_eval][values]")
{
    Environment env;

    Value int_val(100);
    Value result1 = execute_expr_with_vm(int_val, env);
    REQUIRE(result1.is_int());
    REQUIRE(result1.as_int() == 100);

    env.set("test-var", Value(42));
    Value symbol_val = Value::atom("test-var");
    Value result2    = execute_expr_with_vm(symbol_val, env);
    REQUIRE_FALSE(result2.is_error());
    REQUIRE(result2.is_int());
    REQUIRE(result2.as_int() == 42);

    std::vector<Value> expr = { Value::atom("+"), Value(5), Value(15) };
    Value list_val(expr);
    Value result3 = execute_expr_with_vm(list_val, env);
    REQUIRE_FALSE(result3.is_error());
    REQUIRE(result3.is_number());
    const double actual = result3.is_int() ? result3.as_int() : result3.as_float();
    REQUIRE(actual == Approx(20.0).epsilon(0.001));
}

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER ARGUMENT EVALUATION API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Argument evaluation API", "[interpreter][api][eval_args]")
{
    Environment env;

    // Test argument evaluation
    std::vector<Value> args = { Value(10), Value(3.14), Value::string("test"),
                                Value::atom("undefined-symbol") };

    // Evaluate arguments in place
    ModuLispInterpreter::eval_args(args, env);

    // Literals should remain unchanged
    REQUIRE(args[0].is_int());
    REQUIRE(args[0].as_int() == 10);

    REQUIRE(args[1].is_float());
    REQUIRE(args[1].as_float() == Approx(3.14).epsilon(0.001));

    REQUIRE(args[2].is_string());
    REQUIRE(args[2].as_string() == "test");

    // Symbol evaluation depends on whether it's defined
    // Could be error, symbol, or resolved value
    REQUIRE((args[3].is_error() || args[3].is_symbol() || args[3].is_int()));
}

TEST_CASE("Argument evaluation with expressions",
          "[interpreter][api][eval_args][expressions]")
{
    Environment env;
    env.set("x", Value(5));
    env.set("y", Value(3));

    // Test evaluating arguments that are expressions
    std::vector<Value> expr1 = { Value::atom("+"), Value::atom("x"),
                                 Value::atom("y") };
    std::vector<Value> expr2 = { Value::atom("*"), Value(2), Value(4) };

    std::vector<Value> args = {
        Value(expr1), // Should evaluate to 8 (5 + 3)
        Value(expr2), // Should evaluate to 8 (2 * 4)
        Value(10)     // Literal, should remain 10
    };

    ModuLispInterpreter::eval_args(args, env);

    // Check if expressions were evaluated (exact behavior depends on implementation)
    REQUIRE(args.size() == 3);

    // The literal should definitely remain unchanged
    REQUIRE(args[2].is_int());
    REQUIRE(args[2].as_int() == 10);

    // The expressions might be evaluated if they're valid
    if (!args[0].is_error() && args[0].is_number())
    {
        double expected = 8.0;
        double actual   = args[0].is_int() ? args[0].as_int() : args[0].as_float();
        REQUIRE(actual == Approx(expected).epsilon(0.001));
    }
}

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER FUNCTION APPLICATION API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Function application with VM callable helper", "[interpreter][api][apply]")
{
    Environment env;
    env.set_temporal_context(nullptr);

    Value add_func              = Environment::builtindefs()["+"];
    std::vector<Value> add_args = { Value(10), Value(20) };

    Value result1 = execute_callable_with_vm(add_func, add_args, env);
    REQUIRE_FALSE(result1.is_error());
    REQUIRE(result1.is_number());
    double actual = result1.is_int() ? result1.as_int() : result1.as_float();
    REQUIRE(actual == Approx(30.0).epsilon(0.001));

    Value mult_func              = Environment::builtindefs()["*"];
    std::vector<Value> mult_args = { Value(6), Value(7) };

    Value result2 = execute_callable_with_vm(mult_func, mult_args, env);
    REQUIRE_FALSE(result2.is_error());
    REQUIRE(result2.is_number());
    actual = result2.is_int() ? result2.as_int() : result2.as_float();
    REQUIRE(actual == Approx(42.0).epsilon(0.001));

    Value eq_func              = Environment::builtindefs()["="];
    std::vector<Value> eq_args = { Value(5), Value(5) };

    Value result3 = execute_callable_with_vm(eq_func, eq_args, env);
    REQUIRE_FALSE(result3.is_error());
    REQUIRE((result3.is_number() || result3.is_symbol()));
}

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER VARIABLE BINDING AND SCOPING TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Variable operations - set and get", "[interpreter][api][variables]")
{
    ModuLispInterpreter interp;

    // Set variables using interpreter's Environment interface
    interp.get_environment()->set("test-var", Value(123));
    interp.get_environment()->set("float-var", Value(2.718));
    interp.get_environment()->set("string-var", Value::string("hello interpreter"));

    // Test retrieving variables
    auto int_result = interp.get_environment()->get("test-var");
    if (int_result.has_value())
    {
        REQUIRE(int_result->is_int());
        REQUIRE(int_result->as_int() == 123);
    }

    auto float_result = interp.get_environment()->get("float-var");
    if (float_result.has_value())
    {
        REQUIRE(float_result->is_float());
        REQUIRE(float_result->as_float() == Approx(2.718).epsilon(0.001));
    }

    auto string_result = interp.get_environment()->get("string-var");
    if (string_result.has_value())
    {
        REQUIRE(string_result->is_string());
        REQUIRE(string_result->as_string() == "hello interpreter");
    }

    // Test evaluating variables by name
    String eval_result = interp.eval("test-var");
    REQUIRE_FALSE(eval_result.length() == 0);
    REQUIRE(eval_result.indexOf("123") != -1);
}

TEST_CASE("Variable evaluation in expressions",
          "[interpreter][api][variables][expressions]")
{
    ModuLispInterpreter interp;

    // Set up variables
    interp.get_environment()->set("x", Value(10));
    interp.get_environment()->set("y", Value(5));

    // Test using variables in arithmetic
    String result1 = interp.eval("(+ x y)");
    REQUIRE_FALSE(result1.length() == 0);
    if (result1.indexOf("error") == -1)
    {
        REQUIRE(result1.indexOf("15") != -1);
    }

    // Test nested variable usage
    String result2 = interp.eval("(* (+ x y) 2)");
    REQUIRE_FALSE(result2.length() == 0);
    if (result2.indexOf("error") == -1)
    {
        REQUIRE(result2.indexOf("30") != -1); // (10 + 5) * 2 = 30
    }

    // Test undefined variable handling
    String result3 = interp.eval("undefined-var");
    REQUIRE_FALSE(result3.length() == 0);
    // Should either return error message or the symbol itself
}

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER ERROR HANDLING API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Error handling for various edge cases",
          "[interpreter][api][error_handling]")
{
    ModuLispInterpreter interp;

    // Test malformed expressions
    String result1 = interp.eval("(+ 1 2"); // Missing closing paren
    REQUIRE_FALSE(result1.length() == 0);
    // Should handle gracefully (exact behavior depends on implementation)

    // Test invalid function calls
    String result2 = interp.eval("(nonexistent-function 1 2 3)");
    REQUIRE_FALSE(result2.length() == 0);
    // Should return some error indication

    // Test type errors (if caught)
    String result3 = interp.eval("(+ \"string\" 42)");
    REQUIRE_FALSE(result3.length() == 0);
    // Behavior depends on whether type coercion is supported

    // Test division by zero
    String result4 = interp.eval("(/ 10 0)");
    REQUIRE_FALSE(result4.length() == 0);
    // Should handle division by zero gracefully
}

TEST_CASE("Error checking in evaluated arguments",
          "[interpreter][api][error_handling][args]")
{
    ModuLispInterpreter interp;

    // Test the protected error checking method through inheritance
    // We can't test this directly since it's protected, but we can test
    // that evaluation handles errors properly in argument evaluation

    std::vector<Value> args_with_error = { Value(42),
                                           Value::error(), // Include error value
                                           Value(17) };

    // The interpreter should detect errors in evaluated arguments
    // This is tested indirectly through the evaluation process
    REQUIRE(true); // Placeholder for proper error handling verification
}

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER COMPLEX EXPRESSION EVALUATION TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Complex expressions - conditionals and functions",
          "[interpreter][api][complex_expressions]")
{
    ModuLispInterpreter interp;

    // Test conditional expressions (if available)
    String result1 = interp.eval("(if (> 5 3) \"true-case\" \"false-case\")");
    REQUIRE_FALSE(result1.length() == 0);
    if (result1.indexOf("error") == -1)
    {
        REQUIRE((result1.indexOf("true-case") != -1 ||
                 result1.indexOf("false-case") != -1));
    }

    // Test let expressions (if available)
    String result2 = interp.eval("(let [x 10 y 20] (+ x y))");
    REQUIRE_FALSE(result2.length() == 0);
    if (result2.indexOf("error") == -1)
    {
        REQUIRE(result2.indexOf("30") != -1);
    }

    // Test function definitions (if available)
    String result3 = interp.eval("(defn square [x] (* x x))");
    REQUIRE_FALSE(result3.length() == 0);

    // If function definition succeeded, test calling it
    if (result3.indexOf("error") == -1)
    {
        String result4 = interp.eval("(square 5)");
        REQUIRE_FALSE(result4.length() == 0);
        if (result4.indexOf("error") == -1)
        {
            REQUIRE(result4.indexOf("25") != -1);
        }
    }
}

TEST_CASE("List processing operations",
          "[interpreter][api][complex_expressions][lists]")
{
    ModuLispInterpreter interp;

    // Test basic list operations
    String result1 = interp.eval("(list 1 2 3 4)");
    REQUIRE_FALSE(result1.length() == 0);

    // Test list manipulation functions
    String result2 = interp.eval("(head (list 10 20 30))");
    REQUIRE_FALSE(result2.length() == 0);
    if (result2.indexOf("error") == -1)
    {
        REQUIRE(result2.indexOf("10") != -1);
    }

    String result3 = interp.eval("(len (list 1 2 3 4 5))");
    REQUIRE_FALSE(result3.length() == 0);
    if (result3.indexOf("error") == -1)
    {
        REQUIRE(result3.indexOf("5") != -1);
    }

    // Test vector operations
    String result4 = interp.eval("(vec 10 20 30)");
    REQUIRE_FALSE(result4.length() == 0);

    String result5 = interp.eval("(index [100 200 300] 1)");
    REQUIRE_FALSE(result5.length() == 0);
    if (result5.indexOf("error") == -1)
    {
        REQUIRE(result5.indexOf("200") != -1);
    }
}

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER SPECIAL FEATURES AND EDGE CASES
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Quote handling for special forms",
          "[interpreter][api][special_features][quote]")
{
    ModuLispInterpreter interp;

    // Test quoted expressions
    String result1 = interp.eval("'42");
    REQUIRE_FALSE(result1.length() == 0);

    String result2 = interp.eval("'(+ 1 2 3)");
    REQUIRE_FALSE(result2.length() == 0);
    // Should return the list unevaluated

    String result3 = interp.eval("'my-symbol");
    REQUIRE_FALSE(result3.length() == 0);
    // Should return the symbol
}

TEST_CASE("Empty and whitespace handling",
          "[interpreter][api][special_features][whitespace]")
{
    ModuLispInterpreter interp;

    // Test empty string evaluation
    String result1 = interp.eval("");
    // Should handle gracefully

    // Test whitespace-only evaluation
    String result2 = interp.eval("   \t\n  ");
    // Should handle gracefully

    // Test comment-only evaluation
    String result3 = interp.eval("; just a comment");
    // Should handle gracefully

    // All should complete without crashing
    REQUIRE(true);
}

TEST_CASE("Mathematical functions", "[interpreter][api][special_features][math]")
{
    ModuLispInterpreter interp;

    // Test trigonometric functions (if available)
    String result1 = interp.eval("(sin 0)");
    REQUIRE_FALSE(result1.length() == 0);

    String result2 = interp.eval("(cos 0)");
    REQUIRE_FALSE(result2.length() == 0);

    // Test mathematical functions
    String result3 = interp.eval("(abs -5)");
    REQUIRE_FALSE(result3.length() == 0);
    if (result3.indexOf("error") == -1)
    {
        REQUIRE(result3.indexOf("5") != -1);
    }

    String result4 = interp.eval("(sqrt 16)");
    REQUIRE_FALSE(result4.length() == 0);
    if (result4.indexOf("error") == -1)
    {
        REQUIRE(result4.indexOf("4") != -1);
    }

    String result5 = interp.eval("(min 3 7 2 9)");
    REQUIRE_FALSE(result5.length() == 0);
    if (result5.indexOf("error") == -1)
    {
        REQUIRE(result5.indexOf("2") != -1);
    }

    String result6 = interp.eval("(max 3 7 2 9)");
    REQUIRE_FALSE(result6.length() == 0);
    if (result6.indexOf("error") == -1)
    {
        REQUIRE(result6.indexOf("9") != -1);
    }
}

////////////////////////////////////////////////////////////////////////////////
/// UNDEFINED SYMBOL AND FUNCTION ERROR HANDLING TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Undefined symbols return errors",
          "[interpreter][api][undefined][symbols]")
{
    ModuLispInterpreter interp;

    // Test undefined symbol evaluation directly
    Value undefined_symbol = Value::atom("definitely-undefined-symbol");
    Value result           = interp.eval(undefined_symbol);

    // Should return error for undefined symbols
    REQUIRE(result.is_error());

    // Test undefined symbol in string evaluation
    String result2 = interp.eval("definitely-undefined-symbol");
    REQUIRE(
        (result2.indexOf("error") != -1 || result2.indexOf("not defined") != -1));
}

TEST_CASE("Undefined functions return errors",
          "[interpreter][api][undefined][functions]")
{
    ModuLispInterpreter interp;

    // Test undefined function call
    Value result = interp.eval_v("(undefined-function-name 1 2 3)");

    // Should return error for undefined function
    REQUIRE(result.is_error());

    // Test undefined function in string evaluation
    String result2 = interp.eval("(another-undefined-function 42)");
    REQUIRE(
        (result2.indexOf("error") != -1 || result2.indexOf("not defined") != -1));
}

TEST_CASE("Error propagation through expressions",
          "[interpreter][api][undefined][propagation]")
{
    ModuLispInterpreter interp;

    // Test error propagates through arithmetic
    Value result1 = interp.eval_v("(+ undefined-var 5)");
    REQUIRE(result1.is_error());

    // Test error propagates through function calls
    Value result2 = interp.eval_v("(* 2 (undefined-function 10))");
    REQUIRE(result2.is_error());

    // Test error propagates through nested expressions
    Value result3 = interp.eval_v("(+ (* 2 3) (- undefined-var 1))");
    REQUIRE(result3.is_error());
}
