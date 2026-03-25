#include "bytecode_vm.h"
#include "modulisp_interpreter.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <set>
#include <unordered_map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
bool is_output_assignment_symbol(const String& symbol)
{
    if (symbol.length() < 2)
    {
        return false;
    }

    const char prefix = symbol[0];
    if (prefix != 'a' && prefix != 'd' && prefix != 's')
    {
        return false;
    }

    for (unsigned int i = 1; i < symbol.length(); ++i)
    {
        if (!std::isdigit(static_cast<unsigned char>(symbol[i])))
        {
            return false;
        }
    }

    return true;
}

bool is_signal_side_effect_form(const String& op)
{
    return op == "define" || op == "def" || op == "defn" || op == "defun" ||
           op == "defs" || op == "set" || op == "eval" ||
           op == "schedule" || op == "unschedule" || op == "useq-play" ||
           op == "useq-pause" || op == "useq-stop" || op == "useq-rewind" ||
           op == "useq-clear" || op == "set-bpm" || op == "set-time-sig" ||
           op == "useq-set-time-offset" || op == "useq-nudge-time" ||
           is_output_assignment_symbol(op);
}

std::optional<String> find_signal_side_effect_form(const Value& expr)
{
    if (expr.type == Value::QUOTE)
    {
        return std::nullopt;
    }

    if (expr.is_list())
    {
        const std::vector<Value> items = expr.as_list();
        if (!items.empty() && items[0].is_symbol())
        {
            const String op = items[0].as_atom();
            if (is_signal_side_effect_form(op))
            {
                return op;
            }
        }

        for (const Value& item : items)
        {
            if (const std::optional<String> nested =
                    find_signal_side_effect_form(item))
            {
                return nested;
            }
        }
    }

    if (expr.is_vector())
    {
        for (const Value& item : expr.as_vector())
        {
            if (const std::optional<String> nested =
                    find_signal_side_effect_form(item))
            {
                return nested;
            }
        }
    }

    return std::nullopt;
}

struct AffineTimeTransform
{
    double scale = 1.0;
    double offset = 0.0;
};

struct LocalValueBinding
{
    int source_register = -1;
};

// Diagnostic counter: tracks how many times the VM compiler falls back to
// the tree-walker because a callable has a captured scope (closure).
// To compile closures natively, the VM would need to:
//   1. Identify which variables the closure captures from its defining scope
//   2. Pass those captured values as additional CALL arguments
//   3. Bind them in the callee's local scope before executing the body
// This is deferred to a future iteration. For now, we measure the frequency
// to determine whether it's worth the complexity.
static int s_closure_fallback_count = 0;
int get_closure_fallback_count() { return s_closure_fallback_count; }

bool lambda_has_captured_scope(const Value& value)
{
    if (value.type != Value::LAMBDA || !value.lambda_scope)
    {
        return false;
    }

    if (!value.lambda_scope->get_def_exprs().empty())
    {
        return true;
    }

    for (const auto& entry : value.lambda_scope->get_defs())
    {
        if (!entry.second.is_builtin())
        {
            return true;
        }
    }

    return false;
}

struct CompilerCheckpoint
{
    size_t instruction_count = 0;
    size_t constant_count = 0;
    size_t data_segment_count = 0;
    size_t dependency_count = 0;
    size_t function_count = 0;
    size_t intrinsic_count = 0;
    int next_register = 0;
    size_t cse_cache_size = 0;
};

double wrap_phase(double time_seconds, double duration_seconds);
double time_to_count(double time_seconds, double duration_seconds);
TemporalContext apply_time_transform(const TemporalContext& ctx,
                                     const AffineTimeTransform& transform);

class NumericVmCompiler
{
public:
    explicit NumericVmCompiler(const Environment& env, bool signal_context)
        : m_env(env), m_signal_context(signal_context)
    {
    }

    NumericVmCompileResult compile(const Value& expr)
    {
        // Pre-scan: find subexpressions that appear more than once so
        // CSE only pays the preservation-MOV cost for actual duplicates.
        prescan_for_cse(expr, { 1.0, 0.0 });

        NumericVmCompileResult result;
        const int out_reg = compile_expr(expr, { 1.0, 0.0 });
        if (out_reg < 0)
        {
            result.error = m_error;
            return result;
        }

        NumericVmInstruction ret;
        ret.opcode = NumericVmOpcode::RET;
        ret.rs1 = static_cast<uint16_t>(out_reg);
        m_program.instructions.push_back(ret);
        m_program.register_count = m_next_register;

        result.ok = true;
        result.program = m_program;
        return result;
    }

private:
    int compile_expr(const Value& expr, const AffineTimeTransform& transform)
    {
        // CSE: only consider list expressions that the pre-scan found
        // appearing more than once. This avoids wasting a preservation
        // register + MOV on expressions that are never duplicated.
        const bool cse_candidate = expr.is_list() && !expr.as_list().empty();
        std::string cse_sig;
        bool cse_eligible = false;
        if (cse_candidate)
        {
            cse_sig = expr_signature(expr, transform);
            const auto seen_it = m_cse_seen_count.find(cse_sig);
            cse_eligible = (seen_it != m_cse_seen_count.end() && seen_it->second > 1);

            if (cse_eligible)
            {
                if (const auto it = m_cse_cache.find(cse_sig);
                    it != m_cse_cache.end())
                {
                    // Copy from the preserved register to a fresh one
                    // so callers can freely overwrite the returned register.
                    const int copy = allocate_register();
                    emit_mov(copy, it->second);
                    return copy;
                }
            }
        }

        const size_t insn_before = m_program.instructions.size();
        const int result = compile_expr_inner(expr, transform);

        // Cache the result for future CSE hits, unless the compiled
        // code contains CALL_INTRINSIC instructions (which may depend
        // on mutable state or have side effects). We preserve the
        // value in a dedicated register that is never returned to
        // callers, so in-place emit_unary/emit_binary won't corrupt it.
        if (cse_eligible && result >= 0)
        {
            bool has_intrinsic = false;
            for (size_t i = insn_before; i < m_program.instructions.size(); ++i)
            {
                if (m_program.instructions[i].opcode == NumericVmOpcode::CALL_INTRINSIC)
                {
                    has_intrinsic = true;
                    break;
                }
            }
            if (!has_intrinsic)
            {
                const int preserved = allocate_register();
                emit_mov(preserved, result);
                m_cse_cache[cse_sig] = preserved;
            }
        }

        return result;
    }

    int compile_expr_inner(const Value& expr, const AffineTimeTransform& transform)
    {
        if (const std::optional<double> constant =
                try_resolve_numeric_constant(expr))
        {
            record_dependencies(expr);
            return emit_const(Value(*constant));
        }

        if (expr.is_symbol())
        {
            return compile_symbol(expr.as_atom(), transform);
        }

        if (expr.is_vector())
        {
            return compile_vector_as_list(expr.as_vector(), transform);
        }

        if (expr.type == Value::QUOTE)
        {
            return emit_const(expr.list.empty() ? Value::nil() : expr.list[0]);
        }

        if (expr.type == Value::STRING || expr.type == Value::UNIT ||
            expr.type == Value::NIL || expr.type == Value::LAMBDA)
        {
            record_dependencies(expr);
            return emit_const(expr);
        }

        if (!expr.is_list())
        {
            record_dependencies(expr);
            return emit_const(expr);
        }

        const std::vector<Value> items = expr.as_list();
        if (items.empty())
        {
            return emit_const(Value::nil());
        }
        if (items.size() == 2)
        {
            if (const int vector_lookup =
                    compile_vector_lookup(items[0], items[1], transform, false);
                vector_lookup >= 0)
            {
                return vector_lookup;
            }
        }
        if (items.empty() || !items[0].is_symbol())
        {
            int callable_reg = -1;
            if (try_compile_callable_form(items, transform, callable_reg))
            {
                return callable_reg;
            }
            if (callable_reg < 0 && m_error.length() == 0)
            {
                fail("Numeric VM only supports builtin and inline-call forms");
            }
            return -1;
        }

        const String op = items[0].as_atom();
        if (is_signal_side_effect_form(op))
        {
            if (op == "eval")
            {
                fail("Signal VM rejects eval in signal context");
            }
            else
            {
                fail("Signal VM rejects side-effectful form: " + op);
            }
            return -1;
        }
        if (op == "fast" || op == "slow" || op == "offset" || op == "shift")
        {
            return compile_time_warp(op, items, transform);
        }
        if (op == "do")
        {
            return compile_do(items, transform);
        }
        if (op == "if")
        {
            return compile_if(items, transform);
        }
        if (op == "let")
        {
            return compile_let(items, transform);
        }
        if (op == "while")
        {
            return compile_while(items, transform);
        }
        if (op == "for")
        {
            return compile_for(items, transform);
        }

        if (op == "+")
        {
            return compile_fold(items, 1, NumericVmOpcode::ADD, transform, true);
        }
        if (op == "*")
        {
            return compile_fold(items, 1, NumericVmOpcode::MUL, transform, true);
        }
        if (op == "-")
        {
            if (items.size() == 2)
            {
                const int src = compile_expr(items[1], transform);
                if (src < 0)
                {
                    return -1;
                }
                return emit_unary(NumericVmOpcode::NEG, src);
            }
            return compile_fold(items, 1, NumericVmOpcode::SUB, transform, false);
        }
        if (op == "/")
        {
            return compile_binary(items, NumericVmOpcode::DIV, transform);
        }
        if (op == "%")
        {
            return compile_binary(items, NumericVmOpcode::MOD, transform);
        }
        if (op == ">")
        {
            return compile_binary(items, NumericVmOpcode::CMP_GT, transform);
        }
        if (op == "<")
        {
            return compile_binary(items, NumericVmOpcode::CMP_LT, transform);
        }
        if (op == ">=")
        {
            return compile_binary(items, NumericVmOpcode::CMP_GE, transform);
        }
        if (op == "<=")
        {
            return compile_binary(items, NumericVmOpcode::CMP_LE, transform);
        }
        if (op == "=")
        {
            return compile_binary(items, NumericVmOpcode::CMP_EQ, transform);
        }
        if (op == "floor")
        {
            return compile_unary(items, NumericVmOpcode::FLOOR, transform);
        }
        if (op == "ceil")
        {
            return compile_unary(items, NumericVmOpcode::CEIL, transform);
        }
        if (op == "frac")
        {
            return compile_unary(items, NumericVmOpcode::FRAC, transform);
        }
        if (op == "abs")
        {
            return compile_unary(items, NumericVmOpcode::ABS, transform);
        }
        if (op == "min")
        {
            return compile_binary(items, NumericVmOpcode::MIN, transform);
        }
        if (op == "max")
        {
            return compile_binary(items, NumericVmOpcode::MAX, transform);
        }
        if (op == "pow")
        {
            return compile_binary(items, NumericVmOpcode::POW, transform);
        }
        if (op == "sqrt")
        {
            return compile_unary(items, NumericVmOpcode::SQRT, transform);
        }
        if (op == "clamp")
        {
            return compile_clamp(items, transform);
        }
        if (op == "sin")
        {
            return compile_unary(items, NumericVmOpcode::SIN, transform);
        }
        if (op == "cos")
        {
            return compile_unary(items, NumericVmOpcode::COS, transform);
        }
        if (op == "tan")
        {
            return compile_unary(items, NumericVmOpcode::TAN, transform);
        }
        if (op == "usin" || op == "u-sin")
        {
            return compile_unary(items, NumericVmOpcode::U_SIN, transform);
        }
        if (op == "ucos" || op == "u-cos")
        {
            return compile_unary(items, NumericVmOpcode::U_COS, transform);
        }
        if (op == "usinbi" || op == "u-sin-bi")
        {
            return compile_unary(items, NumericVmOpcode::U_SIN_BI, transform);
        }
        if (op == "ucosbi" || op == "u-cos-bi")
        {
            return compile_unary(items, NumericVmOpcode::U_COS_BI, transform);
        }
        if (op == "tri")
        {
            return compile_unary(items, NumericVmOpcode::TRI, transform);
        }
        if (op == "sqr")
        {
            return compile_unary(items, NumericVmOpcode::SQR, transform);
        }
        if (op == "pulse")
        {
            return compile_binary(items, NumericVmOpcode::PULSE, transform);
        }
        if (op == "input")
        {
            if (items.size() != 2 || !items[1].is_number())
            {
                fail("Numeric VM input requires a compile-time constant integer slot index");
                return -1;
            }
            const int reg = allocate_register();
            NumericVmInstruction insn;
            insn.opcode = NumericVmOpcode::LOAD_INPUT;
            insn.rd = static_cast<uint16_t>(reg);
            insn.imm = static_cast<int32_t>(items[1].as_int());
            m_program.instructions.push_back(insn);
            return reg;
        }
        if (op == "from-list")
        {
            return compile_vector_builtin(items, transform, false);
        }
        if (op == "seq")
        {
            return compile_vector_builtin(items, transform, false);
        }
        if (op == "interp" || op == "flatseq")
        {
            return compile_vector_builtin(items, transform, true);
        }
        if (op == "dm")
        {
            return compile_dm(items, transform);
        }
        if (op == "euclid" || op == "eu")
        {
            return compile_euclid(items, transform);
        }
        if (op == "nil?")
        {
            return compile_unary(items, NumericVmOpcode::IS_NIL, transform);
        }
        if (op == "number?")
        {
            return compile_unary(items, NumericVmOpcode::IS_NUMBER, transform);
        }
        if (op == "list?")
        {
            return compile_unary(items, NumericVmOpcode::IS_LIST, transform);
        }
        if (op == "string?")
        {
            return compile_unary(items, NumericVmOpcode::IS_STRING, transform);
        }
        if (op == "not")
        {
            return compile_unary(items, NumericVmOpcode::NOT, transform);
        }
        if (op == "and")
        {
            return compile_binary(items, NumericVmOpcode::AND, transform);
        }
        if (op == "or")
        {
            return compile_binary(items, NumericVmOpcode::OR, transform);
        }
        if (op == "head" || op == "car")
        {
            return compile_unary(items, NumericVmOpcode::LIST_HEAD, transform);
        }
        if (op == "tail" || op == "cdr")
        {
            return compile_unary(items, NumericVmOpcode::LIST_TAIL, transform);
        }
        if (op == "length")
        {
            return compile_unary(items, NumericVmOpcode::LIST_LENGTH, transform);
        }
        if (op == "list")
        {
            return compile_list_constructor(items, transform);
        }

        // --- Pure numeric builtins: inlined as arithmetic ---
        // These are commonly used in signal expressions. Compiling them to
        // native VM arithmetic avoids the full tree-walk via eval_in that
        // would occur in the generic CALL_INTRINSIC fallback path.
        if (op == "b->u" || op == "bi->uni")
        {
            // bipolar-to-unipolar: x * 0.5 + 0.5
            return compile_range_conversion(items, transform, 0.5, 0.5);
        }
        if (op == "u->b" || op == "uni->bi")
        {
            // unipolar-to-bipolar: x * 2.0 - 1.0
            return compile_range_conversion(items, transform, 2.0, -1.0);
        }
        if (op == "scale")
        {
            return compile_scale_intrinsic(items, transform);
        }
        if (op == "lerp")
        {
            return compile_lerp_intrinsic(items, transform);
        }

        int callable_reg = -1;
        if (try_compile_callable_form(items, transform, callable_reg))
        {
            return callable_reg;
        }

        if (m_error.length() == 0)
        {
            return emit_runtime_eval(expr, transform);
        }
        return -1;
    }

