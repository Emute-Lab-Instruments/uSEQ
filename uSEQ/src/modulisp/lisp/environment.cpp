#include "environment.h"
#include "../../utils.h"
#include "../../utils/log.h"
#include "symbol_interner.h"
#include "value.h"
#include <algorithm>
#include <optional>
using modulisp::SymbolId;
using modulisp::SymbolInterner;

// std::ostream &operator<<(std::ostream &os, Environment const &e) {
//   auto itr = e.defs.begin();
//   os << "{ ";
//   for (; itr != e.defs.end(); itr++) {
//     os << '\'' << itr->first << "' : " << itr->second.debug() << ", ";
//   }
//   return os << "}";
// }

const char* ValueMap::KeyView::c_str() const
{
    return SymbolInterner::instance().c_str(id);
}

std::size_t ValueMap::KeyView::length() const
{
    return SymbolInterner::instance().length(id);
}

String ValueMap::KeyView::to_string() const
{
    return SymbolInterner::instance().to_string(id);
}

bool ValueMap::KeyView::equals(const String& other) const
{
    return SymbolInterner::instance().equals(id, other.c_str(), other.length());
}

bool ValueMap::KeyView::equals(const char* data, std::size_t len) const
{
    return SymbolInterner::instance().equals(id, data, len);
}

ValueMap::iterator::iterator(ValueMap* map, std::size_t index)
    : m_map(map), m_index(index)
{
    skip_invalid();
}

ValueMap::iterator::reference ValueMap::iterator::operator*() const
{
    return m_map->m_entries[m_index];
}

ValueMap::iterator::pointer ValueMap::iterator::operator->() const
{
    return &m_map->m_entries[m_index];
}

ValueMap::iterator& ValueMap::iterator::operator++()
{
    ++m_index;
    skip_invalid();
    return *this;
}

void ValueMap::iterator::skip_invalid()
{
    while (m_map && m_index < m_map->m_states.size() &&
           m_map->m_states[m_index] != SLOT_OCCUPIED)
    {
        ++m_index;
    }
}

ValueMap::const_iterator::const_iterator(const ValueMap* map, std::size_t index)
    : m_map(map), m_index(index)
{
    skip_invalid();
}

ValueMap::const_iterator::reference ValueMap::const_iterator::operator*() const
{
    return m_map->m_entries[m_index];
}

ValueMap::const_iterator::pointer ValueMap::const_iterator::operator->() const
{
    return &m_map->m_entries[m_index];
}

ValueMap::const_iterator& ValueMap::const_iterator::operator++()
{
    ++m_index;
    skip_invalid();
    return *this;
}

void ValueMap::const_iterator::skip_invalid()
{
    while (m_map && m_index < m_map->m_states.size() &&
           m_map->m_states[m_index] != SLOT_OCCUPIED)
    {
        ++m_index;
    }
}

ValueMap::ValueMap() : m_size(0) {}

Value& ValueMap::operator[](const String& name)
{
    SymbolId id = SymbolInterner::instance().intern(name);
    return (*this)[id];
}

Value& ValueMap::operator[](SymbolId id)
{
    if (id == modulisp::kInvalidSymbolId)
    {
        static Value dummy;
        return dummy;
    }
    ensure_capacity();
    const std::uint32_t hash = SymbolInterner::instance().hash(id);
    const std::size_t slot   = find_slot(id, hash, true);
    if (m_states[slot] != SLOT_OCCUPIED)
    {
        m_states[slot]       = SLOT_OCCUPIED;
        m_entries[slot].first = KeyView(id);
        m_entries[slot].second = Value();
        ++m_size;
    }
    return m_entries[slot].second;
}

std::optional<Value> ValueMap::get(const String& name) const
{
    auto maybe_id = SymbolInterner::instance().lookup(name);
    if (!maybe_id.has_value())
    {
        return std::nullopt;
    }
    return get(*maybe_id);
}

std::optional<Value> ValueMap::get(SymbolId id) const
{
    if (m_entries.empty() || id == modulisp::kInvalidSymbolId)
    {
        return std::nullopt;
    }
    const std::uint32_t hash = SymbolInterner::instance().hash(id);
    const std::size_t slot   = find_slot(id, hash, false);
    if (slot >= m_states.size() || m_states[slot] != SLOT_OCCUPIED ||
        m_entries[slot].first.id != id)
    {
        return std::nullopt;
    }
    return m_entries[slot].second;
}

bool ValueMap::has(const String& name) const
{
    return get(name).has_value();
}

bool ValueMap::has(SymbolId id) const
{
    return get(id).has_value();
}

ValueMap::iterator ValueMap::find(const String& name)
{
    auto maybe_id = SymbolInterner::instance().lookup(name);
    if (!maybe_id.has_value())
    {
        return end();
    }
    return find(*maybe_id);
}

ValueMap::iterator ValueMap::find(SymbolId id)
{
    if (m_entries.empty() || id == modulisp::kInvalidSymbolId)
    {
        return end();
    }
    const std::uint32_t hash = SymbolInterner::instance().hash(id);
    const std::size_t slot   = find_slot(id, hash, false);
    if (slot >= m_states.size() || m_states[slot] != SLOT_OCCUPIED ||
        m_entries[slot].first.id != id)
    {
        return end();
    }
    return iterator(this, slot);
}

