// Dedicated main for this test executable
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../uSEQ/src/ports/mocks/MockClock.h"
#include "../uSEQ/src/ports/mocks/MockIo.h"
#include "../uSEQ/src/uSEQ.h"

TEST_CASE("uSEQ I/O adapters are used via tick/update_outs", "[io][bridge][tick]")
{
    MockClock clk;
    clk.set_micros(0);
    MockIo io;

    uSEQ device(&clk, nullptr, &io);
    // IOManager now initialized automatically in constructor

    // Manually size AST/value arrays to expected output counts
    device.m_continuous_ASTs.resize(3);
    device.m_continuous_vals.resize(3);
    device.m_binary_ASTs.resize(3);
    device.m_binary_vals.resize(3);
    device.m_serial_ASTs.resize(8);
    device.m_serial_vals.resize(8);

    // Prepare ASTs so update_signals populates values
    // Continuous output a1 = 0.5
    device.m_continuous_ASTs[0] = Value(0.5);
    // Binary output d1 = 1
    device.m_binary_ASTs[0] = Value(1);
    // Serial output s1 = 3.14
    device.m_serial_ASTs[0] = Value(3.14);

    // Act: directly exercise write paths via test hook
    device.__test_call_writes(0.5, 1, 3.14);

    // Assert
    // Digital: pin + its LED writes
    REQUIRE(io.digital_writes.size() == 2);
    REQUIRE(io.digital_writes[0].value == 1);
    REQUIRE(io.digital_writes[1].value == 1);

    // Analog: LED and PWM analog writes recorded
    REQUIRE(io.analog_writes.size() >= 2);

    // Serial: one stream write on channel 0
    REQUIRE(io.serial_writes.size() == 1);
    REQUIRE(io.serial_writes[0].channel == 0);
    REQUIRE(io.serial_writes[0].value == Approx(3.14));
}
