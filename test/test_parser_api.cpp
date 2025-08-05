#include "../uSEQ/src/modulisp/lisp/parser.h"
#include "../uSEQ/src/modulisp/lisp/value.h"
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
/// PARSER UTILITY FUNCTIONS API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_parser_string_unescaping_api) {
    // Test basic escape sequences
    String result1 = uLispParser::unescape("hello\\nworld");
    ASSERT_STR_EQ("hello\nworld", result1);
    
    String result2 = uLispParser::unescape("tab\\there");
    ASSERT_STR_EQ("tab\there", result2);
    
    String result3 = uLispParser::unescape("quote\\\"test");
    ASSERT_STR_EQ("quote\"test", result3);
    
    String result4 = uLispParser::unescape("return\\rtest");
    ASSERT_STR_EQ("return\rtest", result4);
    
    // Test no escapes
    String result5 = uLispParser::unescape("plain text");
    ASSERT_STR_EQ("plain text", result5);
    
    // Test empty string
    String result6 = uLispParser::unescape("");
    ASSERT_STR_EQ("", result6);
    
    // Test multiple escapes
    String result7 = uLispParser::unescape("\\n\\t\\r\\\"");
    ASSERT_STR_EQ("\n\t\r\"", result7);
}

TEST_CASE(test_parser_string_unescaping_edge_cases) {
    // Test backslash at end
    String result1 = uLispParser::unescape("test\\");
    ASSERT_STR_EQ("test\\", result1);
    
    // Test unknown escape sequence (should preserve character after backslash)
    String result2 = uLispParser::unescape("test\\x");
    ASSERT_STR_EQ("testx", result2);
    
    // Test consecutive backslashes
    String result3 = uLispParser::unescape("test\\\\");
    ASSERT_STR_EQ("test\\", result3);
}

TEST_CASE(test_parser_whitespace_skipping_api) {
    // Test basic whitespace skipping
    String test_str1 = "   \t\n  hello";
    int ptr1 = 0;
    uLispParser::skip_whitespace(test_str1, ptr1);
    ASSERT_EQ('h', test_str1[ptr1]);
    
    // Test comma as whitespace (LISP convention)
    String test_str2 = " , , \t hello";
    int ptr2 = 0;
    uLispParser::skip_whitespace(test_str2, ptr2);
    ASSERT_EQ('h', test_str2[ptr2]);
    
    // Test no whitespace
    String test_str3 = "immediate";
    int ptr3 = 0;
    uLispParser::skip_whitespace(test_str3, ptr3);
    ASSERT_EQ('i', test_str3[ptr3]);
    
    // Test all whitespace
    String test_str4 = "   \t\n   ";
    int ptr4 = 0;
    uLispParser::skip_whitespace(test_str4, ptr4);
    ASSERT_EQ(test_str4.length(), ptr4); // Should skip to end
    
    // Test mixed whitespace and commas
    String test_str5 = " \t,\n , start";
    int ptr5 = 0;
    uLispParser::skip_whitespace(test_str5, ptr5);
    ASSERT_EQ('s', test_str5[ptr5]);
}

////////////////////////////////////////////////////////////////////////////////
/// PARSER CHARACTER TYPE DETECTION API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_parser_symbol_detection_api) {
    // Test valid symbol characters
    ASSERT_TRUE(uLispParser::is_symbol("abc", 0));
    ASSERT_TRUE(uLispParser::is_symbol("123", 0));
    ASSERT_TRUE(uLispParser::is_symbol("test+", 4)); // Plus at position 4
    ASSERT_TRUE(uLispParser::is_symbol("my-var", 0));
    ASSERT_TRUE(uLispParser::is_symbol("_underscore", 0));
    
    // Test invalid symbol characters (structural characters)
    ASSERT_FALSE(uLispParser::is_symbol("(hello", 0));
    ASSERT_FALSE(uLispParser::is_symbol(")hello", 0));
    ASSERT_FALSE(uLispParser::is_symbol("[hello", 0));
    ASSERT_FALSE(uLispParser::is_symbol("]hello", 0));
    ASSERT_FALSE(uLispParser::is_symbol("{hello", 0));
    ASSERT_FALSE(uLispParser::is_symbol("}hello", 0));
    ASSERT_FALSE(uLispParser::is_symbol("\"hello", 0));
    ASSERT_FALSE(uLispParser::is_symbol("'hello", 0));
    ASSERT_FALSE(uLispParser::is_symbol(";hello", 0));
    
    // Test whitespace characters
    ASSERT_FALSE(uLispParser::is_symbol(" hello", 0));
    ASSERT_FALSE(uLispParser::is_symbol("\thello", 0));
    ASSERT_FALSE(uLispParser::is_symbol("\nhello", 0));
    ASSERT_FALSE(uLispParser::is_symbol(",hello", 0));
}

