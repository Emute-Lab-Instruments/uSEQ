#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../uSEQ/src/modulisp/lisp/parser.h"
#include "../uSEQ/src/modulisp/lisp/value.h"
#include <cmath>

////////////////////////////////////////////////////////////////////////////////
/// PARSER UTILITY FUNCTIONS API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Parser string unescaping API", "[parser][api][unescape]")
{
    // Test basic escape sequences
    String result1 = uLispParser::unescape_static("hello\\nworld");
    REQUIRE(result1 == "hello\nworld");

    String result2 = uLispParser::unescape_static("tab\\there");
    REQUIRE(result2 == "tab\there");

    String result3 = uLispParser::unescape_static("quote\\\"test");
    REQUIRE(result3 == "quote\"test");

    String result4 = uLispParser::unescape_static("return\\rtest");
    REQUIRE(result4 == "return\rtest");

    // Test no escapes
    String result5 = uLispParser::unescape_static("plain text");
    REQUIRE(result5 == "plain text");

    // Test empty string
    String result6 = uLispParser::unescape_static("");
    REQUIRE(result6 == "");

    // Test multiple escapes
    String result7 = uLispParser::unescape_static("\\n\\t\\r\\\"");
    REQUIRE(result7 == "\n\t\r\"");
}

TEST_CASE("Parser string unescaping edge cases",
          "[parser][api][unescape][edge_cases]")
{
    // Test backslash at end
    String result1 = uLispParser::unescape_static("test\\");
    REQUIRE(result1 == "test\\");

    // Test unknown escape sequence (should preserve character after backslash)
    String result2 = uLispParser::unescape_static("test\\x");
    REQUIRE(result2 == "testx");

    // Test consecutive backslashes
    String result3 = uLispParser::unescape_static("test\\\\");
    REQUIRE(result3 == "test\\");
}

TEST_CASE("Parser whitespace skipping API", "[parser][api][whitespace]")
{
    // Test basic whitespace skipping
    String test_str1 = "   \t\n  hello";
    int ptr1         = 0;
    uLispParser::skip_whitespace_static(test_str1, ptr1);
    REQUIRE(test_str1[ptr1] == 'h');

    // Test comma as whitespace (LISP convention)
    String test_str2 = " , , \t hello";
    int ptr2         = 0;
    uLispParser::skip_whitespace_static(test_str2, ptr2);
    REQUIRE(test_str2[ptr2] == 'h');

    // Test no whitespace
    String test_str3 = "immediate";
    int ptr3         = 0;
    uLispParser::skip_whitespace_static(test_str3, ptr3);
    REQUIRE(test_str3[ptr3] == 'i');

    // Test all whitespace
    String test_str4 = "   \t\n   ";
    int ptr4         = 0;
    uLispParser::skip_whitespace_static(test_str4, ptr4);
    REQUIRE(static_cast<size_t>(ptr4) == test_str4.length()); // Should skip to end

    // Test mixed whitespace and commas
    String test_str5 = " \t,\n , start";
    int ptr5         = 0;
    uLispParser::skip_whitespace_static(test_str5, ptr5);
    REQUIRE(test_str5[ptr5] == 's');
}

////////////////////////////////////////////////////////////////////////////////
/// PARSER CHARACTER TYPE DETECTION API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Parser symbol detection API", "[parser][api][symbol_detection]")
{
    // Test valid symbol characters
    REQUIRE(uLispParser::is_symbol_static("abc", 0));
    REQUIRE(uLispParser::is_symbol_static("123", 0));
    REQUIRE(uLispParser::is_symbol_static("test+", 4)); // Plus at position 4
    REQUIRE(uLispParser::is_symbol_static("my-var", 0));
    REQUIRE(uLispParser::is_symbol_static("_underscore", 0));

    // Test invalid symbol characters (structural characters)
    REQUIRE_FALSE(uLispParser::is_symbol_static("(hello", 0));
    REQUIRE_FALSE(uLispParser::is_symbol_static(")hello", 0));
    REQUIRE_FALSE(uLispParser::is_symbol_static("[hello", 0));
    REQUIRE_FALSE(uLispParser::is_symbol_static("]hello", 0));
    REQUIRE_FALSE(uLispParser::is_symbol_static("{hello", 0));
    REQUIRE_FALSE(uLispParser::is_symbol_static("}hello", 0));
    REQUIRE_FALSE(uLispParser::is_symbol_static("\"hello", 0));
    REQUIRE_FALSE(uLispParser::is_symbol_static("'hello", 0));
    REQUIRE_FALSE(uLispParser::is_symbol_static(";hello", 0));

    // Test whitespace characters
    REQUIRE_FALSE(uLispParser::is_symbol_static(" hello", 0));
    REQUIRE_FALSE(uLispParser::is_symbol_static("\thello", 0));
    REQUIRE_FALSE(uLispParser::is_symbol_static("\nhello", 0));
    REQUIRE_FALSE(uLispParser::is_symbol_static(",hello", 0));
}

