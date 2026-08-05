#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "../../uSEQ/src/firmware/i2c_network.h"
#include "../../uSEQ/src/ports/mocks/MockI2CBus.h"

#include <array>
#include <cmath>
#include <cstring>
#include <limits>

using firmware::I2CIncoming;
using firmware::I2CNetwork;
using firmware::MockI2CBus;
using firmware::MockI2CTransport;

TEST_CASE("expander build selects client mode and the PCB bus pins",
          "[firmware][i2c][expander][init]")
{
    MockI2CBus bus;
    MockI2CTransport transport(bus);
    I2CNetwork network;
    network.set_transport(&transport);

    REQUIRE(network.init());
    REQUIRE(network.client_mode);
    REQUIRE_FALSE(network.host_mode);
    REQUIRE(network.client_address == 0x30); // deterministic native fallback
    REQUIRE(transport.sda_pin() == 4);
    REQUIRE(transport.scl_pin() == 1);
}

TEST_CASE("expander value packets round-trip and clamp at the electrical boundary",
          "[firmware][i2c][expander][codec]")
{
    const std::array<double, 8> source = { -0.25, 0.0, 0.125, 0.5,
                                           0.75,  1.0, 1.25,  0.25 };
    std::array<uint8_t, firmware::I2C_VALUES_MAX_SIZE> packet = {};
    const size_t length = I2CNetwork::encode_output_values(
        source.data(), static_cast<uint8_t>(source.size()), packet.data(),
        packet.size());

    REQUIRE(length == firmware::I2C_VALUES_MAX_SIZE);
    REQUIRE(std::memcmp(packet.data(), "$vals\0", 6) == 0);
    REQUIRE(packet[6] == 8);
    const size_t one_offset = firmware::I2C_VALUES_HEADER_SIZE + 5 * sizeof(double);
    for (size_t index = 0; index < 6; ++index)
        REQUIRE(packet[one_offset + index] == 0x00);
    REQUIRE(packet[one_offset + 6] == 0xf0);
    REQUIRE(packet[one_offset + 7] == 0x3f);

    std::array<double, 8> decoded = {};
    uint8_t count                 = 0;
    REQUIRE(I2CNetwork::decode_output_values(packet.data(), length, decoded.data(),
                                             count));
    REQUIRE(count == 8);
    REQUIRE(decoded ==
            std::array<double, 8>{ 0.0, 0.0, 0.125, 0.5, 0.75, 1.0, 1.0, 0.25 });
}

TEST_CASE("expander rejects malformed value packets without partial publication",
          "[firmware][i2c][expander][validation]")
{
    MockI2CBus bus;
    MockI2CTransport transport(bus);
    I2CNetwork client;
    client.set_transport(&transport);
    REQUIRE(client.init_client(0x2a));

    std::array<double, 8> outputs = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8 };
    const auto expected           = outputs;

    SECTION("truncated packet")
    {
        const std::array<uint8_t, 7> packet = { '$', 'v', 'a', 'l', 's', '\0', 8 };
        REQUIRE(bus.write(0x2a, packet.data(), packet.size()));
    }

    SECTION("too many outputs")
    {
        std::array<uint8_t, 7> packet = { '$', 'v', 'a', 'l', 's', '\0', 9 };
        REQUIRE(bus.write(0x2a, packet.data(), packet.size()));
    }

    SECTION("non-finite output")
    {
        std::array<double, 8> values = {};
        values[3]                    = std::numeric_limits<double>::quiet_NaN();
        std::array<uint8_t, firmware::I2C_VALUES_MAX_SIZE> packet = {};
        const size_t length = I2CNetwork::encode_output_values(
            values.data(), 8, packet.data(), packet.size());
        REQUIRE(length > 0);
        REQUIRE(bus.write(0x2a, packet.data(), length));
    }

    REQUIRE(client.process_incoming(outputs.data(), outputs.size()) == 0);
    REQUIRE(outputs == expected);
    REQUIRE(client.rejected_packets.load() == 1);
}

TEST_CASE("I2C callback queue is bounded and preserves accepted packet order",
          "[firmware][i2c][queue]")
{
    I2CIncoming queue;
    for (uint8_t value = 0; value < I2CIncoming::CAPACITY; ++value)
    {
        REQUIRE(queue.push(&value, 1));
    }
    const uint8_t overflow = 99;
    REQUIRE_FALSE(queue.push(&overflow, 1));

    for (uint8_t expected = 0; expected < I2CIncoming::CAPACITY; ++expected)
    {
        uint8_t actual = 255;
        size_t length  = 0;
        REQUIRE(queue.pop(&actual, 1, length));
        REQUIRE(length == 1);
        REQUIRE(actual == expected);
    }
    REQUIRE_FALSE(queue.any());
}

TEST_CASE("fake bus discovers an expander and delivers all eight outputs",
          "[firmware][i2c][expander][integration]")
{
    MockI2CBus bus;
    MockI2CTransport client_transport(bus);
    MockI2CTransport host_transport(bus);

    I2CNetwork client;
    client.set_transport(&client_transport);
    REQUIRE(client.init_client(0x2a));
    REQUIRE(client.client_mode);
    REQUIRE(client_transport.sda_pin() == 4); // expander PCB contract
    REQUIRE(client_transport.scl_pin() == 1);

    I2CNetwork host;
    host.set_transport(&host_transport);
    REQUIRE(host.init_host());
    REQUIRE(host.host_mode);
    REQUIRE(host.expander_count == 1);
    REQUIRE(host.expander_addrs[0] == 0x2a);

    const std::array<double, 8> sent = { 0.0, 0.125, 0.25, 0.375,
                                         0.5, 0.625, 0.75, 1.0 };
    std::array<double, 8> outputs    = {};

    host.broadcast_output_values(sent.data(), 8);
    REQUIRE(client.has_incoming());
    REQUIRE(client.process_incoming(outputs.data(), outputs.size()) == 1);
    REQUIRE(outputs == sent);
    REQUIRE(client.applied_value_packets.load() == 1);
}

TEST_CASE("a disconnected expander retains its last-known output vector",
          "[firmware][i2c][expander][recovery]")
{
    MockI2CBus bus;
    MockI2CTransport client_transport(bus);
    MockI2CTransport host_transport(bus);
    I2CNetwork client;
    I2CNetwork host;
    client.set_transport(&client_transport);
    host.set_transport(&host_transport);
    REQUIRE(client.init_client(0x2a));
    REQUIRE(host.init_host());

    const std::array<double, 8> first = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8 };
    std::array<double, 8> outputs     = {};
    host.broadcast_output_values(first.data(), 8);
    REQUIRE(client.process_incoming(outputs.data(), outputs.size()) == 1);
    REQUIRE(outputs == first);

    bus.set_nack(0x2a, true);
    const std::array<double, 8> second = { 0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1 };
    host.broadcast_output_values(second.data(), 8);
    REQUIRE_FALSE(client.has_incoming());
    REQUIRE(client.process_incoming(outputs.data(), outputs.size()) == 0);
    REQUIRE(outputs == first);

    bus.set_nack(0x2a, false);
    host.broadcast_output_values(second.data(), 8);
    REQUIRE(client.process_incoming(outputs.data(), outputs.size()) == 1);
    REQUIRE(outputs == second);
}
