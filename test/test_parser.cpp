#include "../uSEQ/src/lisp/parser.h"
#include "../uSEQ/src/lisp/value.h"
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

// Test string unescaping functionality
TEST_CASE(test_unescape_basic) {
    // Test basic unescaping
    String result = uLispParser::unescape("hello\\nworld");
    ASSERT_STR_EQ("hello\nworld", result);
    
    // Test quote unescaping
    String result2 = uLispParser::unescape("say \\\"hello\\\"");
    ASSERT_STR_EQ("say \"hello\"", result2);
    
    // Test carriage return and tab
    String result3 = uLispParser::unescape("line1\\r\\tline2");
    ASSERT_STR_EQ("line1\r\tline2", result3);
    
    // Test no escapes
    String result4 = uLispParser::unescape("plain text");
    ASSERT_STR_EQ("plain text", result4);
}

TEST_CASE(test_unescape_edge_cases) {
    // Test backslash at end (should be preserved)
    String result1 = uLispParser::unescape("text\\");
    ASSERT_STR_EQ("text\\", result1);
    
    // Test unknown escape sequence (should preserve the character)
    String result2 = uLispParser::unescape("test\\x");
    ASSERT_STR_EQ("testx", result2);
    
    // Test empty string
    String result3 = uLispParser::unescape("");
    ASSERT_STR_EQ("", result3);
    
    // Test multiple escapes
    String result4 = uLispParser::unescape("\\n\\r\\t\\\"");
    ASSERT_STR_EQ("\n\r\t\"", result4);
}

// Test whitespace skipping
TEST_CASE(test_skip_whitespace) {
    String test_str = "   \t\n  hello";
    int ptr = 0;
    uLispParser::skip_whitespace(test_str, ptr);
    ASSERT_EQ('h', test_str[ptr]);
    
    // Test with commas (which are treated as whitespace)
    String test_str2 = " , , \t hello";
    int ptr2 = 0;
    uLispParser::skip_whitespace(test_str2, ptr2);
    ASSERT_EQ('h', test_str2[ptr2]);
    
    // Test no whitespace
    String test_str3 = "immediate";
    int ptr3 = 0;
    uLispParser::skip_whitespace(test_str3, ptr3);
    ASSERT_EQ('i', test_str3[ptr3]);
}

// Test character type checking functions
TEST_CASE(test_character_type_checking) {
    // Test symbol characters
    ASSERT_TRUE(uLispParser::is_symbol("abc", 0));
    ASSERT_TRUE(uLispParser::is_symbol("123", 0));
    ASSERT_TRUE(uLispParser::is_symbol("test+", 4));
    ASSERT_FALSE(uLispParser::is_symbol("(hello", 0));
    ASSERT_FALSE(uLispParser::is_symbol(")hello", 0));
    ASSERT_FALSE(uLispParser::is_symbol("[hello", 0));
    ASSERT_FALSE(uLispParser::is_symbol("]hello", 0));
    ASSERT_FALSE(uLispParser::is_symbol("\"hello", 0));
    ASSERT_FALSE(uLispParser::is_symbol("'hello", 0));
    
    // Test other character types
    ASSERT_TRUE(uLispParser::is_comment(";comment", 0));
    ASSERT_FALSE(uLispParser::is_comment("comment", 0));
    
    ASSERT_TRUE(uLispParser::is_quote("'quoted", 0));
    ASSERT_FALSE(uLispParser::is_quote("quoted", 0));
    
    ASSERT_TRUE(uLispParser::is_list("(list)", 0));
    ASSERT_FALSE(uLispParser::is_list("list", 0));
    
    ASSERT_TRUE(uLispParser::is_vector("[vector]", 0));
    ASSERT_FALSE(uLispParser::is_vector("vector", 0));
    
    ASSERT_TRUE(uLispParser::is_map("{map}", 0));
    ASSERT_FALSE(uLispParser::is_map("map", 0));
    
    ASSERT_TRUE(uLispParser::is_midinote("M60", 0));
    ASSERT_FALSE(uLispParser::is_midinote("60", 0));
}

