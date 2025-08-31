# uSEQ Build System Optimization TODO

This document contains a priority-ordered list of atomic refactoring tasks to improve compilation times and build system efficiency. Each task is designed to leave the codebase in a working state while gradually implementing the optimization strategy.

**IMPORTANT**
When you successfully complete a task, before you commit, check the task off here in the list.

## Prerequisites & Cleanup (Priority: Critical)

### CLEANUP-001: Remove obsolete builtin generation script
- [x] Remove obsolete builtin generation script
**Files**: `scripts/generate_builtins.clj`, `scripts/builtins.edn`
**Description**: Remove the Clojure script and EDN configuration file that generated builtin functions, since we now use static builtin files.
**Status**: Files were already removed; updated documentation references in `scripts/README.md` ✓
**Acceptance**: Files deleted, no references to generation script in build files or documentation.
**Impact**: Eliminates confusion about build process, removes unused dependencies.

### CLEANUP-002: Rename generated_builtins files to builtins
- [x] Rename generated_builtins files to builtins
**Files**: 
- `uSEQ/src/modulisp/lisp/generated_builtins.h` → `builtins.h` ✓
- `uSEQ/src/modulisp/lisp/generated_builtins.cpp` → `builtins.cpp` ✓
**Description**: Rename files to reflect that they are now static (not generated). Update all #include statements and build system references.
**Files Updated**: `meson.build`, `interpreter.cpp`, `builtins.cpp`, test files, `test/meson.build`, `scripts/test.sh`, `scripts/build_wasm.sh` ✓
**Acceptance**: All compilation targets build successfully with new names.
**Status**: Files renamed and all references updated successfully.

## Phase 1: Header Dependency Analysis & Forward Declarations (Priority: High)

### HEADERS-001: Create forward declaration header for core types
- [x] Create forward declaration header for core types
**New File**: `uSEQ/src/core_fwd.h`
**Description**: Create a lightweight forward declaration header containing forward declarations for Value, Environment, Interpreter, ModuLispInterpreter, and uSEQ classes.
**Content**: Forward declarations only, no implementations, minimal includes
**Acceptance**: Header compiles cleanly, can be included without pulling in heavy dependencies.
**Estimated Impact**: Reduces header weight in files that only need pointers/references.

### HEADERS-002: Extract SignalMetadata from value.h
- [x] Extract SignalMetadata from value.h
**New Files**: 
- `uSEQ/src/modulisp/lisp/signal_metadata.h`
- `uSEQ/src/modulisp/lisp/signal_metadata.cpp`
**Description**: Move the large SignalMetadata struct (lines 44-114 in value.h) to separate files. Keep only forward declaration in value.h.
**Dependencies**: Include signal_metadata.h in value.cpp and any files using SignalMetadata directly.
**Acceptance**: value.h is significantly smaller, all signal metadata functionality works.
**Estimated Impact**: ~70 lines removed from frequently-included value.h.

### HEADERS-003: Extract signal processing methods from Value class
- [x] Extract signal processing methods from Value class
**New Files**:
- `uSEQ/src/modulisp/lisp/value_signal_processing.h`
- `uSEQ/src/modulisp/lisp/value_signal_processing.cpp`
**Description**: Move signal processing methods (lines 323-357 in value.h) to separate files. Use friend functions or PIMPL pattern.
**Target Methods**: All automation analysis methods, mathematical functions on signals
**Acceptance**: value.h is lighter, signal processing functionality preserved.
**Estimated Impact**: ~35 lines removed from value.h, cleaner separation of concerns.

### HEADERS-004: Split uSEQ.h into functional modules
- [x] Split uSEQ.h into functional modules
**New Files**:
- `uSEQ/src/uSEQ_io.h` (I/O related declarations)
- `uSEQ/src/uSEQ_dsp.h` (DSP and audio processing)
- `uSEQ/src/uSEQ_lisp.h` (LISP integration functions)  
- `uSEQ/src/uSEQ_hardware.h` (Hardware-specific functions)
**Description**: Split the 480-line uSEQ.h into focused headers. Main uSEQ.h includes these modules.
**Target Groupings**: Group LISP_FUNC_DECL declarations by functionality
**Acceptance**: uSEQ.h is under 150 lines, all functionality accessible, builds work.
**Estimated Impact**: Major reduction in compile times when editing specific subsystems.

