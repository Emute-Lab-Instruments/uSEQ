#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"

#include "../../uSEQ/src/modulisp/modulisp.h"

// Test cases for ModuLisp-specific functions

TEST_CASE("Basic arithmetic", "[modulisp][end_to_end][arithmetic]") {
    ModuLisp modulisp;
    ModuLisp::Response response = modulisp.send("(+ 1 2.1)");
    REQUIRE(response.okay());
    REQUIRE(response.type() == "Float");
    REQUIRE(response.as_float() == Approx(3.1).epsilon(0.001));
}

TEST_CASE("Decision making function", "[modulisp][api][useq_dm]") {
    ModuLisp modulisp;
    ModuLisp::Response response = modulisp.send("(if (> 1 0) 2 3)");
    REQUIRE(response.okay());
    REQUIRE(response.type() == "Integer");
    REQUIRE(response.as_int() == 2);
}

TEST_CASE("Defining a variable", "[modulisp][api][defining variable]") {
    ModuLisp modulisp;
    ModuLisp::Response response = modulisp.send("(define x 42)");
    REQUIRE(response.okay());
    REQUIRE(response.type() == "Symbol");
    REQUIRE(response.as_symbol() == "x");

    response = modulisp.send("x");
    REQUIRE(response.okay());
    REQUIRE(response.type() == "Integer");
    REQUIRE(response.as_int() == 42);

    response = modulisp.send("(+ 1 x)");
    REQUIRE(response.okay());
    REQUIRE(response.type() == "Integer");
    REQUIRE(response.as_int() == 43);
}