    int compile_symbol(const String& symbol, const AffineTimeTransform& transform)
    {
        if (const std::optional<int> local_reg = find_local_value_binding(symbol))
        {
            const int copy = allocate_register();
            emit_mov(copy, *local_reg);
            return copy;
        }

        NumericVmTemporalChannel channel;
        if (try_get_temporal_channel(symbol, channel))
        {
            const int reg = allocate_register();
            NumericVmInstruction insn;
            insn.opcode = NumericVmOpcode::LOAD_TIME;
            insn.rd = static_cast<uint16_t>(reg);
            insn.imm = static_cast<int32_t>(channel);
            insn.imm0 = transform.scale;
            insn.imm1 = transform.offset;
            m_program.instructions.push_back(insn);
            return reg;
        }

        if (is_in_recursion_stack(symbol))
        {
            fail("Recursive numeric binding is not supported: " + symbol);
            return -1;
        }

        if (const std::optional<Value> expr = m_env.get_expr(symbol))
        {
            maybe_add_dependency(symbol);
            m_recursion_stack.push_back(symbol);
            const CompilerCheckpoint checkpoint = checkpoint_state();
            const String prior_error = m_error;
            const int reg = compile_expr(*expr, transform);
            if (reg < 0)
            {
                rollback(checkpoint);
                m_error = prior_error;
                m_recursion_stack.pop_back();
                return emit_late_bound_symbol(symbol, transform);
            }
            m_recursion_stack.pop_back();
            return reg;
        }

        if (const std::optional<Value> value = m_env.get(symbol))
        {
            maybe_add_dependency(symbol);
            if (!value->is_builtin())
            {
                return emit_const(*value);
            }
            return emit_late_bound_symbol(symbol, transform);
        }

        return emit_late_bound_symbol(symbol, transform);
    }

    int compile_time_warp(const String& op,
                          const std::vector<Value>& items,
                          const AffineTimeTransform& transform)
    {
        if (items.size() != 3)
        {
            fail(op + " expects exactly 2 arguments");
            return -1;
        }

        const std::optional<double> factor_or_offset =
            try_resolve_numeric_constant(items[1]);
        if (!factor_or_offset.has_value())
        {
            fail(op + " requires a compile-time numeric first argument");
            return -1;
        }

        AffineTimeTransform next = transform;
        if (op == "fast")
        {
            next.scale *= *factor_or_offset;
            next.offset *= *factor_or_offset;
        }
        else if (op == "slow")
        {
            next.scale /= *factor_or_offset;
            next.offset /= *factor_or_offset;
        }
        else
        {
            next.offset += *factor_or_offset;
        }

        return compile_expr(items[2], next);
    }

    int compile_do(const std::vector<Value>& items, const AffineTimeTransform& transform)
    {
        if (items.size() < 2)
        {
            fail("do expects at least one form");
            return -1;
        }

        int result = -1;
        for (size_t i = 1; i < items.size(); ++i)
        {
            result = compile_expr(items[i], transform);
            if (result < 0)
            {
                return -1;
            }
        }
        return result;
    }

    int compile_if(const std::vector<Value>& items, const AffineTimeTransform& transform)
    {
        if (items.size() != 4)
        {
            fail("if expects exactly 3 arguments");
            return -1;
        }

        const std::optional<double> constant_condition =
            try_resolve_numeric_constant(items[1]);
        if (constant_condition.has_value())
        {
            return compile_expr(*constant_condition != 0.0 ? items[2] : items[3],
                                transform);
        }

        const int result_reg = allocate_register();
        const int cond_reg = compile_expr(items[1], transform);
        if (cond_reg < 0)
        {
            return -1;
        }

        NumericVmInstruction branch_to_else;
        branch_to_else.opcode = NumericVmOpcode::BRANCH_UNLESS;
        branch_to_else.rs1 = static_cast<uint16_t>(cond_reg);
        branch_to_else.imm = 0;
        const size_t branch_to_else_index = m_program.instructions.size();
        m_program.instructions.push_back(branch_to_else);

        const int then_reg = compile_expr(items[2], transform);
        if (then_reg < 0)
        {
            return -1;
        }
        emit_mov(result_reg, then_reg);

        NumericVmInstruction branch_to_end;
        branch_to_end.opcode = NumericVmOpcode::BRANCH;
        branch_to_end.imm = 0;
        const size_t branch_to_end_index = m_program.instructions.size();
        m_program.instructions.push_back(branch_to_end);

        const size_t else_start_index = m_program.instructions.size();
        patch_branch(branch_to_else_index, else_start_index);

        const int else_reg = compile_expr(items[3], transform);
        if (else_reg < 0)
        {
            return -1;
        }
        emit_mov(result_reg, else_reg);

        const size_t end_index = m_program.instructions.size();
        patch_branch(branch_to_end_index, end_index);
        return result_reg;
    }

    int compile_let(const std::vector<Value>& items,
                    const AffineTimeTransform& transform)
    {
        if (items.size() < 3)
        {
            fail("let expects a bindings vector and at least one body form");
            return -1;
        }
        if (!items[1].is_sequential())
        {
            fail("let bindings must be a sequential collection");
            return -1;
        }

        const std::vector<Value> bindings = items[1].as_sequential();
        if (bindings.size() % 2 != 0)
        {
            fail("let bindings must contain symbol/value pairs");
            return -1;
        }

        push_local_scope();
        for (size_t i = 0; i < bindings.size(); i += 2)
        {
            if (!bindings[i].is_symbol())
            {
                pop_local_scope();
                fail("let binding names must be symbols");
                return -1;
            }

            const String name = bindings[i].as_atom();
            const Value& value_expr = bindings[i + 1];
            if (try_bind_local_callable(name, value_expr))
            {
                continue;
            }

            const int bound_reg = compile_expr(value_expr, transform);
            if (bound_reg < 0)
            {
                pop_local_scope();
                return -1;
            }
            bind_local_value(name, bound_reg);
        }

        int result = -1;
        for (size_t i = 2; i < items.size(); ++i)
        {
            result = compile_expr(items[i], transform);
            if (result < 0)
            {
                pop_local_scope();
                return -1;
            }
        }

        pop_local_scope();
        return result;
    }

    // Compile (while condition body...) natively using BRANCH instructions.
    // If any sub-expression cannot be compiled, falls back to emit_runtime_eval.
    //
    // NOTE: Infinite-loop protection is not yet implemented in the VM executor.
    // Loops that do not terminate will stall the VM. This should be addressed
    // in a future change by adding an iteration counter to the executor's
    // branch-back handling.
    int compile_while(const std::vector<Value>& items,
                      const AffineTimeTransform& transform)
    {
        if (items.size() < 3)
        {
            fail("while expects a condition and at least one body form");
            return -1;
        }

        const CompilerCheckpoint checkpoint = checkpoint_state();
        const String prior_error = m_error;

        const int result_reg = allocate_register();
        {
            const int nil_reg = emit_const(Value::nil());
            emit_mov(result_reg, nil_reg);
        }

        const size_t loop_start = m_program.instructions.size();
        const int cond_reg = compile_expr(items[1], transform);
        if (cond_reg < 0)
        {
            rollback(checkpoint);
            m_error = prior_error;
            return emit_runtime_eval(Value(items), transform);
        }

        NumericVmInstruction branch_to_exit;
        branch_to_exit.opcode = NumericVmOpcode::BRANCH_UNLESS;
        branch_to_exit.rs1 = static_cast<uint16_t>(cond_reg);
        branch_to_exit.imm = 0;
        const size_t branch_to_exit_index = m_program.instructions.size();
        m_program.instructions.push_back(branch_to_exit);

        int body_reg = -1;
        for (size_t i = 2; i < items.size(); ++i)
        {
            body_reg = compile_expr(items[i], transform);
            if (body_reg < 0)
            {
                rollback(checkpoint);
                m_error = prior_error;
                return emit_runtime_eval(Value(items), transform);
            }
        }
        emit_mov(result_reg, body_reg);

        {
            NumericVmInstruction branch_back;
            branch_back.opcode = NumericVmOpcode::BRANCH;
            branch_back.imm = static_cast<int32_t>(loop_start) -
                              static_cast<int32_t>(m_program.instructions.size() + 1);
            m_program.instructions.push_back(branch_back);
        }

        patch_branch(branch_to_exit_index, m_program.instructions.size());
        return result_reg;
    }

    // Compile (for var list body...) — delegates to tree-walker since
    // list iteration requires dynamic variable binding beyond the numeric VM.
    int compile_for(const std::vector<Value>& items,
                    const AffineTimeTransform& transform)
    {
        if (items.size() < 3)
        {
            fail("for expects a variable, a list, and at least one body form");
            return -1;
        }
        return emit_runtime_eval(Value(items), transform);
    }

    int compile_vector_builtin(const std::vector<Value>& items,
                               const AffineTimeTransform& transform,
                               bool interpolate)
    {
        if (items.size() != 3)
        {
            fail(String(interpolate ? "interp" : "from-list") +
                 " expects exactly 2 arguments");
            return -1;
        }

        return compile_vector_lookup(items[1], items[2], transform, interpolate);
    }