ValueMap::const_iterator ValueMap::find(const String& name) const
{
    auto maybe_id = SymbolInterner::instance().lookup(name);
    if (!maybe_id.has_value())
    {
        return end();
    }
    return find(*maybe_id);
}

ValueMap::const_iterator ValueMap::find(SymbolId id) const
{
    if (m_entries.empty() || id == modulisp::kInvalidSymbolId)
    {
        return end();
    }
    const std::uint32_t hash = SymbolInterner::instance().hash(id);
    const std::size_t slot   = find_slot(id, hash, false);
    if (slot >= m_states.size() || m_states[slot] != SLOT_OCCUPIED ||
        m_entries[slot].first.id != id)
    {
        return end();
    }
    return const_iterator(this, slot);
}

ValueMap::iterator ValueMap::begin() { return iterator(this, 0); }

ValueMap::iterator ValueMap::end()
{
    return iterator(this, m_entries.size());
}

ValueMap::const_iterator ValueMap::begin() const
{
    return const_iterator(this, 0);
}

ValueMap::const_iterator ValueMap::end() const
{
    return const_iterator(this, m_entries.size());
}

void ValueMap::erase(const String& name)
{
    auto maybe_id = SymbolInterner::instance().lookup(name);
    if (!maybe_id.has_value())
    {
        return;
    }
    erase(*maybe_id);
}

void ValueMap::erase(SymbolId id)
{
    if (m_entries.empty() || id == modulisp::kInvalidSymbolId)
    {
        return;
    }
    const std::uint32_t hash = SymbolInterner::instance().hash(id);
    const std::size_t slot   = find_slot(id, hash, false);
    if (slot >= m_states.size() || m_states[slot] != SLOT_OCCUPIED ||
        m_entries[slot].first.id != id)
    {
        return;
    }

    m_states[slot]       = SLOT_TOMBSTONE;
    m_entries[slot].first = KeyView();
    m_entries[slot].second = Value();
    if (m_size > 0)
        --m_size;
}

void ValueMap::clear()
{
    for (auto& entry : m_entries)
    {
        entry.first  = KeyView();
        entry.second = Value();
    }
    std::fill(m_states.begin(), m_states.end(), SLOT_EMPTY);
    m_size = 0;
}

void ValueMap::ensure_capacity()
{
    constexpr std::size_t kInitialCapacity = 16;
    if (m_entries.empty())
    {
        m_entries.resize(kInitialCapacity);
        m_states.assign(kInitialCapacity, SLOT_EMPTY);
        m_size = 0;
        return;
    }
    if ((m_size + 1) * 10 >= m_entries.size() * 7)
    {
        rehash(m_entries.size() * 2);
    }
}

void ValueMap::rehash(std::size_t new_capacity)
{
    if (new_capacity < 16)
    {
        new_capacity = 16;
    }
    if ((new_capacity & (new_capacity - 1)) != 0)
    {
        std::size_t pow_two = 1;
        while (pow_two < new_capacity)
        {
            pow_two <<= 1U;
        }
        new_capacity = pow_two;
    }

    std::vector<Entry> old_entries;
    std::vector<std::uint8_t> old_states;
    old_entries.swap(m_entries);
    old_states.swap(m_states);

    m_entries.resize(new_capacity);
    m_states.assign(new_capacity, SLOT_EMPTY);
    m_size = 0;

    for (std::size_t i = 0; i < old_entries.size(); ++i)
    {
        if (old_states[i] == SLOT_OCCUPIED)
        {
            insert_existing(old_entries[i].first.id,
                            SymbolInterner::instance().hash(old_entries[i].first.id),
                            old_entries[i].second);
        }
    }
}

std::size_t ValueMap::find_slot(SymbolId id, std::uint32_t hash, bool for_insert) const
{
    if (m_entries.empty())
    {
        return 0;
    }
    const std::size_t mask_value = mask();
    std::size_t idx              = hash & mask_value;
    std::size_t first_tomb       = static_cast<std::size_t>(-1);

    while (true)
    {
        std::uint8_t state = m_states[idx];
        if (state == SLOT_EMPTY)
        {
            return (for_insert && first_tomb != static_cast<std::size_t>(-1)) ? first_tomb
                                                                              : idx;
        }
        if (state == SLOT_OCCUPIED && m_entries[idx].first.id == id)
        {
            return idx;
        }
        if (for_insert && state == SLOT_TOMBSTONE &&
            first_tomb == static_cast<std::size_t>(-1))
        {
            first_tomb = idx;
        }
        idx = (idx + 1) & mask_value;
    }
}

std::size_t ValueMap::find_slot(SymbolId id, std::uint32_t hash, bool for_insert)
{
    return const_cast<const ValueMap*>(this)->find_slot(id, hash, for_insert);
}

void ValueMap::insert_existing(SymbolId id, std::uint32_t hash, const Value& value)
{
    const std::size_t slot = find_slot(id, hash, true);
    m_states[slot]         = SLOT_OCCUPIED;
    m_entries[slot].first  = KeyView(id);
    m_entries[slot].second = value;
    ++m_size;
}

