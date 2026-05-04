#ifndef SYMBOL_INTERN_H_
#define SYMBOL_INTERN_H_

#include "../../utils/string.h"
#include <map>  // Using std::map instead of unordered_map due to arduino::String compatibility
#include <vector>
#include <cstdint>

// Symbol interning system for fast symbol comparison
// Instead of comparing strings, we compare integer IDs
class SymbolIntern {
public:
    using SymbolID = uint32_t;
    static constexpr SymbolID INVALID_ID = 0;

    // Get the singleton instance
    static SymbolIntern& getInstance() {
        static SymbolIntern instance;
        return instance;
    }

    // Intern a symbol and get its ID
    // If the symbol already exists, returns the existing ID
    SymbolID intern(const String& symbol) {
        auto it = symbol_to_id.find(symbol);
        if (it != symbol_to_id.end()) {
            return it->second;
        }

        // Allocate new ID
        SymbolID id = next_id++;
        symbol_to_id[symbol] = id;

        // Store the string for reverse lookup if needed
        if (id >= id_to_symbol.size()) {
            id_to_symbol.resize(id + 1);
        }
        id_to_symbol[id] = symbol;

        return id;
    }

    // Get the string for a symbol ID (for debugging/display)
    const String& getString(SymbolID id) const {
        static const String empty_string;
        if (id == INVALID_ID || id >= id_to_symbol.size()) {
            return empty_string;
        }
        return id_to_symbol[id];
    }

    // Get ID without interning (returns INVALID_ID if not found)
    SymbolID getID(const String& symbol) const {
        auto it = symbol_to_id.find(symbol);
        return (it != symbol_to_id.end()) ? it->second : INVALID_ID;
    }

private:
    SymbolIntern() : next_id(1) {
        // Reserve space for common case
        id_to_symbol.reserve(256);
        // Note: std::map doesn't have reserve()
    }

    std::map<String, SymbolID> symbol_to_id;
    std::vector<String> id_to_symbol;
    SymbolID next_id;

    // Prevent copying
    SymbolIntern(const SymbolIntern&) = delete;
    SymbolIntern& operator=(const SymbolIntern&) = delete;
};

// Convenience function for quick access
inline SymbolIntern::SymbolID internSymbol(const String& symbol) {
    return SymbolIntern::getInstance().intern(symbol);
}

inline const String& getSymbolString(SymbolIntern::SymbolID id) {
    return SymbolIntern::getInstance().getString(id);
}

#endif // SYMBOL_INTERN_H_
