#ifndef FIRMWARE_I2C_NETWORK_H
#define FIRMWARE_I2C_NETWORK_H

// Optional — multi-module I2C communication
// Wraps the old i2cHost/i2cClient code behind a clean interface.
// Active only when ENABLE_I2C_NETWORKING is defined (see configure.h).

#ifdef ENABLE_I2C_NETWORKING

#include "../ports/II2CTransport.h"
#include "factory_identity.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace firmware
{

// Maximum size of one Wire transaction. Larger writes are chunked by the
// host; the output-expander frame is at most 71 bytes.
static constexpr size_t I2C_MAX_MSG_SIZE           = 250;
static constexpr uint8_t I2C_EXPANDER_OUTPUT_COUNT = 8;
static constexpr size_t I2C_VALUES_HEADER_SIZE     = 7;
static constexpr size_t I2C_VALUES_MAX_SIZE =
    I2C_VALUES_HEADER_SIZE + I2C_EXPANDER_OUTPUT_COUNT * sizeof(double);
static constexpr size_t I2C_IDENTITY_RESPONSE_SIZE = 206;

struct ExpanderDescriptor
{
    uint8_t address = 0;
    bool factory_identity_valid = false;
    FactoryIdentity factory_identity;
    char firmware_version[32] = {};
    char firmware_target[32] = {};
    uint16_t protocol_version = 0;
    uint32_t capabilities = 0;
};

// Simple ring buffer for incoming I2C messages.
struct I2CIncoming
{
    static constexpr size_t CAPACITY     = 4;
    static constexpr size_t STORAGE_SIZE = CAPACITY + 1;
    struct Slot
    {
        uint8_t data[I2C_MAX_MSG_SIZE] = {};
        size_t length                  = 0;
    };
    Slot slots[STORAGE_SIZE] = {};
    std::atomic<uint8_t> head{ 0 };
    std::atomic<uint8_t> tail{ 0 };

    bool push(const uint8_t* buf, size_t len);
    bool pop(uint8_t* buf, size_t buf_size, size_t& bytes_read);
    bool any() const;
};

struct I2CNetwork
{
    // ── State ─────────────────────────────────────────────────────────────
    bool host_mode   = false;
    bool client_mode = false;
    I2CIncoming incoming;
    II2CTransport* transport = nullptr;
    uint8_t client_address   = 0;
    // 0 = none, 1 = legacy type response, 2 = full identity response.
    std::atomic<uint8_t> response_pending{ 0 };
    std::atomic<uint32_t> applied_value_packets{ 0 };
    std::atomic<uint32_t> rejected_packets{ 0 };

    // Fixed scratch keeps packet processing allocation-free in Firmware::tick.
    uint8_t read_buffer[I2C_MAX_MSG_SIZE]            = {};
    uint8_t write_buffer[I2C_VALUES_MAX_SIZE]        = {};
    double decoded_values[I2C_EXPANDER_OUTPUT_COUNT] = {};

    // Discovered expander addresses (max 5)
    static constexpr uint8_t MAX_EXPANDERS = 5;
    uint8_t expander_addrs[MAX_EXPANDERS]  = {};
    ExpanderDescriptor expanders[MAX_EXPANDERS] = {};
    uint8_t expander_count                 = 0;
    FactoryIdentity local_factory_identity;
    bool local_factory_identity_valid = false;

    // ── Methods ───────────────────────────────────────────────────────────
    void set_transport(II2CTransport* value);
    bool init();
    bool init_client(uint8_t address);
    bool send_to(uint8_t address, const uint8_t* data, size_t len);
    bool has_incoming();
    bool read_incoming(uint8_t* buf, size_t buf_size, size_t& bytes_read);

    // Host-mode: sync all connected expanders
    void sync_all();
    void send_eval_to(uint8_t expander_index, const char* code);
    void broadcast_tempo(double bpm, double beat_phase);
    void broadcast_output_values(const double* values, uint8_t count);

    // Client-mode: consume complete packets queued by the Wire callback and
    // atomically publish valid output vectors to the caller-owned buffer.
    size_t process_incoming(double* outputs, size_t output_capacity);

    // Pure protocol helpers, also used by native simulation tests.
    static size_t encode_output_values(const double* values, uint8_t count,
                                       uint8_t* buffer, size_t capacity);
    static bool decode_output_values(const uint8_t* buffer, size_t length,
                                     double* values, uint8_t& count);
    static size_t encode_identity_response(const FactoryIdentity& identity,
                                           bool identity_valid,
                                           const char* firmware_version,
                                           const char* firmware_target,
                                           uint16_t protocol_version,
                                           uint32_t capabilities,
                                           uint8_t* buffer, size_t capacity);
    static bool decode_identity_response(const uint8_t* buffer, size_t length,
                                         ExpanderDescriptor& descriptor);

    // Manufacturing/test seam. Production init loads the reserved blob.
    void set_local_factory_identity(const FactoryIdentity& identity, bool valid);

    // Host-mode helpers
    bool init_host();
    void scan_for_expanders();

    // Transport callbacks. They only copy bytes or produce a fixed response;
    // decoding and hardware writes stay in the main loop.
    void receive_from_transport(const uint8_t* data, size_t length);
    size_t respond_to_transport(uint8_t* data, size_t capacity);

    static void receive_callback(void* context, const uint8_t* data, size_t length);
    static size_t request_callback(void* context, uint8_t* data, size_t capacity);
};

} // namespace firmware

#endif // ENABLE_I2C_NETWORKING

#endif // FIRMWARE_I2C_NETWORK_H
