#ifndef INTERPRETER_H_
#define INTERPRETER_H_

#include "../../utils/string.h"
#include "error_context.h"
#include "environment.h"
#include "parser.h"
#include <memory>

class uSEQ;

extern bool user_interaction;

class Interpreter {
  public:
    // Constructor with dependency injection
    Interpreter(Environment* env, uLispParser* parser, ErrorManager* error_mgr);
    
    // Default constructor for backward compatibility
    Interpreter();

    void init();

    // Static helper functions for testing and initialization
    static void init_builtin_functions();
    static Interpreter create_fresh_interpreter();

    // Allows the user of an interpreter class to provide
    // its own function that takes a BuiltinMap and inserts
    // entries for functions
    // void insert_external_builtins(ExternalBuiltinInserter);

    // Value parse(const String& code);
    // void set(const String&, Value);
    // void set(const String&, const String&);

    // TODO test
    String eval(const String &code);
    Value eval(Value);
    Value eval_v(const String &code);

    static String eval_in(const String &code, Environment &env);
    static Value eval_in(Value &, Environment &);
    static Value apply(Value &f, LispFuncArgsVec &args, Environment &env);
    
    // New methods with error manager support
    static String eval_in(const String &code, Environment &env, ErrorManager* error_mgr);
    static Value eval_in(Value &, Environment &, ErrorManager* error_mgr);
    static Value apply(Value &f, LispFuncArgsVec &args, Environment &env, ErrorManager* error_mgr);

    // This function is NOT a builtin function, but it is used
    // by almost all of them.
    //
    // Special forms are just builtin functions that don't evaluate
    // their arguments. To make a regular builtin that evaluates its
    // arguments, we just call this function in our builtin definition.
    static void eval_args(std::vector<Value> &args, Environment &env);

    static uSEQ *useq_instance_ptr;
    
    // Error handling
    ErrorManager* get_error_manager() { return m_error_manager; }
    const ErrorManager* get_error_manager() const { return m_error_manager; }
    
    // Atom evaluation tracking
    static void set_atom_currently_being_evaluated(const String& atom_name) {
        m_atom_currently_being_evaluated = atom_name;
    }
    static bool get_attempt_expr_eval_first() { return m_attempt_expr_eval_first; }
    static void set_attempt_expr_eval_first(bool value) { m_attempt_expr_eval_first = value; }
    static bool get_update_loop_evaluation() { return m_update_loop_evaluation; }
    static void set_update_loop_evaluation(bool value) { m_update_loop_evaluation = value; }
    static bool get_manual_evaluation() { return m_manual_evaluation; }
    static void set_manual_evaluation(bool value) { m_manual_evaluation = value; }
    
    // Environment access
    Environment* get_environment() { return m_environment; }
    const Environment* get_environment() const { return m_environment; }
    
    // Parser access
    uLispParser* get_parser() { return m_parser; }
    const uLispParser* get_parser() const { return m_parser; }

  protected:
    // may be used by uSEQ class
    bool evalled_args_contain_errors(std::vector<Value> &args);

    static bool m_attempt_expr_eval_first;
    static bool m_eval_expr_if_def_not_found;
    static bool m_manual_evaluation;
    static bool m_update_loop_evaluation;
    static String m_atom_currently_being_evaluated;

  private:
    bool m_builtindefs_init = false;
    void loadBuiltinDefs();
    
    // Injected dependencies
    Environment* m_environment;
    uLispParser* m_parser;
    ErrorManager* m_error_manager;
    
    // Fallback components for backward compatibility
    std::unique_ptr<Environment> m_fallback_environment;
    std::unique_ptr<uLispParser> m_fallback_parser;
    std::unique_ptr<ErrorManager> m_fallback_error_manager;
};

#endif // INTERPRETER_H_};