    int compile_dm(const std::vector<Value>& items,
                   const AffineTimeTransform& transform)
    {
        if (items.size() != 4)
        {
            fail("dm expects exactly 3 arguments");
            return -1;
        }

        const int result_reg = allocate_register();
        const int cond_reg = compile_expr(items[1], transform);
        if (cond_reg < 0)
        {
            return -1;
        }

        NumericVmInstruction branch_to_else;
        branch_to_else.opcode = NumericVmOpcode::BRANCH_UNLESS;
        branch_to_else.rs1 = static_cast<uint16_t>(cond_reg);
        branch_to_else.imm = 0;
        const size_t branch_to_else_index = m_program.instructions.size();
        m_program.instructions.push_back(branch_to_else);

        const int then_reg = compile_expr(items[3], transform);
        if (then_reg < 0)
        {
            return -1;
        }
        emit_mov(result_reg, then_reg);

        NumericVmInstruction branch_to_end;
        branch_to_end.opcode = NumericVmOpcode::BRANCH;
        branch_to_end.imm = 0;
        const size_t branch_to_end_index = m_program.instructions.size();
        m_program.instructions.push_back(branch_to_end);

        const size_t else_start_index = m_program.instructions.size();
        patch_branch(branch_to_else_index, else_start_index);

        const int else_reg = compile_expr(items[2], transform);
        if (else_reg < 0)
        {
            return -1;
        }
        emit_mov(result_reg, else_reg);

        patch_branch(branch_to_end_index, m_program.instructions.size());
        return result_reg;
    }

    int compile_euclid(const std::vector<Value>& items,
                       const AffineTimeTransform& transform)
    {
        if (items.size() < 4 || items.size() > 6)
        {
            fail("euclid expects 3 to 5 arguments");
            return -1;
        }

        const std::optional<double> n_value =
            try_resolve_numeric_constant(items[1]);
        const std::optional<double> k_value =
            try_resolve_numeric_constant(items[2]);
        const std::optional<double> pulse_width =
            items.size() >= 5 ? try_resolve_numeric_constant(items[3])
                              : std::optional<double>(0.5);
        const std::optional<double> offset =
            items.size() == 6 ? try_resolve_numeric_constant(items[4])
                              : std::optional<double>(0.0);
        if (n_value.has_value() && k_value.has_value() && pulse_width.has_value() &&
            offset.has_value())
        {
            const int n = static_cast<int>(*n_value);
            const int k = static_cast<int>(*k_value);
            if (n > 0)
            {
                std::vector<double> pattern;
                pattern.reserve(static_cast<size_t>(n));
                for (int i = 0; i < n; ++i)
                {
                    const int idx = ((i + n - static_cast<int>(*offset)) * k) % n;
                    pattern.push_back(idx < k ? 1.0 : 0.0);
                }

                std::vector<Value> expanded_items = {
                    Value::atom("from-list"),
                    Value::vector({}),
                    items.back()
                };
                expanded_items[1] = Value::vector({});
                for (double value : pattern)
                {
                    expanded_items[1].push(Value(value));
                }

                if (*pulse_width < 1.0)
                {
                    std::vector<Value> gate_items = {
                        Value::atom("if"),
                        Value({ Value::atom("<"),
                                Value({ Value::atom("frac"),
                                        Value({ Value::atom("*"),
                                                Value(static_cast<double>(n)),
                                                items.back() }) }),
                                Value(*pulse_width) }),
                        Value(expanded_items),
                        Value(0.0)
                    };
                    return compile_expr(Value(gate_items), transform);
                }

                return compile_expr(Value(expanded_items), transform);
            }
        }

        int result = -1;
        if (try_emit_intrinsic_call(items[0].as_atom(),
                                    std::vector<Value>(items.begin() + 1, items.end()),
                                    transform, result))
        {
            return result;
        }
        fail("Failed to lower euclid builtin");
        return -1;
    }

    // Compile a vector literal [a b c] into a MAKE_LIST instruction.
    // This avoids the tree-walker fallback that was previously used for
    // vector-as-expression forms.
    int compile_vector_as_list(const std::vector<Value>& elements,
                               const AffineTimeTransform& transform)
    {
        std::vector<int> element_regs;
        for (const Value& elem : elements)
        {
            const int reg = compile_expr(elem, transform);
            if (reg < 0)
            {
                return -1;
            }
            element_regs.push_back(reg);
        }

        const int first_reg = allocate_argument_window(element_regs);
        const int dst = allocate_register();
        NumericVmInstruction insn;
        insn.opcode = NumericVmOpcode::MAKE_LIST;
        insn.rd = static_cast<uint16_t>(dst);
        insn.rs1 = static_cast<uint16_t>(first_reg >= 0 ? first_reg : 0);
        insn.rs2 = static_cast<uint16_t>(element_regs.size());
        m_program.instructions.push_back(insn);
        return dst;
    }

    int compile_list_constructor(const std::vector<Value>& items,
                                 const AffineTimeTransform& transform)
    {
        std::vector<int> element_regs;
        for (size_t i = 1; i < items.size(); ++i)
        {
            const int reg = compile_expr(items[i], transform);
            if (reg < 0)
            {
                return -1;
            }
            element_regs.push_back(reg);
        }

        const int first_reg = allocate_argument_window(element_regs);
        const int dst = allocate_register();
        NumericVmInstruction insn;
        insn.opcode = NumericVmOpcode::MAKE_LIST;
        insn.rd = static_cast<uint16_t>(dst);
        insn.rs1 = static_cast<uint16_t>(first_reg >= 0 ? first_reg : 0);
        insn.rs2 = static_cast<uint16_t>(element_regs.size());
        m_program.instructions.push_back(insn);
        return dst;
    }

    int compile_vector_lookup(const Value& source_expr,
                              const Value& phasor_expr,
                              const AffineTimeTransform& transform,
                              bool interpolate)
    {
        const std::optional<std::vector<double>> values =
            try_resolve_numeric_sequence(source_expr);
        if (!values.has_value())
        {
            return -1;
        }

        const int phasor_reg = compile_expr(phasor_expr, transform);
        if (phasor_reg < 0)
        {
            return -1;
        }

        NumericVmInstruction insn;
        insn.opcode = interpolate ? NumericVmOpcode::VEC_LERP
                                  : NumericVmOpcode::VEC_INDEX;
        insn.rd = static_cast<uint16_t>(phasor_reg);
        insn.rs1 = static_cast<uint16_t>(phasor_reg);
        insn.imm = static_cast<int32_t>(store_data_segment(*values));
        m_program.instructions.push_back(insn);
        return phasor_reg;
    }

    int compile_fold(const std::vector<Value>& items,
                     size_t arg_start,
                     NumericVmOpcode opcode,
                     const AffineTimeTransform& transform,
                     bool allow_single_arg_identity)
    {
        if (items.size() <= arg_start)
        {
            fail("Not enough arguments for numeric VM fold");
            return -1;
        }

        if (items.size() == arg_start + 1)
        {
            if (allow_single_arg_identity)
            {
                return compile_expr(items[arg_start], transform);
            }
            fail("Not enough arguments for numeric VM fold");
            return -1;
        }

        int acc = compile_expr(items[arg_start], transform);
        if (acc < 0)
        {
            return -1;
        }

        for (size_t i = arg_start + 1; i < items.size(); ++i)
        {
            const int rhs = compile_expr(items[i], transform);
            if (rhs < 0)
            {
                return -1;
            }
            acc = emit_binary(opcode, acc, rhs);
        }
        return acc;
    }

    int compile_binary(const std::vector<Value>& items,
                       NumericVmOpcode opcode,
                       const AffineTimeTransform& transform)
    {
        if (items.size() != 3)
        {
            fail("Numeric VM binary builtin expects exactly 2 arguments");
            return -1;
        }

        const int lhs = compile_expr(items[1], transform);
        const int rhs = compile_expr(items[2], transform);
        if (lhs < 0 || rhs < 0)
        {
            return -1;
        }
        return emit_binary(opcode, lhs, rhs);
    }

    int compile_unary(const std::vector<Value>& items,
                      NumericVmOpcode opcode,
                      const AffineTimeTransform& transform)
    {
        if (items.size() != 2)
        {
            fail("Numeric VM unary builtin expects exactly 1 argument");
            return -1;
        }

        const int src = compile_expr(items[1], transform);
        if (src < 0)
        {
            return -1;
        }
        return emit_unary(opcode, src);
    }

    int compile_clamp(const std::vector<Value>& items,
                      const AffineTimeTransform& transform)
    {
        if (items.size() != 4)
        {
            fail("clamp expects exactly 3 arguments");
            return -1;
        }

        const int value = compile_expr(items[1], transform);
        const int low = compile_expr(items[2], transform);
        const int high = compile_expr(items[3], transform);
        if (value < 0 || low < 0 || high < 0)
        {
            return -1;
        }

        NumericVmInstruction insn;
        insn.opcode = NumericVmOpcode::CLAMP;
        insn.rd = static_cast<uint16_t>(value);
        insn.rs1 = static_cast<uint16_t>(value);
        insn.rs2 = static_cast<uint16_t>(low);
        insn.rs3 = static_cast<uint16_t>(high);
        m_program.instructions.push_back(insn);
        return value;
    }

    // Compile (b->u x) as x * scale + offset, or (u->b x) similarly.
    // Avoids tree-walker fallback for these very common range conversions.
    int compile_range_conversion(const std::vector<Value>& items,
                                 const AffineTimeTransform& transform,
                                 double scale, double offset)
    {
        if (items.size() != 2)
        {
            fail("Range conversion expects exactly 1 argument");
            return -1;
        }

        const int src = compile_expr(items[1], transform);
        if (src < 0)
        {
            return -1;
        }

        const int scale_reg = emit_const(Value(scale));
        const int result = emit_binary(NumericVmOpcode::MUL, src, scale_reg);
        const int offset_reg = emit_const(Value(offset));
        return emit_binary(NumericVmOpcode::ADD, result, offset_reg);
    }

    // Compile (scale out_min out_max phasor) — 3-arg form — as
    //   out_min + (out_max - out_min) * phasor
    // The 5-arg form (scale in_min in_max out_min out_max phasor) is also
    // supported:
    //   out_min + (out_max - out_min) * ((phasor - in_min) / (in_max - in_min))
    int compile_scale_intrinsic(const std::vector<Value>& items,
                                const AffineTimeTransform& transform)
    {
        if (items.size() == 4)
        {
            // 3-arg: (scale out_min out_max phasor)
            const int out_min = compile_expr(items[1], transform);
            const int out_max = compile_expr(items[2], transform);
            const int phasor = compile_expr(items[3], transform);
            if (out_min < 0 || out_max < 0 || phasor < 0)
            {
                return -1;
            }

            // range = out_max - out_min
            const int range = emit_binary(NumericVmOpcode::SUB, out_max, out_min);
            // scaled = range * phasor
            const int scaled = emit_binary(NumericVmOpcode::MUL, range, phasor);
            // result = out_min + scaled
            return emit_binary(NumericVmOpcode::ADD, out_min, scaled);
        }
        if (items.size() == 6)
        {
            // 5-arg: (scale in_min in_max out_min out_max phasor)
            const int in_min = compile_expr(items[1], transform);
            const int in_max = compile_expr(items[2], transform);
            const int out_min = compile_expr(items[3], transform);
            const int out_max = compile_expr(items[4], transform);
            const int phasor = compile_expr(items[5], transform);
            if (in_min < 0 || in_max < 0 || out_min < 0 || out_max < 0 || phasor < 0)
            {
                return -1;
            }

            // norm = (phasor - in_min) / (in_max - in_min)
            const int num = emit_binary(NumericVmOpcode::SUB, phasor, in_min);
            const int den = emit_binary(NumericVmOpcode::SUB, in_max, in_min);
            const int norm = emit_binary(NumericVmOpcode::DIV, num, den);
            // out_range = out_max - out_min
            const int out_range = emit_binary(NumericVmOpcode::SUB, out_max, out_min);
            // scaled = out_range * norm
            const int scaled = emit_binary(NumericVmOpcode::MUL, out_range, norm);
            // result = out_min + scaled
            return emit_binary(NumericVmOpcode::ADD, out_min, scaled);
        }

        fail("scale expects 3 or 5 arguments");
        return -1;
    }