// Test basic number parsing
TEST_CASE(test_parse_integers) {
    // Test positive integer
    Value result1 = uLispParser::parse("42");
    ASSERT_TRUE(result1.is_int());
    ASSERT_EQ(42, result1.as_int());
    
    // Test negative integer
    Value result2 = uLispParser::parse("-17");
    ASSERT_TRUE(result2.is_int());
    ASSERT_EQ(-17, result2.as_int());
    
    // Test zero
    Value result3 = uLispParser::parse("0");
    ASSERT_TRUE(result3.is_int());
    ASSERT_EQ(0, result3.as_int());
}

TEST_CASE(test_parse_floats) {
    // Test positive float
    Value result1 = uLispParser::parse("3.14");
    ASSERT_TRUE(result1.is_float());
    ASSERT_NEAR(3.14, result1.as_float(), 0.001);
    
    // Test negative float
    Value result2 = uLispParser::parse("-2.71");
    ASSERT_TRUE(result2.is_float());
    ASSERT_NEAR(-2.71, result2.as_float(), 0.001);
    
    // Test float with leading zero
    Value result3 = uLispParser::parse("0.5");
    ASSERT_TRUE(result3.is_float());
    ASSERT_NEAR(0.5, result3.as_float(), 0.001);
    
    // Test float without leading zero
    Value result4 = uLispParser::parse(".25");
    ASSERT_TRUE(result4.is_float());
    ASSERT_NEAR(0.25, result4.as_float(), 0.001);
}

// Test string parsing
TEST_CASE(test_parse_strings) {
    // Test basic string
    Value result1 = uLispParser::parse("\"hello world\"");
    ASSERT_TRUE(result1.is_string());
    ASSERT_STR_EQ("hello world", result1.as_string());
    
    // Test empty string
    Value result2 = uLispParser::parse("\"\"");
    ASSERT_TRUE(result2.is_string());
    ASSERT_STR_EQ("", result2.as_string());
    
    // Test string with escape sequences
    Value result3 = uLispParser::parse("\"line1\\nline2\"");
    ASSERT_TRUE(result3.is_string());
    ASSERT_STR_EQ("line1\nline2", result3.as_string());
    
    // Test string with quotes
    Value result4 = uLispParser::parse("\"say \\\"hello\\\"\"");
    ASSERT_TRUE(result4.is_string());
    ASSERT_STR_EQ("say \"hello\"", result4.as_string());
}

// Test symbol parsing
TEST_CASE(test_parse_symbols) {
    // Test basic symbol
    Value result1 = uLispParser::parse("hello");
    ASSERT_TRUE(result1.is_symbol());
    ASSERT_STR_EQ("hello", result1.as_atom());
    
    // Test symbol with special characters
    Value result2 = uLispParser::parse("+");
    ASSERT_TRUE(result2.is_symbol());
    ASSERT_STR_EQ("+", result2.as_atom());
    
    // Test complex symbol
    Value result3 = uLispParser::parse("my-var-123");
    ASSERT_TRUE(result3.is_symbol());
    ASSERT_STR_EQ("my-var-123", result3.as_atom());
}

// Test list parsing
TEST_CASE(test_parse_lists) {
    // Test empty list
    Value result1 = uLispParser::parse("()");
    ASSERT_TRUE(result1.is_list());
    ASSERT_EQ(0, result1.as_list().size());
    
    // Test list with single element
    Value result2 = uLispParser::parse("(42)");
    ASSERT_TRUE(result2.is_list());
    ASSERT_EQ(1, result2.as_list().size());
    ASSERT_TRUE(result2.as_list()[0].is_int());
    ASSERT_EQ(42, result2.as_list()[0].as_int());
    
    // Test list with multiple elements
    Value result3 = uLispParser::parse("(1 2 3)");
    ASSERT_TRUE(result3.is_list());
    ASSERT_EQ(3, result3.as_list().size());
    ASSERT_EQ(1, result3.as_list()[0].as_int());
    ASSERT_EQ(2, result3.as_list()[1].as_int());
    ASSERT_EQ(3, result3.as_list()[2].as_int());
    
    // Test list with mixed types
    Value result4 = uLispParser::parse("(+ 1 2.5 \"hello\")");
    ASSERT_TRUE(result4.is_list());
    ASSERT_EQ(4, result4.as_list().size());
    ASSERT_TRUE(result4.as_list()[0].is_symbol());
    ASSERT_STR_EQ("+", result4.as_list()[0].as_atom());
    ASSERT_TRUE(result4.as_list()[1].is_int());
    ASSERT_EQ(1, result4.as_list()[1].as_int());
    ASSERT_TRUE(result4.as_list()[2].is_float());
    ASSERT_NEAR(2.5, result4.as_list()[2].as_float(), 0.001);
    ASSERT_TRUE(result4.as_list()[3].is_string());
    ASSERT_STR_EQ("hello", result4.as_list()[3].as_string());
}