## Phase 2: Build System Improvements (Priority: High)

### BUILD-001: Add precompiled header support to Meson
- [x] Add precompiled header support to Meson
**Files**: `meson.build`, new `uSEQ/src/pch.h`
**Description**: Create precompiled header containing stable system includes (STL, optional Arduino headers). Configure Meson to use PCH.
**PCH Content**: `<vector>`, `<memory>`, `<string>`, `<functional>`, `<optional>`, `<cmath>`, Arduino core headers
**Acceptance**: Builds use PCH, clean build times reduced by 20-30%.
**Estimated Impact**: Significant reduction in preprocessing time for stable headers.

### BUILD-002: Implement proper dependency tracking in Meson
- [x] Implement proper dependency tracking in Meson
**Files**: `meson.build`, `test/meson.build`
**Description**: Add explicit dependency declarations between compilation units. Use Meson dependency tracking to enable incremental builds.
**Implementation**: Created static libraries with proper dependency chains: utils → lisp_core → modulisp → hw
**Target**: Ensure only affected files recompile when headers change
**Acceptance**: Incremental builds work correctly, only changed dependencies rebuild. ✓
**Estimated Impact**: 70-80% reduction in incremental build times.
**Status**: Successfully implemented static libraries with dependency tracking. Tests show significant improvement in incremental build times.

### BUILD-003: Create static library for LISP interpreter core
- [x] Create static library for LISP interpreter core
**Files**: `meson.build`, new `uSEQ/src/modulisp/meson.build`
**Description**: Package the LISP interpreter (parser, value, environment, interpreter, builtins) as a separate static library.
**Library Name**: `libuseq_modulisp` (implemented as `useq_lisp_core` and `useq_modulisp`)
**Acceptance**: LISP core compiles as library, main targets link against it, incremental builds faster. ✓
**Estimated Impact**: LISP changes don't force recompilation of hardware/DSP code.
**Status**: Implemented as part of BUILD-002. Created `useq_lisp_core` and `useq_modulisp` static libraries with proper dependency chains.

### BUILD-004: Create static library for utilities
- [x] Create static library for utilities
**Files**: `meson.build`, new `uSEQ/src/utils/meson.build`  
**Description**: Package utility functions (string, log, common, flags, etc.) as static library.
**Library Name**: `libuseq_utils` (implemented as `useq_utils`)
**Acceptance**: Utils compile as library, reduced rebuilding when main code changes. ✓
**Estimated Impact**: Utility changes isolated from main codebase compilation.
**Status**: Implemented as part of BUILD-002. Created `useq_utils` static library as the base dependency layer.

### BUILD-005: Create static library for DSP engine
- [x] Create static library for DSP engine
**Files**: `meson.build`, new `uSEQ/src/dsp/meson.build`
**Description**: Package DSP components (tempo estimator, filters, DSP engine) as static library.
**Library Name**: `libuseq_dsp` (implemented as `useq_dsp`)
**Special Handling**: Arduino-specific code conditionally included
**Acceptance**: DSP compiles separately, hardware changes don't affect DSP builds. ✓
**Estimated Impact**: Better separation between real-time DSP and control logic.
**Status**: Implemented as part of BUILD-002. Created `useq_dsp` static library with utils dependency.

## Phase 3: Interface Improvements (Priority: Medium)

