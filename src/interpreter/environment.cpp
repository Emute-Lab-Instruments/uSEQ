#include "environment.h"
#include "interpreter/value.h"

void Environment::combine(Environment const &other) {
  // Normally, I would use the `insert` method of the `map` class,
  // but it doesn't overwrite previously declared values for keys.
  auto itr = other.defs.begin();
  for (; itr != other.defs.end(); itr++) {
    // Iterate through the keys and assign each value.
    defs[itr->first] = itr->second;
  }
}

// std::ostream &operator<<(std::ostream &os, Environment const &e) {
//   auto itr = e.defs.begin();
//   os << "{ ";
//   for (; itr != e.defs.end(); itr++) {
//     os << '\'' << itr->first << "' : " << itr->second.debug() << ", ";
//   }
//   return os << "}";
// }

String Environment::toString(Environment const &e) {
  auto itr = e.defs.begin();
  String os = "{ ";
  for (; itr != e.defs.end(); itr++) {

    os += '\'';
    os += itr->first;
    os += "' : ";
    os += itr->second.debug();
    os += ", ";
  }
  os += "}";
  return os;
}

void Environment::set(String name, Value value) {
  // for multicore
  //  mutex_enter_blocking(&write_mutex);
  defs[name] = value;
  // mutex_exit(&write_mutex);
}

void Environment::set_global(String name, Value value) {
  set(name, value);
  if (parent_scope) {
    parent_scope->set_global(name, value);
  }
}
