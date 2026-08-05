#include "i2c_network.h"
#include "build_info.h"

#ifdef ENABLE_I2C_NETWORKING

#include <algorithm>
#include <cmath>
#include <cstring>

// The expander PCB routes its I2C data line to GPIO4; GPIO0 is LED 8.
#ifndef USEQ_I2C_SDA_PIN
#if defined(USEQHARDWARE_EXPANDER_OUT_0_1)
#define USEQ_I2C_SDA_PIN 4
#else
#define USEQ_I2C_SDA_PIN 0
#endif
#endif
#ifndef USEQ_I2C_SCL_PIN
#define USEQ_I2C_SCL_PIN 1
#endif

#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h>

#ifndef USEQ_I2C_CLOCK_HZ
#define USEQ_I2C_CLOCK_HZ 400000
#endif
#endif

namespace firmware
{
namespace
{

static constexpr uint8_t GET_TYPE_COMMAND[]       = { '$', 'g', 'e', 't',
                                                      't', 'y', 'p', 'e' };
static constexpr uint8_t GET_IDENTITY_COMMAND[]   = { '$', 'i', 'd', 'e', 'n',
                                                      't', 'i', 'f', 'y' };
static constexpr uint8_t EXPANDER_TYPE_RESPONSE[] = { 'a', 'o', 'u', 't',
                                                      '0', '8', '\0' };
static constexpr uint8_t VALUES_PREFIX[] = { '$', 'v', 'a', 'l', 's', '\0' };
static constexpr uint8_t IDENTITY_PREFIX[] = { 'U', 'I', 'D', '1' };
static constexpr uint32_t EXPANDER_CAPABILITIES = 1u; // output-values-v1

static bool is_get_type_command(const uint8_t* data, size_t length)
{
    return data != nullptr && length == sizeof(GET_TYPE_COMMAND) &&
           std::memcmp(data, GET_TYPE_COMMAND, sizeof(GET_TYPE_COMMAND)) == 0;
}

static bool is_get_identity_command(const uint8_t* data, size_t length)
{
    return data != nullptr && length == sizeof(GET_IDENTITY_COMMAND) &&
           std::memcmp(data, GET_IDENTITY_COMMAND,
                       sizeof(GET_IDENTITY_COMMAND)) == 0;
}

static void write_u16_le(uint8_t* output, uint16_t value)
{
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8);
}

static uint16_t read_u16_le(const uint8_t* input)
{
    return static_cast<uint16_t>(input[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(input[1]) << 8);
}

static void write_u32_le(uint8_t* output, uint32_t value)
{
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8);
    output[2] = static_cast<uint8_t>(value >> 16);
    output[3] = static_cast<uint8_t>(value >> 24);
}

static uint32_t read_u32_le(const uint8_t* input)
{
    return static_cast<uint32_t>(input[0]) |
           (static_cast<uint32_t>(input[1]) << 8) |
           (static_cast<uint32_t>(input[2]) << 16) |
           (static_cast<uint32_t>(input[3]) << 24);
}

static void write_binary64_le(uint8_t* destination, double value)
{
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    for (size_t index = 0; index < sizeof(bits); ++index)
    {
        destination[index] = static_cast<uint8_t>(bits >> (index * 8));
    }
}

static double read_binary64_le(const uint8_t* source)
{
    uint64_t bits = 0;
    for (size_t index = 0; index < sizeof(bits); ++index)
    {
        bits |= static_cast<uint64_t>(source[index]) << (index * 8);
    }
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

#ifdef ARDUINO

#if defined(ENABLE_I2C_CLIENT) &&                                                   \
    (defined(USEQHARDWARE_EXPANDER_OUT_0_1) || !defined(ENABLE_I2C_HOST))
static uint8_t hex_nibble(char value)
{
    if (value >= '0' && value <= '9')
        return static_cast<uint8_t>(value - '0');
    if (value >= 'a' && value <= 'f')
        return static_cast<uint8_t>(value - 'a' + 10);
    if (value >= 'A' && value <= 'F')
        return static_cast<uint8_t>(value - 'A' + 10);
    return 0;
}

static uint8_t default_client_address()
{
#ifdef USEQ_I2C_CLIENT_ADDRESS
    constexpr unsigned configured = USEQ_I2C_CLIENT_ADDRESS;
    static_assert(configured >= 1 && configured <= 126,
                  "USEQ_I2C_CLIENT_ADDRESS must be in [1,126]");
    return static_cast<uint8_t>(configured);
#else
    const char* chip_id = rp2040.getChipID();
    const size_t length = chip_id == nullptr ? 0 : std::strlen(chip_id);
    if (length < 2)
        return 0x30;
    const uint8_t raw = static_cast<uint8_t>((hex_nibble(chip_id[length - 2]) << 4) |
                                             hex_nibble(chip_id[length - 1]));
    const uint8_t address = static_cast<uint8_t>(raw % 127);
    return address == 0 ? 1 : address;
#endif
}
#endif

class ArduinoWireTransport final : public II2CTransport
{
public:
    bool begin_host(uint8_t sda_pin, uint8_t scl_pin) override
    {
        active_client_ = nullptr;
        Wire.end();
        Wire.setSDA(sda_pin);
        Wire.setSCL(scl_pin);
        Wire.begin();
        Wire.setClock(USEQ_I2C_CLOCK_HZ);
        return true;
    }

    bool begin_client(uint8_t address, uint8_t sda_pin, uint8_t scl_pin,
                      void* callback_context, I2CReceiveCallback receive_callback,
                      I2CRequestCallback request_callback) override
    {
        if (address == 0 || address >= 127 || receive_callback == nullptr ||
            request_callback == nullptr)
        {
            return false;
        }

        context_          = callback_context;
        receive_callback_ = receive_callback;
        request_callback_ = request_callback;
        active_client_    = this;

        Wire.end();
        Wire.setSDA(sda_pin);
        Wire.setSCL(scl_pin);
        Wire.begin(address);
        Wire.setClock(USEQ_I2C_CLOCK_HZ);
        Wire.onReceive(receive_thunk);
        Wire.onRequest(request_thunk);
        return true;
    }

    bool probe(uint8_t address) override
    {
        Wire.beginTransmission(address);
        return Wire.endTransmission() == 0;
    }

    bool write(uint8_t address, const uint8_t* data, size_t length) override
    {
        Wire.beginTransmission(address);
        const size_t written = Wire.write(data, length);
        return written == length && Wire.endTransmission() == 0;
    }

    size_t request(uint8_t address, uint8_t* data, size_t capacity) override
    {
        const uint8_t requested =
            static_cast<uint8_t>(std::min<size_t>(capacity, 255));
        Wire.requestFrom(address, requested);
        size_t length = 0;
        while (Wire.available() && length < capacity)
        {
            data[length++] = static_cast<uint8_t>(Wire.read());
        }
        return length;
    }

private:
    static constexpr size_t CALLBACK_BUFFER_SIZE = I2C_MAX_MSG_SIZE;
    static ArduinoWireTransport* active_client_;

    void* context_                                = nullptr;
    I2CReceiveCallback receive_callback_          = nullptr;
    I2CRequestCallback request_callback_          = nullptr;
    uint8_t receive_buffer_[CALLBACK_BUFFER_SIZE] = {};
    uint8_t response_buffer_[32]                  = {};

    static void receive_thunk(int announced_length)
    {
        if (active_client_ == nullptr)
            return;

        size_t length = 0;
        bool overflow =
            announced_length < 0 || static_cast<size_t>(announced_length) >
                                        sizeof(active_client_->receive_buffer_);
        while (Wire.available())
        {
            const uint8_t byte = static_cast<uint8_t>(Wire.read());
            if (length < sizeof(active_client_->receive_buffer_))
                active_client_->receive_buffer_[length++] = byte;
            else
                overflow = true;
        }

        if (!overflow && length > 0 && active_client_->receive_callback_ != nullptr)
        {
            active_client_->receive_callback_(
                active_client_->context_, active_client_->receive_buffer_, length);
        }
    }

    static void request_thunk()
    {
        if (active_client_ == nullptr ||
            active_client_->request_callback_ == nullptr)
        {
            return;
        }
        const size_t length = active_client_->request_callback_(
            active_client_->context_, active_client_->response_buffer_,
            sizeof(active_client_->response_buffer_));
        if (length > 0)
            Wire.write(active_client_->response_buffer_, length);
    }
};

ArduinoWireTransport* ArduinoWireTransport::active_client_ = nullptr;

static ArduinoWireTransport& default_transport()
{
    static ArduinoWireTransport instance;
    return instance;
}

#endif // ARDUINO

} // namespace

// ── I2CIncoming SPSC ring buffer ─────────────────────────────────────────

bool I2CIncoming::push(const uint8_t* buffer, size_t length)
{
    if (buffer == nullptr || length == 0 || length > I2C_MAX_MSG_SIZE)
    {
        return false;
    }

    const uint8_t current_head = head.load(std::memory_order_relaxed);
    const uint8_t next_head =
        static_cast<uint8_t>((current_head + 1) % STORAGE_SIZE);
    if (next_head == tail.load(std::memory_order_acquire))
    {
        return false;
    }

    Slot& slot = slots[current_head];
    std::memcpy(slot.data, buffer, length);
    slot.length = length;
    head.store(next_head, std::memory_order_release);
    return true;
}

bool I2CIncoming::pop(uint8_t* buffer, size_t buffer_size, size_t& bytes_read)
{
    bytes_read = 0;
    if (buffer == nullptr || buffer_size == 0)
        return false;

    const uint8_t current_tail = tail.load(std::memory_order_relaxed);
    if (current_tail == head.load(std::memory_order_acquire))
        return false;

    const Slot& slot = slots[current_tail];
    bytes_read       = std::min(slot.length, buffer_size);
    std::memcpy(buffer, slot.data, bytes_read);
    tail.store(static_cast<uint8_t>((current_tail + 1) % STORAGE_SIZE),
               std::memory_order_release);
    return true;
}

bool I2CIncoming::any() const
{
    return tail.load(std::memory_order_acquire) !=
           head.load(std::memory_order_acquire);
}

// ── I2CNetwork lifecycle ─────────────────────────────────────────────────

void I2CNetwork::set_transport(II2CTransport* value) { transport = value; }

bool I2CNetwork::init()
{
#ifdef ARDUINO
    if (transport == nullptr)
        transport = &default_transport();
#endif

#if defined(ENABLE_I2C_CLIENT)
    if (!local_factory_identity_valid)
        local_factory_identity_valid =
            load_factory_identity(local_factory_identity);
#endif

#if defined(ENABLE_I2C_CLIENT) && defined(USEQHARDWARE_EXPANDER_OUT_0_1)
#ifdef ARDUINO
    return init_client(local_factory_identity_valid &&
                               local_factory_identity.default_i2c_address != 0
                           ? local_factory_identity.default_i2c_address
                           : default_client_address());
#else
    return init_client(0x30);
#endif
#elif defined(ENABLE_I2C_HOST)
    return init_host();
#elif defined(ENABLE_I2C_CLIENT)
#ifdef ARDUINO
    return init_client(local_factory_identity_valid &&
                               local_factory_identity.default_i2c_address != 0
                           ? local_factory_identity.default_i2c_address
                           : default_client_address());
#else
    return init_client(0x30);
#endif
#else
    return false;
#endif
}

bool I2CNetwork::init_host()
{
    host_mode      = true;
    client_mode    = false;
    expander_count = 0;
    if (transport == nullptr ||
        !transport->begin_host(USEQ_I2C_SDA_PIN, USEQ_I2C_SCL_PIN))
    {
        host_mode = false;
        return false;
    }
    scan_for_expanders();
    return true;
}

bool I2CNetwork::init_client(uint8_t address)
{
    host_mode      = false;
    client_mode    = true;
    client_address = address;
    if (!local_factory_identity_valid)
        local_factory_identity_valid =
            load_factory_identity(local_factory_identity);
    if (transport == nullptr ||
        !transport->begin_client(address, USEQ_I2C_SDA_PIN, USEQ_I2C_SCL_PIN, this,
                                 receive_callback, request_callback))
    {
        client_mode    = false;
        client_address = 0;
        return false;
    }
    return true;
}

void I2CNetwork::scan_for_expanders()
{
    expander_count = 0;
    for (auto& expander : expanders)
        expander = ExpanderDescriptor{};
    if (!host_mode || transport == nullptr)
        return;

    for (uint8_t address = 1; address < 127 && expander_count < MAX_EXPANDERS;
         ++address)
    {
        if (!transport->probe(address))
            continue;
        if (!send_to(address, GET_TYPE_COMMAND, sizeof(GET_TYPE_COMMAND)))
            continue;

        uint8_t type_buffer[sizeof(EXPANDER_TYPE_RESPONSE)] = {};
        const size_t length =
            transport->request(address, type_buffer, sizeof(type_buffer));
        if (length >= 4 && std::memcmp(type_buffer, "aout", 4) == 0)
        {
            ExpanderDescriptor& descriptor = expanders[expander_count];
            descriptor.address = address;
            if (send_to(address, GET_IDENTITY_COMMAND,
                        sizeof(GET_IDENTITY_COMMAND)))
            {
                uint8_t identity_buffer[I2C_IDENTITY_RESPONSE_SIZE] = {};
                const size_t identity_length = transport->request(
                    address, identity_buffer, sizeof(identity_buffer));
                decode_identity_response(identity_buffer, identity_length,
                                         descriptor);
                descriptor.address = address;
            }
            expander_addrs[expander_count++] = address;
        }
    }
}

// ── Host operations ──────────────────────────────────────────────────────

void I2CNetwork::sync_all()
{
    static constexpr uint8_t message[] = { '$', 's', 'y', 'n', 'c' };
    for (uint8_t index = 0; index < expander_count; ++index)
    {
        send_to(expander_addrs[index], message, sizeof(message));
    }
}

void I2CNetwork::send_eval_to(uint8_t expander_index, const char* code)
{
    if (code == nullptr || expander_index >= expander_count)
        return;
    send_to(expander_addrs[expander_index], reinterpret_cast<const uint8_t*>(code),
            std::strlen(code));
}

void I2CNetwork::broadcast_tempo(double bpm, double beat_phase)
{
    uint8_t buffer[22] = {};
    std::memcpy(buffer, "$tempo", 6);
    std::memcpy(buffer + 6, &bpm, sizeof(bpm));
    std::memcpy(buffer + 14, &beat_phase, sizeof(beat_phase));
    for (uint8_t index = 0; index < expander_count; ++index)
    {
        send_to(expander_addrs[index], buffer, sizeof(buffer));
    }
}

void I2CNetwork::broadcast_output_values(const double* values, uint8_t count)
{
    const size_t length =
        encode_output_values(values, count, write_buffer, sizeof(write_buffer));
    if (length == 0)
        return;
    for (uint8_t index = 0; index < expander_count; ++index)
    {
        send_to(expander_addrs[index], write_buffer, length);
    }
}

bool I2CNetwork::send_to(uint8_t address, const uint8_t* data, size_t length)
{
    if (transport == nullptr || data == nullptr || length == 0 || address == 0 ||
        address >= 127)
    {
        return false;
    }

    constexpr size_t PACKET_SIZE = I2C_MAX_MSG_SIZE;
    size_t offset                = 0;
    while (offset < length)
    {
        const size_t chunk = std::min(PACKET_SIZE, length - offset);
        if (!transport->write(address, data + offset, chunk))
            return false;
        offset += chunk;
#ifdef ARDUINO
        if (offset < length)
            delayMicroseconds(5);
#endif
    }
    return true;
}

// ── Expander protocol ────────────────────────────────────────────────────

size_t I2CNetwork::encode_output_values(const double* values, uint8_t count,
                                        uint8_t* buffer, size_t capacity)
{
    static_assert(sizeof(double) == 8, "$vals requires IEEE-754 binary64");
    if (values == nullptr || buffer == nullptr || count == 0 ||
        count > I2C_EXPANDER_OUTPUT_COUNT)
    {
        return 0;
    }
    const size_t length = I2C_VALUES_HEADER_SIZE + count * sizeof(double);
    if (capacity < length)
        return 0;

    std::memcpy(buffer, VALUES_PREFIX, sizeof(VALUES_PREFIX));
    buffer[6] = count;
    for (uint8_t index = 0; index < count; ++index)
    {
        write_binary64_le(buffer + I2C_VALUES_HEADER_SIZE + index * sizeof(double),
                          values[index]);
    }
    return length;
}

bool I2CNetwork::decode_output_values(const uint8_t* buffer, size_t length,
                                      double* values, uint8_t& count)
{
    count = 0;
    if (buffer == nullptr || values == nullptr || length < I2C_VALUES_HEADER_SIZE ||
        std::memcmp(buffer, VALUES_PREFIX, sizeof(VALUES_PREFIX)) != 0)
    {
        return false;
    }

    const uint8_t packet_count = buffer[6];
    if (packet_count == 0 || packet_count > I2C_EXPANDER_OUTPUT_COUNT ||
        length != I2C_VALUES_HEADER_SIZE + packet_count * sizeof(double))
    {
        return false;
    }

    // Validate the complete packet before publishing any decoded value.
    for (uint8_t index = 0; index < packet_count; ++index)
    {
        const double candidate = read_binary64_le(buffer + I2C_VALUES_HEADER_SIZE +
                                                  index * sizeof(double));
        if (!std::isfinite(candidate))
            return false;
    }
    for (uint8_t index = 0; index < packet_count; ++index)
    {
        const double candidate = read_binary64_le(buffer + I2C_VALUES_HEADER_SIZE +
                                                  index * sizeof(double));
        values[index]          = std::max(0.0, std::min(1.0, candidate));
    }
    count = packet_count;
    return true;
}

size_t I2CNetwork::encode_identity_response(const FactoryIdentity& identity,
                                            bool identity_valid,
                                            const char* firmware_version,
                                            const char* firmware_target,
                                            uint16_t protocol_version,
                                            uint32_t capabilities,
                                            uint8_t* buffer, size_t capacity)
{
    if (buffer == nullptr || capacity < I2C_IDENTITY_RESPONSE_SIZE)
        return 0;
    std::memset(buffer, 0, I2C_IDENTITY_RESPONSE_SIZE);
    std::memcpy(buffer, IDENTITY_PREFIX, sizeof(IDENTITY_PREFIX));
    buffer[4] = 1; // response schema
    buffer[5] = identity_valid ? 1 : 0;
    write_u16_le(buffer + 6, FACTORY_IDENTITY_RECORD_SIZE);
    if (identity_valid &&
        !encode_factory_identity(identity, buffer + 8,
                                 FACTORY_IDENTITY_RECORD_SIZE))
        return 0;
    if (firmware_version != nullptr)
        std::strncpy(reinterpret_cast<char*>(buffer + 136), firmware_version, 31);
    if (firmware_target != nullptr)
        std::strncpy(reinterpret_cast<char*>(buffer + 168), firmware_target, 31);
    write_u16_le(buffer + 200, protocol_version);
    write_u32_le(buffer + 202, capabilities);
    return I2C_IDENTITY_RESPONSE_SIZE;
}

bool I2CNetwork::decode_identity_response(const uint8_t* buffer, size_t length,
                                          ExpanderDescriptor& descriptor)
{
    if (buffer == nullptr || length != I2C_IDENTITY_RESPONSE_SIZE ||
        std::memcmp(buffer, IDENTITY_PREFIX, sizeof(IDENTITY_PREFIX)) != 0 ||
        buffer[4] != 1 || read_u16_le(buffer + 6) != FACTORY_IDENTITY_RECORD_SIZE)
    {
        return false;
    }
    descriptor.factory_identity_valid =
        buffer[5] == 1 && decode_factory_identity(
                              buffer + 8, FACTORY_IDENTITY_RECORD_SIZE,
                              descriptor.factory_identity);
    std::memcpy(descriptor.firmware_version, buffer + 136, 31);
    descriptor.firmware_version[31] = '\0';
    std::memcpy(descriptor.firmware_target, buffer + 168, 31);
    descriptor.firmware_target[31] = '\0';
    descriptor.protocol_version = read_u16_le(buffer + 200);
    descriptor.capabilities     = read_u32_le(buffer + 202);
    return true;
}

void I2CNetwork::set_local_factory_identity(const FactoryIdentity& identity,
                                            bool valid)
{
    local_factory_identity       = identity;
    local_factory_identity_valid = valid;
}

size_t I2CNetwork::process_incoming(double* outputs, size_t output_capacity)
{
    if (!client_mode || outputs == nullptr || output_capacity == 0)
        return 0;

    size_t packets_applied = 0;
    size_t bytes_read      = 0;
    while (incoming.pop(read_buffer, sizeof(read_buffer), bytes_read))
    {
        uint8_t count = 0;
        if (!decode_output_values(read_buffer, bytes_read, decoded_values, count) ||
            count > output_capacity)
        {
            rejected_packets.fetch_add(1, std::memory_order_relaxed);
            continue;
        }

        std::memcpy(outputs, decoded_values, count * sizeof(double));
        ++packets_applied;
        applied_value_packets.fetch_add(1, std::memory_order_relaxed);
    }
    return packets_applied;
}

bool I2CNetwork::has_incoming() { return incoming.any(); }

bool I2CNetwork::read_incoming(uint8_t* buffer, size_t buffer_size,
                               size_t& bytes_read)
{
    return incoming.pop(buffer, buffer_size, bytes_read);
}

void I2CNetwork::receive_from_transport(const uint8_t* data, size_t length)
{
    if (is_get_type_command(data, length))
    {
        response_pending.store(1, std::memory_order_release);
        return;
    }
    if (is_get_identity_command(data, length))
    {
        response_pending.store(2, std::memory_order_release);
        return;
    }
    if (!incoming.push(data, length))
    {
        rejected_packets.fetch_add(1, std::memory_order_relaxed);
    }
}

size_t I2CNetwork::respond_to_transport(uint8_t* data, size_t capacity)
{
    if (data == nullptr)
        return 0;
    const uint8_t response = response_pending.exchange(0, std::memory_order_acq_rel);
    if (response == 1)
    {
        if (capacity < sizeof(EXPANDER_TYPE_RESPONSE))
            return 0;
        std::memcpy(data, EXPANDER_TYPE_RESPONSE, sizeof(EXPANDER_TYPE_RESPONSE));
        return sizeof(EXPANDER_TYPE_RESPONSE);
    }
    if (response == 2)
        return encode_identity_response(
            local_factory_identity, local_factory_identity_valid,
            build_info::VERSION, build_info::HARDWARE_TARGET,
            build_info::PROTOCOL_VERSION,
            EXPANDER_CAPABILITIES, data, capacity);
    return 0;
}

void I2CNetwork::receive_callback(void* context, const uint8_t* data, size_t length)
{
    if (context == nullptr)
        return;
    static_cast<I2CNetwork*>(context)->receive_from_transport(data, length);
}

size_t I2CNetwork::request_callback(void* context, uint8_t* data, size_t capacity)
{
    if (context == nullptr)
        return 0;
    return static_cast<I2CNetwork*>(context)->respond_to_transport(data, capacity);
}

} // namespace firmware

#endif // ENABLE_I2C_NETWORKING
