#define CATCH_CONFIG_MAIN
#include "../uSEQ/src/modulisp/modulisp.h"
#include "catch.hpp"
#include <algorithm>
#include <string>
#include <vector>

// Test comprehensive error handling for a live coding environment
// These tests guide the implementation of user-friendly error reporting

TEST_CASE("Response class basic functionality", "[error-handling][response]")
{
    ModuLisp modulisp;

    SECTION("Successful evaluation")
    {
        ModuLisp::Response response = modulisp.send("(+ 1 2)");
        REQUIRE(response.okay());
        REQUIRE(response.type() == "Integer");
        REQUIRE(response.as_int() == 3);
        REQUIRE(response.get_warnings().empty());
    }

    SECTION("Simple error - undefined function")
    {
        ModuLisp::Response response = modulisp.send("(asdfqwerty 440)");
        REQUIRE_FALSE(response.okay());
        REQUIRE(response.is_error());
        REQUIRE(response.get_error_type() ==
                ModuLisp::ErrorType::UNDEFINED_FUNCTION);
        REQUIRE(response.get_error_message() ==
                "Function 'asdfqwerty' is not defined");
    }
}

TEST_CASE("Syntax errors with helpful messages", "[error-handling][syntax]")
{
    ModuLisp modulisp;

    SECTION("Unbalanced parentheses - missing closing")
    {
        auto response = modulisp.send("(defn make-beat [phase] (sin (* phase 2)");
        REQUIRE_FALSE(response.okay());
        REQUIRE(response.is_error());
        REQUIRE(response.get_error_type() == ModuLisp::ErrorType::SYNTAX_ERROR);
        REQUIRE(response.get_error_message() == "Missing closing parenthesis");
        REQUIRE(response.get_line() == 1);
        REQUIRE(response.get_column() == 40); // Points to where ) should be
        REQUIRE(response.get_suggestion() ==
                "Add ')' at the end of your expression");
        REQUIRE(response.get_context_snippet() ==
                "(defn make-beat [phase] (sin (* phase 2)\n                         "
                "               ^-- Expected ')' here");
    }

    SECTION("Unbalanced parentheses - extra closing")
    {
        auto response = modulisp.send("(sin 0.5))");
        REQUIRE_FALSE(response.okay());
        REQUIRE(response.is_error());
        REQUIRE(response.get_error_type() == ModuLisp::ErrorType::SYNTAX_ERROR);
        REQUIRE(response.get_error_message() == "Unexpected closing parenthesis");
        REQUIRE(response.get_line() == 1);
        REQUIRE(response.get_column() == 10);
        REQUIRE(response.get_suggestion() == "Remove the extra ')' at position 10");
    }

    SECTION("Unclosed string literal")
    {
        auto response = modulisp.send("(println \"Hello live coders)");
        REQUIRE_FALSE(response.okay());
        REQUIRE(response.is_error());
        REQUIRE(response.get_error_type() == ModuLisp::ErrorType::SYNTAX_ERROR);
        REQUIRE(response.get_error_message() == "Unclosed string literal");
        REQUIRE(response.get_line() == 1);
        REQUIRE(response.get_column() == 10); // Start of string
        REQUIRE(response.get_suggestion() ==
                "Add a closing '\"' to complete the string");
        REQUIRE(response.get_context_snippet().find("\"Hello live coders") !=
                std::string::npos);
    }

    SECTION("Invalid character in symbol")
    {
        auto response = modulisp.send("(def my-var(123 42)");
        REQUIRE_FALSE(response.okay());
        REQUIRE(response.is_error());
        REQUIRE(response.get_error_type() == ModuLisp::ErrorType::SYNTAX_ERROR);
        REQUIRE(response.get_error_message() ==
                "Invalid character '(' in symbol name");
        REQUIRE(response.get_line() == 1);
        REQUIRE(response.get_column() == 12);
        REQUIRE(response.get_suggestion() ==
                "Symbol names can only contain letters, numbers, hyphens, and "
                "underscores");
    }
}

