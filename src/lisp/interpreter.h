#ifndef INTERPRETER_H_
#define INTERPRETER_H_

/* #include "utils/string.h" */
#include "lisp/environment.h"
#include "lisp/parser.h"
// #include "lisp/parser_old.h"
#include "utils/string.h"

using ExternalBuiltinInserter = std::function<void(BuiltinMap&)>;

class Interpreter : public Environment, public uLispParser
{
public:
    Interpreter();

    // Allows the user of an interpreter class to provide
    // its own function that takes a BuiltinMap and inserts
    // entries for functions
    // void insert_external_builtins(ExternalBuiltinInserter);

    // TODO test
    String eval(const String& code);
    Value eval(Value);

    // Value parse(const String& code);
    void set(String, Value);

private:
    static bool m_builtindefs_init;
    void loadBuiltinDefs();

    Environment m_environment;

    Value eval_in(Value, Environment);
    String eval_in(const String& code, Environment env);
};

// This function is NOT a builtin function, but it is used
// by almost all of them.
//
// Special forms are just builtin functions that don't evaluate
// their arguments. To make a regular builtin that evaluates its
// arguments, we just call this function in our builtin definition.
void eval_args(std::vector<Value>& args, Environment& env);

#endif // INTERPRETER_H_};
