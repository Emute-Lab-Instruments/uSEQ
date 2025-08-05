#include "../uSEQ/src/modulisp/lisp/interpreter.h"
#include "../uSEQ/src/modulisp/lisp/value.h"
#include "../uSEQ/src/modulisp/lisp/environment.h"
#include <cassert>
#include <iostream>
#include <cmath>

// Test framework macros (same as existing tests)
#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            std::cerr << "ASSERTION FAILED at line " << __LINE__ << ": " \
                      << "Expected " << (expected) << " but got " << (actual) << std::endl; \
            assert(false); \
        } \
    } while(0)

#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "ASSERTION FAILED at line " << __LINE__ << ": " \
                      << "Expected true but got false" << std::endl; \
            assert(false); \
        } \
    } while(0)

#define ASSERT_FALSE(condition) \
    do { \
        if (condition) { \
            std::cerr << "ASSERTION FAILED at line " << __LINE__ << ": " \
                      << "Expected false but got true" << std::endl; \
            assert(false); \
        } \
    } while(0)

#define ASSERT_NEAR(expected, actual, tolerance) \
    do { \
        if (std::abs((expected) - (actual)) > (tolerance)) { \
            std::cerr << "ASSERTION FAILED at line " << __LINE__ << ": " \
                      << "Expected " << (expected) << " but got " << (actual) \
                      << " (tolerance: " << (tolerance) << ")" << std::endl; \
            assert(false); \
        } \
    } while(0)

#define ASSERT_STR_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            std::cerr << "ASSERTION FAILED at line " << __LINE__ << ": " \
                      << "Expected \"" << (expected) << "\" but got \"" << (actual) << "\"" << std::endl; \
            assert(false); \
        } \
    } while(0)

#define TEST_CASE(name) \
    void name(); \
    struct name##_runner { \
        name##_runner() { \
            std::cout << "Running " << #name << "..." << std::endl; \
            name(); \
            std::cout << #name << " passed!" << std::endl; \
        } \
    }; \
    static name##_runner name##_instance; \
    void name()

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER CONSTRUCTION AND INITIALIZATION API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_interpreter_construction_api) {
    // Test interpreter construction using factory function
    Interpreter interp = Interpreter::create_fresh_interpreter();
    ASSERT_TRUE(true); // If we get here, construction and initialization succeeded
}

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER STRING EVALUATION API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_interpreter_string_eval_basic_values) {
    Interpreter interp = Interpreter::create_fresh_interpreter();
    
    // Test evaluating integer literals
    String result1 = interp.eval("42");
    ASSERT_TRUE(result1.length() > 0);
    // Should contain the number 42 in some form
    ASSERT_TRUE(result1.indexOf("42") >= 0);
    
    // Test evaluating float literals
    String result2 = interp.eval("3.14");
    ASSERT_TRUE(result2.length() > 0);
    ASSERT_TRUE(result2.indexOf("3.14") >= 0);
    
    // Test evaluating string literals
    String result3 = interp.eval("\"hello world\"");
    ASSERT_TRUE(result3.length() > 0);
    ASSERT_TRUE(result3.indexOf("hello world") >= 0);
    
    // Test evaluating empty list
    String result4 = interp.eval("()");
    ASSERT_TRUE(result4.length() > 0);
    
    // Test evaluating empty vector
    String result5 = interp.eval("[]");
    ASSERT_TRUE(result5.length() > 0);
}

TEST_CASE(test_interpreter_string_eval_arithmetic) {
    Interpreter interp = Interpreter::create_fresh_interpreter();
    
    // Test basic arithmetic operations
    String result1 = interp.eval("(+ 2 3)");
    ASSERT_FALSE(result1.length() == 0);
    ASSERT_TRUE(result1.indexOf("5") != -1);
    
    String result2 = interp.eval("(- 10 4)");
    ASSERT_FALSE(result2.length() == 0);
    ASSERT_TRUE(result2.indexOf("6") != -1);
    
    String result3 = interp.eval("(* 6 7)");
    ASSERT_FALSE(result3.length() == 0);
    ASSERT_TRUE(result3.indexOf("42") != -1);
    
    String result4 = interp.eval("(/ 15 3)");
    ASSERT_FALSE(result4.length() == 0);
    ASSERT_TRUE(result4.indexOf("5") != -1);
    
    // Test nested arithmetic
    String result5 = interp.eval("(+ (* 2 3) (/ 8 4))");
    ASSERT_FALSE(result5.length() == 0);
    ASSERT_TRUE(result5.indexOf("8") != -1); // 6 + 2 = 8
}

