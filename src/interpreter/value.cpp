#include "value.h"

Value Value::apply(std::vector<Value> &args, Environment &env) {

  switch (type) {
  case LAMBDA: {
    Environment e;
    std::vector<Value> *params;

    // Get the list of parameter atoms
    params = &list[0].list;
    if (params->size() != args.size()) {
      println(args.size() > params->size() ? TOO_MANY_ARGS : TOO_FEW_ARGS);
      return Value::error();
    }

    // throw Error(Value(args), env, args.size() > params.size()?
    //     TOO_MANY_ARGS : TOO_FEW_ARGS
    // );

    // Get the captured scope from the lambda
    e = lambda_scope;
    // And make this scope the parent scope
    e.set_parent_scope(&env);

    // Iterate through the list of parameters and
    // insert the arguments into the scope.
    for (size_t i = 0; i < params->size(); i++) {
      if ((*params)[i].type != ATOM)
        println(INVALID_LAMBDA);
      // throw Error(*this, env, INVALID_LAMBDA);
      // Set the parameter name into the scope.
      e.set((*params)[i].str, args[i]);
    }
    // Evaluate the function body with the function scope
    auto result = list[1].eval(e);
    return result;
  }
  case BUILTIN: {
    // Here, we call the builtin function with the current scope.
    // This allows us to write special forms without syntactic sugar.
    // For functions that are not special forms, we just evaluate
    // the arguments before we run the function.
    auto result = (stack_data.b)(args, env);
    return result;
  }
  default:
    // We can only call lambdas and builtins
    print(CALL_NON_FUNCTION);
    println(str);
    return Value::error();
  }
}
