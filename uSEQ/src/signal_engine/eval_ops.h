#ifndef SIGNAL_ENGINE_EVAL_OPS_H
#define SIGNAL_ENGINE_EVAL_OPS_H

// Pure-math NodeOp evaluation — shared by executor (hot path) and constant
// folding (node_pool).  Load ops (Const, CellLoad, InputLoad, etc.) and
// data ops (VecIndex, VecLerp) are NOT included here; they require runtime
// context and stay in executor.cpp.

#include "node_pool.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace sig {

static inline double eval_unary_op(NodeOp op, double a) {
    switch (op) {
        case NodeOp::Neg:     return -a;
        case NodeOp::Abs:     return fabs(a);
        case NodeOp::Floor:   return floor(a);
        case NodeOp::Ceil:    return ceil(a);
        case NodeOp::Frac:    return a - floor(a);
        case NodeOp::Sqrt:    return sqrt(fabs(a));
        case NodeOp::Sin:     return sin(a);
        case NodeOp::Cos:     return cos(a);
        case NodeOp::Tan:     return tan(a);
        case NodeOp::USin:    return (sin(a * 2.0 * M_PI) + 1.0) * 0.5;
        case NodeOp::UCos:    return (cos(a * 2.0 * M_PI) + 1.0) * 0.5;
        case NodeOp::Tri:     return 1.0 - fabs(2.0 * (a - floor(a)) - 1.0);
        case NodeOp::Sqr:     return ((a - floor(a)) < 0.5) ? 1.0 : 0.0;
        case NodeOp::Not:     return (a == 0.0) ? 1.0 : 0.0;
        case NodeOp::BiToUni: return (a + 1.0) * 0.5;
        case NodeOp::UniToBi: return a * 2.0 - 1.0;
        case NodeOp::HashIndex: {
            uint32_t v = (uint32_t)(int32_t)a;
            v = ((v >> 16) ^ v) * 0x45d9f3bu;
            v = ((v >> 16) ^ v) * 0x45d9f3bu;
            v = (v >> 16) ^ v;
            return (double)(v & 0x7fffffffu) / (double)0x7fffffffu;
        }
        default: return 0.0;
    }
}

static inline double eval_binary_op(NodeOp op, double a, double b) {
    switch (op) {
        case NodeOp::Add:   return a + b;
        case NodeOp::Sub:   return a - b;
        case NodeOp::Mul:   return a * b;
        case NodeOp::Div:   return (b != 0.0) ? a / b : 0.0;
        case NodeOp::Mod:   return (b != 0.0) ? fmod(a, b) : 0.0;
        case NodeOp::Expt:  return pow(a, b);
        case NodeOp::Min:   return (a < b) ? a : b;
        case NodeOp::Max:   return (a > b) ? a : b;
        case NodeOp::Pulse: return ((a - floor(a)) < b) ? 1.0 : 0.0;
        case NodeOp::CmpGt: return (a > b)  ? 1.0 : 0.0;
        case NodeOp::CmpLt: return (a < b)  ? 1.0 : 0.0;
        case NodeOp::CmpGe: return (a >= b) ? 1.0 : 0.0;
        case NodeOp::CmpLe: return (a <= b) ? 1.0 : 0.0;
        case NodeOp::CmpEq: return (a == b) ? 1.0 : 0.0;
        case NodeOp::And:   return (a != 0.0 && b != 0.0) ? 1.0 : 0.0;
        case NodeOp::Or:    return (a != 0.0 || b != 0.0) ? 1.0 : 0.0;
        default: return 0.0;
    }
}

// Convention: ternary ops take their "main" argument LAST so that the
// thing-being-affected can be a nested expression at the end of a lisp form.
//   (clamp lo  hi  value) — value is last
//   (scale min max value) — value is last
//   (lerp  a   b   t)    — interpolation parameter t is last
//   (if    cond then else) — condition is first (Select)
static inline double eval_ternary_op(NodeOp op, double a, double b, double c) {
    switch (op) {
        case NodeOp::Clamp:  return (c < a) ? a : (c > b) ? b : c; // (clamp lo hi value)
        case NodeOp::Lerp:   return a + (b - a) * c;
        case NodeOp::Scale:  return c * (b - a) + a;
        case NodeOp::Select: return (a != 0.0) ? b : c;
        default: return 0.0;
    }
}

} // namespace sig

#endif // SIGNAL_ENGINE_EVAL_OPS_H