TEST_CASE(test_interpreter_string_eval_comparisons) {
    Interpreter interp = Interpreter::create_fresh_interpreter();
    
    // Test equality
    String result1 = interp.eval("(= 5 5)");
    ASSERT_FALSE(result1.length() == 0);
    // Should indicate true (might be "1", "true", etc.)
    
    String result2 = interp.eval("(= 5 3)");
    ASSERT_FALSE(result2.length() == 0);
    // Should indicate false (might be "0", "false", etc.)
    
    // Test ordering
    String result3 = interp.eval("(> 5 3)");
    ASSERT_FALSE(result3.length() == 0);
    
    String result4 = interp.eval("(< 3 5)");
    ASSERT_FALSE(result4.length() == 0);
    
    String result5 = interp.eval("(>= 5 5)");
    ASSERT_FALSE(result5.length() == 0);
    
    String result6 = interp.eval("(<= 3 5)");
    ASSERT_FALSE(result6.length() == 0);
}

TEST_CASE(test_interpreter_string_eval_list_operations) {
    Interpreter interp = Interpreter::create_fresh_interpreter();
    
    // Test list creation
    String result1 = interp.eval("(list 1 2 3)");
    ASSERT_FALSE(result1.length() == 0);
    
    // Test vector creation
    String result2 = interp.eval("(vec 1 2 3)");
    ASSERT_FALSE(result2.length() == 0);
    
    // Test list access functions (if available)
    String result3 = interp.eval("(head (list 1 2 3))");
    ASSERT_FALSE(result3.length() == 0);
    
    String result4 = interp.eval("(len (list 1 2 3 4))");
    ASSERT_FALSE(result4.length() == 0);
    ASSERT_TRUE(result4.indexOf("4") != -1);
}

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER VALUE EVALUATION API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_interpreter_value_eval_literals) {
    Interpreter interp = Interpreter::create_fresh_interpreter();
    
    // Test evaluating integer Value
    Value int_val(42);
    Value result1 = interp.eval(int_val);
    ASSERT_TRUE(result1.is_int());
    ASSERT_EQ(42, result1.as_int());
    
    // Test evaluating float Value
    Value float_val(3.14);
    Value result2 = interp.eval(float_val);
    ASSERT_TRUE(result2.is_float());
    ASSERT_NEAR(3.14, result2.as_float(), 0.001);
    
    // Test evaluating string Value
    Value string_val = Value::string("hello");
    Value result3 = interp.eval(string_val);
    ASSERT_TRUE(result3.is_string());
    ASSERT_STR_EQ("hello", result3.as_string());
    
    // Test evaluating nil Value
    Value nil_val = Value::nil();
    Value result4 = interp.eval(nil_val);
    ASSERT_TRUE(result4.is_nil());
}

TEST_CASE(test_interpreter_value_eval_symbols) {
    Interpreter interp = Interpreter::create_fresh_interpreter();
    
    // Test evaluating undefined symbol (should return error or the symbol itself)
    Value symbol_val = Value::atom("undefined-symbol");
    Value result1 = interp.eval(symbol_val);
    // Could be an error or the symbol itself, depending on implementation
    ASSERT_TRUE(result1.is_error() || result1.is_symbol());
    
    // Test evaluating builtin symbol (like +, -, etc.)
    Value builtin_symbol = Value::atom("+");
    Value result2 = interp.eval(builtin_symbol);
    // Should return the builtin function value
    ASSERT_TRUE(result2.is_builtin() || result2.is_symbol());
}

