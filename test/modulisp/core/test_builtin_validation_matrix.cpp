#define CATCH_CONFIG_MAIN
#include "../uSEQ/src/modulisp/modulisp.h"
#include "catch.hpp"

#include <vector>

namespace
{
struct ErrorCase
{
    const char* expr;
    ModuLisp::ErrorType expected_type;
    const char* expected_message_fragment;
};
} // namespace

TEST_CASE("Builtin validation matrix for arity/type diagnostics",
          "[builtins][validation][matrix]")
{
    ModuLisp modulisp;

    const std::vector<ErrorCase> cases = {
        {"(sin 0.5 1.0)", ModuLisp::ErrorType::ARITY_ERROR,
         "expects exactly 1 argument"},
        {"(from-list [1 2 3])", ModuLisp::ErrorType::ARITY_ERROR,
         "expects exactly 2 arguments"},
        {"(sin \"hello\")", ModuLisp::ErrorType::TYPE_ERROR,
         "expects argument 1 to be a number"},
    };

    for (const auto& c : cases)
    {
        CAPTURE(c.expr);
        auto response = modulisp.send(c.expr);
        REQUIRE_FALSE(response.okay());
        REQUIRE(response.is_error());
        REQUIRE(response.get_error_type() == c.expected_type);
        REQUIRE(response.get_error_message().find(c.expected_message_fragment) !=
                std::string::npos);
    }
}
