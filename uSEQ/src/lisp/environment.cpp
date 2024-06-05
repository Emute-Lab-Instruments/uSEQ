#include "environment.h"
#include "../utils.h"
#include "../utils/log.h"
#include "value.h"
#include <optional>
// std::ostream &operator<<(std::ostream &os, Environment const &e) {
//   auto itr = e.defs.begin();
//   os << "{ ";
//   for (; itr != e.defs.end(); itr++) {
//     os << '\'' << itr->first << "' : " << itr->second.debug() << ", ";
//   }
//   return os << "}";
// }

BuiltinMap Environment::builtindefs;

std::optional<Value> ValueMap::get(const String& name) const
{
    std::optional<Value> result;

    auto it = this->find(name);
    if (it != this->end())
    {
        result = it->second;
    }
    return result;
}

bool ValueMap::has(const String& name) const { return (bool)this->get(name); }

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

    // debug("get: " + name);
    std::optional<Value> result;
    // 1. Look in regular defs
    // debug("searching defs...");
    result = m_defs.get(name);
    // 2. If not there, check if there's a parent env
    if (!result)
    {
        if (m_parent_env != NULL)
        {
            // debug("searching parent env...");
            result = m_parent_env->get(name);
        }
    }
    // 3. If not there either, check builtindefs
    if (!result)
    {
        // debug("searching builtindefs...");
        result = Environment::builtindefs.get(name);
    }
    // 4. If still not found, return empty
    if (!result)
    {
        error_atom_not_defined(name);
        return std::nullopt;
    }
    else
    {
        return *result;
    }
}

std::optional<Value> Environment::get_expr(const String& name) const
{
    DBG("Environment::get_expr");
    dbg("Name: " + name);

    std::optional<Value> result;
    // 1. Look in regular defs
    result = m_def_exprs.get(name);
    // 2. If not there, check if there's a parent env
    if (!result)
    {
        if (m_parent_env != NULL)
        {
            // debug("searching parent env...");
            result = m_parent_env->get_expr(name);
        }
    }
    // 3. If still not found, return error
    if (!result)
    {
        // NOTE don't print error here as this may be used to check
        // whether we have a signal expr for a var or whether
        // we should default to its cached value
        return std::nullopt;
    }
    else
    {
        return *result;
    }
}

void Environment::set(const String& name, Value value)
{
    // debug("Environment::set setting... (" + name + ")");
    // for multicore
    //  mutex_enter_blocking(&write_mutex);
    m_defs[name] = value;
    // debug("Environment::set setting...DONE");
    // mutex_exit(&write_mutex);
}

void Environment::set_expr(const String& name, Value value)
{
    m_def_exprs[name] = value;
}

void Environment::set_global(String name, Value value)
{
    set(name, value);
    if (m_parent_env)
    {
        m_parent_env->set_global(name, value);
    }
}

void Environment::set_global_expr(String name, Value value)
{
    set_expr(name, value);
    if (m_parent_env)
    {
        m_parent_env->set_global_expr(name, value);
    }
}

void Environment::combine(Environment const& other)
{
    // Normally, I would use the `insert` method of the `map` class,
    // but it doesn't overwrite previously declared values for keys.
    auto itr = other.m_defs.begin();
    for (; itr != other.m_defs.end(); itr++)
    {
        // Iterate through the keys and assign each value.
        m_defs[itr->first] = itr->second;
    }

    // NOTE
    auto itr2 = other.m_def_exprs.begin();
    for (; itr2 != other.m_def_exprs.end(); itr2++)
    {
        // Iterate through the keys and assign each value.
        m_def_exprs[itr2->first] = itr2->second;
    }
}

// TODO flesh out serialization
String Environment::toString(Environment const& e)
{
    auto itr  = e.m_defs.begin();
    String os = "{ ";
    for (; itr != e.m_defs.end(); itr++)
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
