// Explicit template instantiations for commonly used templates
// This file reduces compile-time template processing by pre-instantiating
// frequently used template specializations

#include "modulisp/lisp/value.h"
#include "modulisp/lisp/environment.h"
#include <vector>
#include <optional>
#include <memory>
#include <functional>

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