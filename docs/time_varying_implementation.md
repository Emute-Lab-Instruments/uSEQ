# Time-Varying Expression Implementation

## Overview

This document describes the implementation of automatic time-varying expression detection and evaluation in the uSEQ ModuLisp interpreter.

## Problem Statement

When defining symbols in terms of time variables:

```lisp
(def phase (+ t 0.5))
```

The environment stores both:
1. **Cached value** (`m_defs`) - evaluated at definition time
2. **Expression** (`m_def_exprs`) - unevaluated `(+ t 0.5)`

Without proper handling, retrieving `phase` later would return the stale cached value instead of re-evaluating with the current time.

## Solution Architecture

The solution implements automatic detection and handling of time-varying expressions through:

1. **Time-varying flag** on Value objects
2. **Recursive dependency analysis** to detect time variables
3. **Smart evaluation** that uses cached values or re-evaluates as needed
4. **Transparent API** that requires no changes to user code

### Core Time Variables

The following variables are considered time-varying:
- `t` - Normalized time phasor [0, 1]
- `time` - Absolute time in seconds
- `beat` - Beat phasor (derived from t)
- `bar` - Bar phasor (derived from t)
- `phrase` - Phrase phasor (derived from t)
- `section` - Section phasor (derived from t)

## Implementation Details

### 1. Value Class Flag

**File:** `/root/repo/uSEQ/src/modulisp/lisp/value.h`

Added `is_time_varying` boolean flag to the `Value` class:

```cpp
class Value {
    // ... existing fields ...

    // Time-varying flag: true if this value depends on time variables
    // Used to optimize evaluation - time-varying expressions must be re-evaluated
    // each time, while static expressions can use cached values.
    bool is_time_varying = false;
};
```

### 2. Time-Dependency Analysis

**File:** `/root/repo/uSEQ/src/modulisp/modulisp_interpreter_core.cpp`

Implemented recursive analysis function:

```cpp
bool ModuLispInterpreter::is_expression_time_varying(
    const Value& expr,
    Environment& env,
    std::set<String>& visited)
{
    switch (expr.type) {
    case Value::ATOM:
        // Check if it's a core time variable
        if (symbol_name == "t" || symbol_name == "time" || ...)
            return true;

        // Check if defined in terms of time-varying expressions
        // (recursive check with cycle detection)
        ...

    case Value::LIST:
        // Check all list elements
        for (const auto& elem : expr.list)
            if (is_expression_time_varying(elem, env, visited))
                return true;
        ...
    }
}
```

**Features:**
- Detects direct time variable references
- Recursively checks symbol definitions
- Prevents infinite recursion with visited set
- Handles lists, lambdas, quotes appropriately

### 3. Smart Evaluation at Time

**File:** `/root/repo/uSEQ/src/modulisp/modulisp_interpreter_core.cpp`

Implemented `eval_at_time` method:

```cpp
Value ModuLispInterpreter::eval_at_time(const Value& v, TimeValue time_seconds)
{
    // Update time variables to specified time
    update_logical_time(time_seconds);
    update_lisp_time_variables();

    // Check if expression is time-varying
    if (v.is_time_varying || is_expression_time_varying(v, env)) {
        // Re-evaluate with current time
        result = eval_in(v, *m_environment);
    } else {
        // Use cached value for efficiency
        result = m_environment->get(symbol_name);
    }

    // Restore previous time
    update_logical_time(old_time);
    update_lisp_time_variables();

    return result;
}
```

**Features:**
- Sets time context before evaluation
- Checks time-varying flag (cached analysis)
- Falls back to analysis if not yet checked
- Uses cached values for static expressions
- Restores time context after evaluation

### 4. Definition Analysis

**File:** `/root/repo/uSEQ/src/modulisp/lisp/builtins.cpp`

Updated `def` builtin to analyze and mark expressions:

```cpp
Value def(std::vector<Value>& args, Environment& env)
{
    // ... argument validation ...

    Value body = args[1];  // Unevaluated expression

    // Analyze if expression is time-varying
    bool is_varying = ModuLispInterpreter::is_expression_time_varying(body, env);
    body.is_time_varying = is_varying;

    // Store both expression and evaluated value
    env.set_expr(name, body);
    Value evaluated = body.eval(env);
    evaluated.is_time_varying = is_varying;  // Propagate flag
    env.set(name, evaluated);

    return Value::atom(name);
}
```

**Benefits:**
- Analysis happens once at definition time
- Flag is cached on both expression and value
- Subsequent lookups can skip analysis

### 5. uSEQ Integration

**File:** `/root/repo/uSEQ/src/uSEQ.h`

Added convenience method:

```cpp
/**
 * Evaluate an expression at the current top-level time.
 *
 * This is the primary method uSEQ should use for evaluating output expressions.
 */
Value eval_at_current_time(const Value& v)
{
    TimeValue current_time = m_interpreter.get_transport_time();
    return m_interpreter.eval_at_time(v, current_time);
}
```

**File:** `/root/repo/uSEQ/src/uSEQ_update.cpp`

Updated output evaluation:

```cpp
void uSEQ::update_continuous_signals()
{
    for (int i = 0; i < m_num_continuous_outs; i++) {
        Value expr = m_continuous_ASTs[i];
        if (!expr.is_nil()) {
            // Uses eval_at_current_time instead of eval
            Value result = eval_at_current_time(expr);
            m_continuous_vals[i] = result.as_float();
        }
    }
}
```

Similarly updated for `update_binary_signals()` and `update_serial_signals()`.

## Performance Characteristics

### Time Complexity

