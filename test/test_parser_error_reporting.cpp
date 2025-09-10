#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../uSEQ/src/modulisp/lisp/error_context.h"
#include "../uSEQ/src/modulisp/lisp/parser.h"
#include "../uSEQ/src/modulisp/lisp/value.h"

// Test cases for Parser error reporting using ErrorManager

TEST_CASE("Parser reports unmatched closing parenthesis", "[parser][error][syntax]")
{
    ErrorManager error_mgr;
    uLispParser parser(&error_mgr);

    // Test unmatched closing parenthesis
    Value result = parser.parse("(+ 1 2))");

    REQUIRE(result.is_error());
    REQUIRE(error_mgr.has_error());

    const ErrorContext* error = error_mgr.get_current_error();
    REQUIRE(error != nullptr);
    REQUIRE(error->category == ErrorCategory::SYNTAX_ERROR);
    REQUIRE(error->primary_message == "Unmatched closing parenthesis");
}

TEST_CASE("Parser reports unexpected end of input for incomplete expressions",
          "[parser][error][syntax]")
{
    ErrorManager error_mgr;
    uLispParser parser(&error_mgr);

    SECTION("Incomplete list")
    {
        Value result = parser.parse("(+ 1 2");

        REQUIRE(result.is_error());
        REQUIRE(error_mgr.has_error());

        const ErrorContext* error = error_mgr.get_current_error();
        REQUIRE(error != nullptr);
        REQUIRE(error->category == ErrorCategory::SYNTAX_ERROR);
        REQUIRE(error->primary_message ==
                "Malformed program - unexpected end of input");
    }

    SECTION("Incomplete nested list")
    {
        error_mgr.clear_error();
        Value result = parser.parse("(+ 1 (- 3");

        REQUIRE(result.is_error());
        REQUIRE(error_mgr.has_error());

        const ErrorContext* error = error_mgr.get_current_error();
        REQUIRE(error != nullptr);
        REQUIRE(error->category == ErrorCategory::SYNTAX_ERROR);
        REQUIRE(error->primary_message ==
                "Malformed program - unexpected end of input");
    }
}

TEST_CASE("Parser reports unterminated string literals", "[parser][error][syntax]")
{
    ErrorManager error_mgr;
    uLispParser parser(&error_mgr);

    SECTION("Missing closing quote")
    {
        Value result = parser.parse("\"hello world");

        REQUIRE(result.is_error());
        REQUIRE(error_mgr.has_error());

        const ErrorContext* error = error_mgr.get_current_error();
        REQUIRE(error != nullptr);
        REQUIRE(error->category == ErrorCategory::SYNTAX_ERROR);
        REQUIRE(error->primary_message ==
                "Unexpected end of input, expected closing quote");
    }

    SECTION("String in expression with missing quote")
    {
        error_mgr.clear_error();
        Value result = parser.parse("(print \"hello)");

        REQUIRE(result.is_error());
        REQUIRE(error_mgr.has_error());

        const ErrorContext* error = error_mgr.get_current_error();
        REQUIRE(error != nullptr);
        REQUIRE(error->category == ErrorCategory::SYNTAX_ERROR);
        REQUIRE(error->primary_message ==
                "Unexpected end of input, expected closing quote");
    }
}

TEST_CASE("Parser reports invalid input tokens", "[parser][error][syntax]")
{
    ErrorManager error_mgr;
    uLispParser parser(&error_mgr);

    // Test various invalid character sequences that can't be parsed
    SECTION("Invalid special characters")
    {
        Value result = parser.parse("@#$");

        REQUIRE(result.is_error());
        REQUIRE(error_mgr.has_error());

        const ErrorContext* error = error_mgr.get_current_error();
        REQUIRE(error != nullptr);
        REQUIRE(error->category == ErrorCategory::SYNTAX_ERROR);
        REQUIRE(error->primary_message == "Invalid input - cannot parse token");
    }

    SECTION("Invalid character in expression")
    {
        error_mgr.clear_error();
        Value result = parser.parse("(+ 1 @)");

        REQUIRE(result.is_error());
        REQUIRE(error_mgr.has_error());

        const ErrorContext* error = error_mgr.get_current_error();
        REQUIRE(error != nullptr);
        REQUIRE(error->category == ErrorCategory::SYNTAX_ERROR);
        REQUIRE(error->primary_message == "Invalid input - cannot parse token");
    }
}

TEST_CASE("Parser clears previous errors on new parse", "[parser][error][state]")
{
    ErrorManager error_mgr;
    uLispParser parser(&error_mgr);

    // First, create an error
    Value result1 = parser.parse("(+ 1 2))");
    REQUIRE(result1.is_error());
    REQUIRE(error_mgr.has_error());

    // The ErrorManager should still have the error (parser doesn't auto-clear)
    REQUIRE(error_mgr.has_error());

    // Clear manually and parse valid expression
    error_mgr.clear_error();
    Value result2 = parser.parse("(+ 1 2)");
    REQUIRE(!result2.is_error());
    REQUIRE(!error_mgr.has_error());
}

