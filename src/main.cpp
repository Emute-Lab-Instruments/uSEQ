#include "interpreter.h"
#include <iostream>

Environment env;

Value make_signal(Value body) {
  Value preamble = parse("(lambda (t))");
  std::vector<Value> expr = {parse("t"), body};
  return builtin::lambda(expr, env);
}

int main(int argc, char *argv[]) {
  // Value sigfunc = parse("+ 1 t");
  // Value sig = make_signal(sigfunc);
  // print(sig.debug());

  // std::vector<Value> a = {Value(1)};
  // Value result = sig.apply(a);
  // print(result.debug());
  //

  Value expr =
      make_signal(Value({Value::atom("+"), Value(1), Value::atom("t")}));
  std::vector<Value> args = {Value(1)};
  print(expr.apply(args, env).debug());
  // print(expr[0].debug());
  return 0;
}