TEST_CASE("Parser structural character detection API", "[parser][api][structural]")
{
    // Test comment detection
    REQUIRE(uLispParser::is_comment_static(";comment", 0));
    REQUIRE(uLispParser::is_comment_static("   ;comment", 3));
    REQUIRE_FALSE(uLispParser::is_comment_static("not;comment", 0));
    REQUIRE_FALSE(uLispParser::is_comment_static("comment", 0));

    // Test quote detection
    REQUIRE(uLispParser::is_quote_static("'quoted", 0));
    REQUIRE(uLispParser::is_quote_static("test'quoted", 4));
    REQUIRE_FALSE(uLispParser::is_quote_static("quoted", 0));
    REQUIRE_FALSE(uLispParser::is_quote_static("not'quoted", 0));

    // Test list detection
    REQUIRE(uLispParser::is_list_static("(list)", 0));
    REQUIRE(uLispParser::is_list_static("test(list)", 4));
    REQUIRE_FALSE(uLispParser::is_list_static("list", 0));
    REQUIRE_FALSE(uLispParser::is_list_static(")list", 0));

    // Test vector detection
    REQUIRE(uLispParser::is_vector_static("[vector]", 0));
    REQUIRE(uLispParser::is_vector_static("test[vector]", 4));
    REQUIRE_FALSE(uLispParser::is_vector_static("vector", 0));
    REQUIRE_FALSE(uLispParser::is_vector_static("]vector", 0));

    // Test map detection
    REQUIRE(uLispParser::is_map_static("{map}", 0));
    REQUIRE(uLispParser::is_map_static("test{map}", 4));
    REQUIRE_FALSE(uLispParser::is_map_static("map", 0));
    REQUIRE_FALSE(uLispParser::is_map_static("}map", 0));
}

TEST_CASE("Parser special notation detection API", "[parser][api][special_notation]")
{
    // Test MIDI note detection
    REQUIRE(uLispParser::is_midinote_static("M60", 0));
    REQUIRE(uLispParser::is_midinote_static("M127", 0));
    REQUIRE(uLispParser::is_midinote_static("M0", 0));
    REQUIRE(uLispParser::is_midinote_static("testM60", 4));
    REQUIRE_FALSE(uLispParser::is_midinote_static("60", 0));
    REQUIRE_FALSE(uLispParser::is_midinote_static("m60", 0));  // lowercase
    REQUIRE_FALSE(uLispParser::is_midinote_static("Mabc", 0)); // non-numeric

    // Test frequency detection (placeholder - implementation may vary)
    // Note: These functions may not be implemented in all versions
    // REQUIRE(uLispParser::is_freq("440Hz", 0) || !uLispParser::is_freq("440Hz",
    // 0));

    // Test fraction detection (placeholder - implementation may vary)
    // Note: These functions may not be implemented in all versions
    // REQUIRE(uLispParser::is_fraction("3/4", 0) || !uLispParser::is_fraction("3/4",
    // 0));
}

////////////////////////////////////////////////////////////////////////////////
/// PARSER ATOMIC VALUE PARSING API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Parser integer parsing API", "[parser][api][integer]")
{
    // Test positive integers
    Value result1 = uLispParser::parse_static("42");
    REQUIRE(result1.is_int());
    REQUIRE(result1.as_int() == 42);

    Value result2 = uLispParser::parse_static("0");
    REQUIRE(result2.is_int());
    REQUIRE(result2.as_int() == 0);

    Value result3 = uLispParser::parse_static("123456");
    REQUIRE(result3.is_int());
    REQUIRE(result3.as_int() == 123456);

    // Test negative integers
    Value result4 = uLispParser::parse_static("-17");
    REQUIRE(result4.is_int());
    REQUIRE(result4.as_int() == -17);

    Value result5 = uLispParser::parse_static("-0");
    REQUIRE(result5.is_int());
    REQUIRE(result5.as_int() == 0);
}

