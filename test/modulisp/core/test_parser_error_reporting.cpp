#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../uSEQ/src/modulisp/diagnostic.h"
#include "../uSEQ/src/modulisp/lisp/parser.h"
#include "../uSEQ/src/modulisp/lisp/value.h"

// Helper: check if any diagnostic has Error severity
static bool has_error(const std::vector<Diagnostic>& diags)
{
    for (const auto& d : diags)
    {
        if (d.severity == DiagnosticSeverity::Error)
            return true;
    }
    return false;
}

// Helper: get the first error-severity diagnostic
static const Diagnostic* first_error(const std::vector<Diagnostic>& diags)
{
    for (const auto& d : diags)
    {
        if (d.severity == DiagnosticSeverity::Error)
            return &d;
    }
    return nullptr;
}

// Helper: check if any error diagnostic contains a substring
static bool any_error_contains(const std::vector<Diagnostic>& diags,
                               const char* substr)
{
    for (const auto& d : diags)
    {
        if (d.severity == DiagnosticSeverity::Error &&
            d.message.indexOf(substr) >= 0)
        {
            return true;
        }
    }
    return false;
}

// Test cases for Parser error reporting using Diagnostic vector

TEST_CASE("Parser reports unmatched closing parenthesis", "[parser][error][syntax]")
{
    std::vector<Diagnostic> diags;
    uLispParser parser(&diags);

    // Test unmatched closing parenthesis
    Value result = parser.parse("(+ 1 2))");

    REQUIRE(result.is_error());
    REQUIRE(has_error(diags));

    const Diagnostic* err = first_error(diags);
    REQUIRE(err != nullptr);
    REQUIRE(err->category == DiagnosticCategory::Syntax);
    // The parser now reports "could not parse the entire program" for trailing chars
    REQUIRE(err->message.length() > 0);
}

TEST_CASE("Parser reports unexpected end of input for incomplete expressions",
          "[parser][error][syntax]")
{
    std::vector<Diagnostic> diags;
    uLispParser parser(&diags);

    SECTION("Incomplete list")
    {
        Value result = parser.parse("(+ 1 2");

        REQUIRE(result.is_error());
        REQUIRE(has_error(diags));

        const Diagnostic* err = first_error(diags);
        REQUIRE(err != nullptr);
        REQUIRE(err->category == DiagnosticCategory::Syntax);
        REQUIRE(err->message.length() > 0);
    }

    SECTION("Incomplete nested list")
    {
        diags.clear();
        Value result = parser.parse("(+ 1 (- 3");

        REQUIRE(result.is_error());
        REQUIRE(has_error(diags));

        const Diagnostic* err = first_error(diags);
        REQUIRE(err != nullptr);
        REQUIRE(err->category == DiagnosticCategory::Syntax);
        REQUIRE(err->message.length() > 0);
    }
}

TEST_CASE("Parser reports unterminated string literals", "[parser][error][syntax]")
{
    std::vector<Diagnostic> diags;
    uLispParser parser(&diags);

    SECTION("Missing closing quote")
    {
        Value result = parser.parse("\"hello world");

        REQUIRE(result.is_error());
        REQUIRE(has_error(diags));
        REQUIRE(any_error_contains(diags, "string"));
    }

    SECTION("String in expression with missing quote")
    {
        diags.clear();
        Value result = parser.parse("(print \"hello)");

        REQUIRE(result.is_error());
        REQUIRE(has_error(diags));
        REQUIRE(any_error_contains(diags, "string"));
    }
}

TEST_CASE("Parser reports invalid input tokens", "[parser][error][syntax]")
{
    std::vector<Diagnostic> diags;
    uLispParser parser(&diags);

    SECTION("Bare closing paren is invalid")
    {
        // A bare ')' cannot be parsed by any branch in the inner parser
        Value result = parser.parse(")");

        REQUIRE(result.is_error());
        REQUIRE(has_error(diags));

        const Diagnostic* err = first_error(diags);
        REQUIRE(err != nullptr);
        REQUIRE(err->category == DiagnosticCategory::Syntax);
        REQUIRE(err->message.length() > 0);
    }

    SECTION("Bare closing bracket is invalid")
    {
        diags.clear();
        Value result = parser.parse("]");

        REQUIRE(result.is_error());
        REQUIRE(has_error(diags));

        const Diagnostic* err = first_error(diags);
        REQUIRE(err != nullptr);
        REQUIRE(err->category == DiagnosticCategory::Syntax);
        REQUIRE(err->message.length() > 0);
    }
}