    // Compile (lerp a b t lo hi) — the ModuLisp lerp takes 5 args.
    // The actual computation is lerp(a, b, t) = a + t * (b - a)
    // (lo and hi seem to be unused based on the builtin source, which
    // computes lerp(args[0], args[1], args[0]) — likely a bug in the
    // original builtin, but we match the declared arity.)
    // For the VM, we compile the standard 3-arg lerp semantics using
    // the first 3 arguments: a + t * (b - a).
    // Falls back to intrinsic call if arity doesn't match.
    int compile_lerp_intrinsic(const std::vector<Value>& items,
                               const AffineTimeTransform& transform)
    {
        // Accept both 3-arg (lerp a b t) and 5-arg forms
        if (items.size() != 4 && items.size() != 6)
        {
            fail("lerp expects 3 or 5 arguments");
            return -1;
        }

        const int a = compile_expr(items[1], transform);
        const int b = compile_expr(items[2], transform);
        const int t = compile_expr(items[3], transform);
        if (a < 0 || b < 0 || t < 0)
        {
            return -1;
        }

        // Compile remaining args but discard them (for arity compat)
        for (size_t i = 4; i < items.size(); ++i)
        {
            if (compile_expr(items[i], transform) < 0)
            {
                return -1;
            }
        }

        // lerp(a, b, t) = a + t * (b - a)
        const int diff = emit_binary(NumericVmOpcode::SUB, b, a);
        const int scaled = emit_binary(NumericVmOpcode::MUL, t, diff);
        return emit_binary(NumericVmOpcode::ADD, a, scaled);
    }

    int emit_const(const Value& value)
    {
        const int dst = allocate_register();
        NumericVmInstruction insn;
        insn.opcode = NumericVmOpcode::LOAD_CONST;
        insn.rd = static_cast<uint16_t>(dst);
        insn.imm = static_cast<int32_t>(store_constant(value));
        m_program.instructions.push_back(insn);
        return dst;
    }

    int emit_late_bound_symbol(const String& symbol,
                               const AffineTimeTransform& transform)
    {
        maybe_add_dependency(symbol);
        const size_t intrinsic_index = store_intrinsic(
            [env = &m_env, symbol](const std::vector<Value>&,
                                   const TemporalContext& ctx) -> Value {
                TemporalContext exec_ctx = ctx;
                Environment exec_env;
                exec_env.set_parent_scope(const_cast<Environment*>(env));
                exec_env.set_temporal_context(&exec_ctx);

                if (const std::optional<Value> expr = env->get_expr(symbol))
                {
                    Value mutable_expr = *expr;
                    return ModuLispInterpreter::eval_in(mutable_expr, exec_env);
                }
                if (const std::optional<Value> value = env->get(symbol))
                {
                    return *value;
                }
                return Value::error();
            });

        const int dst = allocate_register();
        NumericVmInstruction insn;
        insn.opcode = NumericVmOpcode::CALL_INTRINSIC;
        insn.rd = static_cast<uint16_t>(dst);
        insn.rs1 = 0;
        insn.rs2 = 0;
        insn.imm = static_cast<int32_t>(intrinsic_index);
        (void)transform;
        m_program.instructions.push_back(insn);
        return dst;
    }

    int emit_runtime_eval(const Value& expr, const AffineTimeTransform& transform)
    {
        if (m_signal_context)
        {
            if (const std::optional<String> side_effect =
                    find_signal_side_effect_form(expr))
            {
                if (*side_effect == "eval")
                {
                    fail("Signal VM rejects eval in signal context");
                }
                else
                {
                    fail("Signal VM rejects side-effectful form: " + *side_effect);
                }
                return -1;
            }
        }

        record_dependencies(expr);

        const std::vector<std::pair<String, int>> local_values =
            collect_active_local_values();
        const std::vector<std::pair<String, Value>> local_callables =
            collect_active_local_callables();

        std::vector<int> arg_regs;
        arg_regs.reserve(local_values.size());
        for (const auto& binding : local_values)
        {
            arg_regs.push_back(binding.second);
        }
        const int first_arg_reg = allocate_argument_window(arg_regs);

        const size_t intrinsic_index = store_intrinsic(
            [env = &m_env, runtime_expr = expr, transform, local_values,
             local_callables](const std::vector<Value>& args,
                              const TemporalContext& ctx) -> Value {
                TemporalContext exec_ctx = apply_time_transform(ctx, transform);
                Environment exec_env;
                exec_env.set_parent_scope(const_cast<Environment*>(env));
                exec_env.set_temporal_context(&exec_ctx);

                for (size_t i = 0; i < local_values.size() && i < args.size(); ++i)
                {
                    exec_env.set(local_values[i].first, args[i]);
                }

                for (const auto& callable : local_callables)
                {
                    Value callable_expr = callable.second;
                    Value callable_value =
                        ModuLispInterpreter::eval_in(callable_expr, exec_env);
                    if (callable_value.is_error())
                    {
                        return callable_value;
                    }
                    exec_env.set(callable.first, callable_value);
                }

                Value mutable_expr = runtime_expr;
                return ModuLispInterpreter::eval_in(mutable_expr, exec_env);
            });

        const int dst = allocate_register();
        NumericVmInstruction insn;
        insn.opcode = NumericVmOpcode::CALL_INTRINSIC;
        insn.rd = static_cast<uint16_t>(dst);
        insn.rs1 = static_cast<uint16_t>(first_arg_reg >= 0 ? first_arg_reg : 0);
        insn.rs2 = static_cast<uint16_t>(local_values.size());
        insn.imm = static_cast<int32_t>(intrinsic_index);
        m_program.instructions.push_back(insn);
        return dst;
    }

    int emit_mov(int dst, int src)
    {
        NumericVmInstruction insn;
        insn.opcode = NumericVmOpcode::MOV;
        insn.rd = static_cast<uint16_t>(dst);
        insn.rs1 = static_cast<uint16_t>(src);
        m_program.instructions.push_back(insn);
        return dst;
    }

    void patch_branch(size_t instruction_index, size_t target_index)
    {
        NumericVmInstruction& insn = m_program.instructions[instruction_index];
        insn.imm = static_cast<int32_t>(target_index) -
                   static_cast<int32_t>(instruction_index + 1);
    }

    int emit_unary(NumericVmOpcode opcode, int src)
    {
        NumericVmInstruction insn;
        insn.opcode = opcode;
        insn.rd = static_cast<uint16_t>(src);
        insn.rs1 = static_cast<uint16_t>(src);
        m_program.instructions.push_back(insn);
        return src;
    }

    int emit_binary(NumericVmOpcode opcode, int lhs, int rhs)
    {
        NumericVmInstruction insn;
        insn.opcode = opcode;
        insn.rd = static_cast<uint16_t>(lhs);
        insn.rs1 = static_cast<uint16_t>(lhs);
        insn.rs2 = static_cast<uint16_t>(rhs);
        m_program.instructions.push_back(insn);
        return lhs;
    }

    int allocate_register()
    {
        return m_next_register++;
    }

    size_t store_intrinsic(const TaggedVmIntrinsic& intrinsic)
    {
        m_program.intrinsics.push_back(intrinsic);
        return m_program.intrinsics.size() - 1;
    }

    size_t store_constant(const Value& value)
    {
        for (size_t i = 0; i < m_program.constants.size(); ++i)
        {
            if (m_program.constants[i] == value)
            {
                return i;
            }
        }

        m_program.constants.push_back(value);
        return m_program.constants.size() - 1;
    }

    size_t store_data_segment(const std::vector<double>& values)
    {
        for (size_t i = 0; i < m_program.data_segments.size(); ++i)
        {
            if (m_program.data_segments[i] == values)
            {
                return i;
            }
        }

        m_program.data_segments.push_back(values);
        return m_program.data_segments.size() - 1;
    }

    void add_dependency(const String& symbol)
    {
        for (const auto& existing : m_program.dependencies)
        {
            if (existing == symbol)
            {
                return;
            }
        }
        m_program.dependencies.push_back(symbol);
    }

    void maybe_add_dependency(const String& symbol)
    {
        if (find_local_value_binding(symbol).has_value() ||
            find_local_callable_binding(symbol).has_value())
        {
            return;
        }
        if (m_env.get_defs().has(symbol) || m_env.get_def_exprs().has(symbol))
        {
            add_dependency(symbol);
        }
    }

    void record_dependencies(const Value& expr)
    {
        if (expr.is_symbol())
        {
            const String symbol = expr.as_atom();
            if (try_get_temporal_channel(symbol, m_unused_channel))
            {
                return;
            }

            if (m_env.get_expr(symbol).has_value() || m_env.get(symbol).has_value())
            {
                maybe_add_dependency(symbol);
            }
            return;
        }

        if (expr.is_list())
        {
            for (const Value& item : expr.as_list())
            {
                record_dependencies(item);
            }
            return;
        }

        if (expr.is_vector())
        {
            for (const Value& item : expr.as_vector())
            {
                record_dependencies(item);
            }
        }
    }