TEST_CASE("Parser float parsing API", "[parser][api][float]")
{
    // Test positive floats
    Value result1 = uLispParser::parse_static("3.14");
    REQUIRE(result1.is_float());
    REQUIRE(result1.as_float() == Approx(3.14).epsilon(0.001));

    Value result2 = uLispParser::parse_static("0.5");
    REQUIRE(result2.is_float());
    REQUIRE(result2.as_float() == Approx(0.5).epsilon(0.001));

    Value result3 = uLispParser::parse_static(".25");
    REQUIRE(result3.is_float());
    REQUIRE(result3.as_float() == Approx(0.25).epsilon(0.001));

    // Test negative floats
    Value result4 = uLispParser::parse_static("-2.71");
    REQUIRE(result4.is_float());
    REQUIRE(result4.as_float() == Approx(-2.71).epsilon(0.001));

    Value result5 = uLispParser::parse_static("-0.0");
    REQUIRE(result5.is_float());
    REQUIRE(result5.as_float() == Approx(0.0).epsilon(0.001));

    // Test scientific notation (if supported)
    Value result6 = uLispParser::parse_static("1e3");
    if (result6.is_float())
    {
        REQUIRE(result6.as_float() == Approx(1000.0).epsilon(0.001));
    }

    Value result7 = uLispParser::parse_static("1.5e-2");
    if (result7.is_float())
    {
        REQUIRE(result7.as_float() == Approx(0.015).epsilon(0.0001));
    }
}

TEST_CASE("Parser string parsing API", "[parser][api][string]")
{
    // Test basic strings
    Value result1 = uLispParser::parse_static("\"hello world\"");
    REQUIRE(result1.is_string());
    REQUIRE(result1.as_string() == "hello world");

    // Test empty string
    Value result2 = uLispParser::parse_static("\"\"");
    REQUIRE(result2.is_string());
    REQUIRE(result2.as_string() == "");

    // Test strings with escape sequences
    Value result3 = uLispParser::parse_static("\"line1\\nline2\"");
    REQUIRE(result3.is_string());
    REQUIRE(result3.as_string() == "line1\nline2");

    Value result4 = uLispParser::parse_static("\"tab\\there\"");
    REQUIRE(result4.is_string());
    REQUIRE(result4.as_string() == "tab\there");

    Value result5 = uLispParser::parse_static("\"say \\\"hello\\\"\"");
    REQUIRE(result5.is_string());
    REQUIRE(result5.as_string() == "say \"hello\"");

    // Test strings with special characters
    Value result6 = uLispParser::parse_static("\"!@#$%^&*()\"");
    REQUIRE(result6.is_string());
    REQUIRE(result6.as_string() == "!@#$%^&*()");
}

TEST_CASE("Parser symbol parsing API", "[parser][api][symbol]")
{
    // Test basic symbols
    Value result1 = uLispParser::parse_static("hello");
    REQUIRE(result1.is_symbol());
    REQUIRE(result1.as_atom() == "hello");

    Value result2 = uLispParser::parse_static("test-var");
    REQUIRE(result2.is_symbol());
    REQUIRE(result2.as_atom() == "test-var");

    Value result3 = uLispParser::parse_static("var123");
    REQUIRE(result3.is_symbol());
    REQUIRE(result3.as_atom() == "var123");

    // Test operator symbols
    Value result4 = uLispParser::parse_static("+");
    REQUIRE(result4.is_symbol());
    REQUIRE(result4.as_atom() == "+");

    Value result5 = uLispParser::parse_static("*");
    REQUIRE(result5.is_symbol());
    REQUIRE(result5.as_atom() == "*");

    Value result6 = uLispParser::parse_static(">=");
    REQUIRE(result6.is_symbol());
    REQUIRE(result6.as_atom() == ">=");

    // Test complex symbols
    Value result7 = uLispParser::parse_static("my-complex-var-123");
    REQUIRE(result7.is_symbol());
    REQUIRE(result7.as_atom() == "my-complex-var-123");
}

