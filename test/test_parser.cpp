#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"

#include "../uSEQ/src/modulisp/lisp/parser.h"
#include "../uSEQ/src/modulisp/lisp/value.h"
#include <cmath>

TEST_CASE("String unescaping functionality", "[parser][unescape]") {
    SECTION("basic unescaping") {
        // Test basic unescaping
        String result = uLispParser::unescape_static("hello\\nworld");
        REQUIRE(result == "hello\nworld");
        
        // Test quote unescaping
        String result2 = uLispParser::unescape_static("say \\\"hello\\\"");
        REQUIRE(result2 == "say \"hello\"");
        
        // Test carriage return and tab
        String result3 = uLispParser::unescape_static("line1\\r\\tline2");
        REQUIRE(result3 == "line1\r\tline2");
        
        // Test no escapes
        String result4 = uLispParser::unescape_static("plain text");
        REQUIRE(result4 == "plain text");
    }

    SECTION("edge cases") {
        // Test backslash at end (should be preserved)
        String result1 = uLispParser::unescape_static("text\\");
        REQUIRE(result1 == "text\\");
        
        // Test unknown escape sequence (should preserve the character)
        String result2 = uLispParser::unescape_static("test\\x");
        REQUIRE(result2 == "testx");
        
        // Test empty string
        String result3 = uLispParser::unescape_static("");
        REQUIRE(result3 == "");
        
        // Test multiple escapes
        String result4 = uLispParser::unescape_static("\\n\\r\\t\\\"");
        REQUIRE(result4 == "\n\r\t\"");
    }
}

TEST_CASE("Whitespace skipping", "[parser][whitespace]") {
    SECTION("skip various whitespace") {
        String test_str = "   \t\n  hello";
        int ptr = 0;
        uLispParser::skip_whitespace_static(test_str, ptr);
        REQUIRE(test_str[ptr] == 'h');
    }

    SECTION("skip commas treated as whitespace") {
        String test_str2 = " , , \t hello";
        int ptr2 = 0;
        uLispParser::skip_whitespace_static(test_str2, ptr2);
        REQUIRE(test_str2[ptr2] == 'h');
    }

    SECTION("no whitespace to skip") {
        String test_str3 = "immediate";
        int ptr3 = 0;
        uLispParser::skip_whitespace_static(test_str3, ptr3);
        REQUIRE(test_str3[ptr3] == 'i');
    }
}

TEST_CASE("Character type checking functions", "[parser][char_types]") {
    SECTION("symbol characters") {
        REQUIRE(uLispParser::is_symbol_static("abc", 0));
        REQUIRE(uLispParser::is_symbol_static("123", 0));
        REQUIRE(uLispParser::is_symbol_static("test+", 4));
        REQUIRE_FALSE(uLispParser::is_symbol_static("(hello", 0));
        REQUIRE_FALSE(uLispParser::is_symbol_static(")hello", 0));
        REQUIRE_FALSE(uLispParser::is_symbol_static("[hello", 0));
        REQUIRE_FALSE(uLispParser::is_symbol_static("]hello", 0));
        REQUIRE_FALSE(uLispParser::is_symbol_static("\"hello", 0));
        REQUIRE_FALSE(uLispParser::is_symbol_static("'hello", 0));
    }

    SECTION("other character types") {
        REQUIRE(uLispParser::is_comment_static(";comment", 0));
        REQUIRE_FALSE(uLispParser::is_comment_static("comment", 0));
        
        REQUIRE(uLispParser::is_quote_static("'quoted", 0));
        REQUIRE_FALSE(uLispParser::is_quote_static("quoted", 0));
        
        REQUIRE(uLispParser::is_list_static("(list)", 0));
        REQUIRE_FALSE(uLispParser::is_list_static("list", 0));
        
        REQUIRE(uLispParser::is_vector_static("[vector]", 0));
        REQUIRE_FALSE(uLispParser::is_vector_static("vector", 0));
        
        REQUIRE(uLispParser::is_map_static("{map}", 0));
        REQUIRE_FALSE(uLispParser::is_map_static("map", 0));
        
        REQUIRE(uLispParser::is_midinote_static("M60", 0));
        REQUIRE_FALSE(uLispParser::is_midinote_static("60", 0));
    }
}

TEST_CASE("Number parsing", "[parser][numbers]") {
    SECTION("integers") {
        // Test positive integer
        Value result1 = uLispParser::parse_static("42");
        REQUIRE(result1.is_int());
        REQUIRE(result1.as_int() == 42);
        
        // Test negative integer
        Value result2 = uLispParser::parse_static("-17");
        REQUIRE(result2.is_int());
        REQUIRE(result2.as_int() == -17);
        
        // Test zero
        Value result3 = uLispParser::parse_static("0");
        REQUIRE(result3.is_int());
        REQUIRE(result3.as_int() == 0);
    }

    SECTION("floats") {
        // Test positive float
        Value result1 = uLispParser::parse_static("3.14");
        REQUIRE(result1.is_float());
        REQUIRE(result1.as_float() == Approx(3.14).epsilon(0.001));
        
        // Test negative float
        Value result2 = uLispParser::parse_static("-2.71");
        REQUIRE(result2.is_float());
        REQUIRE(result2.as_float() == Approx(-2.71).epsilon(0.001));
        
        // Test float with leading zero
        Value result3 = uLispParser::parse_static("0.5");
        REQUIRE(result3.is_float());
        REQUIRE(result3.as_float() == Approx(0.5).epsilon(0.001));
        
        // Test float without leading zero
        Value result4 = uLispParser::parse_static(".25");
        REQUIRE(result4.is_float());
        REQUIRE(result4.as_float() == Approx(0.25).epsilon(0.001));
    }
}