### INTERFACE-001: Create minimal testing interfaces
- [x] Create minimal testing interfaces
**New Files**:
- `uSEQ/src/interfaces/test_helpers.h` ✓
- `uSEQ/src/interfaces/test_helpers.cpp` ✓
**Description**: Create lightweight interfaces for testing that don't pull in full implementation headers.
**Target**: Reduce test compilation times by avoiding heavy headers
**Acceptance**: Tests build faster, test functionality unchanged. ✓
**Estimated Impact**: 50% reduction in test compilation times.
**Status**: Implemented QuickInterpreter wrapper and test utilities. Created test_with_helpers test demonstrating usage.

### INTERFACE-002: Implement PIMPL pattern for Value class
- [x] ~~Implement PIMPL pattern for Value class~~ - ATTEMPTED BUT NOT VIABLE
**Files**: Modify `uSEQ/src/modulisp/lisp/value.h`, `value.cpp`
**New File**: `uSEQ/src/modulisp/lisp/value_impl.h` (created but not used)
**Description**: Use PIMPL (Pointer to Implementation) for Value class to reduce header dependencies and compile times.
**Status**: Attempted implementation but found it too invasive for current codebase structure.
**Issues Found**: 
  - Value class has extensive direct member access throughout codebase
  - Would require updating hundreds of access points across multiple files
  - Performance overhead from pointer indirection in hot paths
  - Risk of introducing bugs in critical interpreter code
**Recommendation**: Consider alternative optimizations that don't require extensive refactoring
**Estimated Impact**: Implementation cost exceeds benefit for this codebase.

### INTERFACE-003: Extract port interfaces to separate module
- [x] Extract port interfaces to separate module
**New Directory**: `uSEQ/src/ports/`
**Files**: Move `IClock.h`, `ILogger.h`, `IIo.h`, etc. to ports directory
**Description**: Group all interface definitions in dedicated directory for cleaner organization.
**Status**: Already implemented. All interface files (`IClock.h`, `ILogger.h`, `IIo.h`, `II2CBus.h`, `IStorage.h`) are properly located in `uSEQ/src/ports/` with mocks in `ports/mocks/`. ✓
**Acceptance**: Interfaces accessible, dependency injection still works. ✓
**Estimated Impact**: Better code organization, cleaner include paths.

## Phase 4: Template and Compilation Optimizations (Priority: Medium)

### TEMPLATE-001: Add explicit template instantiations
- [x] Add explicit template instantiations
**New File**: `uSEQ/src/template_instantiations.cpp` ✓
**Description**: Explicitly instantiate commonly used templates (std::vector<Value>, etc.) to reduce compile-time template processing.
**Target Templates**: std::vector<Value>, std::shared_ptr<Environment>, std::optional<Value>, std::function<Value(...)> ✓
**Acceptance**: Compilation faster, template bloat reduced. ✓
**Estimated Impact**: 15-20% reduction in template processing time.
**Status**: Successfully created template instantiations file and integrated into Meson build. Tests pass.

### TEMPLATE-002: Optimize Value class template usage
- [x] Optimize Value class template usage  
**Files**: `uSEQ/src/template_instantiations.cpp`
**Description**: Review and optimize template usage in Value class. Consider explicit specializations for common types.
**Implementation**: Added explicit instantiations for std::set<String>, std::vector<double>, std::pair<double, double>, and std::vector<std::pair<double, double>>
**Target**: Reduce template instantiation overhead
**Acceptance**: Same functionality, faster compilation of Value-heavy code. ✓
**Estimated Impact**: Reduction in template-heavy compilation units.
**Status**: Successfully added 4 additional template instantiations for commonly used types in Value class and signal processing.

## Phase 5: Advanced Build Optimizations (Priority: Low)

### ADVANCED-001: Add Unity build option
- [x] Add Unity build option
**Files**: `meson.build`, `meson_options.txt`
**Description**: Add Meson option for unity builds (combining multiple .cpp files). Useful for release builds.
**Option Name**: `unity_build=true`
**Implementation**: Added `unity_build` boolean option to `meson_options.txt` and configured all static libraries and executables to use `override_options` with `'unity=on'` when enabled.
**Acceptance**: Optional unity builds work, faster release compilation. ✓
**Estimated Impact**: 30-40% faster clean builds when enabled.
**Status**: Successfully implemented. Build tested with both Unity enabled (`-Dunity_build=true`) and disabled (default).

