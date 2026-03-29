#include "i2c_network.h"

#ifdef ENABLE_I2C_NETWORKING

#ifdef ARDUINO
#include <Wire.h>
#include "../uSEQ/configure.h"

// Fallback I2C pin definitions for hardware variants that omit them.
#ifndef _USEQ_SDA_PIN_
#define _USEQ_SDA_PIN_ 0
#endif
#ifndef _USEQ_SCL_PIN_
#define _USEQ_SCL_PIN_ 1
#endif
#endif

#include <cstring>
#include <algorithm>

namespace firmware {

// ── I2CIncoming ring buffer ───────────────────────────────────────────────

bool I2CIncoming::push(const uint8_t* buf, size_t len) {
    if (len == 0 || len > I2C_MAX_MSG_SIZE)
        return false;

    Slot& slot = slots[head];
    // If full, silently drop the oldest by advancing tail.
    if (slot.occupied) {
        tail = (tail + 1) % CAPACITY;
    }

    std::memcpy(slot.data, buf, len);
    slot.length   = len;
    slot.occupied = true;
    head = (head + 1) % CAPACITY;
    return true;
}

bool I2CIncoming::pop(uint8_t* buf, size_t buf_size, size_t& bytes_read) {
    if (tail == head && !slots[tail].occupied)
        return false;

    Slot& slot = slots[tail];
    if (!slot.occupied)
        return false;

    bytes_read = std::min(slot.length, buf_size);
    std::memcpy(buf, slot.data, bytes_read);
    slot.occupied = false;
    tail = (tail + 1) % CAPACITY;
    return true;
}

bool I2CIncoming::any() const {
    // Check if any slot between tail and head is occupied.
    if (tail == head)
        return slots[tail].occupied;
    return true;
}

// ── I2CNetwork ────────────────────────────────────────────────────────────

void I2CNetwork::init() {
#ifdef ARDUINO
    // Default to host mode (the common case for the primary module).
    host_mode   = true;
    client_mode = false;
    init_host();
#endif
}

void I2CNetwork::init_host() {
#ifdef ARDUINO
    // Setup the default I2C bus as host.
    Wire.end();
    Wire.setSDA(_USEQ_SDA_PIN_);
    Wire.setSCL(_USEQ_SCL_PIN_);
    Wire.begin();

    // Scan for connected expander modules.
    scan_for_expanders();
#endif
}

void I2CNetwork::scan_for_expanders() {
#ifdef ARDUINO
    // Probe all valid 7-bit addresses for devices.
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();
        if (err == 0) {
            // Device found at addr — could query type here.
            // For now, just note its existence.
            // Future: maintain a discovered-device list.
        }
    }
#endif
}

bool I2CNetwork::send_to(uint8_t address, const uint8_t* data, size_t len) {
    if (len == 0)
        return false;

#ifdef ARDUINO
    // Send in packets of up to 250 bytes (Wire library limit per
    // transmission is typically 256, use 250 for safety margin).
    constexpr size_t PACKET_SIZE = 250;
    size_t offset = 0;

    while (offset < len) {
        size_t chunk = std::min(PACKET_SIZE, len - offset);

        Wire.beginTransmission(address);
        Wire.write(data + offset, chunk);
        uint8_t err = Wire.endTransmission();

        if (err != 0) {
            // NACK or bus error — bounded failure.
            return false;
        }

        offset += chunk;

        // Brief inter-packet gap (matches old code: 5us between packets).
        if (offset < len) {
            delayMicroseconds(5);
        }
    }
    return true;
#else
    // Desktop: no-op, always succeed.
    (void)address;
    (void)data;
    (void)len;
    return true;
#endif
}

bool I2CNetwork::has_incoming() {
    return incoming.any();
}

bool I2CNetwork::read_incoming(uint8_t* buf, size_t buf_size, size_t& bytes_read) {
    return incoming.pop(buf, buf_size, bytes_read);
}

} // namespace firmware

#endif // ENABLE_I2C_NETWORKING
