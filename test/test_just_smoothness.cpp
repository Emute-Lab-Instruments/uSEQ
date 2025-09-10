#include "catch.hpp"

#include "../uSEQ/src/modulisp/lisp/environment.h"
#include "../uSEQ/src/modulisp/lisp/value.h"

TEST_CASE("Smoothness and continuity metadata methods", "[smoothness][dsp][value]")
{
    SECTION("basic time parameter properties")
    {
        // Basic time parameter should be analytic (smooth)
        REQUIRE(Value::t.is_smooth());
        REQUIRE(Value::t.is_continuous());
        REQUIRE(Value::t.is_analytic());
    }

    SECTION("polynomial operations preserve smoothness")
    {
        // Polynomial operations should preserve smoothness
        Value t_squared = Value::t * Value::t;
        REQUIRE(t_squared.is_smooth());
        REQUIRE(t_squared.is_continuous());
        REQUIRE(t_squared.is_analytic());
    }

    SECTION("modulo operations create discontinuities")
    {
        // Modulo creates discontinuities
        Value mod_signal = Value::t % Value(2.0);
        REQUIRE_FALSE(mod_signal.is_continuous()); // Jump discontinuities
        REQUIRE_FALSE(mod_signal.is_smooth());
        REQUIRE_FALSE(mod_signal.is_analytic());
    }
}