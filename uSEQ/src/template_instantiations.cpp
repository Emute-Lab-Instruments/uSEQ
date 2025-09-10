// Explicit template instantiations for commonly used templates
// This file reduces compile-time template processing by pre-instantiating
// frequently used template specializations

#include "modulisp/lisp/environment.h"
#include "modulisp/lisp/value.h"
#include "utils/string.h"
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

// Explicitly instantiate std::vector<Value>
// This is the most commonly used template in the codebase
template class std::vector<Value>;

// Explicitly instantiate std::optional<Value>
// Used frequently for return values that may not exist
template class std::optional<Value>;

// Explicitly instantiate std::shared_ptr<Environment>
// Used for environment management throughout the interpreter
template class std::shared_ptr<Environment>;

// Explicitly instantiate std::function with Value signatures
// Used for builtin function implementations
template class std::function<Value(std::vector<Value>&, Environment&)>;

// Explicitly instantiate allocator for Value vectors
// Helps with vector memory management
template class std::allocator<Value>;

// Explicitly instantiate std::set<String>
// Used for tracking atoms in lambda scope analysis
template class std::set<String>;

// Explicitly instantiate std::vector<double>
// Used extensively in signal processing and automation analysis
template class std::vector<double>;

// Explicitly instantiate std::pair<double, double>
// Used for range analysis in signal processing
template class std::pair<double, double>;

// Explicitly instantiate std::vector<std::pair<double, double>>
// Used for find_value_ranges and similar analysis functions
template class std::vector<std::pair<double, double>>;