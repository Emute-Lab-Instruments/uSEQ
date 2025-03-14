#ifndef USEQ_MODULES_H_
#define USEQ_MODULES_H_

/**
 * This header controls which new module implementations are enabled.
 * During refactoring, modules should be enabled one by one after migrating
 * their functionality from uSEQ.cpp to the respective module file.
 * 
 * The development process:
 * 1. Create module headers and stub implementations with proper guards
 * 2. Set USE_NEW_MODULES to 0 initially to avoid duplicate symbol errors
 * 3. After migrating a module's functionality, enable it by setting its flag to 1
 * 4. Build, test, and verify functionality
 * 5. Repeat for each module until all have been migrated
 * 6. Finally set USE_NEW_MODULES to 1 and remove original code from uSEQ.cpp
 */

// Master switch for all modules
#define USE_NEW_MODULES 0

// Individual module switches - only take effect if USE_NEW_MODULES is 1
#define USE_CORE_MODULE 0       // Core functionality: run, tick, etc.
#define USE_SIGNALS_MODULE 0     // Signal processing: filters, etc. 
#define USE_IO_MODULE 0          // Input/output handling
#define USE_INIT_MODULE 0        // Initialization and setup
#define USE_TIMING_MODULE 0      // Clock and timing management
#define USE_SCHEDULER_MODULE 0   // Event scheduling
#define USE_STORAGE_MODULE 0     // Flash memory operations
#define USE_LISPFUNCTIONS_MODULE 0 // LISP interpreter integration
#define USE_HARDWARECONTROL_MODULE 0 // Hardware-specific functionality

// Module headers are included in uSEQ.h before this file

#endif // USEQ_MODULES_H_