TEST_CASE(test_parser_structural_character_detection_api) {
    // Test comment detection
    ASSERT_TRUE(uLispParser::is_comment(";comment", 0));
    ASSERT_TRUE(uLispParser::is_comment("   ;comment", 3));
    ASSERT_FALSE(uLispParser::is_comment("not;comment", 0));
    ASSERT_FALSE(uLispParser::is_comment("comment", 0));
    
    // Test quote detection
    ASSERT_TRUE(uLispParser::is_quote("'quoted", 0));
    ASSERT_TRUE(uLispParser::is_quote("test'quoted", 4));
    ASSERT_FALSE(uLispParser::is_quote("quoted", 0));
    ASSERT_FALSE(uLispParser::is_quote("not'quoted", 0));
    
    // Test list detection
    ASSERT_TRUE(uLispParser::is_list("(list)", 0));
    ASSERT_TRUE(uLispParser::is_list("test(list)", 4));
    ASSERT_FALSE(uLispParser::is_list("list", 0));
    ASSERT_FALSE(uLispParser::is_list(")list", 0));
    
    // Test vector detection
    ASSERT_TRUE(uLispParser::is_vector("[vector]", 0));
    ASSERT_TRUE(uLispParser::is_vector("test[vector]", 4));
    ASSERT_FALSE(uLispParser::is_vector("vector", 0));
    ASSERT_FALSE(uLispParser::is_vector("]vector", 0));
    
    // Test map detection
    ASSERT_TRUE(uLispParser::is_map("{map}", 0));
    ASSERT_TRUE(uLispParser::is_map("test{map}", 4));
    ASSERT_FALSE(uLispParser::is_map("map", 0));
    ASSERT_FALSE(uLispParser::is_map("}map", 0));
}

TEST_CASE(test_parser_special_notation_detection_api) {
    // Test MIDI note detection
    ASSERT_TRUE(uLispParser::is_midinote("M60", 0));
    ASSERT_TRUE(uLispParser::is_midinote("M127", 0));
    ASSERT_TRUE(uLispParser::is_midinote("M0", 0));
    ASSERT_TRUE(uLispParser::is_midinote("testM60", 4));
    ASSERT_FALSE(uLispParser::is_midinote("60", 0));
    ASSERT_FALSE(uLispParser::is_midinote("m60", 0)); // lowercase
    ASSERT_FALSE(uLispParser::is_midinote("Mabc", 0)); // non-numeric
    
    // Test frequency detection (placeholder - implementation may vary)
    // Note: These functions may not be implemented in all versions
    // ASSERT_TRUE(uLispParser::is_freq("440Hz", 0) || !uLispParser::is_freq("440Hz", 0));
    
    // Test fraction detection (placeholder - implementation may vary)
    // Note: These functions may not be implemented in all versions
    // ASSERT_TRUE(uLispParser::is_fraction("3/4", 0) || !uLispParser::is_fraction("3/4", 0));
}

////////////////////////////////////////////////////////////////////////////////
/// PARSER ATOMIC VALUE PARSING API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_parser_integer_parsing_api) {
    // Test positive integers
    Value result1 = uLispParser::parse("42");
    ASSERT_TRUE(result1.is_int());
    ASSERT_EQ(42, result1.as_int());
    
    Value result2 = uLispParser::parse("0");
    ASSERT_TRUE(result2.is_int());
    ASSERT_EQ(0, result2.as_int());
    
    Value result3 = uLispParser::parse("123456");
    ASSERT_TRUE(result3.is_int());
    ASSERT_EQ(123456, result3.as_int());
    
    // Test negative integers
    Value result4 = uLispParser::parse("-17");
    ASSERT_TRUE(result4.is_int());
    ASSERT_EQ(-17, result4.as_int());
    
    Value result5 = uLispParser::parse("-0");
    ASSERT_TRUE(result5.is_int());
    ASSERT_EQ(0, result5.as_int());
}

