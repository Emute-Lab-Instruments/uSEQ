#include "test_helpers.h"
#include "../modulisp/modulisp_interpreter.h"
#include "../modulisp/lisp/value.h"
#include <cmath>
#include <sstream>

namespace test_helpers {

QuickInterpreter::QuickInterpreter() : impl(std::make_unique<ModuLispInterpreter>(nullptr)) {
    impl->init();
}

QuickInterpreter::~QuickInterpreter() = default;

String QuickInterpreter::evalToString(const String& code) {
    return impl->eval(code);
}

float QuickInterpreter::evalToNumber(const String& code) {
    Value result = impl->eval_v(code);
    if (result.is_number()) {
        if (result.type == Value::INT) {
            return static_cast<float>(result.stack_data.i);
        } else if (result.type == Value::FLOAT) {
            return result.stack_data.f;
        }
    }
    return 0.0f;
}

bool QuickInterpreter::evalToBool(const String& code) {
    Value result = impl->eval_v(code);
    // In LISP, anything not NIL is true
    return !result.is_nil();
}

size_t QuickInterpreter::evalListSize(const String& code) {
    Value result = impl->eval_v(code);
    return result.is_list() ? result.list.size() : 0;
}

bool QuickInterpreter::evalIsNumber(const String& code) {
    Value result = impl->eval_v(code);
    return result.is_number();
}

bool QuickInterpreter::evalIsString(const String& code) {
    Value result = impl->eval_v(code);
    return result.is_string();
}

bool QuickInterpreter::evalIsList(const String& code) {
    Value result = impl->eval_v(code);
    return result.is_list();
}

bool QuickInterpreter::evalIsNil(const String& code) {
    Value result = impl->eval_v(code);
    return result.is_nil();
}

bool QuickInterpreter::evalIsTrue(const String& code) {
    Value result = impl->eval_v(code);
    // In LISP, anything not NIL is true
    return !result.is_nil();
}

namespace test_utils {

String makeAddExpr(float a, float b) {
    std::ostringstream ss;
    ss << "(+ " << a << " " << b << ")";
    return String(ss.str().c_str());
}

String makeListExpr(float a, float b, float c) {
    std::ostringstream ss;
    ss << "(list " << a << " " << b << " " << c << ")";
    return String(ss.str().c_str());
}

String makeIfExpr(const String& cond, const String& then_branch, const String& else_branch) {
    String expr = "(if ";
    expr += cond;
    expr += " ";
    expr += then_branch;
    expr += " ";
    expr += else_branch;
    expr += ")";
    return expr;
}

bool floatEquals(float a, float b, float epsilon) {
    return std::abs(a - b) < epsilon;
}

}

}