TEST_CASE("Parser clears previous errors on new parse", "[parser][error][state]")
{
    std::vector<Diagnostic> diags;
    uLispParser parser(&diags);

    // First, create an error
    Value result1 = parser.parse("(+ 1 2))");
    REQUIRE(result1.is_error());
    REQUIRE(has_error(diags));

    // The diagnostics vector should still have the error (parser doesn't auto-clear)
    REQUIRE(has_error(diags));

    // Clear manually and parse valid expression
    diags.clear();
    Value result2 = parser.parse("(+ 1 2)");
    REQUIRE(!result2.is_error());
    REQUIRE(!has_error(diags));
}

TEST_CASE("Parser handles complex nested error scenarios",
          "[parser][error][complex]")
{
    std::vector<Diagnostic> diags;
    uLispParser parser(&diags);

    SECTION("Nested lists with unmatched parens")
    {
        Value result = parser.parse("(+ (- 3 4) (/ 5 6)))");

        REQUIRE(result.is_error());
        REQUIRE(has_error(diags));

        const Diagnostic* err = first_error(diags);
        REQUIRE(err != nullptr);
        REQUIRE(err->category == DiagnosticCategory::Syntax);
    }

    SECTION("Mixed quote and paren errors")
    {
        diags.clear();
        Value result = parser.parse("(print \"hello (+ 1 2)");

        REQUIRE(result.is_error());
        REQUIRE(has_error(diags));

        // The unterminated string diagnostic should be among the errors
        REQUIRE(any_error_contains(diags, "string"));
    }
}

TEST_CASE("Parser works correctly with no diagnostics vector",
          "[parser][error][compatibility]")
{
    // Test backward compatibility - parser should not crash with nullptr
    uLispParser parser_no_diags(nullptr);

    SECTION("Valid expression with no diagnostics")
    {
        Value result = parser_no_diags.parse("(+ 1 2)");
        REQUIRE(!result.is_error());
        REQUIRE(result.is_list());
    }

    SECTION("Invalid expression with no diagnostics")
    {
        Value result = parser_no_diags.parse("(+ 1 2))");
        // Should return error but not crash
        REQUIRE(result.is_error());
    }
}

TEST_CASE("Parser error reporting preserves diagnostic context",
          "[parser][error][context]")
{
    std::vector<Diagnostic> diags;
    uLispParser parser(&diags);

    // Parse an invalid expression
    Value result = parser.parse("(+ 1 2))");

    REQUIRE(result.is_error());
    REQUIRE(has_error(diags));

    const Diagnostic* err = first_error(diags);
    REQUIRE(err != nullptr);

    // Verify diagnostic context is preserved
    REQUIRE(err->category == DiagnosticCategory::Syntax);
    REQUIRE(err->message.length() > 0);

    // The diagnostic should persist until manually cleared
    REQUIRE(has_error(diags));
    diags.clear();
    REQUIRE(!has_error(diags));
}

TEST_CASE("Parser handles edge cases with minimal input", "[parser][error][edge]")
{
    std::vector<Diagnostic> diags;
    uLispParser parser(&diags);

    SECTION("Empty string")
    {
        Value result = parser.parse("");
        // Empty string should not produce an error
        REQUIRE(!result.is_error());
        REQUIRE(!has_error(diags));
    }

    SECTION("Whitespace only")
    {
        // Whitespace-only input may or may not error depending on parser internals;
        // we just verify it doesn't crash.
        Value result = parser.parse("   \n\t  ");
        (void)result;
    }

    SECTION("Single closing paren")
    {
        Value result = parser.parse(")");
        REQUIRE(result.is_error());
        REQUIRE(has_error(diags));

        const Diagnostic* err = first_error(diags);
        REQUIRE(err != nullptr);
        REQUIRE(err->category == DiagnosticCategory::Syntax);
    }

    SECTION("Single double-quote")
    {
        diags.clear();
        Value result = parser.parse("\"");
        REQUIRE(result.is_error());
        REQUIRE(has_error(diags));

        REQUIRE(any_error_contains(diags, "string"));
    }
}

