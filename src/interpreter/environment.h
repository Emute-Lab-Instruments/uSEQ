#ifndef ENVIRONMENT_H_
#define ENVIRONMENT_H_

#include "string.h"
#include <map>

class Value;

// An instance of a function's scope.
class Environment {
public:
  // Default constructor
  Environment() : parent_scope(NULL) {
    // init();
  }

  Environment(const Environment &v) : defs(defs) {
    // init();
  }
  Environment(Environment &&v) : defs(std::move(v.defs)) {
    // init();
  }
  Environment &operator=(const Environment &env2) {
    this->defs = std::move(env2.defs);
    return *this;
  }

  // Does this environment, or its parent environment,
  // have this atom in scope?
  // This is only used to determine which atoms to capture when
  // creating a lambda function.
  bool has(String const &name) const;
  // Get the value associated with this name in this scope
  Value get(const String &name) const;
  // Set the value associated with this name in this scope
  void set(String name, Value value);
  void set_global(String name, Value value);

  void combine(Environment const &other);
  String toString(Environment const &e);

  void set_parent_scope(Environment *parent) { parent_scope = parent; }

  // Output this scope in readable form to a stream.
  // friend std::ostream &operator<<(std::ostream &os, Environment const &v);

  static std::map<String, Value> builtindefs;
  // static etl::unordered_map<String, Value, 256> builtindefs;

private:
  // The definitions in the scope.
  std::map<String, Value> defs;
  // etl::unordered_map<String, Value, 1024> defs; //note can't do this yet
  // because of forward declaration
  Environment *parent_scope;
  // mutex write_mutex;

  // void init() {
  // mutex_init(&write_mutex);
  // }
};
#endif // ENVIRONMENT_H_