### ADVANCED-002: Implement ccache integration
- [x] Implement ccache integration
**Files**: `meson.build`, `meson_options.txt`, `scripts/build_with_ccache.sh`
**Description**: Add ccache support to Meson build for cross-session compilation caching.
**Implementation**: 
  - Added `use_ccache` option to `meson_options.txt` (defaults to true)
  - Added ccache detection and messaging in `meson.build`
  - Created `scripts/build_with_ccache.sh` helper script with statistics
**Configuration**: Automatically detects and uses ccache when available
**Acceptance**: ccache works when available, significant speedup on repeated builds. ✓
**Status**: Successfully implemented. ccache integration tested and working.
**Estimated Impact**: 80-90% speedup on repeated clean builds.

### ADVANCED-003: Add conditional compilation guards
- [x] Add conditional compilation guards
**Files**: Various headers and source files
**Description**: Add more granular #ifdef guards to exclude unused code paths (ARDUINO vs desktop, hardware versions, etc.).
**Implementation**:
  - Added comprehensive feature flags in `uSEQ/configure.h` (DSP, I2C, Flash, LED, etc.)
  - Added conditional compilation guards for all major subsystems
  - Updated Meson build with options to control feature compilation
  - Added `minimal_build` option for reduced compilation scope
**Target**: Reduce compilation units for specific build configurations
**Acceptance**: Same functionality for each target, smaller compilation scope. ✓
**Status**: Successfully implemented conditional compilation for all major features.
**Estimated Impact**: 10-15% reduction in compilation time per target.

## Phase 6: Build System Validation (Priority: Medium)

### VALIDATION-001: Add build time measurement tools
- [ ] Add build time measurement tools
**New Files**: `scripts/measure_build_times.sh`, `scripts/build_profiler.py`
**Description**: Create tools to measure and track compilation times across different build configurations.
**Metrics**: Clean build time, incremental build time, per-file compilation time
**Acceptance**: Tools provide accurate build time metrics.
**Estimated Impact**: Data-driven build optimization decisions.

### VALIDATION-002: Add build dependency visualization
- [ ] Add build dependency visualization
**New File**: `scripts/visualize_dependencies.py`
**Description**: Tool to generate dependency graphs showing which files trigger the most rebuilds.
**Output**: SVG/PNG dependency graphs, hotspot analysis
**Acceptance**: Clear visualization of build dependencies and bottlenecks.
**Estimated Impact**: Better understanding of optimization opportunities.

### VALIDATION-003: Create build system regression tests
- [ ] Create build system regression tests
**New Directory**: `test/build_tests/`
**Description**: Automated tests to ensure incremental builds work correctly and compilation optimizations don't break functionality.
**Tests**: Incremental build correctness, PCH functionality, library linking
**Acceptance**: All build configurations tested automatically.
**Estimated Impact**: Confidence in build system changes.

## Implementation Notes

### Testing Strategy
- Run full test suite after each atomic change
- Verify both Meson and Arduino CLI builds work
- Test on both debug and release configurations  
- Validate incremental build correctness

### Rollback Plan
- Each change should be independently reversible
- Keep original file structure until validation complete
- Use git branches for major structural changes

### Performance Targets
- **Incremental builds**: 70-80% time reduction
- **Clean builds**: 40-50% time reduction  
- **Template compilation**: 20-30% time reduction
- **Test compilation**: 50-60% time reduction

## Priority Guidelines
1. **Critical**: Must be done first, enables other optimizations
2. **High**: Major impact on build times, relatively safe changes
3. **Medium**: Good improvements, may require more testing
4. **Low**: Nice to have, advanced optimizations

Each task is designed to be independently executable by a focused coding agent with minimal context about the overall refactoring strategy.