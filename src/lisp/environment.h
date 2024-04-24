#ifndef ENVIRONMENT_H_
#define ENVIRONMENT_H_

#include "lisp/value.h"
#include "utils/string.h"
#include <map>
#include <optional>
// class Value;

#if defined(USE_ETL)
using StringValueMap = etl::unordered_map<String, Value, 256>;
#else
using StringValueMap = std::map<String, Value>;
#endif

struct DefsMap : StringValueMap
{
    std::optional<Value> get(const String& name) const;
    bool has(const String& name) const;
};

using BuiltinMap = DefsMap;

// NOTE: global
extern BuiltinMap builtindefs;

// An instance of a function's scope.
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
    Value get(const String& name) const;
    // Set the value associated with this name in this scope
    void set(String name, Value value);
    void set_global(String name, Value value);

    void combine(Environment const& other);
    String toString(Environment const& e);

    void set_parent_scope(Environment* parent) { m_parent_env = parent; }

    // Output this scope in readable form to a stream.
    friend std::ostream& operator<<(std::ostream& os, Environment const& v);

private:
    // The definitions in the scope.
    DefsMap m_defs;
    Environment* m_parent_env;
    // Environment* m_child_env;
};

#endif // ENVIRONMENT_H_
