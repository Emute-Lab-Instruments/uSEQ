#ifndef MODULISP_SYMBOL_INTERNER_H_
#define MODULISP_SYMBOL_INTERNER_H_

#include "../../utils/string.h"
#include "../../utils/static_arena.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace modulisp
{

using SymbolId = std::uint32_t;

constexpr SymbolId kInvalidSymbolId = 0;

// Interns symbol names into a stable arena-backed storage so that symbol
// comparisons can be performed via their integer identifiers rather than via
// dynamic strings. Designed for microcontroller targets where heap usage must be
// predictable.
class SymbolInterner
{
public:
    static SymbolInterner& instance();

    SymbolId intern(const char* data, std::size_t length);
    SymbolId intern(const String& str) { return intern(str.c_str(), str.length()); }

    std::optional<SymbolId> lookup(const char* data, std::size_t length) const;
    std::optional<SymbolId> lookup(const String& str) const
    {
        return lookup(str.c_str(), str.length());
    }

    const char* c_str(SymbolId id) const;
    std::size_t length(SymbolId id) const;
    String to_string(SymbolId id) const;

    // Returns the precomputed FNV1a hash for the symbol.
    std::uint32_t hash(SymbolId id) const;

    bool equals(SymbolId id, const char* data, std::size_t length) const;

private:
    SymbolInterner();

    struct Entry
    {
        SymbolId id{ kInvalidSymbolId };
        std::uint32_t hash{ 0 };  // 32-bit FNV-1a hash of the string data
        std::uint32_t length{ 0 };
        const char* data{ nullptr };
    };

    using TableIndex = std::uint32_t; // 0 == empty, UINT32_MAX == tombstone

    void rehash(std::size_t new_capacity);
    std::size_t mask() const { return m_table.size() - 1; }
    std::uint32_t compute_hash(const char* data, std::size_t length) const;
    const char* allocate_storage(const char* data, std::size_t length);

    StaticArena<8192> m_inline_arena;
    std::vector<std::unique_ptr<char[]>> m_overflow_blocks;
    std::vector<Entry> m_entries;
    std::vector<TableIndex> m_table;
    std::size_t m_occupied{ 0 };
};

} // namespace modulisp

#endif // MODULISP_SYMBOL_INTERNER_H_