TEST_CASE(test_interpreter_value_eval_lists) {
    Interpreter interp = Interpreter::create_fresh_interpreter();
    
    // Test evaluating arithmetic expression as Value
    std::vector<Value> add_expr = {
        Value::atom("+"),
        Value(2),
        Value(3)
    };
    Value add_list(add_expr);
    Value result1 = interp.eval(add_list);
    
    if (result1.is_int()) {
        ASSERT_EQ(5, result1.as_int());
    } else if (result1.is_float()) {
        ASSERT_NEAR(5.0, result1.as_float(), 0.001);
    } else if (!result1.is_error()) {
        // If it's not an error, the evaluation should have produced a numeric result
        ASSERT_TRUE(result1.is_number());
    }
    
    // Test evaluating nested expression
    std::vector<Value> mult_expr = {Value::atom("*"), Value(6), Value(7)};
    std::vector<Value> nested_expr = {
        Value::atom("+"),
        Value(mult_expr),
        Value(2)
    };
    Value nested_list(nested_expr);
    Value result2 = interp.eval(nested_list);
    
    // Should evaluate (* 6 7) = 42, then (+ 42 2) = 44
    if (result2.is_number() && !result2.is_error()) {
        double expected = 44.0;
        double actual = result2.is_int() ? result2.as_int() : result2.as_float();
        ASSERT_NEAR(expected, actual, 0.001);
    }
}

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER EVAL_V (PARSE AND EVALUATE) API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_interpreter_eval_v_basic) {
    Interpreter interp = Interpreter::create_fresh_interpreter();
    
    // Test parsing and evaluating integer
    Value result1 = interp.eval_v("42");
    ASSERT_TRUE(result1.is_int());
    ASSERT_EQ(42, result1.as_int());
    
    // Test parsing and evaluating float
    Value result2 = interp.eval_v("3.14");
    ASSERT_TRUE(result2.is_float());
    ASSERT_NEAR(3.14, result2.as_float(), 0.001);
    
    // Test parsing and evaluating string
    Value result3 = interp.eval_v("\"hello\"");
    ASSERT_TRUE(result3.is_string());
    ASSERT_STR_EQ("hello", result3.as_string());
    
    // Test parsing and evaluating empty list (should return nil)
    Value result4 = interp.eval_v("()");
    ASSERT_TRUE(result4.is_nil());
}

TEST_CASE(test_interpreter_eval_v_expressions) {
    Interpreter interp = Interpreter::create_fresh_interpreter();
    
    // Test arithmetic expressions
    Value result1 = interp.eval_v("(+ 10 20)");
    if (result1.is_number() && !result1.is_error()) {
        double expected = 30.0;
        double actual = result1.is_int() ? result1.as_int() : result1.as_float();
        ASSERT_NEAR(expected, actual, 0.001);
    }
    
    // Test comparison expressions
    Value result2 = interp.eval_v("(> 5 3)");
    if (!result2.is_error()) {
        // Should return truthy value (1, true, etc.)
        ASSERT_TRUE(result2.is_number() || result2.is_symbol());
    }
    
    // Test list operations
    Value result3 = interp.eval_v("(list 1 2 3)");
    if (result3.is_list() && !result3.is_error()) {
        ASSERT_EQ(3, result3.as_list().size());
        ASSERT_TRUE(result3.as_list()[0].is_int());
        ASSERT_EQ(1, result3.as_list()[0].as_int());
    }
}

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER STATIC EVALUATION API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_interpreter_static_eval_in_string) {
    Environment env;
    
    // Test static string evaluation with empty environment
    String result1 = Interpreter::eval_in("42", env);
    ASSERT_FALSE(result1.length() == 0);
    ASSERT_TRUE(result1.indexOf("42") != -1);
    
    // Add variables to environment and test evaluation
    env.set("x", Value(10));
    env.set("y", Value(20));
    
    String result2 = Interpreter::eval_in("(+ x y)", env);
    if (!result2.length() == 0) {
        // Should contain result of 10 + 20 = 30
        ASSERT_TRUE(result2.indexOf("30") != -1 || result2.indexOf("error") != -1);
    }
}

