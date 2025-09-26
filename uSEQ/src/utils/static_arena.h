#ifndef USEQ_STATIC_ARENA_H_
#define USEQ_STATIC_ARENA_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

// Simple fixed-capacity linear arena intended for microcontroller builds.
// Provides bump-pointer allocation with optional fallback reporting via
// return value. The arena never frees individual allocations; clients may
// reset() to reuse the entire buffer.
template <std::size_t Capacity>
class StaticArena
{
public:
    StaticArena() : m_offset(0) {}

    StaticArena(const StaticArena&)            = delete;
    StaticArena& operator=(const StaticArena&) = delete;

    void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t))
    {
        std::size_t current = align_up(m_offset, alignment);
        if (current + size > Capacity)
        {
            return nullptr;
        }
        void* ptr = m_buffer.data() + current;
        m_offset  = current + size;
        return ptr;
    }

    void reset() { m_offset = 0; }

    constexpr std::size_t capacity() const { return Capacity; }
    std::size_t used() const { return m_offset; }

private:
    static constexpr std::size_t align_up(std::size_t value, std::size_t alignment)
    {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    std::array<std::uint8_t, Capacity> m_buffer;
    std::size_t m_offset;
};

#endif // USEQ_STATIC_ARENA_H_
