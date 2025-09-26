#ifndef INTERNED_VALUE_MAP_H_
#define INTERNED_VALUE_MAP_H_

#include "symbol_intern.h"
#include "value.h"
#include "../../utils/string.h"
#include <unordered_map>
#include <optional>

// Optimized value map that uses interned symbols for fast lookup
class InternedValueMap {
public:
    // Store values by symbol ID for O(1) lookup
    using Storage = std::unordered_map<SymbolIntern::SymbolID, Value>;

    // Insert or update a value
    void set(const String& name, const Value& value) {
        SymbolIntern::SymbolID id = SymbolIntern::getInstance().intern(name);
        storage[id] = value;
    }

    // Get value by name (returns nullopt if not found)
    std::optional<Value> get(const String& name) const {
        SymbolIntern::SymbolID id = SymbolIntern::getInstance().getID(name);
        if (id == SymbolIntern::INVALID_ID) {
            return std::nullopt;
        }

        auto it = storage.find(id);
        if (it != storage.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    // Get value by interned ID (fastest)
    std::optional<Value> getById(SymbolIntern::SymbolID id) const {
        auto it = storage.find(id);
        if (it != storage.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    // Check if name exists
    bool has(const String& name) const {
        SymbolIntern::SymbolID id = SymbolIntern::getInstance().getID(name);
        return id != SymbolIntern::INVALID_ID && storage.find(id) != storage.end();
    }

    // Check if ID exists (fastest)
    bool hasById(SymbolIntern::SymbolID id) const {
        return storage.find(id) != storage.end();
    }

    // Erase by name
    void erase(const String& name) {
        SymbolIntern::SymbolID id = SymbolIntern::getInstance().getID(name);
        if (id != SymbolIntern::INVALID_ID) {
            storage.erase(id);
        }
    }

    // Clear all values
    void clear() {
        storage.clear();
    }

    // Size
    size_t size() const {
        return storage.size();
    }

    // Iterators for compatibility
    auto begin() { return storage.begin(); }
    auto end() { return storage.end(); }
    auto begin() const { return storage.begin(); }
    auto end() const { return storage.end(); }

    // For compatibility with existing code using std::map interface
    Value& operator[](const String& name) {
        SymbolIntern::SymbolID id = SymbolIntern::getInstance().intern(name);
        return storage[id];
    }

private:
    Storage storage;
};

#endif // INTERNED_VALUE_MAP_H_