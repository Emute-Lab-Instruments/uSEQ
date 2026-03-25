#include "bytecode_vm.h"
#include <cmath>
#include <functional>

namespace
{
struct AffineTimeTransform
{
    double scale = 1.0;
    double offset = 0.0;
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
        if (expr.is_number())
        {
            return emit_const(expr.as_float());
        }

        if (expr.is_symbol())
        {
            return compile_symbol(expr.as_atom(), transform);
        }

        if (!expr.is_list())
        {
            fail("Unsupported non-numeric expression in numeric VM");
            return -1;
        }

        const std::vector<Value> items = expr.as_list();
        if (items.empty() || !items[0].is_symbol())
        {
            fail("Numeric VM only supports builtin call forms");
            return -1;
        }

        const String op = items[0].as_atom();
        if (op == "fast" || op == "slow" || op == "offset" || op == "shift")
        {
            return compile_time_warp(op, items, transform);
        }

        if (op == "+")
        {
            return compile_fold(items, 1, NumericVmOpcode::ADD, transform);
        }
        if (op == "*")
        {
            return compile_fold(items, 1, NumericVmOpcode::MUL, transform);
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
            return compile_fold(items, 1, NumericVmOpcode::SUB, transform);
        }
        if (op == "/")
        {
            return compile_fold(items, 1, NumericVmOpcode::DIV, transform);
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

        fail("Unsupported numeric VM builtin: " + op);
        return -1;
    }

    int compile_symbol(const String& symbol, const AffineTimeTransform& transform)
    {
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
            m_recursion_stack.push_back(symbol);
            const int reg = compile_expr(*expr, transform);
            m_recursion_stack.pop_back();
            return reg;
        }

        if (const std::optional<Value> value = m_env.get(symbol))
        {
            if (value->is_number())
            {
                return emit_const(value->as_float());
            }
        }

        fail("Unsupported numeric VM symbol: " + symbol);
        return -1;
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

