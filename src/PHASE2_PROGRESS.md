# Phase 2 Progress Report

## Completed Tasks

### ✅ 1. Serial Output Configuration Investigation

**Finding:** Serial output 0 (s0) is **RESERVED** across all hardware platforms.

- `s0` always outputs `time_since_boot` in seconds
- User code cannot reassign `s0`
- User-assignable serial outputs start at `s1` (index 1)
- This is consistent across all hardware variants (hardware_v1_0, MUSICTHING, etc.)

**Configuration:**
- `NUM_SERIAL_OUTS = 9` (configured in `uSEQ/src/uSEQ/configure.h`)
- `NUM_SERIAL_INS = 32`
- Outputs: s0 (time), s1-s8 (user-assignable)

### ✅ 2. Core Configuration Constants

**Created:** `src/core/useq_config.h`

This file consolidates all platform-independent configuration constants:

```cpp
namespace useq {
    // Reserved serial outputs
    constexpr size_t SERIAL_OUT_TIME_INDEX = 0;        // s0 = time
    constexpr size_t SERIAL_OUT_FIRST_USER = 1;        // s1 = first user output

    // Default values
    constexpr float DEFAULT_CONTINUOUS_VALUE = 0.5f;
    constexpr float DEFAULT_BINARY_VALUE = 0.0f;

    // Timing
    constexpr unsigned long SERIAL_MESSAGE_RATE_LIMIT_US = 100000;  // 100ms
    constexpr unsigned int UPDATE_LOOP_DELAY_US = 100;
}
```

**Benefits:**
- Single source of truth for constants
- Self-documenting (no magic numbers)
- Easy to adjust globally
- Type-safe (constexpr)

### ✅ 3. Updated Core to Use Constants

**Modified:** `src/core/useq_update.h`

All hardcoded values replaced with named constants:
- Serial output 0 handling uses `useq::SERIAL_OUT_TIME_INDEX`
- User serial outputs loop from `useq::SERIAL_OUT_FIRST_USER`
- Rate limiting uses `useq::SERIAL_MESSAGE_RATE_LIMIT_US`
- Delays use `useq::UPDATE_LOOP_DELAY_US`

**Impact:**
- More readable code
- Easier to understand reserved outputs
- Centralized configuration

### ✅ 4. ModuLisp API Function Categorization

Analyzed the 2,147-line `modulisp_api.cpp` and categorized all 46 functions:

#### Timing Functions (13 functions)
```
eval-at-time, useq-rewind, slow, fast, offset,
set-bpm, set-time-sig, schedule, unschedule,
loop-at, useq-set-time-offset, useq-nudge-time, shift
```

#### Phasor/Sequencing Functions (18 functions)
```
tri, dm, gates, gatesw, trigs, euclid, eu,
rpulse, rstep, ridx, rwarp, shift,
seq, flatseq, step
```

#### List Functions (4 functions)
```
from-list, from-flattened-list, flatten, interp
```

#### Random Functions (2 functions)
```
random, index-rand
```

#### WASM Output Assignment (16 functions)
```
a1-a8 (analog outputs)
d1-d8 (digital outputs)
s1-s8 (serial outputs)
```

## Planned API Split Structure

Based on the categorization, here's the proposed module structure:

```
src/api/
├── api_timing.h/cpp           # Timing and scheduling functions
│   ├── eval-at-time
│   ├── schedule/unschedule
│   ├── set-bpm, set-time-sig
│   ├── fast/slow/offset
│   ├── useq-set-time-offset, useq-nudge-time
│   └── useq-rewind
│
├── api_sequencing.h/cpp       # Phasor-based sequencing
│   ├── tri, dm
│   ├── gates, gatesw, trigs
│   ├── euclid, eu
│   ├── rpulse, rstep, ridx, rwarp
│   ├── shift
│   ├── seq, flatseq, step
│   └── loop-at
│
├── api_lists.h/cpp            # List manipulation
│   ├── from-list
│   ├── from-flattened-list, flatten
│   └── interp
│
├── api_random.h/cpp           # Random functions
│   ├── random
│   └── index-rand
│
├── api_outputs_wasm.h/cpp     # WASM output assignment
│   ├── a1-a8 (continuous outputs)
│   ├── d1-d8 (binary outputs)
│   └── s1-s8 (serial outputs)
│
└── api_registry.h/cpp         # Unified function registration
    └── register_all_api_functions()
```