// Test nested list parsing
TEST_CASE(test_parse_nested_lists) {
    // Test nested list
    Value result1 = uLispParser::parse("((1 2) (3 4))");
    ASSERT_TRUE(result1.is_list());
    ASSERT_EQ(2, result1.as_list().size());
    
    // Check first nested list
    ASSERT_TRUE(result1.as_list()[0].is_list());
    ASSERT_EQ(2, result1.as_list()[0].as_list().size());
    ASSERT_EQ(1, result1.as_list()[0].as_list()[0].as_int());
    ASSERT_EQ(2, result1.as_list()[0].as_list()[1].as_int());
    
    // Check second nested list
    ASSERT_TRUE(result1.as_list()[1].is_list());
    ASSERT_EQ(2, result1.as_list()[1].as_list().size());
    ASSERT_EQ(3, result1.as_list()[1].as_list()[0].as_int());
    ASSERT_EQ(4, result1.as_list()[1].as_list()[1].as_int());
    
    // Test deeply nested
    Value result2 = uLispParser::parse("(+ (* 2 3) (/ 8 4))");
    ASSERT_TRUE(result2.is_list());
    ASSERT_EQ(3, result2.as_list().size());
    ASSERT_STR_EQ("+", result2.as_list()[0].as_atom());
    
    // Check first nested expression (* 2 3)
    ASSERT_TRUE(result2.as_list()[1].is_list());
    ASSERT_EQ(3, result2.as_list()[1].as_list().size());
    ASSERT_STR_EQ("*", result2.as_list()[1].as_list()[0].as_atom());
    
    // Check second nested expression (/ 8 4)
    ASSERT_TRUE(result2.as_list()[2].is_list());
    ASSERT_EQ(3, result2.as_list()[2].as_list().size());
    ASSERT_STR_EQ("/", result2.as_list()[2].as_list()[0].as_atom());
}

// Test vector parsing
TEST_CASE(test_parse_vectors) {
    // Test empty vector
    Value result1 = uLispParser::parse("[]");
    ASSERT_TRUE(result1.is_vector());
    ASSERT_EQ(0, result1.as_vector().size());
    
    // Test vector with elements
    Value result2 = uLispParser::parse("[1 2 3]");
    ASSERT_TRUE(result2.is_vector());
    ASSERT_EQ(3, result2.as_vector().size());
    ASSERT_EQ(1, result2.as_vector()[0].as_int());
    ASSERT_EQ(2, result2.as_vector()[1].as_int());
    ASSERT_EQ(3, result2.as_vector()[2].as_int());
    
    // Test vector with mixed types
    Value result3 = uLispParser::parse("[42 \"hello\" 3.14]");
    ASSERT_TRUE(result3.is_vector());
    ASSERT_EQ(3, result3.as_vector().size());
    ASSERT_TRUE(result3.as_vector()[0].is_int());
    ASSERT_TRUE(result3.as_vector()[1].is_string());
    ASSERT_TRUE(result3.as_vector()[2].is_float());
}

// Test quote parsing
TEST_CASE(test_parse_quotes) {
    // Test quoted number - parser creates QUOTE type, not LIST type
    // We can verify quotes work by checking their string representation
    Value result1 = uLispParser::parse("'42");
    ASSERT_STR_EQ("'42", result1.to_lisp_src());
    
    // Test quoted symbol
    Value result2 = uLispParser::parse("'hello");
    ASSERT_STR_EQ("'hello", result2.to_lisp_src());
    
    // Test quoted list
    Value result3 = uLispParser::parse("'(1 2 3)");
    ASSERT_STR_EQ("'(1 2 3)", result3.to_lisp_src());
}

// Test comment handling
TEST_CASE(test_parse_with_comments) {
    // Test single line comment
    Value result1 = uLispParser::parse("; this is a comment\n42");
    ASSERT_TRUE(result1.is_int());
    ASSERT_EQ(42, result1.as_int());
    
    // Test comment in middle
    Value result2 = uLispParser::parse("(+ 1 ; add one\n 2)");
    ASSERT_TRUE(result2.is_list());
    ASSERT_EQ(3, result2.as_list().size());
    ASSERT_STR_EQ("+", result2.as_list()[0].as_atom());
    ASSERT_EQ(1, result2.as_list()[1].as_int());
    ASSERT_EQ(2, result2.as_list()[2].as_int());
}