    std::optional<double> try_resolve_numeric_constant(const Value& expr)
    {
        if (expr.is_number())
        {
            return expr.as_float();
        }

        if (expr.is_symbol())
        {
            const String symbol = expr.as_atom();
            if (try_get_temporal_channel(symbol, m_unused_channel))
            {
                return std::nullopt;
            }
            if (is_in_recursion_stack(symbol))
            {
                return std::nullopt;
            }
            if (const std::optional<Value> nested = m_env.get_expr(symbol))
            {
                m_recursion_stack.push_back(symbol);
                const std::optional<double> value =
                    try_resolve_numeric_constant(*nested);
                m_recursion_stack.pop_back();
                return value;
            }
            if (m_env.has(symbol))
            {
                const std::optional<Value> value = m_env.get(symbol);
                if (value->is_number())
                {
                    return value->as_float();
                }
            }
            return std::nullopt;
        }

        if (!expr.is_list())
        {
            return std::nullopt;
        }

        const std::vector<Value> items = expr.as_list();
        if (items.empty() || !items[0].is_symbol())
        {
            return std::nullopt;
        }

        const String op = items[0].as_atom();
        auto unary = [&](const std::function<double(double)>& fn)
            -> std::optional<double> {
            if (items.size() != 2)
            {
                return std::nullopt;
            }
            const std::optional<double> arg = try_resolve_numeric_constant(items[1]);
            if (!arg.has_value())
            {
                return std::nullopt;
            }
            return fn(*arg);
        };
        auto binary = [&](const std::function<double(double, double)>& fn)
            -> std::optional<double> {
            if (items.size() != 3)
            {
                return std::nullopt;
            }
            const std::optional<double> lhs = try_resolve_numeric_constant(items[1]);
            const std::optional<double> rhs = try_resolve_numeric_constant(items[2]);
            if (!lhs.has_value() || !rhs.has_value())
            {
                return std::nullopt;
            }
            return fn(*lhs, *rhs);
        };

        if (op == "+")
        {
            double total = 0.0;
            for (size_t i = 1; i < items.size(); ++i)
            {
                const std::optional<double> value =
                    try_resolve_numeric_constant(items[i]);
                if (!value.has_value())
                {
                    return std::nullopt;
                }
                total += *value;
            }
            return total;
        }
        if (op == "*")
        {
            double total = 1.0;
            for (size_t i = 1; i < items.size(); ++i)
            {
                const std::optional<double> value =
                    try_resolve_numeric_constant(items[i]);
                if (!value.has_value())
                {
                    return std::nullopt;
                }
                total *= *value;
            }
            return total;
        }
        if (op == "-")
        {
            if (items.size() == 2)
            {
                return unary([](double value) { return -value; });
            }
            const std::optional<double> first = try_resolve_numeric_constant(items[1]);
            if (!first.has_value())
            {
                return std::nullopt;
            }

            double total = *first;
            for (size_t i = 2; i < items.size(); ++i)
            {
                const std::optional<double> value =
                    try_resolve_numeric_constant(items[i]);
                if (!value.has_value())
                {
                    return std::nullopt;
                }
                total -= *value;
            }
            return total;
        }
        if (op == "/")
        {
            return binary([](double lhs, double rhs) { return lhs / rhs; });
        }
        if (op == "%")
        {
            return binary([](double lhs, double rhs) { return std::fmod(lhs, rhs); });
        }
        if (op == ">")
        {
            return binary([](double lhs, double rhs) { return lhs > rhs ? 1.0 : 0.0; });
        }
        if (op == "<")
        {
            return binary([](double lhs, double rhs) { return lhs < rhs ? 1.0 : 0.0; });
        }
        if (op == ">=")
        {
            return binary([](double lhs, double rhs) { return lhs >= rhs ? 1.0 : 0.0; });
        }
        if (op == "<=")
        {
            return binary([](double lhs, double rhs) { return lhs <= rhs ? 1.0 : 0.0; });
        }
        if (op == "=")
        {
            return binary([](double lhs, double rhs) { return lhs == rhs ? 1.0 : 0.0; });
        }
        if (op == "min")
        {
            return binary([](double lhs, double rhs) { return std::fmin(lhs, rhs); });
        }
        if (op == "max")
        {
            return binary([](double lhs, double rhs) { return std::fmax(lhs, rhs); });
        }
        if (op == "pow")
        {
            return binary([](double lhs, double rhs) { return std::pow(lhs, rhs); });
        }
        if (op == "sin")
        {
            return unary([](double value) { return std::sin(value); });
        }
        if (op == "cos")
        {
            return unary([](double value) { return std::cos(value); });
        }
        if (op == "tan")
        {
            return unary([](double value) { return std::tan(value); });
        }
        if (op == "usin" || op == "u-sin")
        {
            return unary([](double x) { return (std::sin(x * 2.0 * M_PI) + 1.0) / 2.0; });
        }
        if (op == "ucos" || op == "u-cos")
        {
            return unary([](double x) { return (std::cos(x * 2.0 * M_PI) + 1.0) / 2.0; });
        }
        if (op == "usinbi" || op == "u-sin-bi")
        {
            return unary([](double x) { return std::sin(x * 2.0 * M_PI); });
        }
        if (op == "ucosbi" || op == "u-cos-bi")
        {
            return unary([](double x) { return std::cos(x * 2.0 * M_PI); });
        }
        if (op == "tri")
        {
            return unary([](double x) { return 1.0 - std::fabs(2.0 * x - 1.0); });
        }
        if (op == "sqr")
        {
            return unary([](double x) { return x < 0.5 ? 1.0 : 0.0; });
        }
        if (op == "pulse")
        {
            if (items.size() != 3)
            {
                return std::nullopt;
            }
            const std::optional<double> phasor =
                try_resolve_numeric_constant(items[1]);
            const std::optional<double> width =
                try_resolve_numeric_constant(items[2]);
            if (phasor && width)
            {
                return *phasor < *width ? 1.0 : 0.0;
            }
            return std::nullopt;
        }
        if (op == "input")
        {
            return 0.0;
        }
        if (op == "floor")
        {
            return unary([](double value) { return std::floor(value); });
        }
        if (op == "ceil")
        {
            return unary([](double value) { return std::ceil(value); });
        }
        if (op == "sqrt")
        {
            return unary([](double value) { return std::sqrt(value); });
        }
        if (op == "abs")
        {
            return unary([](double value) { return std::fabs(value); });
        }
        if (op == "frac")
        {
            return unary([](double value) { return value - std::floor(value); });
        }
        if (op == "clamp")
        {
            if (items.size() != 4)
            {
                return std::nullopt;
            }
            const std::optional<double> value =
                try_resolve_numeric_constant(items[1]);
            const std::optional<double> low =
                try_resolve_numeric_constant(items[2]);
            const std::optional<double> high =
                try_resolve_numeric_constant(items[3]);
            if (!value.has_value() || !low.has_value() || !high.has_value())
            {
                return std::nullopt;
            }
            return std::fmin(std::fmax(*value, *low), *high);
        }

        return std::nullopt;
    }

    std::optional<std::vector<double>> try_resolve_numeric_sequence(
        const Value& expr)
    {
        if (expr.is_vector())
        {
            const std::vector<Value> items = expr.as_vector();
            if (items.empty())
            {
                return std::nullopt;
            }

            std::vector<double> values;
            values.reserve(items.size());
            for (const Value& item : items)
            {
                const std::optional<double> numeric = try_resolve_numeric_constant(item);
                if (!numeric.has_value())
                {
                    return std::nullopt;
                }
                values.push_back(*numeric);
            }
            return values;
        }

        if (expr.is_symbol())
        {
            const String symbol = expr.as_atom();
            if (find_local_value_binding(symbol).has_value())
            {
                return std::nullopt;
            }
            if (is_in_recursion_stack(symbol))
            {
                return std::nullopt;
            }

            if (const std::optional<Value> nested = m_env.get_expr(symbol))
            {
                maybe_add_dependency(symbol);
                m_recursion_stack.push_back(symbol);
                const std::optional<std::vector<double>> values =
                    try_resolve_numeric_sequence(*nested);
                m_recursion_stack.pop_back();
                return values;
            }

            if (const std::optional<Value> value = m_env.get(symbol))
            {
                maybe_add_dependency(symbol);
                return try_resolve_numeric_sequence(*value);
            }

            return std::nullopt;
        }

        if (!expr.is_sequential())
        {
            return std::nullopt;
        }

        const std::vector<Value> items = expr.as_sequential();
        if (items.empty())
        {
            return std::nullopt;
        }

        std::vector<double> values;
        values.reserve(items.size());
        for (const Value& item : items)
        {
            const std::optional<double> numeric = try_resolve_numeric_constant(item);
            if (!numeric.has_value())
            {
                return std::nullopt;
            }
            values.push_back(*numeric);
        }
        return values;
    }

    bool try_get_temporal_channel(const String& symbol,
                                  NumericVmTemporalChannel& channel) const
    {
        if (symbol == "t")
        {
            channel = NumericVmTemporalChannel::T;
            return true;
        }
        if (symbol == "time")
        {
            channel = NumericVmTemporalChannel::TIME;
            return true;
        }
        if (symbol == "beat")
        {
            channel = NumericVmTemporalChannel::BEAT;
            return true;
        }
        if (symbol == "bar")
        {
            channel = NumericVmTemporalChannel::BAR;
            return true;
        }
        if (symbol == "phrase")
        {
            channel = NumericVmTemporalChannel::PHRASE;
            return true;
        }
        if (symbol == "section")
        {
            channel = NumericVmTemporalChannel::SECTION;
            return true;
        }
        if (symbol == "beat-num")
        {
            channel = NumericVmTemporalChannel::BEAT_NUM;
            return true;
        }
        if (symbol == "bar-num")
        {
            channel = NumericVmTemporalChannel::BAR_NUM;
            return true;
        }
        return false;
    }

    bool parse_lambda_value(const Value& value,
                            std::vector<Value>& params,
                            Value& body) const
    {
        if (value.type == Value::LAMBDA)
        {
            params = value.list[0].as_vector();
            body = value.list[1];
            return true;
        }

        if (!value.is_list())
        {
            return false;
        }

        const std::vector<Value> items = value.as_list();
        if (items.empty() || !items[0].is_symbol())
        {
            return false;
        }
        const String op = items[0].as_atom();
        if (op != "lambda")
        {
            return false;
        }
        if (items.size() < 3 || !items[1].is_vector())
        {
            return false;
        }

        params = items[1].as_vector();
        if (items.size() == 3)
        {
            body = items[2];
        }
        else
        {
            std::vector<Value> body_items;
            body_items.push_back(Value::atom("do"));
            for (size_t i = 2; i < items.size(); ++i)
            {
                body_items.push_back(items[i]);
            }
            body = Value(body_items);
        }
        return true;
    }

    bool try_bind_local_callable(const String& name, const Value& expr)
    {
        std::vector<Value> params;
        Value body;
        if (parse_lambda_value(expr, params, body))
        {
            bind_local_callable(name, expr);
            return true;
        }

        if (expr.is_symbol())
        {
            const String symbol = expr.as_atom();
            if (const std::optional<Value> value = m_env.get(symbol))
            {
                if (value->type == Value::LAMBDA)
                {
                    maybe_add_dependency(symbol);
                    bind_local_callable(name, *value);
                    return true;
                }
            }
        }

        return false;
    }

    bool try_compile_callable_form(const std::vector<Value>& items,
                                   const AffineTimeTransform& transform,
                                   int& out_reg)
    {
        if (items.empty())
        {
            fail("Cannot compile empty call form");
            return false;
        }

        std::vector<Value> params;
        Value body;
        if (items[0].is_symbol())
        {
            const String op = items[0].as_atom();
            if (const std::optional<Value> local_callable =
                    find_local_callable_binding(op))
            {
                if (lambda_has_captured_scope(*local_callable))
                {
                    // CLOSURE FALLBACK: local callable has captured scope.
                    // Falls back to tree-walker because the VM cannot yet
                    // pass captured variables as extra CALL arguments.
                    ++s_closure_fallback_count;
                    out_reg = emit_runtime_eval(Value(items), transform);
                    return out_reg >= 0;
                }
                if (parse_lambda_value(*local_callable, params, body))
                {
                    out_reg = compile_inline_lambda(params, body,
                                                    std::vector<Value>(items.begin() + 1,
                                                                       items.end()),
                                                    transform);
                    return out_reg >= 0;
                }
            }
            if (const std::optional<Value> global_callable = m_env.get(op))
            {
                if (lambda_has_captured_scope(*global_callable))
                {
                    // CLOSURE FALLBACK: global callable has captured scope.
                    // Falls back to tree-walker because the VM cannot yet
                    // pass captured variables as extra CALL arguments.
                    ++s_closure_fallback_count;
                    maybe_add_dependency(op);
                    out_reg = emit_runtime_eval(Value(items), transform);
                    return out_reg >= 0;
                }
                if (parse_lambda_value(*global_callable, params, body))
                {
                    maybe_add_dependency(op);
                    out_reg = compile_inline_lambda(params, body,
                                                    std::vector<Value>(items.begin() + 1,
                                                                       items.end()),
                                                    transform);
                    return out_reg >= 0;
                }
                if (global_callable->is_builtin())
                {
                    return try_emit_intrinsic_call(op,
                                                   std::vector<Value>(items.begin() + 1,
                                                                      items.end()),
                                                   transform, out_reg);
                }
            }
        }

        if (parse_lambda_value(items[0], params, body))
        {
            out_reg = compile_inline_lambda(params, body,
                                            std::vector<Value>(items.begin() + 1,
                                                               items.end()),
                                            transform);
            return out_reg >= 0;
        }

        return false;
    }

    int compile_inline_lambda(const std::vector<Value>& params,
                              const Value& body,
                              const std::vector<Value>& arg_exprs,
                              const AffineTimeTransform& transform)
    {
        if (params.size() != arg_exprs.size())
        {
            fail("Inline lambda call arity mismatch");
            return -1;
        }

        std::vector<int> arg_regs;
        arg_regs.reserve(arg_exprs.size());
        for (const Value& arg_expr : arg_exprs)
        {
            const int arg_reg = compile_expr(arg_expr, transform);
            if (arg_reg < 0)
            {
                return -1;
            }
            arg_regs.push_back(arg_reg);
        }

        push_local_scope();
        for (size_t i = 0; i < params.size(); ++i)
        {
            if (!params[i].is_symbol())
            {
                pop_local_scope();
                fail("Lambda parameters must be symbols");
                return -1;
            }
            bind_local_value(params[i].as_atom(), arg_regs[i]);
        }

        const int result = compile_expr(body, transform);
        pop_local_scope();
        return result;
    }