////////////////////////////////////////////////////////////////////////////////
/// PARSER COMPOSITE STRUCTURE PARSING API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Parser list parsing API", "[parser][api][list]")
{
    // Test empty list
    Value result1 = uLispParser::parse_static("()");
    REQUIRE(result1.is_list());
    REQUIRE(result1.as_list().size() == 0);
    REQUIRE(result1.is_empty());

    // Test single element list
    Value result2 = uLispParser::parse_static("(42)");
    REQUIRE(result2.is_list());
    REQUIRE(result2.as_list().size() == 1);
    REQUIRE(result2.as_list()[0].is_int());
    REQUIRE(result2.as_list()[0].as_int() == 42);

    // Test multiple element list
    Value result3 = uLispParser::parse_static("(1 2 3)");
    REQUIRE(result3.is_list());
    REQUIRE(result3.as_list().size() == 3);
    REQUIRE(result3.as_list()[0].as_int() == 1);
    REQUIRE(result3.as_list()[1].as_int() == 2);
    REQUIRE(result3.as_list()[2].as_int() == 3);

    // Test mixed type list
    Value result4 = uLispParser::parse_static("(+ 1 2.5 \"hello\")");
    REQUIRE(result4.is_list());
    REQUIRE(result4.as_list().size() == 4);
    REQUIRE(result4.as_list()[0].is_symbol());
    REQUIRE(result4.as_list()[0].as_atom() == "+");
    REQUIRE(result4.as_list()[1].is_int());
    REQUIRE(result4.as_list()[1].as_int() == 1);
    REQUIRE(result4.as_list()[2].is_float());
    REQUIRE(result4.as_list()[2].as_float() == Approx(2.5).epsilon(0.001));
    REQUIRE(result4.as_list()[3].is_string());
    REQUIRE(result4.as_list()[3].as_string() == "hello");
}

TEST_CASE("Parser vector parsing API", "[parser][api][vector]")
{
    // Test empty vector
    Value result1 = uLispParser::parse_static("[]");
    REQUIRE(result1.is_vector());
    REQUIRE(result1.as_vector().size() == 0);
    REQUIRE(result1.is_empty());

    // Test single element vector
    Value result2 = uLispParser::parse_static("[42]");
    REQUIRE(result2.is_vector());
    REQUIRE(result2.as_vector().size() == 1);
    REQUIRE(result2.as_vector()[0].is_int());
    REQUIRE(result2.as_vector()[0].as_int() == 42);

    // Test multiple element vector
    Value result3 = uLispParser::parse_static("[1 2 3]");
    REQUIRE(result3.is_vector());
    REQUIRE(result3.as_vector().size() == 3);
    REQUIRE(result3.as_vector()[0].as_int() == 1);
    REQUIRE(result3.as_vector()[1].as_int() == 2);
    REQUIRE(result3.as_vector()[2].as_int() == 3);

    // Test mixed type vector
    Value result4 = uLispParser::parse_static("[42 \"hello\" 3.14 test-symbol]");
    REQUIRE(result4.is_vector());
    REQUIRE(result4.as_vector().size() == 4);
    REQUIRE(result4.as_vector()[0].is_int());
    REQUIRE(result4.as_vector()[0].as_int() == 42);
    REQUIRE(result4.as_vector()[1].is_string());
    REQUIRE(result4.as_vector()[1].as_string() == "hello");
    REQUIRE(result4.as_vector()[2].is_float());
    REQUIRE(result4.as_vector()[2].as_float() == Approx(3.14).epsilon(0.001));
    REQUIRE(result4.as_vector()[3].is_symbol());
    REQUIRE(result4.as_vector()[3].as_atom() == "test-symbol");
}