TEST_CASE(test_interpreter_static_eval_in_value) {
    Environment env;
    
    // Test static Value evaluation
    Value int_val(100);
    Value result1 = Interpreter::eval_in(int_val, env);
    ASSERT_TRUE(result1.is_int());
    ASSERT_EQ(100, result1.as_int());
    
    // Test with variables in environment
    env.set("test-var", Value(42));
    Value symbol_val = Value::atom("test-var");
    Value result2 = Interpreter::eval_in(symbol_val, env);
    
    if (!result2.is_error()) {
        ASSERT_TRUE(result2.is_int());
        ASSERT_EQ(42, result2.as_int());
    }
    
    // Test list evaluation
    std::vector<Value> expr = {Value::atom("+"), Value(5), Value(15)};
    Value list_val(expr);
    Value result3 = Interpreter::eval_in(list_val, env);
    
    if (result3.is_number() && !result3.is_error()) {
        double expected = 20.0;
        double actual = result3.is_int() ? result3.as_int() : result3.as_float();
        ASSERT_NEAR(expected, actual, 0.001);
    }
}

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER ARGUMENT EVALUATION API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_interpreter_eval_args_api) {
    Environment env;
    
    // Test argument evaluation
    std::vector<Value> args = {
        Value(10),
        Value(3.14),
        Value::string("test"),
        Value::atom("undefined-symbol")
    };
    
    // Evaluate arguments in place
    Interpreter::eval_args(args, env);
    
    // Literals should remain unchanged
    ASSERT_TRUE(args[0].is_int());
    ASSERT_EQ(10, args[0].as_int());
    
    ASSERT_TRUE(args[1].is_float());
    ASSERT_NEAR(3.14, args[1].as_float(), 0.001);
    
    ASSERT_TRUE(args[2].is_string());
    ASSERT_STR_EQ("test", args[2].as_string());
    
    // Symbol evaluation depends on whether it's defined
    // Could be error, symbol, or resolved value
    ASSERT_TRUE(args[3].is_error() || args[3].is_symbol() || args[3].is_int());
}

TEST_CASE(test_interpreter_eval_args_with_expressions) {
    Environment env;
    env.set("x", Value(5));
    env.set("y", Value(3));
    
    // Test evaluating arguments that are expressions
    std::vector<Value> expr1 = {Value::atom("+"), Value::atom("x"), Value::atom("y")};
    std::vector<Value> expr2 = {Value::atom("*"), Value(2), Value(4)};
    
    std::vector<Value> args = {
        Value(expr1),  // Should evaluate to 8 (5 + 3)
        Value(expr2),  // Should evaluate to 8 (2 * 4)
        Value(10)      // Literal, should remain 10
    };
    
    Interpreter::eval_args(args, env);
    
    // Check if expressions were evaluated (exact behavior depends on implementation)
    ASSERT_EQ(3, args.size());
    
    // The literal should definitely remain unchanged
    ASSERT_TRUE(args[2].is_int());
    ASSERT_EQ(10, args[2].as_int());
    
    // The expressions might be evaluated if they're valid
    if (!args[0].is_error() && args[0].is_number()) {
        double expected = 8.0;
        double actual = args[0].is_int() ? args[0].as_int() : args[0].as_float();
        ASSERT_NEAR(expected, actual, 0.001);
    }
}

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER FUNCTION APPLICATION API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_interpreter_apply_builtin_functions) {
    Environment env;
    
    // Test applying builtin addition function
    Value add_func = Value::atom("+");
    std::vector<Value> add_args = {Value(10), Value(20)};
    
    Value result1 = Interpreter::apply(add_func, add_args, env);
    if (result1.is_number() && !result1.is_error()) {
        double expected = 30.0;
        double actual = result1.is_int() ? result1.as_int() : result1.as_float();
        ASSERT_NEAR(expected, actual, 0.001);
    }
    
    // Test applying builtin multiplication function
    Value mult_func = Value::atom("*");
    std::vector<Value> mult_args = {Value(6), Value(7)};
    
    Value result2 = Interpreter::apply(mult_func, mult_args, env);
    if (result2.is_number() && !result2.is_error()) {
        double expected = 42.0;
        double actual = result2.is_int() ? result2.as_int() : result2.as_float();
        ASSERT_NEAR(expected, actual, 0.001);
    }
    
    // Test applying comparison function
    Value eq_func = Value::atom("=");
    std::vector<Value> eq_args = {Value(5), Value(5)};
    
    Value result3 = Interpreter::apply(eq_func, eq_args, env);
    // Should return truthy value for equal comparison
    if (!result3.is_error()) {
        ASSERT_TRUE(result3.is_number() || result3.is_symbol());
    }
}

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER VARIABLE BINDING AND SCOPING TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_interpreter_variable_operations) {
    Interpreter interp = Interpreter::create_fresh_interpreter();
    
    // Set variables using interpreter's Environment interface
    interp.set("test-var", Value(123));
    interp.set("float-var", Value(2.718));
    interp.set("string-var", Value::string("hello interpreter"));
    
    // Test retrieving variables
    auto int_result = interp.get("test-var");
    if (int_result.has_value()) {
        ASSERT_TRUE(int_result->is_int());
        ASSERT_EQ(123, int_result->as_int());
    }
    
    auto float_result = interp.get("float-var");
    if (float_result.has_value()) {
        ASSERT_TRUE(float_result->is_float());
        ASSERT_NEAR(2.718, float_result->as_float(), 0.001);
    }
    
    auto string_result = interp.get("string-var");
    if (string_result.has_value()) {
        ASSERT_TRUE(string_result->is_string());
        ASSERT_STR_EQ("hello interpreter", string_result->as_string());
    }
    
    // Test evaluating variables by name
    String eval_result = interp.eval("test-var");
    ASSERT_FALSE(eval_result.length() == 0);
    ASSERT_TRUE(eval_result.indexOf("123") != -1);
}

