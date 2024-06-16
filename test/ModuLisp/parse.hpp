#ifndef PARSE_H_
#define PARSE_H_

#include "lisp/value.h"
#include "uSEQ.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>

uint32_t factorial(uint32_t number)
{
    return number <= 1 ? number : factorial(number - 1) * number;
}

TEST_CASE("Parsing numbers")
{
    uSEQ u;
    u.init();

    REQUIRE(u.parse("123") == Value(123));
    REQUIRE(u.parse("123.0") == Value(123.0));
    REQUIRE(u.parse("-2") == Value(-2));
    REQUIRE(u.parse("-2.5") == Value(-2.5));
}

#endif // PARSE_H_