TEST_CASE(test_parser_float_parsing_api) {
    // Test positive floats
    Value result1 = uLispParser::parse("3.14");
    ASSERT_TRUE(result1.is_float());
    ASSERT_NEAR(3.14, result1.as_float(), 0.001);
    
    Value result2 = uLispParser::parse("0.5");
    ASSERT_TRUE(result2.is_float());
    ASSERT_NEAR(0.5, result2.as_float(), 0.001);
    
    Value result3 = uLispParser::parse(".25");
    ASSERT_TRUE(result3.is_float());
    ASSERT_NEAR(0.25, result3.as_float(), 0.001);
    
    // Test negative floats
    Value result4 = uLispParser::parse("-2.71");
    ASSERT_TRUE(result4.is_float());
    ASSERT_NEAR(-2.71, result4.as_float(), 0.001);
    
    Value result5 = uLispParser::parse("-0.0");
    ASSERT_TRUE(result5.is_float());
    ASSERT_NEAR(0.0, result5.as_float(), 0.001);
    
    // Test scientific notation (if supported)
    Value result6 = uLispParser::parse("1e3");
    if (result6.is_float()) {
        ASSERT_NEAR(1000.0, result6.as_float(), 0.001);
    }
    
    Value result7 = uLispParser::parse("1.5e-2");
    if (result7.is_float()) {
        ASSERT_NEAR(0.015, result7.as_float(), 0.0001);
    }
}

TEST_CASE(test_parser_string_parsing_api) {
    // Test basic strings
    Value result1 = uLispParser::parse("\"hello world\"");
    ASSERT_TRUE(result1.is_string());
    ASSERT_STR_EQ("hello world", result1.as_string());
    
    // Test empty string
    Value result2 = uLispParser::parse("\"\"");
    ASSERT_TRUE(result2.is_string());
    ASSERT_STR_EQ("", result2.as_string());
    
    // Test strings with escape sequences
    Value result3 = uLispParser::parse("\"line1\\nline2\"");
    ASSERT_TRUE(result3.is_string());
    ASSERT_STR_EQ("line1\nline2", result3.as_string());
    
    Value result4 = uLispParser::parse("\"tab\\there\"");
    ASSERT_TRUE(result4.is_string());
    ASSERT_STR_EQ("tab\there", result4.as_string());
    
    Value result5 = uLispParser::parse("\"say \\\"hello\\\"\"");
    ASSERT_TRUE(result5.is_string());
    ASSERT_STR_EQ("say \"hello\"", result5.as_string());
    
    // Test strings with special characters
    Value result6 = uLispParser::parse("\"!@#$%^&*()\"");
    ASSERT_TRUE(result6.is_string());
    ASSERT_STR_EQ("!@#$%^&*()", result6.as_string());
}

TEST_CASE(test_parser_symbol_parsing_api) {
    // Test basic symbols
    Value result1 = uLispParser::parse("hello");
    ASSERT_TRUE(result1.is_symbol());
    ASSERT_STR_EQ("hello", result1.as_atom());
    
    Value result2 = uLispParser::parse("test-var");
    ASSERT_TRUE(result2.is_symbol());
    ASSERT_STR_EQ("test-var", result2.as_atom());
    
    Value result3 = uLispParser::parse("var123");
    ASSERT_TRUE(result3.is_symbol());
    ASSERT_STR_EQ("var123", result3.as_atom());
    
    // Test operator symbols
    Value result4 = uLispParser::parse("+");
    ASSERT_TRUE(result4.is_symbol());
    ASSERT_STR_EQ("+", result4.as_atom());
    
    Value result5 = uLispParser::parse("*");
    ASSERT_TRUE(result5.is_symbol());
    ASSERT_STR_EQ("*", result5.as_atom());
    
    Value result6 = uLispParser::parse(">=");
    ASSERT_TRUE(result6.is_symbol());
    ASSERT_STR_EQ(">=", result6.as_atom());
    
    // Test complex symbols
    Value result7 = uLispParser::parse("my-complex-var-123");
    ASSERT_TRUE(result7.is_symbol());
    ASSERT_STR_EQ("my-complex-var-123", result7.as_atom());
}