    bool try_emit_intrinsic_call(const String& name,
                                 const std::vector<Value>& arg_exprs,
                                 const AffineTimeTransform& transform,
                                 int& out_reg)
    {
        const std::optional<Value> callable = m_env.get(name);
        if (!callable.has_value() || !callable->is_builtin())
        {
            return false;
        }

        std::vector<int> arg_regs;
        arg_regs.reserve(arg_exprs.size());
        for (const Value& arg_expr : arg_exprs)
        {
            const int arg_reg = compile_expr(arg_expr, transform);
            if (arg_reg < 0)
            {
                return false;
            }
            arg_regs.push_back(arg_reg);
        }

        maybe_add_dependency(name);
        const int first_arg_reg = allocate_argument_window(arg_regs);
        const size_t intrinsic_index = store_intrinsic(
            [callable_value = *callable, env = &m_env](const std::vector<Value>& args,
                                                       const TemporalContext& ctx)
                -> Value {
                Environment exec_env(*env);
                TemporalContext exec_ctx = ctx;
                exec_env.set_temporal_context(&exec_ctx);
                std::vector<Value> call_args = args;
                Value mutable_callable = callable_value;
                return mutable_callable.apply(call_args, exec_env);
            });

        out_reg = allocate_register();
        NumericVmInstruction insn;
        insn.opcode = NumericVmOpcode::CALL_INTRINSIC;
        insn.rd = static_cast<uint16_t>(out_reg);
        insn.rs1 = static_cast<uint16_t>(first_arg_reg >= 0 ? first_arg_reg : 0);
        insn.rs2 = static_cast<uint16_t>(arg_exprs.size());
        insn.imm = static_cast<int32_t>(intrinsic_index);
        m_program.instructions.push_back(insn);
        return true;
    }

    int allocate_argument_window(const std::vector<int>& arg_regs)
    {
        if (arg_regs.empty())
        {
            return -1;
        }

        const int first = allocate_register();
        emit_mov(first, arg_regs[0]);
        for (size_t i = 1; i < arg_regs.size(); ++i)
        {
            const int dst = allocate_register();
            emit_mov(dst, arg_regs[i]);
        }
        return first;
    }

    void push_local_scope()
    {
        m_local_value_scopes.emplace_back();
        m_local_callable_scopes.emplace_back();
        m_cse_scope_stack.push_back(m_cse_cache.size());
    }

    void pop_local_scope()
    {
        if (!m_local_value_scopes.empty())
        {
            m_local_value_scopes.pop_back();
        }
        if (!m_local_callable_scopes.empty())
        {
            m_local_callable_scopes.pop_back();
        }
        // Evict any CSE entries added while the inner scope was active,
        // since they may reference local bindings that are now gone.
        if (!m_cse_scope_stack.empty())
        {
            const size_t target_size = m_cse_scope_stack.back();
            m_cse_scope_stack.pop_back();
            if (m_cse_cache.size() > target_size)
            {
                // Rebuild cache keeping only entries that existed before
                // this scope was pushed. The simple approach: clear all
                // entries added during this scope. Since unordered_map
                // doesn't preserve insertion order, we clear the whole
                // cache when any inner-scope entries exist — this is
                // conservative but correct, and in practice inner scopes
                // are rare (only let/lambda).
                m_cse_cache.clear();
            }
        }
    }

    void bind_local_value(const String& symbol, int source_register)
    {
        if (m_local_value_scopes.empty())
        {
            push_local_scope();
        }
        m_local_value_scopes.back()[symbol] = { source_register };
    }

    void bind_local_callable(const String& symbol, const Value& callable)
    {
        if (m_local_callable_scopes.empty())
        {
            push_local_scope();
        }
        m_local_callable_scopes.back()[symbol] = callable;
    }

    std::optional<int> find_local_value_binding(const String& symbol) const
    {
        for (auto it = m_local_value_scopes.rbegin();
             it != m_local_value_scopes.rend(); ++it)
        {
            const auto found = it->find(symbol);
            if (found != it->end())
            {
                return found->second.source_register;
            }
        }
        return std::nullopt;
    }

    std::optional<Value> find_local_callable_binding(const String& symbol) const
    {
        for (auto it = m_local_callable_scopes.rbegin();
             it != m_local_callable_scopes.rend(); ++it)
        {
            const auto found = it->find(symbol);
            if (found != it->end())
            {
                return found->second;
            }
        }
        return std::nullopt;
    }

    bool is_in_recursion_stack(const String& symbol) const
    {
        for (const auto& active : m_recursion_stack)
        {
            if (active == symbol)
            {
                return true;
            }
        }
        return false;
    }

    // --- CSE pre-scan and signature generation ---

    // Walk the expression tree and record which list-form signatures
    // appear more than once. Only those get cached during compilation.
    void prescan_for_cse(const Value& expr, const AffineTimeTransform& transform)
    {
        if (!expr.is_list())
        {
            return;
        }
        const std::vector<Value> items = expr.as_list();
        if (items.empty())
        {
            return;
        }

        const std::string sig = expr_signature(expr, transform);
        auto it = m_cse_seen_count.find(sig);
        if (it == m_cse_seen_count.end())
        {
            m_cse_seen_count[sig] = 1;
        }
        else
        {
            it->second++;
        }

        // Handle time warps: adjust transform for recursive scan
        if (!items.empty() && items[0].is_symbol())
        {
            const String op = items[0].as_atom();
            if ((op == "fast" || op == "slow" || op == "offset" || op == "shift") &&
                items.size() == 3)
            {
                const std::optional<double> factor =
                    try_resolve_numeric_constant(items[1]);
                if (factor.has_value())
                {
                    AffineTimeTransform next = transform;
                    if (op == "fast")
                    {
                        next.scale *= *factor;
                        next.offset *= *factor;
                    }
                    else if (op == "slow")
                    {
                        next.scale /= *factor;
                        next.offset /= *factor;
                    }
                    else
                    {
                        next.offset += *factor;
                    }
                    prescan_for_cse(items[2], next);
                    return;
                }
            }
        }

        // Recurse into all sub-items with the same transform
        for (const Value& item : items)
        {
            prescan_for_cse(item, transform);
        }
    }

    void append_value_signature(const Value& expr, std::string& sig) const
    {
        if (expr.is_number())
        {
            sig += 'N';
            sig += std::to_string(expr.as_float());
            return;
        }
        if (expr.is_symbol())
        {
            if (find_local_value_binding(expr.as_atom()).has_value())
            {
                sig += 'L';
                sig += expr.as_atom().c_str();
                return;
            }
            sig += 'S';
            sig += expr.as_atom().c_str();
            return;
        }
        if (expr.is_list())
        {
            sig += '(';
            for (const Value& item : expr.as_list())
            {
                append_value_signature(item, sig);
                sig += ' ';
            }
            sig += ')';
            return;
        }
        if (expr.is_vector())
        {
            sig += '[';
            for (const Value& item : expr.as_vector())
            {
                append_value_signature(item, sig);
                sig += ' ';
            }
            sig += ']';
            return;
        }
        // Unique marker for other types — prevents false CSE matches
        sig += '?';
        sig += std::to_string(reinterpret_cast<uintptr_t>(&expr));
    }

    std::string expr_signature(const Value& expr,
                               const AffineTimeTransform& transform) const
    {
        std::string sig;
        sig += std::to_string(transform.scale);
        sig += ',';
        sig += std::to_string(transform.offset);
        sig += ':';
        append_value_signature(expr, sig);
        return sig;
    }

    void fail(const String& error)
    {
        if (m_error.length() == 0)
        {
            m_error = error;
        }
    }

    CompilerCheckpoint checkpoint_state() const
    {
        CompilerCheckpoint checkpoint;
        checkpoint.instruction_count = m_program.instructions.size();
        checkpoint.constant_count = m_program.constants.size();
        checkpoint.data_segment_count = m_program.data_segments.size();
        checkpoint.dependency_count = m_program.dependencies.size();
        checkpoint.function_count = m_program.functions.size();
        checkpoint.intrinsic_count = m_program.intrinsics.size();
        checkpoint.next_register = m_next_register;
        checkpoint.cse_cache_size = m_cse_cache.size();
        return checkpoint;
    }

    void rollback(const CompilerCheckpoint& checkpoint)
    {
        m_program.instructions.resize(checkpoint.instruction_count);
        m_program.constants.resize(checkpoint.constant_count);
        m_program.data_segments.resize(checkpoint.data_segment_count);
        m_program.dependencies.resize(checkpoint.dependency_count);
        m_program.functions.resize(checkpoint.function_count);
        m_program.intrinsics.resize(checkpoint.intrinsic_count);
        m_next_register = checkpoint.next_register;
        // CSE entries added since checkpoint may reference rolled-back
        // registers/instructions — clear them conservatively.
        if (m_cse_cache.size() > checkpoint.cse_cache_size)
        {
            m_cse_cache.clear();
        }
    }

    std::vector<std::pair<String, int>> collect_active_local_values() const
    {
        std::vector<std::pair<String, int>> bindings;
        std::set<String> seen;

        for (auto scope_it = m_local_value_scopes.rbegin();
             scope_it != m_local_value_scopes.rend(); ++scope_it)
        {
            for (const auto& entry : *scope_it)
            {
                if (seen.insert(entry.first).second)
                {
                    bindings.push_back({ entry.first, entry.second.source_register });
                }
            }
        }

        std::reverse(bindings.begin(), bindings.end());
        return bindings;
    }

    std::vector<std::pair<String, Value>> collect_active_local_callables() const
    {
        std::vector<std::pair<String, Value>> bindings;
        std::set<String> seen;

        for (auto scope_it = m_local_callable_scopes.rbegin();
             scope_it != m_local_callable_scopes.rend(); ++scope_it)
        {
            for (const auto& entry : *scope_it)
            {
                if (seen.insert(entry.first).second)
                {
                    bindings.push_back(entry);
                }
            }
        }

        std::reverse(bindings.begin(), bindings.end());
        return bindings;
    }

    const Environment& m_env;
    bool m_signal_context = false;
    NumericVmProgram m_program;
    int m_next_register = 0;
    String m_error;
    std::vector<String> m_recursion_stack;
    std::vector<std::map<String, LocalValueBinding>> m_local_value_scopes;
    std::vector<std::map<String, Value>> m_local_callable_scopes;
    mutable NumericVmTemporalChannel m_unused_channel = NumericVmTemporalChannel::T;

    // Pre-scan: how many times each list-expression signature appears.
    // Only signatures with count > 1 are eligible for CSE caching.
    std::unordered_map<std::string, int> m_cse_seen_count;
    // CSE cache: maps expression signature → preserved register containing
    // the result. The preserved register is never returned to callers
    // (a copy is returned instead), so it cannot be overwritten by
    // subsequent in-place operations like emit_unary/emit_binary.
    std::unordered_map<std::string, int> m_cse_cache;
    // Saved CSE cache sizes at each scope boundary, used to invalidate
    // entries that reference locals from inner scopes.
    std::vector<size_t> m_cse_scope_stack;
};

double wrap_phase(double time_seconds, double duration_seconds)
{
    if (!(duration_seconds > 0.0))
    {
        return 0.0;
    }
    double phase = std::fmod(time_seconds, duration_seconds);
    if (phase < 0.0)
    {
        phase += duration_seconds;
    }
    return phase / duration_seconds;
}

double time_to_count(double time_seconds, double duration_seconds)
{
    if (!(duration_seconds > 0.0))
    {
        return 0.0;
    }
    return std::floor(time_seconds / duration_seconds);
}

TemporalContext apply_time_transform(const TemporalContext& ctx,
                                     const AffineTimeTransform& transform)
{
    TemporalContext adjusted = ctx;
    const double warped_t = (ctx.t * transform.scale) + transform.offset;
    adjusted.t = warped_t;
    adjusted.beat = wrap_phase(warped_t, ctx.beatDur);
    adjusted.bar = wrap_phase(warped_t, ctx.barDur);
    adjusted.phrase = wrap_phase(warped_t, ctx.phraseDur);
    adjusted.section = wrap_phase(warped_t, ctx.sectionDur);
    adjusted.beatNum = static_cast<int>(time_to_count(warped_t, ctx.beatDur));
    adjusted.barNum = static_cast<int>(time_to_count(warped_t, ctx.barDur));
    return adjusted;
}

bool is_truthy(const Value& value)
{
    if (value.is_nil())
    {
        return false;
    }
    if (value.is_number())
    {
        return value.as_float() != 0.0;
    }
    return true;
}

bool validate_register_index(uint16_t index, size_t register_count, String& error,
                             const char* role)
{
    if (static_cast<size_t>(index) >= register_count)
    {
        error = String("Numeric VM ") + role + " register out of range";
        return false;
    }
    return true;
}

bool resolve_branch_target(size_t pc, int32_t offset, size_t instruction_count,
                           size_t& target_pc, String& error)
{
    const int64_t target = static_cast<int64_t>(pc) + 1 + static_cast<int64_t>(offset);
    if (target < 0 || target >= static_cast<int64_t>(instruction_count))
    {
        error = "Numeric VM branch target out of range";
        return false;
    }

    target_pc = static_cast<size_t>(target);
    return true;
}

