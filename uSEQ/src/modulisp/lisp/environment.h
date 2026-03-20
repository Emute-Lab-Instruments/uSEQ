#ifndef ENVIRONMENT_H_
#define ENVIRONMENT_H_

#include "../../utils/string.h"
#include "value.h"
#include <map>
#include <optional>

struct TemporalContext;

#ifdef ARDUINO
#define FAST_MEM_ENV __not_in_flash("ENVIRONMENTDATA")
#else
#define FAST_MEM_ENV // Empty for desktop builds
#endif
// class Value;

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
    String toString();

    void set_parent_scope(Environment* parent) { m_parent_env = parent; }

    void set_temporal_context(TemporalContext* ctx) { m_temporal_ctx = ctx; }
    TemporalContext* get_temporal_context() const { return m_temporal_ctx; }

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
    TemporalContext* m_temporal_ctx = nullptr;
    // Environment* m_child_env;
};

#endif // ENVIRONMENT_H_