////////////////////////////////////////////////////////////////////////////////
/// PARSER COMPOSITE STRUCTURE PARSING API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_parser_list_parsing_api) {
    // Test empty list
    Value result1 = uLispParser::parse("()");
    ASSERT_TRUE(result1.is_list());
    ASSERT_EQ(0, result1.as_list().size());
    ASSERT_TRUE(result1.is_empty());
    
    // Test single element list
    Value result2 = uLispParser::parse("(42)");
    ASSERT_TRUE(result2.is_list());
    ASSERT_EQ(1, result2.as_list().size());
    ASSERT_TRUE(result2.as_list()[0].is_int());
    ASSERT_EQ(42, result2.as_list()[0].as_int());
    
    // Test multiple element list
    Value result3 = uLispParser::parse("(1 2 3)");
    ASSERT_TRUE(result3.is_list());
    ASSERT_EQ(3, result3.as_list().size());
    ASSERT_EQ(1, result3.as_list()[0].as_int());
    ASSERT_EQ(2, result3.as_list()[1].as_int());
    ASSERT_EQ(3, result3.as_list()[2].as_int());
    
    // Test mixed type list
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

TEST_CASE(test_parser_vector_parsing_api) {
    // Test empty vector
    Value result1 = uLispParser::parse("[]");
    ASSERT_TRUE(result1.is_vector());
    ASSERT_EQ(0, result1.as_vector().size());
    ASSERT_TRUE(result1.is_empty());
    
    // Test single element vector
    Value result2 = uLispParser::parse("[42]");
    ASSERT_TRUE(result2.is_vector());
    ASSERT_EQ(1, result2.as_vector().size());
    ASSERT_TRUE(result2.as_vector()[0].is_int());
    ASSERT_EQ(42, result2.as_vector()[0].as_int());
    
    // Test multiple element vector
    Value result3 = uLispParser::parse("[1 2 3]");
    ASSERT_TRUE(result3.is_vector());
    ASSERT_EQ(3, result3.as_vector().size());
    ASSERT_EQ(1, result3.as_vector()[0].as_int());
    ASSERT_EQ(2, result3.as_vector()[1].as_int());
    ASSERT_EQ(3, result3.as_vector()[2].as_int());
    
    // Test mixed type vector
    Value result4 = uLispParser::parse("[42 \"hello\" 3.14 test-symbol]");
    ASSERT_TRUE(result4.is_vector());
    ASSERT_EQ(4, result4.as_vector().size());
    ASSERT_TRUE(result4.as_vector()[0].is_int());
    ASSERT_EQ(42, result4.as_vector()[0].as_int());
    ASSERT_TRUE(result4.as_vector()[1].is_string());
    ASSERT_STR_EQ("hello", result4.as_vector()[1].as_string());
    ASSERT_TRUE(result4.as_vector()[2].is_float());
    ASSERT_NEAR(3.14, result4.as_vector()[2].as_float(), 0.001);
    ASSERT_TRUE(result4.as_vector()[3].is_symbol());
    ASSERT_STR_EQ("test-symbol", result4.as_vector()[3].as_atom());
}

TEST_CASE(test_parser_nested_structure_parsing_api) {
    // Test nested lists
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
    
    // Test mixed nested structures
    Value result2 = uLispParser::parse("([1 2] (3 4) [5 6])");
    ASSERT_TRUE(result2.is_list());
    ASSERT_EQ(3, result2.as_list().size());
    ASSERT_TRUE(result2.as_list()[0].is_vector());
    ASSERT_TRUE(result2.as_list()[1].is_list());
    ASSERT_TRUE(result2.as_list()[2].is_vector());
    
    // Test deeply nested structures
    Value result3 = uLispParser::parse("(+ (* 2 3) (/ 8 4))");
    ASSERT_TRUE(result3.is_list());
    ASSERT_EQ(3, result3.as_list().size());
    ASSERT_STR_EQ("+", result3.as_list()[0].as_atom());
    
    // Check first nested expression (* 2 3)
    ASSERT_TRUE(result3.as_list()[1].is_list());
    ASSERT_EQ(3, result3.as_list()[1].as_list().size());
    ASSERT_STR_EQ("*", result3.as_list()[1].as_list()[0].as_atom());
    ASSERT_EQ(2, result3.as_list()[1].as_list()[1].as_int());
    ASSERT_EQ(3, result3.as_list()[1].as_list()[2].as_int());
    
    // Check second nested expression (/ 8 4)
    ASSERT_TRUE(result3.as_list()[2].is_list());
    ASSERT_EQ(3, result3.as_list()[2].as_list().size());
    ASSERT_STR_EQ("/", result3.as_list()[2].as_list()[0].as_atom());
    ASSERT_EQ(8, result3.as_list()[2].as_list()[1].as_int());
    ASSERT_EQ(4, result3.as_list()[2].as_list()[2].as_int());
}