bool load_data_segment(const NumericVmProgram& program, int32_t index,
                       const std::vector<double>*& data, String& error)
{
    if (index < 0 || static_cast<size_t>(index) >= program.data_segments.size())
    {
        error = "Numeric VM data segment index out of range";
        return false;
    }
    data = &program.data_segments[static_cast<size_t>(index)];
    return true;
}

bool load_phasor_value(const std::vector<Value>& registers, uint16_t index,
                       double& phasor, String& error)
{
    if (static_cast<size_t>(index) >= registers.size())
    {
        error = "Numeric VM source register out of range";
        return false;
    }

    const Value& value = registers[index];
    if (!value.is_number())
    {
        error = "Numeric VM vector opcode requires a numeric phasor";
        return false;
    }

    phasor = value.as_float();
    return true;
}
TaggedVmExecutionResult execute_tagged_program_impl(const NumericVmProgram& program,
                                                    const TemporalContext& ctx,
                                                    const std::vector<Value>& initial_registers =
                                                        {})
{
    TaggedVmExecutionResult result;
    if (program.register_count == 0 || program.instructions.empty())
    {
        result.error = "Numeric VM program is empty";
        return result;
    }

    std::vector<Value> registers(program.register_count, Value::nil());
    for (size_t i = 0; i < initial_registers.size() && i < registers.size(); ++i)
    {
        registers[i] = initial_registers[i];
    }
// Computed-goto dispatch: enabled for GCC/Clang on non-WASM builds when
// USE_COMPUTED_GOTO is defined.  Falls back to a normal switch otherwise.
#if defined(__GNUC__) && !defined(WASM_BUILD) && defined(USE_COMPUTED_GOTO)
#define VM_USE_COMPUTED_GOTO 1
#else
#define VM_USE_COMPUTED_GOTO 0
#endif

#if VM_USE_COMPUTED_GOTO
    // 53 opcodes: LOAD_CONST(0) through LOAD_INPUT(52)
    static const void* dispatch_table[] = {
        &&op_LOAD_CONST,    &&op_LOAD_TIME,     &&op_MOV,
        &&op_VEC_INDEX,     &&op_VEC_LERP,      &&op_ADD,
        &&op_SUB,           &&op_MUL,           &&op_DIV,
        &&op_MOD,           &&op_NEG,           &&op_CMP_GT,
        &&op_CMP_LT,       &&op_CMP_GE,       &&op_CMP_LE,
        &&op_CMP_EQ,       &&op_FLOOR,         &&op_CEIL,
        &&op_FRAC,          &&op_ABS,           &&op_MIN,
        &&op_MAX,           &&op_POW,           &&op_SQRT,
        &&op_CLAMP,         &&op_SIN,           &&op_COS,
        &&op_TAN,           &&op_BRANCH,        &&op_BRANCH_IF,
        &&op_BRANCH_UNLESS, &&op_CALL,          &&op_CALL_INTRINSIC,
        &&op_RET,           &&op_IS_NIL,        &&op_IS_NUMBER,
        &&op_IS_LIST,       &&op_IS_STRING,     &&op_NOT,
        &&op_AND,           &&op_OR,            &&op_MAKE_LIST,
        &&op_LIST_HEAD,     &&op_LIST_TAIL,     &&op_LIST_LENGTH,
        &&op_U_SIN,         &&op_U_COS,         &&op_U_SIN_BI,
        &&op_U_COS_BI,      &&op_TRI,           &&op_SQR,
        &&op_PULSE,         &&op_LOAD_INPUT
    };
    #define VM_DISPATCH() do { \
        if (pc >= program.instructions.size()) goto vm_loop_exit; \
        insn = program.instructions[pc]; \
        goto *dispatch_table[static_cast<int>(insn.opcode)]; \
    } while (0)
    #define VM_CASE(op) op_##op:
    #define VM_NEXT() do { ++pc; VM_DISPATCH(); } while (0)
    #define VM_BREAK VM_NEXT()
    #define VM_BRANCH_DONE() VM_DISPATCH()
#else
    // Note: VM_NEXT and VM_BRANCH_DONE use bare braces (not do-while)
    // because continue/break inside do{...}while(0) would target the
    // do-while loop instead of the enclosing for loop.
    #define VM_DISPATCH() continue
    #define VM_CASE(op) case NumericVmOpcode::op:
    #define VM_NEXT() { ++pc; continue; }
    #define VM_BREAK break
    #define VM_BRANCH_DONE() break