TEST_CASE("Undefined function errors with suggestions",
          "[error-handling][undefined]")
{
    ModuLisp modulisp;

    SECTION("Typo in function name - close match")
    {
        auto response = modulisp.send("(sni 0.5)"); // Typo of 'sin'
        REQUIRE_FALSE(response.okay());
        REQUIRE(response.is_error());
        REQUIRE(response.get_error_type() ==
                ModuLisp::ErrorType::UNDEFINED_FUNCTION);
        REQUIRE(response.get_error_message() == "Function 'sni' is not defined");
        REQUIRE(response.get_line() == 1);
        REQUIRE(response.get_column() == 2);

        // Should suggest similar function names
        auto suggestions = response.get_did_you_mean();
        REQUIRE(suggestions.size() > 0);
        REQUIRE(suggestions[0] == "sin"); // Levenshtein distance = 1
        REQUIRE(response.get_suggestion() ==
                "Did you mean 'sin'? (sine wave function)");
    }

    SECTION("Common misspelling - oscillator")
    {
        auto response = modulisp.send("(oscilator 440)"); // Missing 'l'
        REQUIRE_FALSE(response.okay());
        REQUIRE(response.is_error());
        auto suggestions = response.get_did_you_mean();
        REQUIRE(std::find(suggestions.begin(), suggestions.end(), "oscillator") !=
                suggestions.end());
        REQUIRE(response.get_suggestion() ==
                "Did you mean 'oscillator'? (generates audio waveforms)");
    }

    SECTION("Wrong case sensitivity")
    {
        auto response = modulisp.send("(Sin 0.5)"); // Capital S
        REQUIRE_FALSE(response.okay());
        REQUIRE(response.is_error());
        REQUIRE(response.get_suggestion() ==
                "Did you mean 'sin'? Note: function names are case-sensitive");
    }

    SECTION("No close matches")
    {
        auto response = modulisp.send("(xyzabc123 42)");
        REQUIRE_FALSE(response.okay());
        REQUIRE(response.is_error());
        REQUIRE(response.get_error_message() ==
                "Function 'xyzabc123' is not defined");
        REQUIRE(response.get_did_you_mean().empty());
        REQUIRE(
            response.get_suggestion() ==
            "No similar functions found. Type (help) to see available functions");
    }
}

TEST_CASE("Argument errors with helpful explanations", "[error-handling][arguments]")
{
    ModuLisp modulisp;

    SECTION("Wrong number of arguments - too few")
    {
        auto response =
            modulisp.send("(from-list [1 2 3])"); // + needs at least 2 args
        REQUIRE_FALSE(response.okay());
        REQUIRE(response.is_error());
        REQUIRE(response.get_error_type() == ModuLisp::ErrorType::ARITY_ERROR);
        REQUIRE(response.get_error_message() ==
                "Function 'from-list' expects exactly 2 arguments, but got 1");
        REQUIRE(
            response.get_suggestion() ==
            "Usage: (from-list vector phasor)\nExample: (from-list [1 2 3] bar)");
    }

    SECTION("Wrong number of arguments - too many")
    {
        auto response = modulisp.send("(sin 0.5 1.0)"); // sin takes exactly 1 arg
        REQUIRE_FALSE(response.okay());
        REQUIRE(response.is_error());
        REQUIRE(response.get_error_type() == ModuLisp::ErrorType::ARITY_ERROR);
        REQUIRE(response.get_error_message() ==
                "Function 'sin' expects exactly 1 argument, but got 2");
        REQUIRE(response.get_suggestion() ==
                "Usage: (sin phase)\nExample: (sin 0.5) returns sine wave value at "
                "phase 0.5");
    }

    SECTION("defn with missing parameters")
    {
        auto response =
            modulisp.send("(defn make-beat)"); // Missing param list and body
        REQUIRE_FALSE(response.okay());
        REQUIRE(response.is_error());
        REQUIRE(response.get_error_type() == ModuLisp::ErrorType::ARITY_ERROR);
        REQUIRE(response.get_error_message() ==
                "defn requires a name, parameter list, and body");
        REQUIRE(response.get_suggestion() ==
                "Usage: (defn function-name [param1 param2 ...] "
                "body-expression)\nExample: (defn double [x] (* x 2))");
    }

    SECTION("Wrong argument type")
    {
        auto response = modulisp.send("(sin \"hello\")"); // sin expects number
        REQUIRE_FALSE(response.okay());
        REQUIRE(response.is_error());
        REQUIRE(response.get_error_type() == ModuLisp::ErrorType::TYPE_ERROR);
        REQUIRE(response.get_error_message() ==
                "Function 'sin' expects argument 1 to be a number, but got string "
                "\"hello\"");
        REQUIRE(response.get_suggestion() ==
                "The sin function calculates the sine of a phase value (0.0 to "
                "1.0)\nExample: (sin 0.25) returns 1.0 (peak of sine wave)");
    }
}

