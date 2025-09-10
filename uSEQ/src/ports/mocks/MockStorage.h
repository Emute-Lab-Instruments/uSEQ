#ifndef MOCKSTORAGE_H_
#define MOCKSTORAGE_H_

#include "../IStorage.h"
#include <cstring>
#include <vector>

class MockStorage : public IStorage
{
public:
    explicit MockStorage(size_t size = 64 * 1024) : buffer_(size, 0) {}
    ~MockStorage() override = default;

    bool write(uint32_t offset, const uint8_t* data, size_t len) override
    {
        if (offset + len > buffer_.size())
        {
            return false; // Out of bounds
        }

        std::memcpy(buffer_.data() + offset, data, len);
        return true;
    }

    bool read(uint32_t offset, uint8_t* data, size_t len) override
    {
        if (offset + len > buffer_.size())
        {
            return false; // Out of bounds
        }

        std::memcpy(data, buffer_.data() + offset, len);
        return true;
    }

    bool erase(uint32_t offset, size_t len) override
    {
        if (offset + len > buffer_.size())
        {
            return false; // Out of bounds
        }

        std::memset(buffer_.data() + offset, 0, len);
        return true;
    }

    // Test helpers
    size_t size() const { return buffer_.size(); }

    const uint8_t* data() const { return buffer_.data(); }

    void clear() { std::fill(buffer_.begin(), buffer_.end(), 0); }

    void resize(size_t new_size) { buffer_.resize(new_size, 0); }

private:
    std::vector<uint8_t> buffer_;
};

#endif // MOCKSTORAGE_H_