## Size Breakdown

| Module | Estimated Lines | Functions |
|--------|----------------|-----------|
| api_timing.cpp | ~600 lines | 13 functions |
| api_sequencing.cpp | ~800 lines | 18 functions |
| api_lists.cpp | ~300 lines | 4 functions |
| api_random.cpp | ~100 lines | 2 functions |
| api_outputs_wasm.cpp | ~200 lines | 16 functions |
| api_registry.cpp | ~100 lines | Registration only |
| **Total** | **~2,100 lines** | **53 functions** |

**Result:** Each file < 800 lines (vs original 2,147 lines)

## Next Steps

### Phase 2A: API Splitting (Remaining Work)

1. **Create Module Structure**
   - Create `src/api/` directory
   - Create header/source pairs for each module
   - Define module-specific includes

2. **Extract Timing Functions** (~2-3 hours)
   - Move 13 timing functions to `api_timing.cpp`
   - Update includes and dependencies
   - Test compilation

3. **Extract Sequencing Functions** (~3-4 hours)
   - Move 18 sequencing functions to `api_sequencing.cpp`
   - Handle shared dependencies
   - Test compilation

4. **Extract List Functions** (~1 hour)
   - Move 4 list functions to `api_lists.cpp`
   - Test compilation

5. **Extract Random Functions** (~30 min)
   - Move 2 random functions to `api_random.cpp`
   - Test compilation

6. **Extract WASM Outputs** (~1 hour)
   - Move 16 output functions to `api_outputs_wasm.cpp`
   - Conditional compilation for WASM only

7. **Create Unified Registry** (~2 hours)
   - Create `api_registry.cpp`
   - Consolidate all `INSERT_BUILTINDEF` calls
   - Replace scattered registration with single call

**Estimated Total Time:** 10-13 hours

### Phase 2B: Platform Conversion (After API Split)

1. **Complete Hardware v1.0 Config**
   - Implement all I/O functions
   - Add tempo detection support
   - Add I2C support

2. **Create Arduino Sketch**
   - Use new core architecture
   - Test on real hardware

3. **Verify Against Legacy**
   - Run same code on old and new implementations
   - Ensure identical behavior

## Benefits of API Split

### 1. Maintainability
- **Before:** 2,147-line file, hard to navigate
- **After:** 6 focused files, each < 800 lines

### 2. Build Performance
- **Before:** Change one function → recompile all 2,147 lines
- **After:** Change one function → recompile only its module (~300-800 lines)

### 3. Organization
- **Before:** Functions scattered alphabetically
- **After:** Functions grouped by purpose (timing, sequencing, lists, etc.)

### 4. Onboarding
- **Before:** New developers see one massive file
- **After:** New developers can focus on one domain at a time

### 5. Testing
- **Before:** Hard to test individual API domains
- **After:** Can unit test each module independently

## Example: How the Split Works

### Before (modulisp_api.cpp):
```cpp
void ModuLispInterpreter::init_builtinfuncs() {
    INSERT_BUILTINDEF("set-bpm", useq_setbpm);
    INSERT_BUILTINDEF("tri", useq_tri);
    INSERT_BUILTINDEF("from-list", useq_fromList);
    INSERT_BUILTINDEF("random", useq_random);
    // ... 42 more functions ...
}

Value ModuLispInterpreter::useq_setbpm(...) { /* 50 lines */ }
Value ModuLispInterpreter::useq_tri(...) { /* 80 lines */ }
Value ModuLispInterpreter::useq_fromList(...) { /* 60 lines */ }
// ... 2000 more lines ...
```

