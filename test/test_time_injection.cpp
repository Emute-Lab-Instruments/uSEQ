#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../uSEQ/src/modulisp/lisp/error_context.h"
#include "../uSEQ/src/modulisp/modulisp.h"
#include "../uSEQ/src/ports/mocks/MockClock.h"

TEST_CASE("Injected clock drives time deterministically", "[time][di][clock]")
{
    // Arrange: a controllable clock at 1.5 seconds
    MockClock clk;
    clk.set_micros(1500000ULL);
    ErrorManager error_mgr;
    Environment env;
    uLispParser parser(&error_mgr);

    ModuLispInterpreter interp(&error_mgr, nullptr, nullptr, &clk);

    // Initialize BPM-dependent durations to avoid divide-by-zero in phasors
    interp.set_bpm(120.0, 0.0);

    // Act
    interp.update_time();

    // Assert: Lisp environment holds seconds values derived from injected micros
    auto t_total     = interp.get_environment()->get("time");
    auto t_transport = interp.get_environment()->get("t");

    REQUIRE(t_total.has_value());
    REQUIRE(t_transport.has_value());

    REQUIRE(t_total->is_float());
    REQUIRE(t_transport->is_float());

    REQUIRE(t_total->as_float() == Approx(1.5).epsilon(1e-6));
    REQUIRE(t_transport->as_float() == Approx(1.5).epsilon(1e-6));
}