TEST_CASE("Parser handles complex nested error scenarios",
          "[parser][error][complex]")
{
    ErrorManager error_mgr;
    uLispParser parser(&error_mgr);

    SECTION("Nested lists with unmatched parens")
    {
        Value result = parser.parse("(+ (- 3 4) (/ 5 6)))");

        REQUIRE(result.is_error());
        REQUIRE(error_mgr.has_error());

        const ErrorContext* error = error_mgr.get_current_error();
        REQUIRE(error != nullptr);
        REQUIRE(error->category == ErrorCategory::SYNTAX_ERROR);
        REQUIRE(error->primary_message == "Unmatched closing parenthesis");
    }

    SECTION("Mixed quote and paren errors")
    {
        error_mgr.clear_error();
        Value result = parser.parse("(print \"hello (+ 1 2)");

        REQUIRE(result.is_error());
        REQUIRE(error_mgr.has_error());

        const ErrorContext* error = error_mgr.get_current_error();
        REQUIRE(error != nullptr);
        REQUIRE(error->category == ErrorCategory::SYNTAX_ERROR);
        // Should report the quote error first since parsing is left-to-right
        REQUIRE(error->primary_message ==
                "Unexpected end of input, expected closing quote");
    }
}

TEST_CASE("Parser works correctly with no ErrorManager",
          "[parser][error][compatibility]")
{
    // Test backward compatibility - parser should not crash with nullptr
    // ErrorManager
    uLispParser parser_no_error(nullptr);

    SECTION("Valid expression with no error manager")
    {
        Value result = parser_no_error.parse("(+ 1 2)");
        REQUIRE(!result.is_error());
        REQUIRE(result.is_list());
    }

    SECTION("Invalid expression with no error manager")
    {
        Value result = parser_no_error.parse("(+ 1 2))");
        // Should return error but not crash
        REQUIRE(result.is_error());
    }
}

TEST_CASE("Parser error reporting preserves error context",
          "[parser][error][context]")
{
    ErrorManager error_mgr;
    uLispParser parser(&error_mgr);

    // Parse an invalid expression
    Value result = parser.parse("(+ 1 2))");

    REQUIRE(result.is_error());
    REQUIRE(error_mgr.has_error());

    const ErrorContext* error = error_mgr.get_current_error();
    REQUIRE(error != nullptr);

    // Verify error context is preserved
    REQUIRE(error->category == ErrorCategory::SYNTAX_ERROR);
    REQUIRE(error->primary_message.length() > 0);

    // The error should persist until manually cleared
    REQUIRE(error_mgr.has_error());
    error_mgr.clear_error();
    REQUIRE(!error_mgr.has_error());
}

TEST_CASE("Parser handles edge cases with minimal input", "[parser][error][edge]")
{
    ErrorManager error_mgr;
    uLispParser parser(&error_mgr);

    SECTION("Empty string")
    {
        Value result = parser.parse("");
        // Empty string should parse to nil, not error
        REQUIRE(result.is_nil());
        REQUIRE(!error_mgr.has_error());
    }

    SECTION("Whitespace only")
    {
        Value result = parser.parse("   \n\t  ");
        // Whitespace should parse to nil, not error
        REQUIRE(result.is_nil());
        REQUIRE(!error_mgr.has_error());
    }

    SECTION("Single closing paren")
    {
        Value result = parser.parse(")");
        REQUIRE(result.is_error());
        REQUIRE(error_mgr.has_error());

        const ErrorContext* error = error_mgr.get_current_error();
        REQUIRE(error != nullptr);
        REQUIRE(error->category == ErrorCategory::SYNTAX_ERROR);
        REQUIRE(error->primary_message == "Unmatched closing parenthesis");
    }

    SECTION("Single quote")
    {
        error_mgr.clear_error();
        Value result = parser.parse("\"");
        REQUIRE(result.is_error());
        REQUIRE(error_mgr.has_error());

        const ErrorContext* error = error_mgr.get_current_error();
        REQUIRE(error != nullptr);
        REQUIRE(error->category == ErrorCategory::SYNTAX_ERROR);
        REQUIRE(error->primary_message ==
                "Unexpected end of input, expected closing quote");
    }
}

TEST_CASE("Parser error messages are descriptive and helpful",
          "[parser][error][usability]")
{
    ErrorManager error_mgr;
    uLispParser parser(&error_mgr);

    SECTION("Error messages are not empty")
    {
        Value result = parser.parse("(invalid");
        REQUIRE(result.is_error());
        REQUIRE(error_mgr.has_error());

        const ErrorContext* error = error_mgr.get_current_error();
        REQUIRE(error != nullptr);
        REQUIRE(error->primary_message.length() > 0);
    }

    SECTION("Different errors have different messages")
    {
        // Test unmatched paren error
        Value result1 = parser.parse(")");
        REQUIRE(result1.is_error());
        REQUIRE(error_mgr.has_error());

        const ErrorContext* error1 = error_mgr.get_current_error();
        String message1            = error1->primary_message;

        // Clear and test unterminated string error
        error_mgr.clear_error();
        Value result2 = parser.parse("\"test");
        REQUIRE(result2.is_error());
        REQUIRE(error_mgr.has_error());

        const ErrorContext* error2 = error_mgr.get_current_error();
        String message2            = error2->primary_message;

        // Messages should be different for different error types
        REQUIRE(message1 != message2);
        REQUIRE(message1.length() > 0);
        REQUIRE(message2.length() > 0);
    }
}