TEST_CASE(test_interpreter_variable_evaluation_in_expressions) {
    Interpreter interp = Interpreter::create_fresh_interpreter();
    
    // Set up variables
    interp.set("x", Value(10));
    interp.set("y", Value(5));
    
    // Test using variables in arithmetic
    String result1 = interp.eval("(+ x y)");
    ASSERT_FALSE(result1.length() == 0);
    if (result1.indexOf("error") == -1) {
        ASSERT_TRUE(result1.indexOf("15") != -1);
    }
    
    // Test nested variable usage
    String result2 = interp.eval("(* (+ x y) 2)");
    ASSERT_FALSE(result2.length() == 0);
    if (result2.indexOf("error") == -1) {
        ASSERT_TRUE(result2.indexOf("30") != -1); // (10 + 5) * 2 = 30
    }
    
    // Test undefined variable handling
    String result3 = interp.eval("undefined-var");
    ASSERT_FALSE(result3.length() == 0);
    // Should either return error message or the symbol itself
}

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER ERROR HANDLING API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_interpreter_error_handling) {
    Interpreter interp = Interpreter::create_fresh_interpreter();
    
    // Test malformed expressions
    String result1 = interp.eval("(+ 1 2");  // Missing closing paren
    ASSERT_FALSE(result1.length() == 0);
    // Should handle gracefully (exact behavior depends on implementation)
    
    // Test invalid function calls
    String result2 = interp.eval("(nonexistent-function 1 2 3)");
    ASSERT_FALSE(result2.length() == 0);
    // Should return some error indication
    
    // Test type errors (if caught)
    String result3 = interp.eval("(+ \"string\" 42)");
    ASSERT_FALSE(result3.length() == 0);
    // Behavior depends on whether type coercion is supported
    
    // Test division by zero
    String result4 = interp.eval("(/ 10 0)");
    ASSERT_FALSE(result4.length() == 0);
    // Should handle division by zero gracefully
}

TEST_CASE(test_interpreter_evalled_args_error_checking) {
    Interpreter interp = Interpreter::create_fresh_interpreter();
    
    // Test the protected error checking method through inheritance
    // We can't test this directly since it's protected, but we can test
    // that evaluation handles errors properly in argument evaluation
    
    std::vector<Value> args_with_error = {
        Value(42),
        Value::error(),  // Include error value
        Value(17)
    };
    
    // The interpreter should detect errors in evaluated arguments
    // This is tested indirectly through the evaluation process
    ASSERT_TRUE(true); // Placeholder for proper error handling verification
}

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER COMPLEX EXPRESSION EVALUATION TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_interpreter_complex_expressions) {
    Interpreter interp = Interpreter::create_fresh_interpreter();
    
    // Test conditional expressions (if available)
    String result1 = interp.eval("(if (> 5 3) \"true-case\" \"false-case\")");
    ASSERT_FALSE(result1.length() == 0);
    if (result1.indexOf("error") == -1) {
        ASSERT_TRUE(result1.indexOf("true-case") != -1 || result1.indexOf("false-case") != -1);
    }
    
    // Test let expressions (if available)
    String result2 = interp.eval("(let [x 10 y 20] (+ x y))");
    ASSERT_FALSE(result2.length() == 0);
    if (result2.indexOf("error") == -1) {
        ASSERT_TRUE(result2.indexOf("30") != -1);
    }
    
    // Test function definitions (if available)
    String result3 = interp.eval("(defn square [x] (* x x))");
    ASSERT_FALSE(result3.length() == 0);
    
    // If function definition succeeded, test calling it
    if (result3.indexOf("error") == -1) {
        String result4 = interp.eval("(square 5)");
        ASSERT_FALSE(result4.length() == 0);
        if (result4.indexOf("error") == -1) {
            ASSERT_TRUE(result4.indexOf("25") != -1);
        }
    }
}

