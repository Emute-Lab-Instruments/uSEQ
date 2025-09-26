#ifndef ENVIRONMENT_H_
#define ENVIRONMENT_H_

#include "../../utils/string.h"
#include "symbol_interner.h"
#include "value.h"
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <vector>

#ifdef ARDUINO
#define FAST_MEM_ENV __not_in_flash("ENVIRONMENTDATA")
#else
#define FAST_MEM_ENV // Empty for desktop builds
#endif
// class Value;

class ValueMap
{
public:
    struct KeyView
    {
        KeyView() = default;
        explicit KeyView(modulisp::SymbolId symbol) : id(symbol) {}

        const char* c_str() const;
        std::size_t length() const;
        String to_string() const;
        operator String() const { return to_string(); }
        bool equals(const String& other) const;
        bool equals(const char* data, std::size_t len) const;
        bool operator==(const KeyView& other) const { return id == other.id; }

        modulisp::SymbolId id{ modulisp::kInvalidSymbolId };
    };

    struct Entry
    {
        KeyView first;
        Value second;
    };

    class iterator
    {
    public:
        using difference_type   = std::ptrdiff_t;
        using value_type        = Entry;
        using pointer           = Entry*;
        using reference         = Entry&;
        using iterator_category = std::forward_iterator_tag;

        iterator(ValueMap* map, std::size_t index);

        reference operator*() const;
        pointer operator->() const;
        iterator& operator++();
        iterator operator++(int)
        {
            iterator tmp(*this);
            ++(*this);
            return tmp;
        }
        bool operator==(const iterator& other) const
        {
            return m_map == other.m_map && m_index == other.m_index;
        }
        bool operator!=(const iterator& other) const { return !(*this == other); }

    private:
        void skip_invalid();

        ValueMap* m_map;
        std::size_t m_index;
    };

    class const_iterator
    {
    public:
        using difference_type   = std::ptrdiff_t;
        using value_type        = Entry;
        using pointer           = const Entry*;
        using reference         = const Entry&;
        using iterator_category = std::forward_iterator_tag;

        const_iterator(const ValueMap* map, std::size_t index);

        reference operator*() const;
        pointer operator->() const;
        const_iterator& operator++();
        const_iterator operator++(int)
        {
            const_iterator tmp(*this);
            ++(*this);
            return tmp;
        }
        bool operator==(const const_iterator& other) const
        {
            return m_map == other.m_map && m_index == other.m_index;
        }
        bool operator!=(const const_iterator& other) const
        {
            return !(*this == other);
        }

    private:
        void skip_invalid();

        const ValueMap* m_map;
        std::size_t m_index;
    };

    ValueMap();

    Value& operator[](const String& name);
    Value& operator[](modulisp::SymbolId id);

    std::optional<Value> get(const String& name) const;
    std::optional<Value> get(modulisp::SymbolId id) const;

    bool has(const String& name) const;
    bool has(modulisp::SymbolId id) const;

    iterator find(const String& name);
    iterator find(modulisp::SymbolId id);
    const_iterator find(const String& name) const;
    const_iterator find(modulisp::SymbolId id) const;

    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

    void erase(const String& name);
    void erase(modulisp::SymbolId id);
    void clear();

    std::size_t size() const { return m_size; }
    bool empty() const { return m_size == 0; }

private:
    friend class iterator;
    friend class const_iterator;

    enum SlotState : std::uint8_t
    {
        SLOT_EMPTY     = 0,
        SLOT_OCCUPIED  = 1,
        SLOT_TOMBSTONE = 2
    };

    void ensure_capacity();
    void rehash(std::size_t new_capacity);
    std::size_t mask() const { return m_entries.empty() ? 0 : (m_entries.size() - 1); }
    std::size_t find_slot(modulisp::SymbolId id, std::uint32_t hash, bool for_insert) const;
    std::size_t find_slot(modulisp::SymbolId id, std::uint32_t hash, bool for_insert);
    void insert_existing(modulisp::SymbolId id, std::uint32_t hash, const Value& value);

    std::vector<Entry> m_entries;
    std::vector<std::uint8_t> m_states;
    std::size_t m_size;
};

using BuiltinMap = ValueMap;

// An instance of a function's scope.
// template <size_t MAX_SIZE>
class Environment
{
public:
    // Default constructor
    Environment() : m_parent_env(NULL)
    {
        // init();
    }

    // Copy constructor: copy defs, def_exprs, and parent pointer
    Environment(const Environment& v)
        : m_defs(v.m_defs), m_def_exprs(v.m_def_exprs), m_parent_env(v.m_parent_env)
    {
        // init();
    }
    // Move constructor: move maps and copy parent pointer
    Environment(Environment&& v)
        : m_defs(std::move(v.m_defs)), m_def_exprs(std::move(v.m_def_exprs)),
          m_parent_env(v.m_parent_env)
    {
        // init();
    }
    // Copy assignment: copy defs, def_exprs, and parent pointer
    Environment& operator=(const Environment& env2)
    {
        if (this != &env2)
        {
            this->m_defs       = env2.m_defs;
            this->m_def_exprs  = env2.m_def_exprs;
            this->m_parent_env = env2.m_parent_env;
        }
        return *this;
    }

    // Does this environment, or its parent environment,
    // have this atom in scope?
    // This is only used to determine which atoms to capture when
    // creating a lambda function.
    bool has(String const& name) const;
    // Get the value associated with this name in this scope
    std::optional<Value> get(const String& name) const;
    std::optional<Value> get(modulisp::SymbolId id) const;
    std::optional<Value> get_expr(const String& name) const;
    std::optional<Value> get_expr(modulisp::SymbolId id) const;
    // Set the value associated with this name in this scope
    void set(const String& name, Value value);
    void set(modulisp::SymbolId id, Value value);
    void set_expr(const String& name, Value value);
    void set_expr(modulisp::SymbolId id, Value value);

    void unset(const String& name);
    void unset(modulisp::SymbolId id);
    void unset_expr(const String& name);
    void unset_expr(modulisp::SymbolId id);
    void set_global(const String& name, Value value);
    void set_global(modulisp::SymbolId id, Value value);
    void set_global_expr(const String& name, Value value);
    void set_global_expr(modulisp::SymbolId id, Value value);

    void combine(Environment const& other);
    String toString();

    void set_parent_scope(Environment* parent) { m_parent_env = parent; }

    // Output this scope in readable form to a stream.
    friend std::ostream& operator<<(std::ostream& os, Environment const& v);

    static BuiltinMap& FAST_MEM_ENV builtindefs();

    // Test accessors for internal maps
    ValueMap& __test_get_defs() { return m_defs; }
    ValueMap& __test_get_def_exprs() { return m_def_exprs; }

    // Public accessors for def_exprs and defs
    ValueMap& get_defs() { return m_defs; }
    ValueMap& get_def_exprs() { return m_def_exprs; }
    const ValueMap& get_defs() const { return m_defs; }
    const ValueMap& get_def_exprs() const { return m_def_exprs; }

protected:
    ValueMap m_defs;
    ValueMap m_def_exprs;

    // The definitions in the scope.
    Environment* m_parent_env;
    // Environment* m_child_env;
};

#endif // ENVIRONMENT_H_