BuiltinMap& Environment::builtindefs()
{
    static BuiltinMap builtin_map;
    return builtin_map;
}

bool Environment::has(String const& name) const
{
    if (m_defs.has(name))
    {
        return true;
    }
    else if (m_parent_env != NULL)
    {
        return m_parent_env->has(name);
    }
    else
    {
        return false;
    }
}

// Get the value associated with this name in this scope
// NOTE: most functions will probably live in builtindefs,
// so always searching for it last may have a performance hit,
// but searching for it first means that we don't allow users
// to re-define built in symbols
std::optional<Value> Environment::get(const String& name) const
{
    DBG("Environment::get");
    dbg("Name: " + name);

    auto maybe_id = SymbolInterner::instance().lookup(name);
    if (!maybe_id.has_value())
    {
        report_error_atom_not_defined(name);
        return std::nullopt;
    }
    return get(*maybe_id);
}

std::optional<Value> Environment::get(SymbolId id) const
{
    if (id == modulisp::kInvalidSymbolId)
    {
        return std::nullopt;
    }

    std::optional<Value> result = m_defs.get(id);
    if (!result && m_parent_env != NULL)
    {
        result = m_parent_env->get(id);
    }
    if (!result)
    {
        result = Environment::builtindefs().get(id);
    }
    if (!result)
    {
        report_error_atom_not_defined(SymbolInterner::instance().to_string(id));
        return std::nullopt;
    }
    return *result;
}

std::optional<Value> Environment::get_expr(const String& name) const
{
    DBG("Environment::get_expr");
    dbg("Name: " + name);

    auto maybe_id = SymbolInterner::instance().lookup(name);
    if (!maybe_id.has_value())
    {
        return std::nullopt;
    }
    return get_expr(*maybe_id);
}

std::optional<Value> Environment::get_expr(SymbolId id) const
{
    if (id == modulisp::kInvalidSymbolId)
    {
        return std::nullopt;
    }

    std::optional<Value> result = m_def_exprs.get(id);
    if (!result && m_parent_env != NULL)
    {
        result = m_parent_env->get_expr(id);
    }
    return result;
}

void Environment::set(const String& name, Value value)
{
    SymbolId id = SymbolInterner::instance().intern(name);
    set(id, value);
}

void Environment::set(SymbolId id, Value value)
{
    if (id == modulisp::kInvalidSymbolId)
    {
        return;
    }
    m_defs[id] = value;
}

void Environment::set_expr(const String& name, Value value)
{
    SymbolId id = SymbolInterner::instance().intern(name);
    set_expr(id, value);
}

void Environment::set_expr(SymbolId id, Value value)
{
    if (id == modulisp::kInvalidSymbolId)
    {
        return;
    }
    m_def_exprs[id] = value;
}

void Environment::unset(const String& name)
{
    auto maybe_id = SymbolInterner::instance().lookup(name);
    if (maybe_id.has_value())
    {
        unset(*maybe_id);
    }
}

void Environment::unset(SymbolId id)
{
    if (id == modulisp::kInvalidSymbolId)
    {
        return;
    }
    m_defs.erase(id);
}

void Environment::unset_expr(const String& name)
{
    auto maybe_id = SymbolInterner::instance().lookup(name);
    if (maybe_id.has_value())
    {
        unset_expr(*maybe_id);
    }
}

void Environment::unset_expr(SymbolId id)
{
    if (id == modulisp::kInvalidSymbolId)
    {
        return;
    }
    m_def_exprs.erase(id);
}

void Environment::set_global(const String& name, Value value)
{
    SymbolId id = SymbolInterner::instance().intern(name);
    set_global(id, value);
}

void Environment::set_global(SymbolId id, Value value)
{
    set(id, value);
    if (m_parent_env)
    {
        m_parent_env->set_global(id, value);
    }
}

void Environment::set_global_expr(const String& name, Value value)
{
    SymbolId id = SymbolInterner::instance().intern(name);
    set_global_expr(id, value);
}

void Environment::set_global_expr(SymbolId id, Value value)
{
    set_expr(id, value);
    if (m_parent_env)
    {
        m_parent_env->set_global_expr(id, value);
    }
}

void Environment::combine(Environment const& other)
{
    // Normally, I would use the `insert` method of the `map` class,
    // but it doesn't overwrite previously declared values for keys.
    for (auto it = other.m_defs.begin(); it != other.m_defs.end(); ++it)
    {
        m_defs[it->first.id] = it->second;
    }

    for (auto it = other.m_def_exprs.begin(); it != other.m_def_exprs.end(); ++it)
    {
        m_def_exprs[it->first.id] = it->second;
    }
}

// TODO flesh out serialization
String Environment::toString()
{
    auto itr  = m_defs.begin();
    String os = "{ ";
    for (; itr != m_defs.end(); itr++)
    {

        os += '\'';
        os += itr->first;
        os += "' : ";
        os += itr->second.to_lisp_src();
        os += ", ";
    }
    os += "}";
    return os;
}
