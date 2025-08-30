# uSEQ API Validation Macro System Design

## Overview

This document describes the design and implementation of a new macro-based validation system for uSEQ API functions that addresses performance, maintainability, and automatic registration concerns.

## Problem Statement

The current uSEQ API function implementation has several issues:

1. **Repetitive validation code** - Each function manually validates arguments with boilerplate code
2. **Inconsistent error messages** - Different functions report errors differently
3. **Manual registration** - Functions must be manually registered in `init_builtinfuncs()`
4. **Performance concerns** - Validation code isn't optimized for the common case
5. **Maintenance burden** - Adding new functions requires updates in multiple places

## Solution: Advanced Validation Macros with Auto-Registration

### Core Design Principles

1. **Declarative validation** - Express validation rules clearly at function definition
2. **Zero-cost abstractions** - Macro expansion should produce optimal code
3. **Single source of truth** - Function name and registration in one place
4. **Compile-time safety** - Catch errors during compilation when possible

### Macro Architecture

#### 1. Function Definition Macro

```cpp
#define USEQ_API_FUNC(internal_name, user_name, args_spec, ...) \
    Value uSEQ::internal_name(std::vector<Value>& args, Environment& env) { \
        constexpr const char* user_facing_name = user_name; \
        VALIDATE_ARGS(args_spec); \
        __VA_ARGS__ \
    }
```

#### 2. Argument Specification DSL

The `args_spec` parameter uses a mini-DSL for expressing validation requirements:

```cpp
// Argument count specifications
COUNT(min, max)     // Variable argument count
EXACT(n)            // Exactly n arguments

// Type specifications  
TYPE(idx, pred)     // Argument at index must match predicate
OPT_TYPE(idx, pred) // Optional argument type check
ALL_TYPE(pred)      // All arguments must match predicate

// Predicates
NUMBER              // is_number()
SEQUENTIAL          // is_sequential()
SYMBOL              // is_symbol()
STRING              // is_string()
LIST                // is_list()
VECTOR              // is_vector()
```

#### 3. Example Usage

```cpp
USEQ_API_FUNC(useq_setbpm, "set-bpm", 
    ARGS(COUNT(1, 2), TYPE(0, NUMBER), OPT_TYPE(1, NUMBER)),
    {
        double newBpm = args[0].as_float();
        double thresh = args.size() == 2 ? args[1].as_float() : 0.0;
        set_bpm(newBpm, thresh);
        return args[0];
    })

USEQ_API_FUNC(useq_gates, "gates",
    ARGS(COUNT(2, 3), TYPE(0, SEQUENTIAL), TYPE(1, NUMBER), OPT_TYPE(2, NUMBER)),
    {
        // Function implementation
    })
```

### Auto-Registration System

#### X-Macro Approach (Chosen Solution)

We use X-macros to maintain a single list of all API functions:

```cpp
// Define all API functions in one place
#define USEQ_BUILTIN_LIST \
    X(useq_setbpm, "set-bpm") \
    X(useq_get_input_bpm, "get-input-bpm") \
    X(useq_schedule, "schedule") \
    X(useq_unschedule, "unschedule") \
    // ... all other functions

// Auto-generate registration function
inline void register_all_builtins() {
    #define X(func, name) \
        Environment::builtindefs[name] = Value((String)name, &uSEQ::func);
    USEQ_BUILTIN_LIST
    #undef X
}
```

This approach provides:
- **Single source of truth** - All functions listed in one place
- **Compile-time verification** - Missing functions cause compilation errors
- **Optimal performance** - Single initialization loop
- **Easy maintenance** - Add new functions by adding to the list

### Performance Optimizations

1. **Branch prediction hints**
   ```cpp
   if (__builtin_expect(args.size() < min || args.size() > max, 0)) {
       // Unlikely error path
   }
   ```

2. **Inline validation functions**
   ```cpp
   inline bool validate_arg_type(size_t idx, const Value& arg, uint32_t type_mask) {
       // Fast path validation
   }
   ```

3. **Compile-time validation tables**
   ```cpp
   constexpr ValidationSpec spec = parse_validation_spec(args_spec);
   ```

4. **Loop unrolling for small argument counts**
   - Manually unroll for functions with ≤ 3 arguments
   - Use template metaprogramming for larger counts

### Implementation Plan

1. **Phase 1: Create macro framework**
   - Define `USEQ_API_FUNC` macro
   - Implement `VALIDATE_ARGS` expansion
   - Create predicate definitions

2. **Phase 2: Implement validation logic**
   - Argument count checking
   - Type validation
   - Error reporting

3. **Phase 3: Setup auto-registration**
   - Create `USEQ_BUILTIN_LIST` X-macro
   - Modify `init_builtinfuncs()` to use registration function

4. **Phase 4: Convert existing functions**
   - Gradually convert functions to new system
   - Verify behavior matches original

5. **Phase 5: Performance optimization**
   - Add branch prediction hints
   - Implement inline validation
   - Benchmark improvements

### Benefits

1. **Reduced code duplication** - Validation logic in one place
2. **Improved maintainability** - Clear, declarative function definitions
3. **Better performance** - Optimized validation paths
4. **Automatic registration** - No manual registration needed
5. **Consistent error messages** - Standardized error reporting
6. **Type safety** - Compile-time verification of macro usage

### Migration Strategy

1. Keep existing functions working during transition
2. Convert functions incrementally
3. Run tests after each conversion
4. Remove old validation code once all functions converted
5. Update documentation

### Future Enhancements

1. **Custom validators** - Support for complex validation logic
2. **Performance profiling** - Built-in timing for validation overhead
3. **Debug mode** - Extra validation in debug builds
4. **Static analysis** - Tools to verify validation completeness