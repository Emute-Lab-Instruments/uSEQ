#ifndef PORTS_II2C_TRANSPORT_H
#define PORTS_II2C_TRANSPORT_H

#include <cstddef>
#include <cstdint>

namespace firmware
{

using I2CReceiveCallback = void (*)(void* context, const uint8_t* data,
                                    size_t length);
using I2CRequestCallback = size_t (*)(void* context, uint8_t* data, size_t capacity);

// The hardware adapter owns bus mechanics. I2CNetwork owns discovery,
// protocol framing, buffering, and expander state. This keeps the latter
// executable in native tests without emulating Arduino's Wire globals.
struct II2CTransport
{
    virtual ~II2CTransport() = default;

    virtual bool begin_host(uint8_t sda_pin, uint8_t scl_pin)      = 0;
    virtual bool begin_client(uint8_t address, uint8_t sda_pin, uint8_t scl_pin,
                              void* callback_context,
                              I2CReceiveCallback receive_callback,
                              I2CRequestCallback request_callback) = 0;

    virtual bool probe(uint8_t address)                                     = 0;
    virtual bool write(uint8_t address, const uint8_t* data, size_t length) = 0;
    virtual size_t request(uint8_t address, uint8_t* data, size_t capacity) = 0;
};

} // namespace firmware

#endif // PORTS_II2C_TRANSPORT_H
