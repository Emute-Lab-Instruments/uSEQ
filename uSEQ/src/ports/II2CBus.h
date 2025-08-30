#ifndef II2CBUS_H_
#define II2CBUS_H_

#include <cstdint>
#include <vector>

struct I2CMessage {
    uint8_t src;
    uint8_t dst;
    std::vector<uint8_t> payload;
    
    I2CMessage() : src(0), dst(0) {}
    I2CMessage(uint8_t s, uint8_t d, const std::vector<uint8_t>& p)
        : src(s), dst(d), payload(p) {}
};

struct II2CBus {
    virtual ~II2CBus() = default;
    virtual bool send(const I2CMessage& msg) = 0;
    virtual bool available(uint8_t addr) = 0;
    virtual bool receive(uint8_t addr, I2CMessage& out) = 0;
};

#endif // II2CBUS_H_