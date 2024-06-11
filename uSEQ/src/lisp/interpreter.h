#ifndef INTERPRETER_H_
#define INTERPRETER_H_

#include "../utils/string.h"
#include "environment.h"
#include "parser.h"

class uSEQ;

extern bool user_interaction;

class Interpreter : public Environment, public uLispParser
{
public:
    Interpreter();

    void init();

    // Allows the user of an interpreter class to provide
    // its own function that takes a BuiltinMap and inserts
    // entries for functions
    // void insert_external_builtins(ExternalBuiltinInserter);

    // Value parse(const String& code);
    // void set(const String&, Value);
    // void set(const String&, const String&);

    // TODO test
    String eval(const String& code);
    Value eval(Value);
    Value eval_v(const String& code);

    static String eval_in(const String& code, Environment& env);
    static Value eval_in(Value&, Environment&);
    static Value apply(Value& f, LispFuncArgsVec& args, Environment& env);

    // This function is NOT a builtin function, but it is used
    // by almost all of them.
    //
    // Special forms are just builtin functions that don't evaluate
    // their arguments. To make a regular builtin that evaluates its
    // arguments, we just call this function in our builtin definition.
    static void eval_args(std::vector<Value>& args, Environment& env);

    static uSEQ* useq_instance_ptr;

protected:
    // may be used by uSEQ class
    bool evalled_args_contain_errors(std::vector<Value>& args);

    static bool m_attempt_expr_eval_first;
    static bool m_eval_expr_if_def_not_found;
    static bool m_manual_evaluation;
    static bool m_update_loop_evaluation;
    static String m_atom_currently_being_evaluated;

private:
    bool m_builtindefs_init = false;
    void loadBuiltinDefs();
};

#endif // INTERPRETER_H_};