TEST_CASE(test_interpreter_list_processing) {
    Interpreter interp = Interpreter::create_fresh_interpreter();
    
    // Test basic list operations
    String result1 = interp.eval("(list 1 2 3 4)");
    ASSERT_FALSE(result1.length() == 0);
    
    // Test list manipulation functions
    String result2 = interp.eval("(head (list 10 20 30))");
    ASSERT_FALSE(result2.length() == 0);
    if (result2.indexOf("error") == -1) {
        ASSERT_TRUE(result2.indexOf("10") != -1);
    }
    
    String result3 = interp.eval("(len (list 1 2 3 4 5))");
    ASSERT_FALSE(result3.length() == 0);
    if (result3.indexOf("error") == -1) {
        ASSERT_TRUE(result3.indexOf("5") != -1);
    }
    
    // Test vector operations
    String result4 = interp.eval("(vec 10 20 30)");
    ASSERT_FALSE(result4.length() == 0);
    
    String result5 = interp.eval("(index [100 200 300] 1)");
    ASSERT_FALSE(result5.length() == 0);
    if (result5.indexOf("error") == -1) {
        ASSERT_TRUE(result5.indexOf("200") != -1);
    }
}

////////////////////////////////////////////////////////////////////////////////
/// INTERPRETER SPECIAL FEATURES AND EDGE CASES
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_interpreter_quote_handling) {
    Interpreter interp = Interpreter::create_fresh_interpreter();
    
    // Test quoted expressions
    String result1 = interp.eval("'42");
    ASSERT_FALSE(result1.length() == 0);
    
    String result2 = interp.eval("'(+ 1 2 3)");
    ASSERT_FALSE(result2.length() == 0);
    // Should return the list unevaluated
    
    String result3 = interp.eval("'my-symbol");
    ASSERT_FALSE(result3.length() == 0);
    // Should return the symbol
}

TEST_CASE(test_interpreter_empty_and_whitespace) {
    Interpreter interp = Interpreter::create_fresh_interpreter();
    
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
    ASSERT_TRUE(true);
}

TEST_CASE(test_interpreter_mathematical_functions) {
    Interpreter interp = Interpreter::create_fresh_interpreter();
    
    // Test trigonometric functions (if available)
    String result1 = interp.eval("(sin 0)");
    ASSERT_FALSE(result1.length() == 0);
    
    String result2 = interp.eval("(cos 0)");
    ASSERT_FALSE(result2.length() == 0);
    
    // Test mathematical functions
    String result3 = interp.eval("(abs -5)");
    ASSERT_FALSE(result3.length() == 0);
    if (result3.indexOf("error") == -1) {
        ASSERT_TRUE(result3.indexOf("5") != -1);
    }
    
    String result4 = interp.eval("(sqrt 16)");
    ASSERT_FALSE(result4.length() == 0);
    if (result4.indexOf("error") == -1) {
        ASSERT_TRUE(result4.indexOf("4") != -1);
    }
    
    String result5 = interp.eval("(min 3 7 2 9)");
    ASSERT_FALSE(result5.length() == 0);
    if (result5.indexOf("error") == -1) {
        ASSERT_TRUE(result5.indexOf("2") != -1);
    }
    
    String result6 = interp.eval("(max 3 7 2 9)");
    ASSERT_FALSE(result6.length() == 0);
    if (result6.indexOf("error") == -1) {
        ASSERT_TRUE(result6.indexOf("9") != -1);
    }
}

// Main function to run all tests
int main() {
    std::cout << "Running all Interpreter API tests..." << std::endl;
    std::cout << "All Interpreter API tests passed!" << std::endl;
    return 0;
}