TEST_CASE("String parsing", "[parser][strings]") {
    // Test basic string
    Value result1 = uLispParser::parse_static("\"hello world\"");
    REQUIRE(result1.is_string());
    REQUIRE(result1.as_string() == "hello world");
    
    // Test empty string
    Value result2 = uLispParser::parse_static("\"\"");
    REQUIRE(result2.is_string());
    REQUIRE(result2.as_string() == "");
    
    // Test string with escape sequences
    Value result3 = uLispParser::parse_static("\"line1\\nline2\"");
    REQUIRE(result3.is_string());
    REQUIRE(result3.as_string() == "line1\nline2");
    
    // Test string with quotes
    Value result4 = uLispParser::parse_static("\"say \\\"hello\\\"\"");
    REQUIRE(result4.is_string());
    REQUIRE(result4.as_string() == "say \"hello\"");
}

TEST_CASE("Symbol parsing", "[parser][symbols]") {
    // Test basic symbol
    Value result1 = uLispParser::parse_static("hello");
    REQUIRE(result1.is_symbol());
    REQUIRE(result1.as_atom() == "hello");
    
    // Test symbol with special characters
    Value result2 = uLispParser::parse_static("+");
    REQUIRE(result2.is_symbol());
    REQUIRE(result2.as_atom() == "+");
    
    // Test complex symbol
    Value result3 = uLispParser::parse_static("my-var-123");
    REQUIRE(result3.is_symbol());
    REQUIRE(result3.as_atom() == "my-var-123");
}