### After (Split):

**api_timing.cpp:**
```cpp
#include "api_timing.h"

Value ModuLispInterpreter::useq_setbpm(...) { /* 50 lines */ }
Value ModuLispInterpreter::useq_fast(...) { /* 40 lines */ }
// ... only timing functions ...
```

**api_sequencing.cpp:**
```cpp
#include "api_sequencing.h"

Value ModuLispInterpreter::useq_tri(...) { /* 80 lines */ }
Value ModuLispInterpreter::useq_dm(...) { /* 100 lines */ }
// ... only sequencing functions ...
```

**api_lists.cpp:**
```cpp
#include "api_lists.h"

Value ModuLispInterpreter::useq_fromList(...) { /* 60 lines */ }
Value ModuLispInterpreter::useq_flatten(...) { /* 70 lines */ }
// ... only list functions ...
```

**api_registry.cpp:**
```cpp
#include "api_timing.h"
#include "api_sequencing.h"
#include "api_lists.h"
#include "api_random.h"

void ModuLispInterpreter::init_builtinfuncs() {
    // Timing
    INSERT_BUILTINDEF("set-bpm", useq_setbpm);
    INSERT_BUILTINDEF("fast", useq_fast);
    // ...

    // Sequencing
    INSERT_BUILTINDEF("tri", useq_tri);
    INSERT_BUILTINDEF("dm", useq_dm);
    // ...

    // Lists
    INSERT_BUILTINDEF("from-list", useq_fromList);
    // ...

    // Random
    INSERT_BUILTINDEF("random", useq_random);
    // ...
}
```

## Reserved Outputs Documentation

### Serial Output Allocation

| Index | Name | Type | Usage |
|-------|------|------|-------|
| 0 | s0 | **RESERVED** | Time in seconds (auto-assigned) |
| 1-8 | s1-s8 | User | User-assignable expressions |

**Important Notes:**
- User code **cannot** assign to `s0`
- Attempting to set `s0` will have no effect (time always overrides)
- All platforms must respect this reservation
- New platforms should document their reserved outputs

### Platform-Specific Reserved Outputs

Each platform may have additional reserved outputs:

**MUSICTHING:**
- `a1` (audio left) - Reserved for DAC output
- `a2` (audio right) - Reserved for DAC output
- `a3`, `a4` - User-assignable CV
- `d1`, `d2` - User-assignable gates

**Hardware v1.0:**
- `a1`, `a2`, `a3` - User-assignable CV
- `d1`, `d2`, `d3` - User-assignable gates

**WASM:**
- All outputs user-assignable (no hardware restrictions)
- Dynamic number of outputs possible

## Recommendations

### For Immediate Next Steps:

1. **Complete API Split** (highest priority)
   - Improves maintainability immediately
   - Makes codebase easier to navigate
   - Reduces compilation times

2. **Document Reserved Outputs** (documentation)
   - Add to platform config headers
   - Update user documentation
   - Warn in error messages if user tries to override

3. **Convert Hardware v1.0** (proof of concept)
   - Validates new architecture with real hardware
   - Identifies any missing abstractions
   - Provides template for other platforms

### For Future Consideration:

1. **API Versioning**
   - Track which functions are in which API version
   - Allow deprecation of old functions
   - Smooth migration path for user code

2. **Function Documentation**
   - Generate API docs from code comments
   - Include examples for each function
   - Link to user manual

3. **Performance Profiling**
   - Measure impact of modular API
   - Identify hot paths
   - Optimize critical functions

## Summary

Phase 2 is well underway with important foundational work completed:

✅ **Serial output handling** - Properly documented and implemented
✅ **Core constants** - Centralized configuration
✅ **API categorization** - Clear structure for splitting
✅ **Split strategy** - Well-defined module boundaries

**Next:** Complete the API module split (10-13 hours of work) to transform the 2,147-line monolith into 6 focused, maintainable modules.

The architecture is solid, the plan is clear, and the benefits are significant!
