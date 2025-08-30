# uSEQ Build System Optimization TODO

This document contains a priority-ordered list of atomic refactoring tasks to improve compilation times and build system efficiency. Each task is designed to leave the codebase in a working state while gradually implementing the optimization strategy.

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
- [ ] Create forward declaration header for core types
**New File**: `uSEQ/src/core_fwd.h`
**Description**: Create a lightweight forward declaration header containing forward declarations for Value, Environment, Interpreter, ModuLispInterpreter, and uSEQ classes.
**Content**: Forward declarations only, no implementations, minimal includes
**Acceptance**: Header compiles cleanly, can be included without pulling in heavy dependencies.
**Estimated Impact**: Reduces header weight in files that only need pointers/references.

### HEADERS-002: Extract SignalMetadata from value.h
- [ ] Extract SignalMetadata from value.h
**New Files**: 
- `uSEQ/src/modulisp/lisp/signal_metadata.h`
- `uSEQ/src/modulisp/lisp/signal_metadata.cpp`
**Description**: Move the large SignalMetadata struct (lines 44-114 in value.h) to separate files. Keep only forward declaration in value.h.
**Dependencies**: Include signal_metadata.h in value.cpp and any files using SignalMetadata directly.
**Acceptance**: value.h is significantly smaller, all signal metadata functionality works.
**Estimated Impact**: ~70 lines removed from frequently-included value.h.

### HEADERS-003: Extract signal processing methods from Value class
- [ ] Extract signal processing methods from Value class
**New Files**:
- `uSEQ/src/modulisp/lisp/value_signal_processing.h`
- `uSEQ/src/modulisp/lisp/value_signal_processing.cpp`
**Description**: Move signal processing methods (lines 323-357 in value.h) to separate files. Use friend functions or PIMPL pattern.
**Target Methods**: All automation analysis methods, mathematical functions on signals
**Acceptance**: value.h is lighter, signal processing functionality preserved.
**Estimated Impact**: ~35 lines removed from value.h, cleaner separation of concerns.

### HEADERS-004: Split uSEQ.h into functional modules
- [ ] Split uSEQ.h into functional modules
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
- [ ] Add precompiled header support to Meson
**Files**: `meson.build`, new `uSEQ/src/pch.h`
**Description**: Create precompiled header containing stable system includes (STL, optional Arduino headers). Configure Meson to use PCH.
**PCH Content**: `<vector>`, `<memory>`, `<string>`, `<functional>`, `<optional>`, `<cmath>`, Arduino core headers
**Acceptance**: Builds use PCH, clean build times reduced by 20-30%.
**Estimated Impact**: Significant reduction in preprocessing time for stable headers.

### BUILD-002: Implement proper dependency tracking in Meson
- [ ] Implement proper dependency tracking in Meson
**Files**: `meson.build`
**Description**: Add explicit dependency declarations between compilation units. Use Meson dependency tracking to enable incremental builds.
**Target**: Ensure only affected files recompile when headers change
**Acceptance**: Incremental builds work correctly, only changed dependencies rebuild.
**Estimated Impact**: 70-80% reduction in incremental build times.

### BUILD-003: Create static library for LISP interpreter core
- [ ] Create static library for LISP interpreter core
**Files**: `meson.build`, new `uSEQ/src/modulisp/meson.build`
**Description**: Package the LISP interpreter (parser, value, environment, interpreter, builtins) as a separate static library.
**Library Name**: `libuseq_modulisp`
**Acceptance**: LISP core compiles as library, main targets link against it, incremental builds faster.
**Estimated Impact**: LISP changes don't force recompilation of hardware/DSP code.

### BUILD-004: Create static library for utilities
- [ ] Create static library for utilities
**Files**: `meson.build`, new `uSEQ/src/utils/meson.build`  
**Description**: Package utility functions (string, log, common, flags, etc.) as static library.
**Library Name**: `libuseq_utils`
**Acceptance**: Utils compile as library, reduced rebuilding when main code changes.
**Estimated Impact**: Utility changes isolated from main codebase compilation.