TEST_CASE("Parser error messages are descriptive and helpful",
          "[parser][error][usability]")
{
    std::vector<Diagnostic> diags;
    uLispParser parser(&diags);

    SECTION("Error messages are not empty")
    {
        Value result = parser.parse("(invalid");
        REQUIRE(result.is_error());
        REQUIRE(has_error(diags));

        const Diagnostic* err = first_error(diags);
        REQUIRE(err != nullptr);
        REQUIRE(err->message.length() > 0);
    }

    SECTION("Different errors have different messages")
    {
        // Test unmatched paren error
        Value result1 = parser.parse(")");
        REQUIRE(result1.is_error());
        REQUIRE(has_error(diags));

        const Diagnostic* err1 = first_error(diags);
        String message1        = err1->message;

        // Clear and test unterminated string error
        diags.clear();
        Value result2 = parser.parse("\"test");
        REQUIRE(result2.is_error());
        REQUIRE(has_error(diags));

        const Diagnostic* err2 = first_error(diags);
        String message2        = err2->message;

        // Messages should be different for different error types
        REQUIRE(message1 != message2);
        REQUIRE(message1.length() > 0);
        REQUIRE(message2.length() > 0);
    }
}

TEST_CASE("Diagnostics include suggestions", "[parser][error][suggestions]")
{
    std::vector<Diagnostic> diags;
    uLispParser parser(&diags);

    Value result = parser.parse("\"unterminated string");
    REQUIRE(result.is_error());

    const Diagnostic* err = first_error(diags);
    REQUIRE(err != nullptr);
    REQUIRE(err->suggestion.length() > 0);
}

TEST_CASE("Parser populates SourceSpan on Value nodes", "[parser][span]")
{
    uLispParser parser;

    SECTION("Simple list: (sin beat)")
    {
        // Input: "(sin beat)"
        //         0123456789
        Value result = parser.parse("(sin beat)");

        REQUIRE(result.is_list());
        // Outer list spans the whole string
        REQUIRE(result.span.start == 0);
        REQUIRE(result.span.end == 10);

        auto elems = result.as_list();
        REQUIRE(elems.size() == 2);

        // "sin" at positions 1..3
        REQUIRE(elems[0].is_symbol());
        REQUIRE(elems[0].span.start == 1);
        REQUIRE(elems[0].span.end == 4);

        // "beat" at positions 5..8
        REQUIRE(elems[1].is_symbol());
        REQUIRE(elems[1].span.start == 5);
        REQUIRE(elems[1].span.end == 9);
    }

    SECTION("Number literals")
    {
        // Input: "(+ 42 3.14)"
        //         0123456789AB  (A=10, B=11)
        Value result = parser.parse("(+ 42 3.14)");
        REQUIRE(result.is_list());

        auto elems = result.as_list();
        REQUIRE(elems.size() == 3);

        // "+" at position 1
        REQUIRE(elems[0].span.start == 1);
        REQUIRE(elems[0].span.end == 2);

        // "42" at positions 3..4
        REQUIRE(elems[1].span.start == 3);
        REQUIRE(elems[1].span.end == 5);

        // "3.14" at positions 6..9
        REQUIRE(elems[2].span.start == 6);
        REQUIRE(elems[2].span.end == 10);
    }

    SECTION("String literal")
    {
        // Input: "\"hello\""
        //         0123456
        Value result = parser.parse("\"hello\"");
        REQUIRE(result.is_string());
        REQUIRE(result.span.start == 0);
        REQUIRE(result.span.end == 7);
    }

    SECTION("Quoted form")
    {
        // Input: "'foo"
        //         0123
        Value result = parser.parse("'foo");
        REQUIRE(result.span.start == 0);
        REQUIRE(result.span.end == 4);
    }

    SECTION("Vector literal")
    {
        // Input: "[1 2]"
        //         01234
        Value result = parser.parse("[1 2]");
        REQUIRE(result.is_vector());
        REQUIRE(result.span.start == 0);
        REQUIRE(result.span.end == 5);
    }

    SECTION("Multi-expression implicit do gets zero span")
    {
        // When multiple top-level forms are wrapped in implicit do,
        // the synthesised wrapper should have span {0,0}
        Value result = parser.parse("1 2");
        REQUIRE(result.is_list());
        REQUIRE(result.span.start == 0);
        REQUIRE(result.span.end == 0);
    }

    SECTION("Nested list spans")
    {
        // Input: "(+ (- 1 2) 3)"
        //         0123456789012
        //         length = 13
        Value result = parser.parse("(+ (- 1 2) 3)");
        REQUIRE(result.is_list());
        REQUIRE(result.span.start == 0);
        REQUIRE(result.span.end == 13);

        auto elems = result.as_list();
        REQUIRE(elems.size() == 3);

        // Inner list "(- 1 2)" at positions 3..9
        REQUIRE(elems[1].is_list());
        REQUIRE(elems[1].span.start == 3);
        REQUIRE(elems[1].span.end == 10);
    }
}
