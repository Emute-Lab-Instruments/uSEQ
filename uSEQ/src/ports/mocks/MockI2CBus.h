#ifndef PORTS_MOCKS_MOCK_I2C_BUS_H
#define PORTS_MOCKS_MOCK_I2C_BUS_H

#include "../II2CTransport.h"

#include <array>

namespace firmware
{

class MockI2CBus
{
public:
    static constexpr size_t ADDRESS_COUNT = 127;

    struct Endpoint
    {
        bool attached                       = false;
        void* context                       = nullptr;
        I2CReceiveCallback receive_callback = nullptr;
        I2CRequestCallback request_callback = nullptr;
    };

    bool attach(uint8_t address, void* context, I2CReceiveCallback receive_callback,
                I2CRequestCallback request_callback)
    {
        if (address == 0 || address >= ADDRESS_COUNT ||
            receive_callback == nullptr || request_callback == nullptr)
        {
            return false;
        }
        endpoints_[address] = { true, context, receive_callback, request_callback };
        return true;
    }

    bool probe(uint8_t address) const
    {
        return address < ADDRESS_COUNT && endpoints_[address].attached &&
               !nack_[address];
    }

    bool write(uint8_t address, const uint8_t* data, size_t length)
    {
        if (!probe(address) || data == nullptr || length == 0)
        {
            return false;
        }
        ++write_count_;
        endpoints_[address].receive_callback(endpoints_[address].context, data,
                                             length);
        return true;
    }

    size_t request(uint8_t address, uint8_t* data, size_t capacity)
    {
        if (!probe(address) || data == nullptr || capacity == 0)
        {
            return 0;
        }
        return endpoints_[address].request_callback(endpoints_[address].context,
                                                    data, capacity);
    }

    void set_nack(uint8_t address, bool nack)
    {
        if (address < ADDRESS_COUNT)
        {
            nack_[address] = nack;
        }
    }

    size_t write_count() const { return write_count_; }

private:
    std::array<Endpoint, ADDRESS_COUNT> endpoints_ = {};
    std::array<bool, ADDRESS_COUNT> nack_          = {};
    size_t write_count_                            = 0;
};

class MockI2CTransport final : public II2CTransport
{
public:
    explicit MockI2CTransport(MockI2CBus& bus) : bus_(bus) {}

    bool begin_host(uint8_t sda_pin, uint8_t scl_pin) override
    {
        host_started_ = true;
        sda_pin_      = sda_pin;
        scl_pin_      = scl_pin;
        return true;
    }

    bool begin_client(uint8_t address, uint8_t sda_pin, uint8_t scl_pin,
                      void* callback_context, I2CReceiveCallback receive_callback,
                      I2CRequestCallback request_callback) override
    {
        address_        = address;
        sda_pin_        = sda_pin;
        scl_pin_        = scl_pin;
        client_started_ = bus_.attach(address, callback_context, receive_callback,
                                      request_callback);
        return client_started_;
    }

    bool probe(uint8_t address) override { return bus_.probe(address); }

    bool write(uint8_t address, const uint8_t* data, size_t length) override
    {
        return bus_.write(address, data, length);
    }

    size_t request(uint8_t address, uint8_t* data, size_t capacity) override
    {
        return bus_.request(address, data, capacity);
    }

    bool host_started() const { return host_started_; }
    bool client_started() const { return client_started_; }
    uint8_t address() const { return address_; }
    uint8_t sda_pin() const { return sda_pin_; }
    uint8_t scl_pin() const { return scl_pin_; }

private:
    MockI2CBus& bus_;
    bool host_started_   = false;
    bool client_started_ = false;
    uint8_t address_     = 0;
    uint8_t sda_pin_     = 0;
    uint8_t scl_pin_     = 0;
};

} // namespace firmware

#endif // PORTS_MOCKS_MOCK_I2C_BUS_H
