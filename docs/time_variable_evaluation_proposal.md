# Time Variable Evaluation - Elegant Solution Proposal

## Current Problem

When you define a symbol that depends on the time variable `t`:

```lisp
(def foo (+ t 1))
```

The environment stores:
1. `m_defs["foo"]` = evaluated value at definition time (e.g., `1.1` if `t=0.1`)
2. `m_def_exprs["foo"]` = the unevaluated expression `(+ t 1)`

Later, when you retrieve `foo`, you want the **current** time value, not the cached value from definition time.

## Current Solution (with `m_attempt_expr_eval_first`)

The current approach uses a global flag to control evaluation order:

```cpp
if (m_attempt_expr_eval_first) {
    // Try to re-evaluate the expression first
    if (auto evaluated = try_eval_expr_binding(symbol_name)) {
        result = *evaluated;
        break;
    }
}

// Otherwise, return the cached value from m_defs
auto tmp = env.get(symbol_name);
if (tmp.has_value()) {
    result = tmp.value();
    break;
}
```

**Problems:**
1. Global state affects all symbol lookups
2. Re-evaluates even expressions that don't depend on time
3. Not semantically clear from the code what's happening

## Proposed Elegant Solutions

### Option 1: Mark Time-Dependent Expressions (Recommended)

Add a flag to track whether an expression depends on time variables:

```cpp
struct DefBinding {
    Value value;           // Cached evaluated value
    Value expr;            // Original expression
    bool is_time_dependent; // Whether expr references t, beat, bar, etc.
};
```

When defining a variable, analyze the expression for time dependencies:

```cpp
bool is_expression_time_dependent(const Value& expr) {
    if (expr.is_atom()) {
        String name = expr.str;
        // Check if it's a time variable
        return name == "t" || name == "beat" || name == "bar" ||
               name == "phrase" || name == "section";
    }
    if (expr.is_list()) {
        // Recursively check list elements
        for (const auto& item : expr.list) {
            if (is_expression_time_dependent(item)) {
                return true;
            }
        }
    }
    return false;
}
```

During symbol lookup:

```cpp
case Value::ATOM: {
    const String& symbol_name = v.str;

    // Get binding info
    auto binding = env.get_binding(symbol_name);
    if (binding) {
        // If time-dependent, re-evaluate; otherwise use cached value
        if (binding->is_time_dependent) {
            return eval_in(binding->expr, env);
        } else {
            return binding->value;
        }
    }

    // Not found...
    report_error(...);
}
```

**Advantages:**
- ✅ Clear semantic intent
- ✅ Only re-evaluates when necessary
- ✅ No global state
- ✅ Efficient (analyze once at definition time)

**Implementation notes:**
- Add `is_time_dependent` field to environment storage
- Update `def` special form to analyze expressions
- Update symbol lookup to check the flag

### Option 2: Lazy Evaluation with Thunks

Store expressions as "thunks" (unevaluated expressions) and evaluate on demand:

```cpp
struct DefBinding {
    enum Type { VALUE, THUNK } type;
    Value value;  // Either final value or unevaluated expression

    Value force_eval(Environment& env) {
        if (type == THUNK) {
            return eval_in(value, env);
        }
        return value;
    }
};
```

Mark time-dependent expressions as `THUNK`, others as `VALUE`:

```cpp
// During (def foo (+ t 1))
if (is_expression_time_dependent(expr)) {
    env.set_binding("foo", DefBinding{THUNK, expr});
} else {
    Value evaluated = eval(expr, env);
    env.set_binding("foo", DefBinding{VALUE, evaluated});
}
```

**Advantages:**
- ✅ Lazy evaluation pattern (functional programming)
- ✅ Clear distinction between values and computations
- ✅ No global flags

**Disadvantages:**
- ⚠️ More complex environment storage
- ⚠️ May require larger refactor

### Option 3: Separate Namespaces for Time-Dependent Symbols

Create a special namespace for time-dependent bindings:

```cpp
class Environment {
    ValueMap m_defs;           // Static values
    ValueMap m_def_exprs;      // All expressions
    ValueMap m_time_dependent; // Time-dependent expressions only
};
```

During lookup:

```cpp
// First check if it's time-dependent
if (env.has_time_dependent(symbol_name)) {
    Value expr = env.get_time_dependent_expr(symbol_name);
    return eval_in(expr, env);
}

// Otherwise, return static value
return env.get(symbol_name);
```

**Advantages:**
- ✅ Clear separation of concerns
- ✅ Fast lookup (separate maps)

**Disadvantages:**
- ⚠️ Duplicates some storage
- ⚠️ Need to keep maps in sync

### Option 4: Store Dependency Set Instead of Boolean

Instead of a boolean flag, store which time variables an expression depends on:

```cpp
struct DefBinding {
    Value value;
    Value expr;
    std::set<String> time_dependencies; // {"t", "beat"}
};
```

This allows more sophisticated caching:

```cpp
// Only re-evaluate if the relevant time variables have changed
if (has_any_changed(binding->time_dependencies)) {
    return eval_in(binding->expr, env);
} else {
    return binding->value; // Use cache
}
```

**Advantages:**
- ✅ Most efficient (fine-grained caching)
- ✅ Can track multiple time scales

**Disadvantages:**
- ⚠️ More complex implementation
- ⚠️ Need to track time variable changes

## Recommendation: Option 1 (Mark Time-Dependent Expressions)

This is the best balance of simplicity and correctness:

1. **Clear semantics**: The flag makes intent explicit
2. **Efficient**: Analyze once at definition, not every lookup
3. **No global state**: Each binding knows its own behavior
4. **Minimal refactor**: Can be added incrementally

### Implementation Steps

1. Add `is_time_dependent` to environment storage:
   ```cpp
   struct DefEntry {
       Value value;
       Value expr;
       bool is_time_dependent;
   };
   ```

2. Add analysis function to detect time dependencies:
   ```cpp
   bool depends_on_time_variables(const Value& expr);
   ```

3. Update `def` to analyze and store the flag:
   ```cpp
   bool is_time_dep = depends_on_time_variables(expr);
   env.set_with_metadata(name, evaluated, expr, is_time_dep);
   ```

4. Update symbol lookup to check flag:
   ```cpp
   auto binding = env.get_with_metadata(name);
   if (binding && binding->is_time_dependent) {
       return eval_in(binding->expr, env);
   }
   return binding->value;
   ```

5. Remove global `m_attempt_expr_eval_first` flag

## Alternative: Keep Current Approach but Clarify

If major refactoring is not desired right now, the current approach can be improved by:

1. **Rename the flag** to be more descriptive:
   ```cpp
   bool m_reevaluate_time_dependent_defs = false;
   ```

2. **Add documentation** explaining the pattern:
   ```cpp
   /**
    * When enabled, symbol lookup will re-evaluate the original expression
    * instead of returning the cached value. This is necessary for symbols
    * that depend on time variables (t, beat, bar, etc.) which change each
    * evaluation cycle.
    *
    * Enable this during update loops where time-dependent expressions need
    * current values. Disable during user interactions where stable values
    * are expected.
    */
   bool m_reevaluate_time_dependent_defs = false;
   ```

3. **Create RAII helper** to manage the flag:
   ```cpp
   class ReevaluateScope {
       bool prev_value;
   public:
       ReevaluateScope() {
           prev_value = ModuLispInterpreter::get_reevaluate_time_dependent_defs();
           ModuLispInterpreter::set_reevaluate_time_dependent_defs(true);
       }
       ~ReevaluateScope() {
           ModuLispInterpreter::set_reevaluate_time_dependent_defs(prev_value);
       }
   };

   // Usage:
   void uSEQ::update_continuous_signals() {
       ReevaluateScope scope;  // Auto-enable/disable
       // ... evaluation code
   }
   ```

This keeps the current logic but makes it clearer and more maintainable.