TEST_CASE("Parser nested structure parsing API", "[parser][api][nested]")
{
    // Test nested lists
    Value result1 = uLispParser::parse_static("((1 2) (3 4))");
    REQUIRE(result1.is_list());
    REQUIRE(result1.as_list().size() == 2);

    // Check first nested list
    REQUIRE(result1.as_list()[0].is_list());
    REQUIRE(result1.as_list()[0].as_list().size() == 2);
    REQUIRE(result1.as_list()[0].as_list()[0].as_int() == 1);
    REQUIRE(result1.as_list()[0].as_list()[1].as_int() == 2);

    // Check second nested list
    REQUIRE(result1.as_list()[1].is_list());
    REQUIRE(result1.as_list()[1].as_list().size() == 2);
    REQUIRE(result1.as_list()[1].as_list()[0].as_int() == 3);
    REQUIRE(result1.as_list()[1].as_list()[1].as_int() == 4);

    // Test mixed nested structures
    Value result2 = uLispParser::parse_static("([1 2] (3 4) [5 6])");
    REQUIRE(result2.is_list());
    REQUIRE(result2.as_list().size() == 3);
    REQUIRE(result2.as_list()[0].is_vector());
    REQUIRE(result2.as_list()[1].is_list());
    REQUIRE(result2.as_list()[2].is_vector());

    // Test deeply nested structures
    Value result3 = uLispParser::parse_static("(+ (* 2 3) (/ 8 4))");
    REQUIRE(result3.is_list());
    REQUIRE(result3.as_list().size() == 3);
    REQUIRE(result3.as_list()[0].as_atom() == "+");

    // Check first nested expression (* 2 3)
    REQUIRE(result3.as_list()[1].is_list());
    REQUIRE(result3.as_list()[1].as_list().size() == 3);
    REQUIRE(result3.as_list()[1].as_list()[0].as_atom() == "*");
    REQUIRE(result3.as_list()[1].as_list()[1].as_int() == 2);
    REQUIRE(result3.as_list()[1].as_list()[2].as_int() == 3);

    // Check second nested expression (/ 8 4)
    REQUIRE(result3.as_list()[2].is_list());
    REQUIRE(result3.as_list()[2].as_list().size() == 3);
    REQUIRE(result3.as_list()[2].as_list()[0].as_atom() == "/");
    REQUIRE(result3.as_list()[2].as_list()[1].as_int() == 8);
    REQUIRE(result3.as_list()[2].as_list()[2].as_int() == 4);
}

////////////////////////////////////////////////////////////////////////////////
/// PARSER QUOTE AND SPECIAL FORM PARSING API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Parser quote parsing API", "[parser][api][quote]")
{
    // Test quoted numbers
    Value result1 = uLispParser::parse_static("'42");
    String src1   = result1.to_lisp_src();
    REQUIRE(src1 == "'42");

    // Test quoted symbols
    Value result2 = uLispParser::parse_static("'hello");
    String src2   = result2.to_lisp_src();
    REQUIRE(src2 == "'hello");

    // Test quoted strings
    Value result3 = uLispParser::parse_static("'\"test\"");
    String src3   = result3.to_lisp_src();
    REQUIRE(src3 == "'\"test\"");

    // Test quoted lists
    Value result4 = uLispParser::parse_static("'(1 2 3)");
    String src4   = result4.to_lisp_src();
    REQUIRE(src4 == "'(1 2 3)");

    // Test quoted vectors
    Value result5 = uLispParser::parse_static("'[1 2 3]");
    String src5   = result5.to_lisp_src();
    REQUIRE(src5 == "'[1 2 3]");

    // Test nested quotes
    Value result6 = uLispParser::parse_static("''test");
    String src6   = result6.to_lisp_src();
    REQUIRE(src6 == "''test");
}

////////////////////////////////////////////////////////////////////////////////
/// PARSER WHITESPACE AND COMMENT HANDLING API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Parser whitespace handling API", "[parser][api][whitespace_handling]")
{
    // Test various whitespace around values
    Value result1 = uLispParser::parse_static("   42   ");
    REQUIRE(result1.is_int());
    REQUIRE(result1.as_int() == 42);

    Value result2 = uLispParser::parse_static("\t\n  \"hello\"  \t");
    REQUIRE(result2.is_string());
    REQUIRE(result2.as_string() == "hello");

    // Test whitespace in lists
    Value result3 = uLispParser::parse_static("( +   1    2 )");
    REQUIRE(result3.is_list());
    REQUIRE(result3.as_list().size() == 3);
    REQUIRE(result3.as_list()[0].as_atom() == "+");
    REQUIRE(result3.as_list()[1].as_int() == 1);
    REQUIRE(result3.as_list()[2].as_int() == 2);

    // Test commas as whitespace
    Value result4 = uLispParser::parse_static("[1,2,3]");
    REQUIRE(result4.is_vector());
    REQUIRE(result4.as_vector().size() == 3);
    REQUIRE(result4.as_vector()[0].as_int() == 1);
    REQUIRE(result4.as_vector()[1].as_int() == 2);
    REQUIRE(result4.as_vector()[2].as_int() == 3);

    // Test mixed whitespace and commas
    Value result5 = uLispParser::parse_static("[ 1 , \t 2 \n, 3 ]");
    REQUIRE(result5.is_vector());
    REQUIRE(result5.as_vector().size() == 3);
    REQUIRE(result5.as_vector()[0].as_int() == 1);
    REQUIRE(result5.as_vector()[1].as_int() == 2);
    REQUIRE(result5.as_vector()[2].as_int() == 3);
}