// Test whitespace handling
TEST_CASE(test_parse_with_whitespace) {
    // Test various whitespace
    Value result1 = uLispParser::parse("   \t\n  42   ");
    ASSERT_TRUE(result1.is_int());
    ASSERT_EQ(42, result1.as_int());
    
    // Test list with whitespace
    Value result2 = uLispParser::parse("( +   1    2 )");
    ASSERT_TRUE(result2.is_list());
    ASSERT_EQ(3, result2.as_list().size());
    ASSERT_STR_EQ("+", result2.as_list()[0].as_atom());
    
    // Test commas as whitespace
    Value result3 = uLispParser::parse("[1,2,3]");
    ASSERT_TRUE(result3.is_vector());
    ASSERT_EQ(3, result3.as_vector().size());
    ASSERT_EQ(1, result3.as_vector()[0].as_int());
    ASSERT_EQ(2, result3.as_vector()[1].as_int());
    ASSERT_EQ(3, result3.as_vector()[2].as_int());
}

// Test edge cases and error conditions
TEST_CASE(test_parse_edge_cases) {
    // Test empty string
    Value result1 = uLispParser::parse("");
    // Empty string should return a unit/nil value
    ASSERT_FALSE(result1.is_int());
    ASSERT_FALSE(result1.is_string());
    
    // Test single whitespace
    Value result2 = uLispParser::parse("   ");
    ASSERT_FALSE(result2.is_int());
    
    // Test just comment
    Value result3 = uLispParser::parse("; just a comment");
    ASSERT_FALSE(result3.is_int());
    
    // Test number boundaries
    Value result4 = uLispParser::parse("0");
    ASSERT_TRUE(result4.is_int());
    ASSERT_EQ(0, result4.as_int());
    
    Value result5 = uLispParser::parse("-0");
    ASSERT_TRUE(result5.is_int());
    ASSERT_EQ(0, result5.as_int());
}

// Test complex real-world expressions
TEST_CASE(test_parse_complex_expressions) {
    // Test function call with nested expressions
    Value result1 = uLispParser::parse("(defn factorial [n] (if (= n 0) 1 (* n (factorial (- n 1)))))");
    ASSERT_TRUE(result1.is_list());
    ASSERT_TRUE(result1.as_list().size() > 0);
    ASSERT_STR_EQ("defn", result1.as_list()[0].as_atom());
    
    // Test let expression
    Value result2 = uLispParser::parse("(let [x 5 y 10] (+ x y))");
    ASSERT_TRUE(result2.is_list());
    ASSERT_EQ(3, result2.as_list().size());
    ASSERT_STR_EQ("let", result2.as_list()[0].as_atom());
    ASSERT_TRUE(result2.as_list()[1].is_vector());
    
    // Test mixed data structures
    Value result3 = uLispParser::parse("{:name \"test\" :value [1 2 3] :fn (lambda [x] (+ x 1))}");
    ASSERT_TRUE(result3.is_list());  // Maps are parsed as lists in this implementation
}

// Test parsing with pointer advancement
TEST_CASE(test_parse_with_pointer) {
    String test_expr = "42 \"hello\" (+ 1 2)";
    int ptr = 0;
    
    // Parse first value
    Value result1 = uLispParser::parse(test_expr, ptr);
    ASSERT_TRUE(result1.is_int());
    ASSERT_EQ(42, result1.as_int());
    
    // Parse second value
    Value result2 = uLispParser::parse(test_expr, ptr);
    ASSERT_TRUE(result2.is_string());
    ASSERT_STR_EQ("hello", result2.as_string());
    
    // Parse third value
    Value result3 = uLispParser::parse(test_expr, ptr);
    ASSERT_TRUE(result3.is_list());
    ASSERT_EQ(3, result3.as_list().size());
    ASSERT_STR_EQ("+", result3.as_list()[0].as_atom());
}

// Main function to run all tests
int main() {
    std::cout << "Running all parser tests..." << std::endl;
    std::cout << "All parser tests passed!" << std::endl;
    return 0;
}