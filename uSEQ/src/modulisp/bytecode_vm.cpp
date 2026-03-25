#include "bytecode_vm.h"
#include <algorithm>
#include <cmath>
#include <functional>

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

struct AffineTimeTransform
{
    double scale = 1.0;
    double offset = 0.0;
};

struct LocalValueBinding
{
    int source_register = -1;
};

class NumericVmCompiler
{
public:
    explicit NumericVmCompiler(const Environment& env)
        : m_env(env)
    {
    }

    NumericVmCompileResult compile(const Value& expr)
    {
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
            fail("Numeric VM cannot return vector values directly");
            return -1;
        }

        if (!expr.is_list())
        {
            fail("Unsupported non-numeric expression in numeric VM");
            return -1;
        }

        const std::vector<Value> items = expr.as_list();
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

        int callable_reg = -1;
        if (try_compile_callable_form(items, transform, callable_reg))
        {
            return callable_reg;
        }

        if (m_error.length() == 0)
        {
            fail("Unsupported numeric VM builtin: " + op);
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
            const int reg = compile_expr(*expr, transform);
            m_recursion_stack.pop_back();
            return reg;
        }

        if (const std::optional<Value> value = m_env.get(symbol))
        {
            if (value->is_number())
            {
                maybe_add_dependency(symbol);
                return emit_const(Value(value->as_float()));
            }
            if (value->is_builtin())
            {
                return emit_late_bound_numeric_symbol(symbol);
            }
        }

        return emit_late_bound_numeric_symbol(symbol);
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

    int emit_late_bound_numeric_symbol(const String& symbol)
    {
        maybe_add_dependency(symbol);
        const size_t intrinsic_index = store_intrinsic(
            [env = &m_env, symbol](const std::vector<Value>&,
                                   const TemporalContext&) -> Value {
                if (const std::optional<Value> value = env->get(symbol))
                {
                    if (value->is_number())
                    {
                        return Value(value->as_float());
                    }
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

    void fail(const String& error)
    {
        if (m_error.length() == 0)
        {
            m_error = error;
        }
    }

    const Environment& m_env;
    NumericVmProgram m_program;
    int m_next_register = 0;
    String m_error;
    std::vector<String> m_recursion_stack;
    std::vector<std::map<String, LocalValueBinding>> m_local_value_scopes;
    std::vector<std::map<String, Value>> m_local_callable_scopes;
    mutable NumericVmTemporalChannel m_unused_channel = NumericVmTemporalChannel::T;
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
    for (size_t pc = 0; pc < program.instructions.size();)
    {
        const NumericVmInstruction& insn = program.instructions[pc];
        switch (insn.opcode)
        {
        case NumericVmOpcode::LOAD_CONST:
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
            ++pc;
            break;
        case NumericVmOpcode::LOAD_TIME:
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
            ++pc;
            break;
        }
        case NumericVmOpcode::MOV:
            if (!validate_register_index(insn.rd, registers.size(), result.error,
                                         "destination") ||
                !validate_register_index(insn.rs1, registers.size(), result.error,
                                         "source"))
            {
                return result;
            }
            registers[insn.rd] = registers[insn.rs1];
            ++pc;
            break;
        case NumericVmOpcode::VEC_INDEX:
        case NumericVmOpcode::VEC_LERP:
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
            ++pc;
            break;
        }
        case NumericVmOpcode::ADD:
        case NumericVmOpcode::SUB:
        case NumericVmOpcode::MUL:
        case NumericVmOpcode::DIV:
        case NumericVmOpcode::MOD:
        case NumericVmOpcode::NEG:
        case NumericVmOpcode::CMP_GT:
        case NumericVmOpcode::CMP_LT:
        case NumericVmOpcode::CMP_GE:
        case NumericVmOpcode::CMP_LE:
        case NumericVmOpcode::CMP_EQ:
        case NumericVmOpcode::FLOOR:
        case NumericVmOpcode::CEIL:
        case NumericVmOpcode::FRAC:
        case NumericVmOpcode::ABS:
        case NumericVmOpcode::MIN:
        case NumericVmOpcode::MAX:
        case NumericVmOpcode::POW:
        case NumericVmOpcode::SQRT:
        case NumericVmOpcode::CLAMP:
        case NumericVmOpcode::SIN:
        case NumericVmOpcode::COS:
        case NumericVmOpcode::TAN:
        {
            if (!validate_register_index(insn.rd, registers.size(), result.error,
                                         "destination") ||
                !validate_register_index(insn.rs1, registers.size(), result.error,
                                         "source") ||
                (insn.opcode != NumericVmOpcode::NEG &&
                 !validate_register_index(insn.rs2, registers.size(), result.error,
                                          "source")))
            {
                return result;
            }

            const Value lhs = registers[insn.rs1];
            const Value rhs = insn.opcode == NumericVmOpcode::NEG ? Value::nil()
                                                                  : registers[insn.rs2];
            if (!lhs.is_number() ||
                (insn.opcode != NumericVmOpcode::NEG && !rhs.is_number()))
            {
                result.error = "Numeric VM arithmetic requires numeric operands";
                return result;
            }

            const double left = lhs.as_float();
            const double right = insn.opcode == NumericVmOpcode::NEG ? 0.0
                                                                     : rhs.as_float();
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
            case NumericVmOpcode::NEG:
                out = -left;
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
            case NumericVmOpcode::MIN:
                out = std::fmin(left, right);
                break;
            case NumericVmOpcode::MAX:
                out = std::fmax(left, right);
                break;
            case NumericVmOpcode::POW:
                out = std::pow(left, right);
                break;
            case NumericVmOpcode::SQRT:
                out = std::sqrt(left);
                break;
            case NumericVmOpcode::CLAMP:
                if (!validate_register_index(insn.rs3, registers.size(), result.error,
                                             "source"))
                {
                    return result;
                }
                {
                    const Value high = registers[insn.rs3];
                    if (!high.is_number())
                    {
                        result.error = "Numeric VM clamp requires numeric operands";
                        return result;
                    }
                    out = std::fmin(std::fmax(left, right), high.as_float());
                }
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
            default:
                break;
            }
            registers[insn.rd] = Value(out);
            ++pc;
            break;
        }
        case NumericVmOpcode::BRANCH:
        {
            size_t target = 0;
            if (!resolve_branch_target(pc, insn.imm, program.instructions.size(), target,
                                       result.error))
            {
                return result;
            }
            pc = target;
            break;
        }
        case NumericVmOpcode::BRANCH_IF:
        case NumericVmOpcode::BRANCH_UNLESS:
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
            break;
        case NumericVmOpcode::CALL:
        case NumericVmOpcode::CALL_INTRINSIC:
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
            ++pc;
            break;
        }
        case NumericVmOpcode::RET:
            if (!validate_register_index(insn.rs1, registers.size(), result.error,
                                         "source"))
            {
                return result;
            }
            result.ok = true;
            result.value = registers[insn.rs1];
            return result;
        }
    }

    result.error = "Numeric VM program terminated without RET";
    return result;
}
} // namespace

NumericVmCompileResult compile_numeric_program(const Value& expr,
                                               const Environment& env)
{
    NumericVmCompiler compiler(env);
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