    int compile_fold(const std::vector<Value>& items,
                     size_t arg_start,
                     NumericVmOpcode opcode,
                     const AffineTimeTransform& transform)
    {
        if (items.size() <= arg_start + 1)
        {
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

        const int dst = allocate_register();
        NumericVmInstruction insn;
        insn.opcode = NumericVmOpcode::CLAMP;
        insn.rd = static_cast<uint16_t>(dst);
        insn.rs1 = static_cast<uint16_t>(value);
        insn.rs2 = static_cast<uint16_t>(low);
        insn.rs3 = static_cast<uint16_t>(high);
        m_program.instructions.push_back(insn);
        return dst;
    }

    int emit_const(double value)
    {
        const int dst = allocate_register();
        NumericVmInstruction insn;
        insn.opcode = NumericVmOpcode::LOAD_CONST;
        insn.rd = static_cast<uint16_t>(dst);
        insn.imm = static_cast<int32_t>(m_program.constants.size());
        m_program.constants.push_back(value);
        m_program.instructions.push_back(insn);
        return dst;
    }

    int emit_unary(NumericVmOpcode opcode, int src)
    {
        const int dst = allocate_register();
        NumericVmInstruction insn;
        insn.opcode = opcode;
        insn.rd = static_cast<uint16_t>(dst);
        insn.rs1 = static_cast<uint16_t>(src);
        m_program.instructions.push_back(insn);
        return dst;
    }

    int emit_binary(NumericVmOpcode opcode, int lhs, int rhs)
    {
        const int dst = allocate_register();
        NumericVmInstruction insn;
        insn.opcode = opcode;
        insn.rd = static_cast<uint16_t>(dst);
        insn.rs1 = static_cast<uint16_t>(lhs);
        insn.rs2 = static_cast<uint16_t>(rhs);
        m_program.instructions.push_back(insn);
        return dst;
    }

    int allocate_register()
    {
        return m_next_register++;
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
            if (const std::optional<Value> value = m_env.get(symbol))
            {
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
            return binary([](double lhs, double rhs) { return lhs - rhs; });
        }
        if (op == "/")
        {
            return binary([](double lhs, double rhs) { return lhs / rhs; });
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

        return std::nullopt;
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
} // namespace

NumericVmCompileResult compile_numeric_program(const Value& expr,
                                               const Environment& env)
{
    NumericVmCompiler compiler(env);
    return compiler.compile(expr);
}

NumericVmExecutionResult execute_numeric_program(const NumericVmProgram& program,
                                                 const TemporalContext& ctx)
{
    NumericVmExecutionResult result;
    if (program.register_count == 0 || program.instructions.empty())
    {
        result.error = "Numeric VM program is empty";
        return result;
    }

    std::vector<double> registers(program.register_count, 0.0);
    for (const auto& insn : program.instructions)
    {
        switch (insn.opcode)
        {
        case NumericVmOpcode::LOAD_CONST:
            if (insn.imm < 0 ||
                static_cast<size_t>(insn.imm) >= program.constants.size())
            {
                result.error = "Numeric VM constant index out of range";
                return result;
            }
            registers[insn.rd] = program.constants[static_cast<size_t>(insn.imm)];
            break;
        case NumericVmOpcode::LOAD_TIME:
        {
            const NumericVmTemporalChannel channel =
                static_cast<NumericVmTemporalChannel>(insn.imm);
            const double warped_t = (ctx.t * insn.imm0) + insn.imm1;
            switch (channel)
            {
            case NumericVmTemporalChannel::T:
            case NumericVmTemporalChannel::TIME:
                registers[insn.rd] = warped_t;
                break;
            case NumericVmTemporalChannel::BEAT:
                registers[insn.rd] = wrap_phase(warped_t, ctx.beatDur);
                break;
            case NumericVmTemporalChannel::BAR:
                registers[insn.rd] = wrap_phase(warped_t, ctx.barDur);
                break;
            case NumericVmTemporalChannel::PHRASE:
                registers[insn.rd] = wrap_phase(warped_t, ctx.phraseDur);
                break;
            case NumericVmTemporalChannel::SECTION:
                registers[insn.rd] = wrap_phase(warped_t, ctx.sectionDur);
                break;
            case NumericVmTemporalChannel::BEAT_NUM:
                registers[insn.rd] = time_to_count(warped_t, ctx.beatDur);
                break;
            case NumericVmTemporalChannel::BAR_NUM:
                registers[insn.rd] = time_to_count(warped_t, ctx.barDur);
                break;
            }
            break;
        }
        case NumericVmOpcode::MOV:
            registers[insn.rd] = registers[insn.rs1];
            break;
        case NumericVmOpcode::ADD:
            registers[insn.rd] = registers[insn.rs1] + registers[insn.rs2];
            break;
        case NumericVmOpcode::SUB:
            registers[insn.rd] = registers[insn.rs1] - registers[insn.rs2];
            break;
        case NumericVmOpcode::MUL:
            registers[insn.rd] = registers[insn.rs1] * registers[insn.rs2];
            break;
        case NumericVmOpcode::DIV:
            if (registers[insn.rs2] == 0.0)
            {
                result.error = "Numeric VM division by zero";
                return result;
            }
            registers[insn.rd] = registers[insn.rs1] / registers[insn.rs2];
            break;
        case NumericVmOpcode::MOD:
            if (registers[insn.rs2] == 0.0)
            {
                result.error = "Numeric VM modulo by zero";
                return result;
            }
            registers[insn.rd] = std::fmod(registers[insn.rs1], registers[insn.rs2]);
            break;
        case NumericVmOpcode::NEG:
            registers[insn.rd] = -registers[insn.rs1];
            break;
        case NumericVmOpcode::CMP_GT:
            registers[insn.rd] = registers[insn.rs1] > registers[insn.rs2] ? 1.0 : 0.0;
            break;
        case NumericVmOpcode::CMP_LT:
            registers[insn.rd] = registers[insn.rs1] < registers[insn.rs2] ? 1.0 : 0.0;
            break;
        case NumericVmOpcode::CMP_GE:
            registers[insn.rd] = registers[insn.rs1] >= registers[insn.rs2] ? 1.0 : 0.0;
            break;
        case NumericVmOpcode::CMP_LE:
            registers[insn.rd] = registers[insn.rs1] <= registers[insn.rs2] ? 1.0 : 0.0;
            break;
        case NumericVmOpcode::CMP_EQ:
            registers[insn.rd] = registers[insn.rs1] == registers[insn.rs2] ? 1.0 : 0.0;
            break;
        case NumericVmOpcode::FLOOR:
            registers[insn.rd] = std::floor(registers[insn.rs1]);
            break;
        case NumericVmOpcode::CEIL:
            registers[insn.rd] = std::ceil(registers[insn.rs1]);
            break;
        case NumericVmOpcode::FRAC:
            registers[insn.rd] = registers[insn.rs1] - std::floor(registers[insn.rs1]);
            break;
        case NumericVmOpcode::ABS:
            registers[insn.rd] = std::fabs(registers[insn.rs1]);
            break;
        case NumericVmOpcode::MIN:
            registers[insn.rd] = std::fmin(registers[insn.rs1], registers[insn.rs2]);
            break;
        case NumericVmOpcode::MAX:
            registers[insn.rd] = std::fmax(registers[insn.rs1], registers[insn.rs2]);
            break;
        case NumericVmOpcode::POW:
            registers[insn.rd] = std::pow(registers[insn.rs1], registers[insn.rs2]);
            break;
        case NumericVmOpcode::SQRT:
            registers[insn.rd] = std::sqrt(registers[insn.rs1]);
            break;
        case NumericVmOpcode::CLAMP:
            registers[insn.rd] = std::fmin(
                std::fmax(registers[insn.rs1], registers[insn.rs2]),
                registers[insn.rs3]);
            break;
        case NumericVmOpcode::SIN:
            registers[insn.rd] = std::sin(registers[insn.rs1]);
            break;
        case NumericVmOpcode::COS:
            registers[insn.rd] = std::cos(registers[insn.rs1]);
            break;
        case NumericVmOpcode::TAN:
            registers[insn.rd] = std::tan(registers[insn.rs1]);
            break;
        case NumericVmOpcode::RET:
            result.ok = true;
            result.value = registers[insn.rs1];
            return result;
        }
    }

    result.error = "Numeric VM program terminated without RET";
    return result;
}
