#ifndef FIRMWARE_I2C_NETWORK_H
#define FIRMWARE_I2C_NETWORK_H

// Optional — multi-module I2C communication
// Wraps the old i2cHost/i2cClient code behind a clean interface.
// Active only when ENABLE_I2C_NETWORKING is defined (see configure.h).

#ifdef ENABLE_I2C_NETWORKING

#include <cstdint>
#include <cstddef>

namespace firmware {

// Maximum size of a single I2C message payload (bytes).
static constexpr size_t I2C_MAX_MSG_SIZE = 500;

// Simple ring buffer for incoming I2C messages.
struct I2CIncoming {
    static constexpr size_t CAPACITY = 4;
    struct Slot {
        uint8_t data[I2C_MAX_MSG_SIZE] = {};
        size_t  length                 = 0;
        bool    occupied               = false;
    };
    Slot    slots[CAPACITY] = {};
    uint8_t head            = 0;
    uint8_t tail            = 0;

    bool push(const uint8_t* buf, size_t len);
    bool pop(uint8_t* buf, size_t buf_size, size_t& bytes_read);
    bool any() const;
};

struct I2CNetwork {
    // ── State ─────────────────────────────────────────────────────────────
    bool host_mode   = false;
    bool client_mode = false;
    I2CIncoming incoming;

    // ── Methods ───────────────────────────────────────────────────────────
    void init();
    bool send_to(uint8_t address, const uint8_t* data, size_t len);
    bool has_incoming();
    bool read_incoming(uint8_t* buf, size_t buf_size, size_t& bytes_read);

    // Host-mode helpers
    void init_host();
    void scan_for_expanders();
};

} // namespace firmware

#endif // ENABLE_I2C_NETWORKING

#endif // FIRMWARE_I2C_NETWORK_H
