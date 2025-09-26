#include "symbol_interner.h"

#include "../../utils/log.h"
#include <cstring>
#include <limits>

namespace modulisp
{

namespace
{
constexpr std::size_t kInitialCapacity = 64;
constexpr std::uint32_t kFnvOffset     = 2166136261u;
constexpr std::uint32_t kFnvPrime      = 16777619u;
} // namespace

SymbolInterner& SymbolInterner::instance()
{
    static SymbolInterner g_instance;
    return g_instance;
}

SymbolInterner::SymbolInterner()
{
    m_table.resize(kInitialCapacity, 0);
    m_entries.reserve(kInitialCapacity / 2);
}

SymbolId SymbolInterner::intern(const char* data, std::size_t length)
{
    if (length == 0)
    {
        // The empty symbol gets a dedicated ID so lookups stay cheap.
        auto existing = lookup(data, length);
        if (existing.has_value())
        {
            return *existing;
        }
    }

    if ((m_occupied + 1) * 10 >= m_table.size() * 7)
    {
        rehash(m_table.size() * 2);
    }

    const std::uint32_t hash   = compute_hash(data, length);
    const std::size_t table_sz = m_table.size();
    const std::size_t m        = table_sz - 1;

    std::size_t idx          = hash & m;
    std::size_t first_tomb   = static_cast<std::size_t>(-1);
    bool has_first_tombstone = false;

    while (true)
    {
        TableIndex slot = m_table[idx];
        if (slot == 0)
        {
            if (has_first_tombstone)
            {
                idx = first_tomb;
            }
            Entry entry;
            entry.id     = static_cast<SymbolId>(m_entries.size() + 1);
            entry.hash   = hash;
            entry.length = static_cast<std::uint32_t>(length);
            entry.data   = allocate_storage(data, length);
            m_entries.push_back(entry);
            m_table[idx] = static_cast<TableIndex>(m_entries.size());
            ++m_occupied;
            return entry.id;
        }
        if (slot == std::numeric_limits<TableIndex>::max())
        {
            if (!has_first_tombstone)
            {
                first_tomb         = idx;
                has_first_tombstone = true;
            }
        }
        else
        {
            Entry& existing = m_entries[slot - 1];
            if (existing.hash == hash && existing.length == length &&
                std::memcmp(existing.data, data, length) == 0)
            {
                return existing.id;
            }
        }
        idx = (idx + 1) & m;
    }
}

std::optional<SymbolId> SymbolInterner::lookup(const char* data, std::size_t length) const
{
    if (m_table.empty())
    {
        return std::nullopt;
    }

    const std::uint32_t hash   = compute_hash(data, length);
    const std::size_t m        = mask();
    std::size_t idx            = hash & m;

    while (true)
    {
        TableIndex slot = m_table[idx];
        if (slot == 0)
        {
            return std::nullopt;
        }
        if (slot != std::numeric_limits<TableIndex>::max())
        {
            const Entry& existing = m_entries[slot - 1];
            if (existing.hash == hash && existing.length == length &&
                std::memcmp(existing.data, data, length) == 0)
            {
                return existing.id;
            }
        }
        idx = (idx + 1) & m;
    }
}

const char* SymbolInterner::c_str(SymbolId id) const
{
    if (id == kInvalidSymbolId)
    {
        return "";
    }
    const std::size_t index = static_cast<std::size_t>(id - 1);
    if (index >= m_entries.size())
    {
        return "";
    }
    return m_entries[index].data;
}

std::size_t SymbolInterner::length(SymbolId id) const
{
    if (id == kInvalidSymbolId)
    {
        return 0;
    }
    const std::size_t index = static_cast<std::size_t>(id - 1);
    if (index >= m_entries.size())
    {
        return 0;
    }
    return m_entries[index].length;
}

String SymbolInterner::to_string(SymbolId id) const
{
    const char* ptr = c_str(id);
    return String(ptr);
}

std::uint32_t SymbolInterner::hash(SymbolId id) const
{
    if (id == kInvalidSymbolId)
    {
        return 0;
    }
    const std::size_t index = static_cast<std::size_t>(id - 1);
    if (index >= m_entries.size())
    {
        return 0;
    }
    return m_entries[index].hash;
}

bool SymbolInterner::equals(SymbolId id, const char* data, std::size_t length) const
{
    if (id == kInvalidSymbolId)
    {
        return false;
    }
    const std::size_t index = static_cast<std::size_t>(id - 1);
    if (index >= m_entries.size())
    {
        return false;
    }
    const Entry& entry = m_entries[index];
    if (entry.length != length)
    {
        return false;
    }
    return std::memcmp(entry.data, data, length) == 0;
}

void SymbolInterner::rehash(std::size_t new_capacity)
{
    if (new_capacity < kInitialCapacity)
    {
        new_capacity = kInitialCapacity;
    }
    if ((new_capacity & (new_capacity - 1)) != 0)
    {
        // Ensure power-of-two capacity for masking arithmetic.
        std::size_t power_of_two = 1;
        while (power_of_two < new_capacity)
        {
            power_of_two <<= 1U;
        }
        new_capacity = power_of_two;
    }

    std::vector<TableIndex> new_table(new_capacity, 0);
    const std::size_t new_mask = new_capacity - 1;

    for (std::size_t i = 0; i < m_entries.size(); ++i)
    {
        const Entry& entry = m_entries[i];
        std::size_t idx    = entry.hash & new_mask;
        while (new_table[idx] != 0)
        {
            idx = (idx + 1) & new_mask;
        }
        new_table[idx] = static_cast<TableIndex>(i + 1);
    }

    m_table.swap(new_table);
    m_occupied = m_entries.size();
}

std::uint32_t SymbolInterner::compute_hash(const char* data, std::size_t length) const
{
    std::uint32_t hash = kFnvOffset;
    for (std::size_t i = 0; i < length; ++i)
    {
        hash ^= static_cast<std::uint8_t>(data[i]);
        hash *= kFnvPrime;
    }
    return hash ? hash : 1; // Avoid zero hash to preserve tombstone sentinel
}

const char* SymbolInterner::allocate_storage(const char* data, std::size_t length)
{
    const std::size_t bytes = length + 1; // include null terminator
    void* storage           = m_inline_arena.allocate(bytes, alignof(char));
    char* out               = nullptr;
    if (storage)
    {
        out = static_cast<char*>(storage);
    }
    else
    {
        auto owned = std::make_unique<char[]>(bytes);
        out        = owned.get();
        m_overflow_blocks.push_back(std::move(owned));
    }
    std::memcpy(out, data, length);
    out[length] = '\0';
    return out;
}

} // namespace modulisp