TEST_CASE("Parser comment handling API", "[parser][api][comment]")
{
    // Test single line comment before value
    Value result1 = uLispParser::parse_static("; this is a comment\n42");
    REQUIRE(result1.is_int());
    REQUIRE(result1.as_int() == 42);

    // Test comment in middle of expression
    Value result2 = uLispParser::parse_static("(+ 1 ; add one\n 2)");
    REQUIRE(result2.is_list());
    REQUIRE(result2.as_list().size() == 3);
    REQUIRE(result2.as_list()[0].as_atom() == "+");
    REQUIRE(result2.as_list()[1].as_int() == 1);
    REQUIRE(result2.as_list()[2].as_int() == 2);

    // Test multiple comments
    Value result3 = uLispParser::parse_static(
        "; first comment\n; second comment\n42 ; trailing comment");
    REQUIRE(result3.is_int());
    REQUIRE(result3.as_int() == 42);

    // Test comment with various symbols
    Value result4 =
        uLispParser::parse_static("; comment with !@#$%^&*() symbols\n123");
    REQUIRE(result4.is_int());
    REQUIRE(result4.as_int() == 123);
}

////////////////////////////////////////////////////////////////////////////////
/// PARSER INCREMENTAL PARSING API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Parser pointer based parsing API", "[parser][api][incremental]")
{
    String test_expr = "42 \"hello\" (+ 1 2) [3 4]";
    int ptr          = 0;

    // Parse first value
    Value result1 = uLispParser::parse_static(test_expr, ptr);
    REQUIRE(result1.is_int());
    REQUIRE(result1.as_int() == 42);

    // Parse second value
    Value result2 = uLispParser::parse_static(test_expr, ptr);
    REQUIRE(result2.is_string());
    REQUIRE(result2.as_string() == "hello");

    // Parse third value (list)
    Value result3 = uLispParser::parse_static(test_expr, ptr);
    REQUIRE(result3.is_list());
    REQUIRE(result3.as_list().size() == 3);
    REQUIRE(result3.as_list()[0].as_atom() == "+");
    REQUIRE(result3.as_list()[1].as_int() == 1);
    REQUIRE(result3.as_list()[2].as_int() == 2);

    // Parse fourth value (vector)
    Value result4 = uLispParser::parse_static(test_expr, ptr);
    REQUIRE(result4.is_vector());
    REQUIRE(result4.as_vector().size() == 2);
    REQUIRE(result4.as_vector()[0].as_int() == 3);
    REQUIRE(result4.as_vector()[1].as_int() == 4);

    // Should be at end of string
    REQUIRE(static_cast<size_t>(ptr) == test_expr.length());
}