TEST_CASE("Runtime errors with context", "[error-handling][runtime]")
{
    ModuLisp modulisp;

    SECTION("Division by zero")
    {
        auto response = modulisp.send("(/ 10 0)");
        REQUIRE(response.is_error());
        REQUIRE(response.get_error_type() == ModuLisp::ErrorType::ARITHMETIC_ERROR);
        REQUIRE(response.get_error_message() == "Division by zero");
    }

    SECTION("List index out of bounds")
    {
        modulisp.send("(def notes [60 62 64 67])");     // C major chord
        auto response = modulisp.send("(nth notes 5)"); // Index 5 doesn't exist
        REQUIRE(response.is_error());
        REQUIRE(response.get_error_type() == ModuLisp::ErrorType::INDEX_ERROR);
        REQUIRE(response.get_error_message() ==
                "Index 5 is out of bounds for list of size 4");
        REQUIRE(response.get_suggestion() ==
                "Valid indices are 0 to 3. Use (count notes) to check list size.");
    }

    SECTION("Stack overflow from infinite recursion")
    {
        modulisp.send("(defn loop-forever [] (loop-forever))");
        auto response = modulisp.send("(loop-forever)");
        REQUIRE(response.is_error());
        REQUIRE(response.get_error_type() == ModuLisp::ErrorType::RECURSION_ERROR);
        REQUIRE(response.get_error_message() ==
                "Maximum recursion depth exceeded (stack overflow)");
        REQUIRE(response.get_suggestion() ==
                "Your function is calling itself infinitely. Add a base case to "
                "stop recursion.\nExample: (defn countdown [n] (if (<= n 0) "
                "\"done\" (countdown (- n 1))))");
        REQUIRE(response.get_stack_trace().size() > 0);
        REQUIRE(response.get_stack_trace()[0] == "loop-forever at line 1");
    }

    SECTION("Timeout in long-running computation")
    {
        auto response =
            modulisp.send("(reduce + (range 1000000))"); // Very long computation
        // This would need timeout detection in the interpreter
        if (response.is_error() &&
            response.get_error_type() == ModuLisp::ErrorType::TIMEOUT_ERROR)
        {
            REQUIRE(response.get_error_message() ==
                    "Evaluation timed out after 100ms");
            REQUIRE(response.get_suggestion() ==
                    "Your code is taking too long. Consider using smaller values or "
                    "optimizing the algorithm.");
        }
    }
}

TEST_CASE("Variable and binding errors", "[error-handling][variables]")
{
    ModuLisp modulisp;

    SECTION("Undefined variable")
    {
        auto response = modulisp.send("(+ x 5)"); // x not defined
        REQUIRE(response.is_error());
        REQUIRE(response.get_error_type() ==
                ModuLisp::ErrorType::UNDEFINED_VARIABLE);
        REQUIRE(response.get_error_message() == "Variable 'x' is not defined");
        REQUIRE(response.get_suggestion() ==
                "Define the variable first using (def x value) or use it as a "
                "function parameter");
    }

    SECTION("Typo in variable name with suggestion")
    {
        modulisp.send("(def amplitude 0.8)");
        auto response = modulisp.send("(* amplitued 2)"); // Typo
        REQUIRE(response.is_error());
        REQUIRE(response.get_error_type() ==
                ModuLisp::ErrorType::UNDEFINED_VARIABLE);
        REQUIRE(response.get_error_message() ==
                "Variable 'amplitued' is not defined");
        auto suggestions = response.get_did_you_mean();
        REQUIRE(std::find(suggestions.begin(), suggestions.end(), "amplitude") !=
                suggestions.end());
        REQUIRE(response.get_suggestion() == "Did you mean 'amplitude'?");
    }

    SECTION("Attempting to redefine built-in")
    {
        auto response = modulisp.send("(def sin 3.14)"); // Can't redefine sin
        REQUIRE(response.is_error());
        REQUIRE(response.get_error_type() == ModuLisp::ErrorType::IMMUTABLE_ERROR);
        REQUIRE(response.get_error_message() ==
                "Cannot redefine built-in function 'sin'");
        REQUIRE(response.get_suggestion() ==
                "Choose a different name for your variable. Built-in functions are "
                "protected.");
    }
}