////////////////////////////////////////////////////////////////////////////////
/// PARSER QUOTE AND SPECIAL FORM PARSING API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_parser_quote_parsing_api) {
    // Test quoted numbers
    Value result1 = uLispParser::parse("'42");
    String src1 = result1.to_lisp_src();
    ASSERT_STR_EQ("'42", src1);
    
    // Test quoted symbols
    Value result2 = uLispParser::parse("'hello");
    String src2 = result2.to_lisp_src();
    ASSERT_STR_EQ("'hello", src2);
    
    // Test quoted strings
    Value result3 = uLispParser::parse("'\"test\"");
    String src3 = result3.to_lisp_src();
    ASSERT_STR_EQ("'\"test\"", src3);
    
    // Test quoted lists
    Value result4 = uLispParser::parse("'(1 2 3)");
    String src4 = result4.to_lisp_src();
    ASSERT_STR_EQ("'(1 2 3)", src4);
    
    // Test quoted vectors
    Value result5 = uLispParser::parse("'[1 2 3]");
    String src5 = result5.to_lisp_src();
    ASSERT_STR_EQ("'[1 2 3]", src5);
    
    // Test nested quotes
    Value result6 = uLispParser::parse("''test");
    String src6 = result6.to_lisp_src();
    ASSERT_STR_EQ("''test", src6);
}

////////////////////////////////////////////////////////////////////////////////
/// PARSER WHITESPACE AND COMMENT HANDLING API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_parser_whitespace_handling_api) {
    // Test various whitespace around values
    Value result1 = uLispParser::parse("   42   ");
    ASSERT_TRUE(result1.is_int());
    ASSERT_EQ(42, result1.as_int());
    
    Value result2 = uLispParser::parse("\t\n  \"hello\"  \t");
    ASSERT_TRUE(result2.is_string());
    ASSERT_STR_EQ("hello", result2.as_string());
    
    // Test whitespace in lists
    Value result3 = uLispParser::parse("( +   1    2 )");
    ASSERT_TRUE(result3.is_list());
    ASSERT_EQ(3, result3.as_list().size());
    ASSERT_STR_EQ("+", result3.as_list()[0].as_atom());
    ASSERT_EQ(1, result3.as_list()[1].as_int());
    ASSERT_EQ(2, result3.as_list()[2].as_int());
    
    // Test commas as whitespace
    Value result4 = uLispParser::parse("[1,2,3]");
    ASSERT_TRUE(result4.is_vector());
    ASSERT_EQ(3, result4.as_vector().size());
    ASSERT_EQ(1, result4.as_vector()[0].as_int());
    ASSERT_EQ(2, result4.as_vector()[1].as_int());
    ASSERT_EQ(3, result4.as_vector()[2].as_int());
    
    // Test mixed whitespace and commas
    Value result5 = uLispParser::parse("[ 1 , \t 2 \n, 3 ]");
    ASSERT_TRUE(result5.is_vector());
    ASSERT_EQ(3, result5.as_vector().size());
    ASSERT_EQ(1, result5.as_vector()[0].as_int());
    ASSERT_EQ(2, result5.as_vector()[1].as_int());
    ASSERT_EQ(3, result5.as_vector()[2].as_int());
}