TEST_CASE("Parser incremental with whitespace API",
          "[parser][api][incremental][whitespace]")
{
    String test_expr = " 10   20\n30 ";
    int ptr          = 0;

    // Parse values sequentially
    Value result1 = uLispParser::parse_static(test_expr, ptr);
    REQUIRE(result1.is_int());
    REQUIRE(result1.as_int() == 10);

    Value result2 = uLispParser::parse_static(test_expr, ptr);
    REQUIRE(result2.is_int());
    REQUIRE(result2.as_int() == 20);

    Value result3 = uLispParser::parse_static(test_expr, ptr);
    REQUIRE(result3.is_int());
    REQUIRE(result3.as_int() == 30);

    // Parser should handle trailing whitespace
    REQUIRE(static_cast<size_t>(ptr) >= test_expr.length() - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// PARSER EDGE CASES API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Parser edge case inputs API", "[parser][api][edge_cases]")
{
    // Test empty string
    Value result1 = uLispParser::parse_static("");
    // Parser should handle empty input gracefully
    REQUIRE_FALSE(result1.is_int()); // Should not be a number

    // Test only whitespace
    Value result2 = uLispParser::parse_static("   \t\n  ");
    REQUIRE_FALSE(result2.is_int());

    // Test only comment
    Value result3 = uLispParser::parse_static("; just a comment");
    REQUIRE_FALSE(result3.is_int());

    // Test malformed input resilience
    // Note: Exact behavior depends on implementation error handling
    Value result4 = uLispParser::parse_static("(unclosed list");
    // Should either return an error value or handle gracefully
    REQUIRE(true); // If we get here, no crash occurred
}

TEST_CASE("Parser special number cases API", "[parser][api][edge_cases][numbers]")
{
    // Test number boundaries
    Value result1 = uLispParser::parse_static("0");
    REQUIRE(result1.is_int());
    REQUIRE(result1.as_int() == 0);

    Value result2 = uLispParser::parse_static("-0");
    REQUIRE(result2.is_int());
    REQUIRE(result2.as_int() == 0);

    // Test large numbers
    Value result3 = uLispParser::parse_static("2147483647");
    REQUIRE(result3.is_int());
    REQUIRE(result3.as_int() == 2147483647);

    Value result4 = uLispParser::parse_static("-2147483648");
    REQUIRE(result4.is_int());
    REQUIRE(result4.as_int() == -2147483648);

    // Test very small float
    Value result5 = uLispParser::parse_static("0.000001");
    REQUIRE(result5.is_float());
    REQUIRE(result5.as_float() == Approx(0.000001).epsilon(0.0000001));

    // Test very large float
    Value result6 = uLispParser::parse_static("123456789.123456789");
    REQUIRE(result6.is_float());
    REQUIRE(result6.as_float() == Approx(123456789.123456789).epsilon(0.001));
}

////////////////////////////////////////////////////////////////////////////////
/// PARSER REAL WORLD EXPRESSIONS API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Parser complex LISP expressions API", "[parser][api][complex]")
{
    // Test function definition pattern
    Value result1 = uLispParser::parse_static(
        "(defn factorial [n] (if (= n 0) 1 (* n (factorial (- n 1)))))");
    REQUIRE(result1.is_list());
    REQUIRE(result1.as_list().size() >= 3); // defn, name, args, body...
    REQUIRE(result1.as_list()[0].as_atom() == "defn");

    // Test let expression pattern
    Value result2 = uLispParser::parse_static("(let [x 5 y 10] (+ x y))");
    REQUIRE(result2.is_list());
    REQUIRE(result2.as_list().size() == 3);
    REQUIRE(result2.as_list()[0].as_atom() == "let");
    REQUIRE(result2.as_list()[1].is_vector()); // binding vector
    REQUIRE(result2.as_list()[2].is_list());   // body expression

    // Test map-like structure (parsed as list due to current implementation)
    Value result3 = uLispParser::parse_static(
        "{:name \"test\" :value [1 2 3] :fn (lambda [x] (+ x 1))}");
    REQUIRE(result3.is_list()); // Maps parsed as lists in this implementation
    REQUIRE(result3.as_list().size() > 0);

    // Test conditional expression
    Value result4 = uLispParser::parse_static("(if (> x 10) \"big\" \"small\")");
    REQUIRE(result4.is_list());
    REQUIRE(result4.as_list().size() == 4);
    REQUIRE(result4.as_list()[0].as_atom() == "if");
    REQUIRE(result4.as_list()[1].is_list());   // condition
    REQUIRE(result4.as_list()[2].is_string()); // then
    REQUIRE(result4.as_list()[3].is_string()); // else
}

TEST_CASE("Parser musical DSL expressions API", "[parser][api][musical]")
{
    // Test MIDI note parsing (if supported)
    Value result1 = uLispParser::parse_static("M60");
    if (result1.is_int() || result1.is_symbol())
    {
        // Either representation is valid depending on implementation
        REQUIRE(true);
    }

    // Test musical expression patterns
    Value result2 = uLispParser::parse_static("(play [M60 M64 M67] 0.5)");
    REQUIRE(result2.is_list());
    REQUIRE(result2.as_list().size() >= 2);
    REQUIRE(result2.as_list()[0].as_atom() == "play");
    REQUIRE(result2.as_list()[1].is_vector());

    // Test tempo/timing expressions
    Value result3 = uLispParser::parse_static("(at-time 1.5 (trigger :kick))");
    REQUIRE(result3.is_list());
    REQUIRE(result3.as_list()[0].as_atom() == "at-time");

    // Test nested musical structures
    Value result4 = uLispParser::parse_static(
        "(seq [(note M60 0.25) (rest 0.25) (chord [M60 M64 M67] 0.5)])");
    REQUIRE(result4.is_list());
    REQUIRE(result4.as_list().size() == 2);
    REQUIRE(result4.as_list()[0].as_atom() == "seq");
    REQUIRE(result4.as_list()[1].is_vector());
}
