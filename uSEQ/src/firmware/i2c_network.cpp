#include "i2c_network.h"

#ifdef ENABLE_I2C_NETWORKING

#ifdef ARDUINO
#include <Wire.h>

// I2C pin definitions — override via PlatformIO build flags if needed
#ifndef USEQ_I2C_SDA_PIN
#define USEQ_I2C_SDA_PIN 0
#endif
#ifndef USEQ_I2C_SCL_PIN
#define USEQ_I2C_SCL_PIN 1
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
    Wire.setSDA(USEQ_I2C_SDA_PIN);
    Wire.setSCL(USEQ_I2C_SCL_PIN);
    Wire.begin();

    // Scan for connected expander modules.
    scan_for_expanders();
#endif
}

void I2CNetwork::scan_for_expanders() {
    expander_count = 0;
#ifdef ARDUINO
    for (uint8_t addr = 1; addr < 127 && expander_count < MAX_EXPANDERS; addr++) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();
        if (err == 0) {
            // Query device type
            Wire.beginTransmission(addr);
            Wire.write("$gettype", 8);
            Wire.endTransmission();

            Wire.requestFrom(addr, (uint8_t)7);
            char type_buf[8] = {};
            int n = 0;
            while (Wire.available() && n < 7)
                type_buf[n++] = (char)Wire.read();

            if (strstr(type_buf, "aout") || strstr(type_buf, "useq")) {
                expander_addrs[expander_count++] = addr;
            }
        }
    }
#endif
}

void I2CNetwork::sync_all() {
#ifdef ARDUINO
    for (uint8_t i = 0; i < expander_count; i++) {
        const char* msg = "$sync";
        send_to(expander_addrs[i], reinterpret_cast<const uint8_t*>(msg), 5);
    }
#endif
}

void I2CNetwork::send_eval_to(uint8_t expander_index, const char* code) {
#ifdef ARDUINO
    if (expander_index >= expander_count) return;
    size_t len = strlen(code);
    send_to(expander_addrs[expander_index],
            reinterpret_cast<const uint8_t*>(code), len);
#else
    (void)expander_index; (void)code;
#endif
}

void I2CNetwork::broadcast_tempo(double bpm, double beat_phase) {
#ifdef ARDUINO
    // Pack as: "$tempo" + 8 bytes BPM + 8 bytes phase
    uint8_t buf[22];
    std::memcpy(buf, "$tempo", 6);
    std::memcpy(buf + 6, &bpm, 8);
    std::memcpy(buf + 14, &beat_phase, 8);
    for (uint8_t i = 0; i < expander_count; i++)
        send_to(expander_addrs[i], buf, 22);
#else
    (void)bpm; (void)beat_phase;
#endif
}

void I2CNetwork::broadcast_output_values(const double* values, uint8_t count) {
#ifdef ARDUINO
    // Pack as: "$vals" + count byte + N doubles
    if (count > 24) count = 24;
    size_t payload = 6 + 1 + count * 8;
    uint8_t buf[6 + 1 + 24 * 8];
    std::memcpy(buf, "$vals", 5);
    buf[5] = '\0';
    buf[6] = count;
    std::memcpy(buf + 7, values, count * 8);
    for (uint8_t i = 0; i < expander_count; i++)
        send_to(expander_addrs[i], buf, payload);
#else
    (void)values; (void)count;
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