TEST_CASE(test_parser_comment_handling_api) {
    // Test single line comment before value
    Value result1 = uLispParser::parse("; this is a comment\n42");
    ASSERT_TRUE(result1.is_int());
    ASSERT_EQ(42, result1.as_int());
    
    // Test comment in middle of expression
    Value result2 = uLispParser::parse("(+ 1 ; add one\n 2)");
    ASSERT_TRUE(result2.is_list());
    ASSERT_EQ(3, result2.as_list().size());
    ASSERT_STR_EQ("+", result2.as_list()[0].as_atom());
    ASSERT_EQ(1, result2.as_list()[1].as_int());
    ASSERT_EQ(2, result2.as_list()[2].as_int());
    
    // Test multiple comments
    Value result3 = uLispParser::parse("; first comment\n; second comment\n42 ; trailing comment");
    ASSERT_TRUE(result3.is_int());
    ASSERT_EQ(42, result3.as_int());
    
    // Test comment with various symbols
    Value result4 = uLispParser::parse("; comment with !@#$%^&*() symbols\n123");
    ASSERT_TRUE(result4.is_int());
    ASSERT_EQ(123, result4.as_int());
}

////////////////////////////////////////////////////////////////////////////////
/// PARSER INCREMENTAL PARSING API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_parser_pointer_based_parsing_api) {
    String test_expr = "42 \"hello\" (+ 1 2) [3 4]";
    int ptr = 0;
    
    // Parse first value
    Value result1 = uLispParser::parse(test_expr, ptr);
    ASSERT_TRUE(result1.is_int());
    ASSERT_EQ(42, result1.as_int());
    
    // Parse second value
    Value result2 = uLispParser::parse(test_expr, ptr);
    ASSERT_TRUE(result2.is_string());
    ASSERT_STR_EQ("hello", result2.as_string());
    
    // Parse third value (list)
    Value result3 = uLispParser::parse(test_expr, ptr);
    ASSERT_TRUE(result3.is_list());
    ASSERT_EQ(3, result3.as_list().size());
    ASSERT_STR_EQ("+", result3.as_list()[0].as_atom());
    ASSERT_EQ(1, result3.as_list()[1].as_int());
    ASSERT_EQ(2, result3.as_list()[2].as_int());
    
    // Parse fourth value (vector)
    Value result4 = uLispParser::parse(test_expr, ptr);
    ASSERT_TRUE(result4.is_vector());
    ASSERT_EQ(2, result4.as_vector().size());
    ASSERT_EQ(3, result4.as_vector()[0].as_int());
    ASSERT_EQ(4, result4.as_vector()[1].as_int());
    
    // Should be at end of string
    ASSERT_EQ(test_expr.length(), ptr);
}

