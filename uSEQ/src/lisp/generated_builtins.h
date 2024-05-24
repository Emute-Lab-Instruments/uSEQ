#ifndef GENERATED_BUILTINS_H_
#define GENERATED_BUILTINS_H_

#include <vector>

class Value;
class Environment;

namespace builtin
{
Value tail(std::vector<Value>& args, Environment& env);
Value vec(std::vector<Value>& args, Environment& env);
Value zeros(std::vector<Value>& args, Environment& env);
Value list(std::vector<Value>& args, Environment& env);
Value ard_floor(std::vector<Value>& args, Environment& env);
Value ard_ceil(std::vector<Value>& args, Environment& env);
Value do_block(std::vector<Value>& args, Environment& env);
Value neq(std::vector<Value>& args, Environment& env);
Value ard_usin(std::vector<Value>& args, Environment& env);
Value index(std::vector<Value>& args, Environment& env);
Value ard_cos(std::vector<Value>& args, Environment& env);
Value eq(std::vector<Value>& args, Environment& env);
Value ard_abs(std::vector<Value>& args, Environment& env);
Value ard_millis(std::vector<Value>& args, Environment& env);
Value sum(std::vector<Value>& args, Environment& env);
Value for_loop(std::vector<Value>& args, Environment& env);
Value pop(std::vector<Value>& args, Environment& env);
Value ard_min(std::vector<Value>& args, Environment& env);
Value push(std::vector<Value>& args, Environment& env);
Value greater(std::vector<Value>& args, Environment& env);
Value product(std::vector<Value>& args, Environment& env);
Value replace(std::vector<Value>& args, Environment& env);
Value ard_sin(std::vector<Value>& args, Environment& env);
Value ard_ucos(std::vector<Value>& args, Environment& env);
Value eval(std::vector<Value>& args, Environment& env);
Value println(std::vector<Value>& args, Environment& env);
Value cast_to_int(std::vector<Value>& args, Environment& env);
Value remainder(std::vector<Value>& args, Environment& env);
Value subtract(std::vector<Value>& args, Environment& env);
Value define(std::vector<Value>& args, Environment& env);
Value ard_pow(std::vector<Value>& args, Environment& env);
Value while_loop(std::vector<Value>& args, Environment& env);
Value remove(std::vector<Value>& args, Environment& env);
Value scope(std::vector<Value>& args, Environment& env);
Value debug(std::vector<Value>& args, Environment& env);
Value less_eq(std::vector<Value>& args, Environment& env);
Value ard_max(std::vector<Value>& args, Environment& env);
Value ard_delay(std::vector<Value>& args, Environment& env);
Value timeit(std::vector<Value>& args, Environment& env);
Value ard_map(std::vector<Value>& args, Environment& env);
Value head(std::vector<Value>& args, Environment& env);
Value get_type_name(std::vector<Value>& args, Environment& env);
Value less(std::vector<Value>& args, Environment& env);
Value ard_micros(std::vector<Value>& args, Environment& env);
Value divide(std::vector<Value>& args, Environment& env);
Value ard_delaymicros(std::vector<Value>& args, Environment& env);
Value insert(std::vector<Value>& args, Environment& env);
Value ard_sqrt(std::vector<Value>& args, Environment& env);
Value print(std::vector<Value>& args, Environment& env);
Value display(std::vector<Value>& args, Environment& env);
Value greater_eq(std::vector<Value>& args, Environment& env);
Value quote(std::vector<Value>& args, Environment& env);
Value cast_to_float(std::vector<Value>& args, Environment& env);
Value len(std::vector<Value>& args, Environment& env);
Value set(std::vector<Value>& args, Environment& env);
Value defun(std::vector<Value>& args, Environment& env);
Value useq_sqr(std::vector<Value>& args, Environment& env);
Value lambda(std::vector<Value>& args, Environment& env);
Value useq_pulse(std::vector<Value>& args, Environment& env);
Value ard_tan(std::vector<Value>& args, Environment& env);
Value if_then_else(std::vector<Value>& args, Environment& env);
} // namespace builtin

#endif // GENERATED_BUILTINS_H_