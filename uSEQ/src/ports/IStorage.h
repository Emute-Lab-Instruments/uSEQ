#ifndef ISTORAGE_H_
#define ISTORAGE_H_

#include <cstdint>
#include <cstddef>

struct IStorage {
    virtual ~IStorage() = default;
    virtual bool write(uint32_t offset, const uint8_t* data, size_t len) = 0;
    virtual bool read(uint32_t offset, uint8_t* data, size_t len) = 0;
    virtual bool erase(uint32_t offset, size_t len) = 0;
};

#endif // ISTORAGE_H_