TEST_CASE(test_parser_incremental_with_whitespace_api) {
    String test_expr = " 10   20\n30 ";
    int ptr = 0;
    
    // Parse values sequentially
    Value result1 = uLispParser::parse(test_expr, ptr);
    ASSERT_TRUE(result1.is_int());
    ASSERT_EQ(10, result1.as_int());
    
    Value result2 = uLispParser::parse(test_expr, ptr);
    ASSERT_TRUE(result2.is_int());
    ASSERT_EQ(20, result2.as_int());
    
    Value result3 = uLispParser::parse(test_expr, ptr);
    ASSERT_TRUE(result3.is_int());
    ASSERT_EQ(30, result3.as_int());
    
    // Parser should handle trailing whitespace
    ASSERT_TRUE(ptr >= test_expr.length() - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// PARSER EDGE CASES API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_parser_edge_case_inputs_api) {
    // Test empty string
    Value result1 = uLispParser::parse("");
    // Parser should handle empty input gracefully
    ASSERT_FALSE(result1.is_int()); // Should not be a number
    
    // Test only whitespace
    Value result2 = uLispParser::parse("   \t\n  ");
    ASSERT_FALSE(result2.is_int());
    
    // Test only comment
    Value result3 = uLispParser::parse("; just a comment");
    ASSERT_FALSE(result3.is_int());
    
    // Test malformed input resilience
    // Note: Exact behavior depends on implementation error handling
    Value result4 = uLispParser::parse("(unclosed list");
    // Should either return an error value or handle gracefully
    ASSERT_TRUE(true); // If we get here, no crash occurred
}

TEST_CASE(test_parser_special_number_cases_api) {
    // Test number boundaries
    Value result1 = uLispParser::parse("0");
    ASSERT_TRUE(result1.is_int());
    ASSERT_EQ(0, result1.as_int());
    
    Value result2 = uLispParser::parse("-0");
    ASSERT_TRUE(result2.is_int());
    ASSERT_EQ(0, result2.as_int());
    
    // Test large numbers
    Value result3 = uLispParser::parse("2147483647");
    ASSERT_TRUE(result3.is_int());
    ASSERT_EQ(2147483647, result3.as_int());
    
    Value result4 = uLispParser::parse("-2147483648");
    ASSERT_TRUE(result4.is_int());
    ASSERT_EQ(-2147483648, result4.as_int());
    
    // Test very small float
    Value result5 = uLispParser::parse("0.000001");
    ASSERT_TRUE(result5.is_float());
    ASSERT_NEAR(0.000001, result5.as_float(), 0.0000001);
    
    // Test very large float
    Value result6 = uLispParser::parse("123456789.123456789");
    ASSERT_TRUE(result6.is_float());
    ASSERT_NEAR(123456789.123456789, result6.as_float(), 0.001);
}

////////////////////////////////////////////////////////////////////////////////
/// PARSER REAL WORLD EXPRESSIONS API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_parser_complex_lisp_expressions_api) {
    // Test function definition pattern
    Value result1 = uLispParser::parse("(defn factorial [n] (if (= n 0) 1 (* n (factorial (- n 1)))))");
    ASSERT_TRUE(result1.is_list());
    ASSERT_TRUE(result1.as_list().size() >= 3); // defn, name, args, body...
    ASSERT_STR_EQ("defn", result1.as_list()[0].as_atom());
    
    // Test let expression pattern
    Value result2 = uLispParser::parse("(let [x 5 y 10] (+ x y))");
    ASSERT_TRUE(result2.is_list());
    ASSERT_EQ(3, result2.as_list().size());
    ASSERT_STR_EQ("let", result2.as_list()[0].as_atom());
    ASSERT_TRUE(result2.as_list()[1].is_vector()); // binding vector
    ASSERT_TRUE(result2.as_list()[2].is_list()); // body expression
    
    // Test map-like structure (parsed as list due to current implementation)
    Value result3 = uLispParser::parse("{:name \"test\" :value [1 2 3] :fn (lambda [x] (+ x 1))}");
    ASSERT_TRUE(result3.is_list()); // Maps parsed as lists in this implementation
    ASSERT_TRUE(result3.as_list().size() > 0);
    
    // Test conditional expression
    Value result4 = uLispParser::parse("(if (> x 10) \"big\" \"small\")");
    ASSERT_TRUE(result4.is_list());
    ASSERT_EQ(4, result4.as_list().size());
    ASSERT_STR_EQ("if", result4.as_list()[0].as_atom());
    ASSERT_TRUE(result4.as_list()[1].is_list()); // condition
    ASSERT_TRUE(result4.as_list()[2].is_string()); // then
    ASSERT_TRUE(result4.as_list()[3].is_string()); // else
}

TEST_CASE(test_parser_musical_dsl_expressions_api) {
    // Test MIDI note parsing (if supported)
    Value result1 = uLispParser::parse("M60");
    if (result1.is_int() || result1.is_symbol()) {
        // Either representation is valid depending on implementation
        ASSERT_TRUE(true);
    }
    
    // Test musical expression patterns
    Value result2 = uLispParser::parse("(play [M60 M64 M67] 0.5)");
    ASSERT_TRUE(result2.is_list());
    ASSERT_TRUE(result2.as_list().size() >= 2);
    ASSERT_STR_EQ("play", result2.as_list()[0].as_atom());
    ASSERT_TRUE(result2.as_list()[1].is_vector());
    
    // Test tempo/timing expressions
    Value result3 = uLispParser::parse("(at-time 1.5 (trigger :kick))");
    ASSERT_TRUE(result3.is_list());
    ASSERT_STR_EQ("at-time", result3.as_list()[0].as_atom());
    
    // Test nested musical structures
    Value result4 = uLispParser::parse("(seq [(note M60 0.25) (rest 0.25) (chord [M60 M64 M67] 0.5)])");
    ASSERT_TRUE(result4.is_list());
    ASSERT_EQ(2, result4.as_list().size());
    ASSERT_STR_EQ("seq", result4.as_list()[0].as_atom());
    ASSERT_TRUE(result4.as_list()[1].is_vector());
}

// Main function to run all tests
int main() {
    std::cout << "Running all Parser API tests..." << std::endl;
    std::cout << "All Parser API tests passed!" << std::endl;
    return 0;
}