TEST_CASE("Pattern matching and destructuring errors", "[error-handling][patterns]")
{
    ModuLisp modulisp;

    // NOTE: We don't support destructuring yet, leaving this here for the future
    // SECTION("Destructuring mismatch") {
    //     auto response = modulisp.send("(let [[a b c] [1 2]] (+ a b c))");  // Not
    //     enough values REQUIRE(response.is_error());
    //     REQUIRE(response.get_error_type() ==
    //     ModuLisp::ErrorType::DESTRUCTURING_ERROR);
    //     REQUIRE(response.get_error_message() == "Cannot destructure: expected 3
    //     values but got 2"); REQUIRE(response.get_suggestion() == "The pattern [a b
    //     c] expects exactly 3 values.\nEither provide 3 values or adjust the
    //     pattern to match.");
    // }

    SECTION("Invalid binding form")
    {
        auto response = modulisp.send("(let [a] (+ a 1))"); // Odd number of forms
        REQUIRE(response.is_error());
        REQUIRE(response.get_error_type() == ModuLisp::ErrorType::SYNTAX_ERROR);
        REQUIRE(response.get_error_message() ==
                "let requires an even number of forms in binding vector");
        REQUIRE(response.get_suggestion() ==
                "Usage: (let [name1 value1 name2 value2 ...] body)\nExample: (let "
                "[x 10 y 20] (+ x y))");
    }
}

// TEST_CASE("Multi-line error reporting", "[error-handling][multiline]") {
//     ModuLisp modulisp;

//     SECTION("Error in multi-line code") {
//         std::string code = R"(
// (defn make-melody [root]
//   (let [scale [0 2 4 5 7 9 11]]
//     (map (fn [interval]
//            (+ root interval))
//          scle)))  ; Typo here: 'scle' instead of 'scale'
// )";

//         auto response = modulisp.send(code);
//         REQUIRE(response.is_error());
//         REQUIRE(response.get_error_type() ==
//         ModuLisp::ErrorType::UNDEFINED_VARIABLE);
//         REQUIRE(response.get_error_message() == "Variable 'scle' is not defined");
//         REQUIRE(response.get_line() == 6);
//         REQUIRE(response.get_column() == 10);
//         REQUIRE(response.get_did_you_mean()[0] == "scale");

//         // Should show context with line numbers
//         std::string context = response.get_context_snippet();
//         REQUIRE(context.find("5:            (+ root interval))") !=
//         std::string::npos); REQUIRE(context.find("6:          scle)))  ; Typo
//         here") != std::string::npos); REQUIRE(context.find("          ^^^^") !=
//         std::string::npos);  // Error marker
//     }
// }

// TEST_CASE("Error recovery suggestions", "[error-handling][recovery]") {
//     ModuLisp modulisp;

//     SECTION("Common beginner mistake - missing quotes") {
//         auto response = modulisp.send("(println Hello World)");  // Forgot quotes
//         REQUIRE(response.is_error());
//         REQUIRE(response.get_error_type() ==
//         ModuLisp::ErrorType::UNDEFINED_VARIABLE);
//         // But should recognize the pattern and suggest quotes
//         REQUIRE(response.get_suggestion().find("Did you mean to use a string?") !=
//         std::string::npos); REQUIRE(response.get_suggestion().find("(println
//         \"Hello World\")") != std::string::npos);
//     }

//     SECTION("Using = instead of ==") {
//         auto response = modulisp.send("(if (= x 5) true false)");  // Clojure uses
//         = for equality
//         // This should actually work in Clojure-like syntax, but if not
//         implemented: if (response.is_error()) {
//             REQUIRE(response.get_suggestion().find("For equality comparison, use
//             =") != std::string::npos);
//         }
//     }

//     SECTION("Forgot to quote a list") {
//         auto response = modulisp.send("(def notes (60 62 64))");  // Should be [60
//         62 64] or '(60 62 64) REQUIRE(response.is_error());
//         REQUIRE(response.get_error_type() ==
//         ModuLisp::ErrorType::UNDEFINED_FUNCTION);
//         REQUIRE(response.get_error_message() == "Function '60' is not defined");
//         REQUIRE(response.get_suggestion().find("use square brackets [60 62 64]")
//         != std::string::npos);
//     }
// }

// TEST_CASE("Warning system for non-critical issues", "[error-handling][warnings]")
// {
//     ModuLisp modulisp;

//     SECTION("Deprecated function warning") {
//         modulisp.register_deprecated("set-tempo", "set-bpm");
//         auto response = modulisp.send("(set-tempo 120)");
//         REQUIRE(response.okay());  // Still works
//         REQUIRE(response.get_warnings().size() == 1);
//         REQUIRE(response.get_warnings()[0] == "Function 'set-tempo' is deprecated.
//         Use 'set-bpm' instead.");
//     }