**Analysis (one-time per definition):**
- O(N) where N is the number of nodes in expression tree
- Cached result prevents repeated analysis

**Evaluation:**
- **Time-varying:** O(E) where E is evaluation cost
- **Static:** O(1) cached lookup

### Space Complexity

- 1 bit per Value for `is_time_varying` flag
- O(D) recursion depth for analysis (with cycle detection)

## Usage Examples

### Example 1: Static Expression

```lisp
(def x 5)
(def y (* x 2))  ; Not time-varying

; First access: analyzes and caches that y is static
(print y)  ; Uses cached value: 10

; Subsequent accesses: instant cached lookup
(print y)  ; Still 10, no re-evaluation needed
```

### Example 2: Time-Varying Expression

```lisp
(def phase (+ t 0.5))  ; Time-varying

; Analysis at definition detects 't' dependency
; phase.is_time_varying = true

; Each access re-evaluates with current time
(print phase)  ; Evaluates to current_t + 0.5
```

### Example 3: Indirect Time Dependency

```lisp
(def offset 0.25)     ; Static
(def base (+ t offset))  ; Time-varying (uses t)
(def modulated (* base 2))  ; Time-varying (uses base, which uses t)

; Recursive analysis detects that modulated depends on t through base
; All three symbols correctly marked:
;   offset.is_time_varying = false
;   base.is_time_varying = true
;   modulated.is_time_varying = true
```

### Example 4: Lambda with Time Variables

```lisp
(defn wave (freq)
  (sin (* t freq)))  ; Lambda body uses t

; Analysis detects t in lambda body
; wave.is_time_varying = true

; Each call re-evaluates with current t
(wave 2.0)  ; Returns sin(current_t * 2.0)
```

## Comparison with Previous Approach

### Old Approach (ReevaluateScope)

```cpp
void uSEQ::update_signals() {
    ReevaluateScope scope;  // Global flag: re-evaluate ALL symbols

    update_continuous_signals();  // Everything re-evaluated
    update_binary_signals();      // Even static expressions!
    update_serial_signals();
}
```

**Problems:**
- Re-evaluates all expressions, even static ones
- Global state affects all evaluations
- No way to optimize static expressions
- Less clear semantics

### New Approach (Automatic Detection)

```cpp
void uSEQ::update_signals() {
    // No special setup needed

    update_continuous_signals();  // Smart evaluation per expression
    update_binary_signals();
    update_serial_signals();
}

// In update functions:
Value result = eval_at_current_time(expr);  // Automatically optimized
```

**Benefits:**
- ✅ Automatic: No manual flag management
- ✅ Efficient: Static expressions use cached values
- ✅ Correct: Time-varying expressions always fresh
- ✅ Clear: Intent is explicit in code
- ✅ Local: No global state side effects

## Testing

### Manual Tests

```bash
cd /root/repo/build
./standalone
```

```lisp
; Test 1: Static expression (should cache)
> (def x 5)
x
> (def y (* x 2))
y
> y
10

; Test 2: Time-varying expression (should re-evaluate)
> (def phase (+ t 0.1))
phase
> phase
; Returns current_t + 0.1 (changes each time)

; Test 3: Indirect dependency
> (def a t)
a
> (def b (+ a 1))
b
> b
; Returns current_t + 1 (time-varying through a)
```

### Automated Tests

The existing test suite verifies:
- Basic evaluation still works
- def/defn work correctly
- Expression parsing unchanged
- Environment operations correct

## Future Enhancements

### 1. Fine-Grained Time Dependency Tracking

Instead of boolean flag, track specific time variables:

```cpp
struct Value {
    std::set<String> time_dependencies;  // {"t", "beat"}
};
```

Benefits:
- Could optimize based on which time variables changed
- More precise caching strategies

### 2. Lazy Evaluation

Defer all evaluation until access:

```cpp
struct Value {
    enum { EVALUATED, THUNK } state;
    Value force();  // Evaluate if needed
};
```

### 3. Incremental Re-evaluation

Track dependency graph and only re-evaluate changed subtrees.

## Backward Compatibility

All changes are **fully backward compatible**:

- ✅ Existing code works unchanged
- ✅ Old `eval()` method still available
- ✅ ReevaluateScope still works (but not needed)
- ✅ No API breaking changes
- ✅ Default flag value (false) is safe

## Files Modified

1. `/root/repo/uSEQ/src/modulisp/lisp/value.h`
   - Added `is_time_varying` flag

2. `/root/repo/uSEQ/src/modulisp/modulisp_interpreter.h`
   - Added `eval_at_time()` method
   - Added `is_expression_time_varying()` method

3. `/root/repo/uSEQ/src/modulisp/modulisp_interpreter_core.cpp`
   - Implemented time-varying analysis
   - Implemented eval_at_time logic

4. `/root/repo/uSEQ/src/modulisp/lisp/builtins.cpp`
   - Updated `def` to analyze and mark expressions

5. `/root/repo/uSEQ/src/uSEQ.h`
   - Added `eval_at_current_time()` convenience method

6. `/root/repo/uSEQ/src/uSEQ_update.cpp`
   - Updated all eval() calls to eval_at_current_time()
   - Simplified update_signals() (removed ReevaluateScope)

## Conclusion

This implementation provides an elegant, efficient solution to time-varying expression evaluation:

- **Automatic**: No manual tracking required
- **Efficient**: Caches analysis and static values
- **Correct**: Always uses current time for time-varying expressions
- **Clean**: No global state, clear semantics
- **Compatible**: Works with existing code

The solution follows functional programming principles (immutability, referential transparency for static expressions) while providing the dynamic behavior needed for time-based musical sequencing.