#endif

    for (size_t pc = 0; pc < program.instructions.size();)
    {
#if VM_USE_COMPUTED_GOTO
        NumericVmInstruction insn{};
        VM_DISPATCH();
#else
        const NumericVmInstruction& insn = program.instructions[pc];
        switch (insn.opcode)
        {
#endif
        VM_CASE(LOAD_CONST)
            if (insn.imm < 0 ||
                static_cast<size_t>(insn.imm) >= program.constants.size())
            {
                result.error = "Numeric VM constant index out of range";
                return result;
            }
            if (!validate_register_index(insn.rd, registers.size(), result.error,
                                         "destination"))
            {
                return result;
            }
            registers[insn.rd] = program.constants[static_cast<size_t>(insn.imm)];
            VM_NEXT();
        VM_CASE(LOAD_TIME)
        {
            if (!validate_register_index(insn.rd, registers.size(), result.error,
                                         "destination"))
            {
                return result;
            }

            const NumericVmTemporalChannel channel =
                static_cast<NumericVmTemporalChannel>(insn.imm);
            const double warped_t = (ctx.t * insn.imm0) + insn.imm1;
            double value = 0.0;
            switch (channel)
            {
            case NumericVmTemporalChannel::T:
            case NumericVmTemporalChannel::TIME:
                value = warped_t;
                break;
            case NumericVmTemporalChannel::BEAT:
                value = wrap_phase(warped_t, ctx.beatDur);
                break;
            case NumericVmTemporalChannel::BAR:
                value = wrap_phase(warped_t, ctx.barDur);
                break;
            case NumericVmTemporalChannel::PHRASE:
                value = wrap_phase(warped_t, ctx.phraseDur);
                break;
            case NumericVmTemporalChannel::SECTION:
                value = wrap_phase(warped_t, ctx.sectionDur);
                break;
            case NumericVmTemporalChannel::BEAT_NUM:
                value = time_to_count(warped_t, ctx.beatDur);
                break;
            case NumericVmTemporalChannel::BAR_NUM:
                value = time_to_count(warped_t, ctx.barDur);
                break;
            }
            registers[insn.rd] = Value(value);
            VM_NEXT();
        }
        VM_CASE(LOAD_INPUT)
        {
            if (!validate_register_index(insn.rd, registers.size(), result.error,
                                         "destination"))
            {
                return result;
            }
            // Stub: no physical inputs in standalone/WASM mode
            registers[insn.rd] = Value(0.0);
            VM_NEXT();
        }
        VM_CASE(MOV)
            if (!validate_register_index(insn.rd, registers.size(), result.error,
                                         "destination") ||
                !validate_register_index(insn.rs1, registers.size(), result.error,
                                         "source"))
            {
                return result;
            }
            registers[insn.rd] = registers[insn.rs1];
            VM_NEXT();
        VM_CASE(VEC_INDEX)
        VM_CASE(VEC_LERP)
        {
            if (!validate_register_index(insn.rd, registers.size(), result.error,
                                         "destination") ||
                !validate_register_index(insn.rs1, registers.size(), result.error,
                                         "source"))
            {
                return result;
            }

            const std::vector<double>* data = nullptr;
            if (!load_data_segment(program, insn.imm, data, result.error))
            {
                return result;
            }
            if (!data || data->empty())
            {
                result.error = "Numeric VM data segment is empty";
                return result;
            }

            double phasor = 0.0;
            if (!load_phasor_value(registers, insn.rs1, phasor, result.error))
            {
                return result;
            }

            const double clamped_phasor = std::clamp(phasor, 0.0, 1.0);
            const double scaled = clamped_phasor * static_cast<double>(data->size());
            const size_t index =
                std::min(static_cast<size_t>(std::floor(scaled)), data->size() - 1);

            if (insn.opcode == NumericVmOpcode::VEC_INDEX)
            {
                registers[insn.rd] = Value((*data)[index]);
            }
            else
            {
                if (data->size() == 1)
                {
                    registers[insn.rd] = Value((*data)[0]);
                }
                else
                {
                    const double scaled_lerp =
                        clamped_phasor * static_cast<double>(data->size() - 1);
                    size_t base = static_cast<size_t>(std::floor(scaled_lerp));
                    if (base >= data->size() - 1)
                    {
                        base = data->size() - 2;
                    }
                    const double fraction =
                        scaled_lerp - static_cast<double>(base);
                    registers[insn.rd] =
                        Value((*data)[base] +
                              (((*data)[base + 1] - (*data)[base]) * fraction));
                }
            }
            VM_NEXT();
        }
        VM_CASE(NEG)
        VM_CASE(FLOOR)
        VM_CASE(CEIL)
        VM_CASE(FRAC)
        VM_CASE(ABS)
        VM_CASE(SQRT)
        VM_CASE(SIN)
        VM_CASE(COS)
        VM_CASE(TAN)
        VM_CASE(U_SIN)
        VM_CASE(U_COS)
        VM_CASE(U_SIN_BI)
        VM_CASE(U_COS_BI)
        VM_CASE(TRI)
        VM_CASE(SQR)
        {
            if (!validate_register_index(insn.rd, registers.size(), result.error,
                                         "destination") ||
                !validate_register_index(insn.rs1, registers.size(), result.error,
                                         "source"))
            {
                return result;
            }

            const Value lhs = registers[insn.rs1];
            if (!lhs.is_number())
            {
                result.error = "Numeric VM arithmetic requires numeric operands";
                return result;
            }

            const double left = lhs.as_float();
            double out = 0.0;
            switch (insn.opcode)
            {
            case NumericVmOpcode::NEG:
                out = -left;
                break;
            case NumericVmOpcode::FLOOR:
                out = std::floor(left);
                break;
            case NumericVmOpcode::CEIL:
                out = std::ceil(left);
                break;
            case NumericVmOpcode::FRAC:
                out = left - std::floor(left);
                break;
            case NumericVmOpcode::ABS:
                out = std::fabs(left);
                break;
            case NumericVmOpcode::SQRT:
                out = std::sqrt(left);
                break;
            case NumericVmOpcode::SIN:
                out = std::sin(left);
                break;
            case NumericVmOpcode::COS:
                out = std::cos(left);
                break;
            case NumericVmOpcode::TAN:
                out = std::tan(left);
                break;
            case NumericVmOpcode::U_SIN:
                out = (std::sin(left * 2.0 * M_PI) + 1.0) / 2.0;
                break;
            case NumericVmOpcode::U_COS:
                out = (std::cos(left * 2.0 * M_PI) + 1.0) / 2.0;
                break;
            case NumericVmOpcode::U_SIN_BI:
                out = std::sin(left * 2.0 * M_PI);
                break;
            case NumericVmOpcode::U_COS_BI:
                out = std::cos(left * 2.0 * M_PI);
                break;
            case NumericVmOpcode::TRI:
                out = 1.0 - std::fabs(2.0 * left - 1.0);
                break;
            case NumericVmOpcode::SQR:
                out = left < 0.5 ? 1.0 : 0.0;
                break;
            default:
                break;
            }
            registers[insn.rd] = Value(out);
            VM_NEXT();
        }
        VM_CASE(ADD)
        VM_CASE(SUB)
        VM_CASE(MUL)
        VM_CASE(DIV)
        VM_CASE(MOD)
        VM_CASE(CMP_GT)
        VM_CASE(CMP_LT)
        VM_CASE(CMP_GE)
        VM_CASE(CMP_LE)
        VM_CASE(CMP_EQ)
        VM_CASE(MIN)
        VM_CASE(MAX)
        VM_CASE(POW)
        VM_CASE(PULSE)
        {
            if (!validate_register_index(insn.rd, registers.size(), result.error,
                                         "destination") ||
                !validate_register_index(insn.rs1, registers.size(), result.error,
                                         "source") ||
                !validate_register_index(insn.rs2, registers.size(), result.error,
                                         "source"))
            {
                return result;
            }

            const Value lhs = registers[insn.rs1];
            const Value rhs = registers[insn.rs2];
            if (!lhs.is_number() || !rhs.is_number())
            {
                result.error = "Numeric VM arithmetic requires numeric operands";
                return result;
            }

            const double left = lhs.as_float();
            const double right = rhs.as_float();
            double out = 0.0;
            switch (insn.opcode)
            {
            case NumericVmOpcode::ADD:
                out = left + right;
                break;
            case NumericVmOpcode::SUB:
                out = left - right;
                break;
            case NumericVmOpcode::MUL:
                out = left * right;
                break;
            case NumericVmOpcode::DIV:
                if (right == 0.0)
                {
                    result.error = "Numeric VM division by zero";
                    return result;
                }
                out = left / right;
                break;
            case NumericVmOpcode::MOD:
                if (right == 0.0)
                {
                    result.error = "Numeric VM modulo by zero";
                    return result;
                }
                out = std::fmod(left, right);
                break;
            case NumericVmOpcode::CMP_GT:
                out = left > right ? 1.0 : 0.0;
                break;
            case NumericVmOpcode::CMP_LT:
                out = left < right ? 1.0 : 0.0;
                break;
            case NumericVmOpcode::CMP_GE:
                out = left >= right ? 1.0 : 0.0;
                break;
            case NumericVmOpcode::CMP_LE:
                out = left <= right ? 1.0 : 0.0;
                break;
            case NumericVmOpcode::CMP_EQ:
                out = left == right ? 1.0 : 0.0;
                break;
            case NumericVmOpcode::MIN:
                out = std::fmin(left, right);
                break;
            case NumericVmOpcode::MAX:
                out = std::fmax(left, right);
                break;
            case NumericVmOpcode::POW:
                out = std::pow(left, right);
                break;
            case NumericVmOpcode::PULSE:
                out = left < right ? 1.0 : 0.0;
                break;
            default:
                break;
            }
            registers[insn.rd] = Value(out);
            VM_NEXT();
        }
        VM_CASE(CLAMP)
        {
            if (!validate_register_index(insn.rd, registers.size(), result.error,
                                         "destination") ||
                !validate_register_index(insn.rs1, registers.size(), result.error,
                                         "source") ||
                !validate_register_index(insn.rs2, registers.size(), result.error,
                                         "source") ||
                !validate_register_index(insn.rs3, registers.size(), result.error,
                                         "source"))
            {
                return result;
            }

            const Value value = registers[insn.rs1];
            const Value low = registers[insn.rs2];
            const Value high = registers[insn.rs3];
            if (!value.is_number() || !low.is_number() || !high.is_number())
            {
                result.error = "Numeric VM clamp requires numeric operands";
                return result;
            }

            registers[insn.rd] =
                Value(std::fmin(std::fmax(value.as_float(), low.as_float()),
                                high.as_float()));
            VM_NEXT();
        }
        VM_CASE(BRANCH)
        {
            size_t target = 0;
            if (!resolve_branch_target(pc, insn.imm, program.instructions.size(), target,
                                       result.error))
            {
                return result;
            }
            pc = target;
            VM_BRANCH_DONE();
        }
        VM_CASE(BRANCH_IF)
        VM_CASE(BRANCH_UNLESS)
            if (!validate_register_index(insn.rs1, registers.size(), result.error,
                                         "source"))
            {
                return result;
            }
            if ((insn.opcode == NumericVmOpcode::BRANCH_IF &&
                 is_truthy(registers[insn.rs1])) ||
                (insn.opcode == NumericVmOpcode::BRANCH_UNLESS &&
                 !is_truthy(registers[insn.rs1])))
            {
                size_t target = 0;
                if (!resolve_branch_target(pc, insn.imm, program.instructions.size(),
                                           target, result.error))
                {
                    return result;
                }
                pc = target;
            }
            else
            {
                ++pc;
            }
            VM_BRANCH_DONE();
        VM_CASE(CALL)
        VM_CASE(CALL_INTRINSIC)
        {
            if (!validate_register_index(insn.rd, registers.size(), result.error,
                                         "destination"))
            {
                return result;
            }
            const size_t arg_start = static_cast<size_t>(insn.rs1);
            const size_t arg_count = static_cast<size_t>(insn.rs2);
            if (arg_start + arg_count > registers.size())
            {
                result.error = "Numeric VM call arguments out of range";
                return result;
            }

            std::vector<Value> args;
            args.reserve(arg_count);
            for (size_t i = 0; i < arg_count; ++i)
            {
                args.push_back(registers[arg_start + i]);
            }

            if (insn.opcode == NumericVmOpcode::CALL)
            {
                if (insn.imm < 0 ||
                    static_cast<size_t>(insn.imm) >= program.functions.size() ||
                    !program.functions[static_cast<size_t>(insn.imm)])
                {
                    result.error = "Numeric VM function index out of range";
                    return result;
                }

                const TaggedVmExecutionResult callee_result =
                    execute_tagged_program_impl(
                        *program.functions[static_cast<size_t>(insn.imm)], ctx, args);
                if (!callee_result.ok)
                {
                    return callee_result;
                }
                registers[insn.rd] = callee_result.value;
            }
            else
            {
                if (insn.imm < 0 ||
                    static_cast<size_t>(insn.imm) >= program.intrinsics.size() ||
                    !program.intrinsics[static_cast<size_t>(insn.imm)])
                {
                    result.error = "Numeric VM intrinsic index out of range";
                    return result;
                }
                registers[insn.rd] =
                    program.intrinsics[static_cast<size_t>(insn.imm)](args, ctx);
            }
            VM_NEXT();
        }
        VM_CASE(IS_NIL)
        VM_CASE(IS_NUMBER)
        VM_CASE(IS_LIST)
        VM_CASE(IS_STRING)
        VM_CASE(NOT)
        {
            if (!validate_register_index(insn.rd, registers.size(), result.error,
                                         "destination") ||
                !validate_register_index(insn.rs1, registers.size(), result.error,
                                         "source"))
            {
                return result;
            }

            const Value& src = registers[insn.rs1];
            double out = 0.0;
            switch (insn.opcode)
            {
            case NumericVmOpcode::IS_NIL:
                out = src.is_nil() ? 1.0 : 0.0;
                break;
            case NumericVmOpcode::IS_NUMBER:
                out = src.is_number() ? 1.0 : 0.0;
                break;
            case NumericVmOpcode::IS_LIST:
                out = src.is_list() ? 1.0 : 0.0;
                break;
            case NumericVmOpcode::IS_STRING:
                out = (src.type == Value::STRING) ? 1.0 : 0.0;
                break;
            case NumericVmOpcode::NOT:
                out = is_truthy(src) ? 0.0 : 1.0;
                break;
            default:
                break;
            }
            registers[insn.rd] = Value(out);
            VM_NEXT();
        }
        VM_CASE(AND)
        {
            if (!validate_register_index(insn.rd, registers.size(), result.error,
                                         "destination") ||
                !validate_register_index(insn.rs1, registers.size(), result.error,
                                         "source") ||
                !validate_register_index(insn.rs2, registers.size(), result.error,
                                         "source"))
            {
                return result;
            }

            registers[insn.rd] = is_truthy(registers[insn.rs1])
                                     ? registers[insn.rs2]
                                     : registers[insn.rs1];
            VM_NEXT();
        }
        VM_CASE(OR)
        {
            if (!validate_register_index(insn.rd, registers.size(), result.error,
                                         "destination") ||
                !validate_register_index(insn.rs1, registers.size(), result.error,
                                         "source") ||
                !validate_register_index(insn.rs2, registers.size(), result.error,
                                         "source"))
            {
                return result;
            }

            registers[insn.rd] = is_truthy(registers[insn.rs1])
                                     ? registers[insn.rs1]
                                     : registers[insn.rs2];
            VM_NEXT();
        }
        VM_CASE(MAKE_LIST)
        {
            if (!validate_register_index(insn.rd, registers.size(), result.error,
                                         "destination"))
            {
                return result;
            }

            const size_t start = static_cast<size_t>(insn.rs1);
            const size_t count = static_cast<size_t>(insn.rs2);
            if (count > 0 && start + count > registers.size())
            {
                result.error = "Numeric VM MAKE_LIST arguments out of range";
                return result;
            }

            std::vector<Value> elements;
            elements.reserve(count);
            for (size_t i = 0; i < count; ++i)
            {
                elements.push_back(registers[start + i]);
            }
            registers[insn.rd] = Value(elements);
            VM_NEXT();
        }
        VM_CASE(LIST_HEAD)
        {
            if (!validate_register_index(insn.rd, registers.size(), result.error,
                                         "destination") ||
                !validate_register_index(insn.rs1, registers.size(), result.error,
                                         "source"))
            {
                return result;
            }

            const Value& src = registers[insn.rs1];
            if (!src.is_list())
            {
                result.error = "Numeric VM LIST_HEAD requires a list operand";
                return result;
            }

            const std::vector<Value> items = src.as_list();
            if (items.empty())
            {
                registers[insn.rd] = Value::nil();
            }
            else
            {
                registers[insn.rd] = items[0];
            }
            VM_NEXT();
        }
        VM_CASE(LIST_TAIL)
        {
            if (!validate_register_index(insn.rd, registers.size(), result.error,
                                         "destination") ||
                !validate_register_index(insn.rs1, registers.size(), result.error,
                                         "source"))
            {
                return result;
            }

            const Value& src = registers[insn.rs1];
            if (!src.is_list())
            {
                result.error = "Numeric VM LIST_TAIL requires a list operand";
                return result;
            }

            const std::vector<Value> items = src.as_list();
            if (items.size() <= 1)
            {
                registers[insn.rd] = Value(std::vector<Value>{});
            }
            else
            {
                registers[insn.rd] = Value(
                    std::vector<Value>(items.begin() + 1, items.end()));
            }
            VM_NEXT();
        }
        VM_CASE(LIST_LENGTH)
        {
            if (!validate_register_index(insn.rd, registers.size(), result.error,
                                         "destination") ||
                !validate_register_index(insn.rs1, registers.size(), result.error,
                                         "source"))
            {
                return result;
            }

            const Value& src = registers[insn.rs1];
            if (src.is_list())
            {
                registers[insn.rd] =
                    Value(static_cast<double>(src.as_list().size()));
            }
            else
            {
                registers[insn.rd] = Value(0.0);
            }
            VM_NEXT();
        }
        VM_CASE(RET)
            if (!validate_register_index(insn.rs1, registers.size(), result.error,
                                         "source"))
            {
                return result;
            }
            result.ok = true;
            result.value = registers[insn.rs1];
            return result;
#if !VM_USE_COMPUTED_GOTO
        } // end switch
#endif
    } // end for

#if VM_USE_COMPUTED_GOTO
vm_loop_exit:
#endif

    result.error = "Numeric VM program terminated without RET";
    return result;

#undef VM_DISPATCH
#undef VM_CASE
#undef VM_NEXT
#undef VM_BREAK
#undef VM_BRANCH_DONE
#undef VM_USE_COMPUTED_GOTO
}
} // namespace

NumericVmCompileResult compile_numeric_program(const Value& expr,
                                               const Environment& env,
                                               bool signal_context)
{
    NumericVmCompiler compiler(env, signal_context);
    return compiler.compile(expr);
}

TaggedVmExecutionResult execute_tagged_program(const NumericVmProgram& program,
                                               const TemporalContext& ctx)
{
    return execute_tagged_program_impl(program, ctx);
}

NumericVmExecutionResult execute_numeric_program(const NumericVmProgram& program,
                                                 const TemporalContext& ctx)
{
    NumericVmExecutionResult result;
    const TaggedVmExecutionResult tagged_result = execute_tagged_program(program, ctx);
    if (!tagged_result.ok)
    {
        result.error = tagged_result.error;
        return result;
    }

    if (!tagged_result.value.is_number())
    {
        result.error = "Numeric VM did not produce a numeric result";
        return result;
    }

    const double numeric_value = tagged_result.value.as_float();
    if (!std::isfinite(numeric_value))
    {
        result.error = "Numeric VM produced a non-finite result";
        return result;
    }

    result.ok = true;
    result.value = numeric_value;
    return result;
}