//     SECTION("Performance warning") {
//         auto response = modulisp.send("(map sin (range 10000))");  // Large
//         computation REQUIRE(response.okay());
//         REQUIRE(response.get_warnings().size() > 0);
//         REQUIRE(response.get_warnings()[0].find("Large computation") !=
//         std::string::npos);
//     }

//     SECTION("Unused variable warning") {
//         auto response = modulisp.send("(let [x 10 y 20] x)");  // y is unused
//         REQUIRE(response.okay());
//         REQUIRE(response.get_value().as_int() == 10);
//         REQUIRE(response.get_warnings().size() == 1);
//         REQUIRE(response.get_warnings()[0] == "Variable 'y' is defined but never
//         used");
//     }
// }

// TEST_CASE("Progressive error disclosure", "[error-handling][progressive]") {
//     ModuLisp modulisp;

//     SECTION("Simple vs detailed error messages") {
//         auto response = modulisp.send("(oscillator 440 0.5 \"sine\" true false)");
//         // Way too many args

//         // Simple message for beginners
//         REQUIRE(response.get_error_message() == "Too many arguments for
//         oscillator");

//         // Detailed message available
//         REQUIRE(response.get_detailed_error() ==
//             "Function 'oscillator' expects 1-2 arguments:\n"
//             "  (oscillator frequency) - sine wave at frequency\n"
//             "  (oscillator frequency waveform) - specified waveform\n"
//             "You provided 5 arguments: 440, 0.5, \"sine\", true, false");

//         // Examples for learning
//         auto examples = response.get_examples();
//         REQUIRE(examples.size() > 0);
//         REQUIRE(examples[0] == "(oscillator 440)        ; A4 sine wave");
//         REQUIRE(examples[1] == "(oscillator 440 \"saw\")  ; A4 sawtooth wave");
//     }
// }

// TEST_CASE("Context-aware suggestions", "[error-handling][context]") {
//     ModuLisp modulisp;

//     SECTION("Audio context suggestions") {
//         modulisp.set_context(ModuLisp::Context::AUDIO);
//         auto response = modulisp.send("(osc 440)");  // Wrong function name
//         REQUIRE(response.is_error());
//         auto suggestions = response.get_did_you_mean();
//         // Should prioritize audio-related functions
//         REQUIRE(suggestions[0] == "oscillator");  // Most likely in audio context
//         REQUIRE(std::find(suggestions.begin(), suggestions.end(), "osc-bank") !=
//         suggestions.end());
//     }

//     SECTION("Pattern context suggestions") {
//         modulisp.set_context(ModuLisp::Context::PATTERN);
//         auto response = modulisp.send("(seq [1 2 3])");  // Might be looking for
//         sequence functions if (response.is_error()) {
//             auto suggestions = response.get_did_you_mean();
//             REQUIRE(std::find(suggestions.begin(), suggestions.end(), "sequence")
//             != suggestions.end()); REQUIRE(std::find(suggestions.begin(),
//             suggestions.end(), "step-seq") != suggestions.end());
//         }
//     }
// }

// TEST_CASE("Error formatting for different outputs",
// "[error-handling][formatting]") {
//     ModuLisp modulisp;

//     SECTION("Terminal-friendly format") {
//         auto response = modulisp.send("(+ 1 two)");
//         std::string terminal_output = response.format_for_terminal();
//         REQUIRE(terminal_output.find("Error:") != std::string::npos);
//         REQUIRE(terminal_output.find("line 1, column 6") != std::string::npos);
//         REQUIRE(terminal_output.find("Variable 'two' is not defined") !=
//         std::string::npos);
//     }

//     SECTION("JSON format for editors") {
//         auto response = modulisp.send("(+ 1 two)");
//         std::string json = response.to_json();
//         REQUIRE(json.find("\"error\": true") != std::string::npos);
//         REQUIRE(json.find("\"line\": 1") != std::string::npos);
//         REQUIRE(json.find("\"column\": 6") != std::string::npos);
//         REQUIRE(json.find("\"message\": \"Variable 'two' is not defined\"") !=
//         std::string::npos); REQUIRE(json.find("\"suggestions\": [") !=
//         std::string::npos);
//     }

//     SECTION("Compact format for logging") {
//         auto response = modulisp.send("(+ 1 two)");
//         std::string log_line = response.to_log_format();
//         REQUIRE(log_line.find("ERROR") != std::string::npos);
//         REQUIRE(log_line.find("UNDEFINED_VARIABLE") != std::string::npos);
//         REQUIRE(log_line.find("two") != std::string::npos);
//         REQUIRE(log_line.find("1:6") != std::string::npos);  // line:column
//     }
// }