### BUILD-005: Create static library for DSP engine
- [ ] Create static library for DSP engine
**Files**: `meson.build`, new `uSEQ/src/dsp/meson.build`
**Description**: Package DSP components (tempo estimator, filters, DSP engine) as static library.
**Library Name**: `libuseq_dsp`  
**Special Handling**: Arduino-specific code conditionally included
**Acceptance**: DSP compiles separately, hardware changes don't affect DSP builds.
**Estimated Impact**: Better separation between real-time DSP and control logic.

## Phase 3: Interface Improvements (Priority: Medium)

### INTERFACE-001: Create minimal testing interfaces
- [ ] Create minimal testing interfaces
**New Files**:
- `uSEQ/src/interfaces/minimal_interpreter.h`
- `uSEQ/src/interfaces/minimal_value.h`
**Description**: Create lightweight interfaces for testing that don't pull in full implementation headers.
**Target**: Reduce test compilation times by avoiding heavy headers
**Acceptance**: Tests build faster, test functionality unchanged.
**Estimated Impact**: 50% reduction in test compilation times.

### INTERFACE-002: Implement PIMPL pattern for Value class
- [ ] Implement PIMPL pattern for Value class
**Files**: Modify `uSEQ/src/modulisp/lisp/value.h`, `value.cpp`
**New File**: `uSEQ/src/modulisp/lisp/value_impl.h`
**Description**: Use PIMPL (Pointer to Implementation) for Value class to reduce header dependencies and compile times.
**Target**: Hide implementation details, reduce value.h size
**Acceptance**: Value interface unchanged, implementation hidden, faster compilation.
**Estimated Impact**: Major reduction in files affected by Value implementation changes.

### INTERFACE-003: Extract port interfaces to separate module
- [ ] Extract port interfaces to separate module
**New Directory**: `uSEQ/src/ports/`
**Files**: Move `IClock.h`, `ILogger.h`, `IIo.h`, etc. to ports directory
**Description**: Group all interface definitions in dedicated directory for cleaner organization.
**Acceptance**: Interfaces accessible, dependency injection still works.
**Estimated Impact**: Better code organization, cleaner include paths.

## Phase 4: Template and Compilation Optimizations (Priority: Medium)

### TEMPLATE-001: Add explicit template instantiations
- [ ] Add explicit template instantiations
**New File**: `uSEQ/src/template_instantiations.cpp`
**Description**: Explicitly instantiate commonly used templates (std::vector<Value>, etc.) to reduce compile-time template processing.
**Target Templates**: std::vector<Value>, std::shared_ptr<Environment>
**Acceptance**: Compilation faster, template bloat reduced.
**Estimated Impact**: 15-20% reduction in template processing time.

### TEMPLATE-002: Optimize Value class template usage
- [ ] Optimize Value class template usage  
**Files**: `uSEQ/src/modulisp/lisp/value.h`, `value.cpp`
**Description**: Review and optimize template usage in Value class. Consider explicit specializations for common types.
**Target**: Reduce template instantiation overhead
**Acceptance**: Same functionality, faster compilation of Value-heavy code.
**Estimated Impact**: Reduction in template-heavy compilation units.

## Phase 5: Advanced Build Optimizations (Priority: Low)

### ADVANCED-001: Add Unity build option
- [ ] Add Unity build option
**Files**: `meson.build`, new `unity/` directory
**Description**: Add Meson option for unity builds (combining multiple .cpp files). Useful for release builds.
**Option Name**: `unity_build=true`
**Acceptance**: Optional unity builds work, faster release compilation.
**Estimated Impact**: 30-40% faster clean builds when enabled.

### ADVANCED-002: Implement ccache integration
- [ ] Implement ccache integration
**Files**: `meson.build`, documentation
**Description**: Add ccache support to Meson build for cross-session compilation caching.
**Configuration**: Detect ccache availability, configure compilation caching
**Acceptance**: ccache works when available, significant speedup on repeated builds.
**Estimated Impact**: 80-90% speedup on repeated clean builds.

### ADVANCED-003: Add conditional compilation guards
- [ ] Add conditional compilation guards
**Files**: Various headers and source files
**Description**: Add more granular #ifdef guards to exclude unused code paths (ARDUINO vs desktop, hardware versions, etc.).
**Target**: Reduce compilation units for specific build configurations
**Acceptance**: Same functionality for each target, smaller compilation scope.
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