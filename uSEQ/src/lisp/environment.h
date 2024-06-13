#ifndef ENVIRONMENT_H_
#define ENVIRONMENT_H_

#include "../utils/string.h"
#include "value.h"
#include <map>
#include <optional>
// class Value;

#ifndef NO_ETL
//
#define ETL_NO_STL
#include <Embedded_Template_Library.h> // Mandatory for Arduino IDE only
#include <etl/map.h>
#include <etl/string.h>
#include <etl/unordered_map.h>

namespace etl
{

template <>
struct hash<String>
{
    std::size_t operator()(const String& k) const
    {
        using etl::hash;
        etl::string<3> firstThree(k.substring(0, 3).c_str());
        return hash<etl::string<3>>()(firstThree);
    }
};

} // namespace etl
#endif

// template <size_T MAX_SIZE>
struct ValueMap : std::map<String, Value>
{
    std::optional<Value> get(const String& name) const;
    bool has(const String& name) const;
};

// using BuiltinMap = ValueMap<256>;
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

    Environment(const Environment& v) : m_defs(v.m_defs)
    {
        // init();
    }
    Environment(Environment&& v) : m_defs(std::move(v.m_defs))
    {
        // init();
    }
    Environment& operator=(const Environment& env2)
    {
        this->m_defs = std::move(env2.m_defs);
        return *this;
    }

    // Does this environment, or its parent environment,
    // have this atom in scope?
    // This is only used to determine which atoms to capture when
    // creating a lambda function.
    bool has(String const& name) const;
    // Get the value associated with this name in this scope
    std::optional<Value> get(const String& name) const;
    std::optional<Value> get_expr(const String& name) const;
    // Set the value associated with this name in this scope
    void set(const String& name, Value value);
    void set_expr(const String& name, Value value);

    void unset(const String& name);
    void unset_expr(const String& name);
    // void set(const String& name, Value value);
    void set_global(const String name, Value value);
    void set_global_expr(const String name, Value value);

    void combine(Environment const& other);
    String toString(Environment const& e);

    void set_parent_scope(Environment* parent) { m_parent_env = parent; }

    // Output this scope in readable form to a stream.
    friend std::ostream& operator<<(std::ostream& os, Environment const& v);

    static BuiltinMap builtindefs;

protected:
    ValueMap m_defs;
    ValueMap m_def_exprs;

    // The definitions in the scope.
    Environment* m_parent_env;
    // Environment* m_child_env;
};

#endif // ENVIRONMENT_H_
