#ifndef CORE_FWD_H_
#define CORE_FWD_H_

// Forward declarations for core uSEQ types
// This lightweight header provides forward declarations for frequently-used
// classes to reduce compilation dependencies and header weight.

// Core LISP interpreter classes
class Value;
class Environment;
class Interpreter;

// ModuLisp interpreter (extends Interpreter)
class ModuLispInterpreter;

// Hardware interface class (extends ModuLisp)
class uSEQ;

// Additional frequently forward-declared classes
class OutputManager;

#endif // CORE_FWD_H_