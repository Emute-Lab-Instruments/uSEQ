#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "../../uSEQ/src/firmware/i2c_network.h"
#include "../../uSEQ/src/firmware/factory_identity.h"
#include "../../uSEQ/src/ports/mocks/MockI2CBus.h"

#include <array>
#include <cmath>
#include <cstring>
#include <limits>

using firmware::I2CIncoming;
using firmware::I2CNetwork;
using firmware::MockI2CBus;
using firmware::MockI2CTransport;

static firmware::FactoryIdentity production_identity()
{
    firmware::FactoryIdentity identity;
    std::strcpy(identity.product, "useq-exp-aout08");
    std::strcpy(identity.hardware_revision, "0.1");
    std::strcpy(identity.assembly_variant, "preprod");
    std::strcpy(identity.batch, "PP-2026-01");
    std::strcpy(identity.serial, "EXP-0007");
    std::strcpy(identity.manufacture_date, "2026-08-05");
    identity.mcu_family = firmware::McuFamily::RP2040;
    identity.default_i2c_address = 0x2a;
    identity.feature_bits = firmware::factory_feature::I2C_OUTPUTS;
    identity.flash_size_bytes = 2u * 1024u * 1024u;
    return identity;
}

TEST_CASE("factory identity codec validates the complete CRC-protected record",
          "[firmware][i2c][expander][identity]")
{
    const auto identity = production_identity();
    std::array<uint8_t, firmware::FACTORY_IDENTITY_RECORD_SIZE> record = {};
    REQUIRE(firmware::encode_factory_identity(identity, record.data(), record.size()));

    firmware::FactoryIdentity decoded;
    REQUIRE(firmware::decode_factory_identity(record.data(), record.size(), decoded));
    REQUIRE(std::strcmp(decoded.product, "useq-exp-aout08") == 0);
    REQUIRE(std::strcmp(decoded.batch, "PP-2026-01") == 0);
    REQUIRE(std::strcmp(decoded.serial, "EXP-0007") == 0);
    REQUIRE(decoded.mcu_family == firmware::McuFamily::RP2040);
    REQUIRE(decoded.flash_size_bytes == 2u * 1024u * 1024u);

    record[72] ^= 0x01;
    REQUIRE_FALSE(firmware::decode_factory_identity(record.data(), record.size(), decoded));
}

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
    client.set_local_factory_identity(production_identity(), true);
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
    REQUIRE(host.expanders[0].factory_identity_valid);
    REQUIRE(std::strcmp(host.expanders[0].factory_identity.batch, "PP-2026-01") == 0);
    REQUIRE(std::strcmp(host.expanders[0].factory_identity.serial, "EXP-0007") == 0);
    REQUIRE(std::strcmp(host.expanders[0].firmware_version, "1.2.0-beta.1") == 0);
    REQUIRE(std::strcmp(host.expanders[0].firmware_target,
                        "expander_aout08_v0_1") == 0);
    REQUIRE(host.expanders[0].protocol_version == 1);

    const std::array<double, 8> sent = { 0.0, 0.125, 0.25, 0.375,
                                         0.5, 0.625, 0.75, 1.0 };
    std::array<double, 8> outputs    = {};

    host.broadcast_output_values(sent.data(), 8);
    REQUIRE(client.has_incoming());
    REQUIRE(client.process_incoming(outputs.data(), outputs.size()) == 1);
    REQUIRE(outputs == sent);
    REQUIRE(client.applied_value_packets.load() == 1);
}

TEST_CASE("host keeps legacy expander discoverable when identity is absent",
          "[firmware][i2c][expander][identity][compatibility]")
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
    REQUIRE(host.expander_count == 1);
    REQUIRE(host.expanders[0].address == 0x2a);
    REQUIRE_FALSE(host.expanders[0].factory_identity_valid);
    REQUIRE(std::strcmp(host.expanders[0].firmware_version, "1.2.0-beta.1") == 0);
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