TEST_CASE("List parsing", "[parser][lists]") {
    SECTION("basic lists") {
        // Test empty list
        Value result1 = uLispParser::parse_static("()");
        REQUIRE(result1.is_list());
        REQUIRE(result1.as_list().size() == 0);
        
        // Test list with single element
        Value result2 = uLispParser::parse_static("(42)");
        REQUIRE(result2.is_list());
        REQUIRE(result2.as_list().size() == 1);
        REQUIRE(result2.as_list()[0].is_int());
        REQUIRE(result2.as_list()[0].as_int() == 42);
        
        // Test list with multiple elements
        Value result3 = uLispParser::parse_static("(1 2 3)");
        REQUIRE(result3.is_list());
        REQUIRE(result3.as_list().size() == 3);
        REQUIRE(result3.as_list()[0].as_int() == 1);
        REQUIRE(result3.as_list()[1].as_int() == 2);
        REQUIRE(result3.as_list()[2].as_int() == 3);
    }

    SECTION("mixed type lists") {
        // Test list with mixed types
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
}

TEST_CASE("Nested list parsing", "[parser][lists][nested]") {
    SECTION("simple nested lists") {
        // Test nested list
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
    }

    SECTION("deeply nested expressions") {
        // Test deeply nested
        Value result2 = uLispParser::parse_static("(+ (* 2 3) (/ 8 4))");
        REQUIRE(result2.is_list());
        REQUIRE(result2.as_list().size() == 3);
        REQUIRE(result2.as_list()[0].as_atom() == "+");
        
        // Check first nested expression (* 2 3)
        REQUIRE(result2.as_list()[1].is_list());
        REQUIRE(result2.as_list()[1].as_list().size() == 3);
        REQUIRE(result2.as_list()[1].as_list()[0].as_atom() == "*");
        
        // Check second nested expression (/ 8 4)
        REQUIRE(result2.as_list()[2].is_list());
        REQUIRE(result2.as_list()[2].as_list().size() == 3);
        REQUIRE(result2.as_list()[2].as_list()[0].as_atom() == "/");
    }
}

TEST_CASE("Vector parsing", "[parser][vectors]") {
    // Test empty vector
    Value result1 = uLispParser::parse_static("[]");
    REQUIRE(result1.is_vector());
    REQUIRE(result1.as_vector().size() == 0);
    
    // Test vector with elements
    Value result2 = uLispParser::parse_static("[1 2 3]");
    REQUIRE(result2.is_vector());
    REQUIRE(result2.as_vector().size() == 3);
    REQUIRE(result2.as_vector()[0].as_int() == 1);
    REQUIRE(result2.as_vector()[1].as_int() == 2);
    REQUIRE(result2.as_vector()[2].as_int() == 3);
    
    // Test vector with mixed types
    Value result3 = uLispParser::parse_static("[42 \"hello\" 3.14]");
    REQUIRE(result3.is_vector());
    REQUIRE(result3.as_vector().size() == 3);
    REQUIRE(result3.as_vector()[0].is_int());
    REQUIRE(result3.as_vector()[1].is_string());
    REQUIRE(result3.as_vector()[2].is_float());
}

TEST_CASE("Quote parsing", "[parser][quotes]") {
    // Test quoted number - parser creates QUOTE type, not LIST type
    // We can verify quotes work by checking their string representation
    Value result1 = uLispParser::parse_static("'42");
    REQUIRE(result1.to_lisp_src() == "'42");
    
    // Test quoted symbol
    Value result2 = uLispParser::parse_static("'hello");
    REQUIRE(result2.to_lisp_src() == "'hello");
    
    // Test quoted list
    Value result3 = uLispParser::parse_static("'(1 2 3)");
    REQUIRE(result3.to_lisp_src() == "'(1 2 3)");
}

TEST_CASE("Comment handling", "[parser][comments]") {
    // Test single line comment
    Value result1 = uLispParser::parse_static("; this is a comment\n42");
    REQUIRE(result1.is_int());
    REQUIRE(result1.as_int() == 42);
    
    // Test comment in middle
    Value result2 = uLispParser::parse_static("(+ 1 ; add one\n 2)");
    REQUIRE(result2.is_list());
    REQUIRE(result2.as_list().size() == 3);
    REQUIRE(result2.as_list()[0].as_atom() == "+");
    REQUIRE(result2.as_list()[1].as_int() == 1);
    REQUIRE(result2.as_list()[2].as_int() == 2);
}

TEST_CASE("Whitespace handling", "[parser][whitespace]") {
    // Test various whitespace
    Value result1 = uLispParser::parse_static("   \t\n  42   ");
    REQUIRE(result1.is_int());
    REQUIRE(result1.as_int() == 42);
    
    // Test list with whitespace
    Value result2 = uLispParser::parse_static("( +   1    2 )");
    REQUIRE(result2.is_list());
    REQUIRE(result2.as_list().size() == 3);
    REQUIRE(result2.as_list()[0].as_atom() == "+");
    
    // Test commas as whitespace
    Value result3 = uLispParser::parse_static("[1,2,3]");
    REQUIRE(result3.is_vector());
    REQUIRE(result3.as_vector().size() == 3);
    REQUIRE(result3.as_vector()[0].as_int() == 1);
    REQUIRE(result3.as_vector()[1].as_int() == 2);
    REQUIRE(result3.as_vector()[2].as_int() == 3);
}

TEST_CASE("Edge cases and error conditions", "[parser][edge_cases]") {
    // Test empty string
    Value result1 = uLispParser::parse_static("");
    // Empty string should return a unit/nil value
    REQUIRE_FALSE(result1.is_int());
    REQUIRE_FALSE(result1.is_string());
    
    // Test single whitespace
    Value result2 = uLispParser::parse_static("   ");
    REQUIRE_FALSE(result2.is_int());
    
    // Test just comment
    Value result3 = uLispParser::parse_static("; just a comment");
    REQUIRE_FALSE(result3.is_int());
    
    // Test number boundaries
    Value result4 = uLispParser::parse_static("0");
    REQUIRE(result4.is_int());
    REQUIRE(result4.as_int() == 0);
    
    Value result5 = uLispParser::parse_static("-0");
    REQUIRE(result5.is_int());
    REQUIRE(result5.as_int() == 0);
}

TEST_CASE("Complex real-world expressions", "[parser][complex]") {
    // Test function call with nested expressions
    Value result1 = uLispParser::parse_static("(defn factorial [n] (if (= n 0) 1 (* n (factorial (- n 1)))))");
    REQUIRE(result1.is_list());
    REQUIRE(result1.as_list().size() > 0);
    REQUIRE(result1.as_list()[0].as_atom() == "defn");
    
    // Test let expression
    Value result2 = uLispParser::parse_static("(let [x 5 y 10] (+ x y))");
    REQUIRE(result2.is_list());
    REQUIRE(result2.as_list().size() == 3);
    REQUIRE(result2.as_list()[0].as_atom() == "let");
    REQUIRE(result2.as_list()[1].is_vector());
    
    // Test mixed data structures
    Value result3 = uLispParser::parse_static("{:name \"test\" :value [1 2 3] :fn (lambda [x] (+ x 1))}");
    REQUIRE(result3.is_list());  // Maps are parsed as lists in this implementation
}

TEST_CASE("Parsing with pointer advancement", "[parser][pointer]") {
    String test_expr = "42 \"hello\" (+ 1 2)";
    int ptr = 0;
    
    // Parse first value
    Value result1 = uLispParser::parse_static(test_expr, ptr);
    REQUIRE(result1.is_int());
    REQUIRE(result1.as_int() == 42);
    
    // Parse second value
    Value result2 = uLispParser::parse_static(test_expr, ptr);
    REQUIRE(result2.is_string());
    REQUIRE(result2.as_string() == "hello");
    
    // Parse third value
    Value result3 = uLispParser::parse_static(test_expr, ptr);
    REQUIRE(result3.is_list());
    REQUIRE(result3.as_list().size() == 3);
    REQUIRE(result3.as_list()[0].as_atom() == "+");
}