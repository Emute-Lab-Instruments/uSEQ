# Developer Info

## Code Structure
- `uSEQ/uSEQ.ino`: Entry point for the Arduino IDE, which just includes `src/uSEQ.h`, creates a `uSEQ` object, and calls `uSEQ::tick()` on it forever in a loop. This is where you can specify which hardware version you have, or configure other parameters such as number of ins/outs.
- `uSEQ/src/uSEQ.h`: Main uSEQ class, which inherits from `Interpreter` and adds some uSEQ-specific Lisp functionality on top.
- `uSEQ/src/uSEQ`: Various module-specific configuration and functionality, unrelated to the Lisp interpreter.
  - `uSEQ/src/uSEQ/configure.h`: `#define`s to configure various aspects of the module, e.g. number of continuous/binary ins/outs etc.
- `uSEQ/src/lisp`: All classes related to the base Lisp interpreter functionality - `Parser, Value, Environment, Interpreter`.
  - `uSEQ/src/lisp/configure.h`: `#define`s for various aspects of the Lisp interpreter's configuration.
  - `uSEQ/src/lisp/macros.h`: Various convenience macros used throughout the codebas.
  - `uSEQ/src/lisp/LispLibrary.lisp`: uSEQ-style-Lisp library of functions that will be loaded on startup. 
  
    NOTE: whenever this is changed, the `scripts/lisplibrary.py` script needs to be re-run with the input and output files: 
    ```
    cd uSEQ/src/lisp && python3 ../../../scripts/lisplibrary.py LispLibrary.lisp LispLibrary.h
    ```
- `uSEQ/src/utils` & `utils.h`: Various utilities for logging, debugging etc.
- `uSEQ/src/dsp`: DSP (e.g. sampling and some basic synthesis) to run on the second core.
- `uSEQ/src/io`: Functionality relating to hardware and/or software IO (NOTE: not currently used).
- `uSEQ/src/ml`: Functionality for Machine Learning, e.g. input analysis or pattern generation.
