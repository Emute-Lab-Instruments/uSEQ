# Developer Info

## Code Structure
- `uSEQ/uSEQ.ino`: Entry point for the Arduino IDE, which just includes `src/uSEQ.h`, creates a `uSEQ` object, and calls `uSEQ::tick()` on it forever in a loop. This is where you can specify which hardware version you have, or configure other parameters such as number of ins/outs.
- `uSEQ/src/uSEQ.h`: Main uSEQ class, which inherits from `Interpreter` and adds some uSEQ-specific Lisp functionality on top.
- `uSEQ/src/uSEQ_*.h/cpp`: Modular components of the uSEQ system:
  - `uSEQ_Modules.h`: Central configuration for enabling/disabling modules
  - `uSEQ_Core.cpp`: Core functionality (run, tick methods)
  - `uSEQ_Signals.h/cpp`: Signal processing and filter implementations
  - `uSEQ_IO.h/cpp`: Input/output handling
  - `uSEQ_Init.h/cpp`: Initialization and setup
  - `uSEQ_Timing.h/cpp`: Clock and timing management
  - `uSEQ_Scheduler.h/cpp`: Event scheduling
  - `uSEQ_Storage.h/cpp`: Flash memory operations
  - `uSEQ_LispFunctions.h/cpp`: LISP interpreter integration
  - `uSEQ_HardwareControl.h/cpp`: Hardware-specific functionality
- `uSEQ/src/uSEQ`: Various module-specific configuration and functionality, unrelated to the Lisp interpreter.
  - `uSEQ/src/uSEQ/configure.h`: `#define`s to configure various aspects of the module, e.g. number of continuous/binary ins/outs etc.
- `uSEQ/src/lisp`: All classes related to the base Lisp interpreter functionality - `Parser, Value, Environment, Interpreter`.
  - `uSEQ/src/lisp/configure.h`: `#define`s for various aspects of the Lisp interpreter's configuration.
  - `uSEQ/src/lisp/macros.h`: Various convenience macros used throughout the codebase.
  - `uSEQ/src/lisp/LispLibrary.lisp`: uSEQ-style-Lisp library of functions that will be loaded on startup. 
  
    NOTE: whenever this is changed, the `scripts/lisplibrary.py` script needs to be re-run with the input and output files: 
    ```
    cd uSEQ/src/lisp && python3 ../../../scripts/lisplibrary.py LispLibrary.lisp LispLibrary.h
    ```
- `uSEQ/src/utils` & `utils.h`: Various utilities for logging, debugging etc.
- `uSEQ/src/dsp`: DSP (e.g. sampling and some basic synthesis) to run on the second core.
- `uSEQ/src/io`: Functionality relating to hardware and/or software IO (NOTE: not currently used).
- `uSEQ/src/ml`: Functionality for Machine Learning, e.g. input analysis or pattern generation.

## Modular Architecture

The codebase is being refactored to use a modular architecture with preprocessor guards to enable/disable specific modules. This allows for easier maintenance and testing of individual components:

1. Each module has its own header and implementation file
2. The `uSEQ_Modules.h` file controls which modules are enabled
3. Preprocessor guards ensure only enabled modules are compiled

The migration process from the monolithic uSEQ.cpp to the modular architecture:

1. Create module headers and stub implementations with proper preprocessor guards
2. Set USE_NEW_MODULES to 0 initially to avoid duplicate symbol errors
3. After migrating a module's functionality, enable it by setting its flag to 1
4. Build, test, and verify functionality
5. Repeat for each module until all have been migrated
6. Finally set USE_NEW_MODULES to 1 and remove original code from uSEQ.cpp
