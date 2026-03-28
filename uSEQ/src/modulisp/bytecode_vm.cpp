#include "bytecode_vm.h"
#include "modulisp_interpreter.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <functional>
#include <set>
#include <unordered_map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
constexpr size_t VM_MAX_BACKWARD_BRANCHES = 100000;

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

// --- Fuzzy name matching for undefined-symbol hints ---

static int levenshtein(const String& a, const String& b)
{
    const int m = static_cast<int>(a.length());
    const int n = static_cast<int>(b.length());
    std::vector<int> prev(n + 1), curr(n + 1);
    for (int j = 0; j <= n; j++) prev[j] = j;
    for (int i = 1; i <= m; i++)
    {
        curr[0] = i;
        for (int j = 1; j <= n; j++)
        {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            curr[j] = std::min({prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost});
        }
        std::swap(prev, curr);
    }
    return prev[n];
}

static bool strings_equal_icase(const String& a, const String& b)
{
    if (a.length() != b.length()) return false;
    for (size_t i = 0; i < a.length(); i++)
    {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

static bool is_prefix_of(const String& prefix, const String& candidate)
{
    if (prefix.length() >= candidate.length() || prefix.length() < 2)
        return false;
    for (size_t i = 0; i < prefix.length(); i++)
    {
        if (prefix[i] != candidate[i]) return false;
    }
    return true;
}

static String find_fuzzy_match(const String& symbol, const Environment& env)
{
    std::vector<String> candidates;

    static const char* temporals[] = {
        "beat", "bar", "phrase", "section", "t", "time", "beat-num", "bar-num"
    };
    for (const auto& t : temporals) candidates.push_back(String(t));

    env.collect_symbol_names(candidates);

    String best_match;
    int best_distance = 3; // Only suggest if distance <= 2

    for (const auto& candidate : candidates)
    {
        if (candidate == symbol) continue;

        if (strings_equal_icase(symbol, candidate))
            return candidate;

        if (is_prefix_of(symbol, candidate) && best_distance > 1)
        {
            best_distance = 1;
            best_match = candidate;
            continue;
        }

        int dist = levenshtein(symbol, candidate);
        if (dist < best_distance)
        {
            best_distance = dist;
            best_match = candidate;
        }
    }

    return best_match;
}

// --- End fuzzy name matching ---

struct LocalValueBinding
{
    int source_register = -1;
};

// Diagnostic counter: tracks closure fallbacks to the tree-walker.
// With compile-time capture substitution now implemented, this counter
// should remain at zero for closures whose captured values are constants
// or compilable expressions. Non-zero values indicate a closure pattern
// the compiler cannot yet handle (e.g., recursive closures, HOFs).
static int s_closure_fallback_count = 0;
int get_closure_fallback_count() { return s_closure_fallback_count; }
constexpr size_t kInlineLambdaNodeThreshold = 24;

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

static uint64_t hash_value(const Value& v)
{
    uint64_t h = static_cast<uint64_t>(v.type);

    switch (v.type)
    {
    case Value::FLOAT:
    case Value::INT:
    {
        double d = v.as_float();
        uint64_t bits;
        std::memcpy(&bits, &d, sizeof(bits));
        // Normalize -0.0 to +0.0
        if (bits == 0x8000000000000000ULL)
            bits = 0;
        h ^= bits * 0x9e3779b97f4a7c15ULL;
        break;
    }
    case Value::STRING:
    case Value::ATOM:
    {
        const String& s = v.str;
        for (size_t i = 0; i < s.length(); ++i)
        {
            h = h * 31 + static_cast<unsigned char>(s[i]);
        }
        break;
    }
    case Value::NIL:
    case Value::UNIT:
        break;
    default:
        // For lists, vectors, lambdas etc. — use a simple hash based on size.
        // These are less common as constants; linear fallback is fine.
        h ^= v.list.size() * 0x517cc1b727220a95ULL;
        break;
    }

    return h;
}

static uint64_t hash_double_vector(const std::vector<double>& values)
{
    uint64_t h = values.size();
    for (double v : values)
    {
        uint64_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        h ^= bits * 0x9e3779b97f4a7c15ULL + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    return h;
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
    size_t diagnostic_count = 0;
    size_t free_register_count = 0;
    size_t pinned_register_count = 0;
};

double wrap_phase(double time_seconds, double duration_seconds);
double time_to_count(double time_seconds, double duration_seconds);
TemporalContext apply_time_transform(const TemporalContext& ctx,
                                     const AffineTimeTransform& transform);
bool is_truthy(const Value& value);

namespace
{
constexpr int kRuntimeVmBridgeMaxDepth = 32;
constexpr int kRuntimeVmWhileMaxIterations = 1024;

bool try_parse_lambda_expr(const Value& value,
                           std::vector<Value>& params,
                           Value& body)
{
    if (!value.is_list())
    {
        return false;
    }

    const std::vector<Value> items = value.as_list();
    if (items.empty() || !items[0].is_symbol() || items[0].as_atom() != "lambda")
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

Value execute_runtime_expr_with_vm_impl(const Value& expr,
                                        const Environment& env,
                                        bool signal_context,
                                        const TemporalContext& ctx,
                                        int depth);

Value execute_runtime_symbol_with_vm(const String& symbol,
                                     const Environment& env,
                                     bool signal_context,
                                     const TemporalContext& ctx,
                                     int depth)
{
    if (const std::optional<Value> expr_binding = env.get_expr(symbol))
    {
        return execute_runtime_expr_with_vm_impl(*expr_binding, env, signal_context,
                                                 ctx, depth + 1);
    }

    if (const std::optional<Value> value_binding = env.get(symbol))
    {
        return *value_binding;
    }

    return Value::error();
}

Value execute_runtime_for_with_vm(const std::vector<Value>& items,
                                  const Environment& env,
                                  bool signal_context,
                                  const TemporalContext& ctx,
                                  int depth)
{
    if (items.size() < 4 || !items[1].is_symbol())
    {
        return Value::error();
    }

    const Value collection =
        execute_runtime_expr_with_vm_impl(items[2], env, signal_context, ctx,
                                          depth + 1);
    if (collection.is_error() || !collection.is_sequential())
    {
        return Value::error();
    }

    Value result = Value::nil();
    for (const Value& element : collection.as_sequential())
    {
        Environment loop_env(env);
        TemporalContext loop_ctx = ctx;
        loop_env.set_temporal_context(&loop_ctx);
        loop_env.set(items[1].as_atom(), element);
        loop_env.unset_expr(items[1].as_atom());

        for (size_t i = 3; i < items.size(); ++i)
        {
            result = execute_runtime_expr_with_vm_impl(items[i], loop_env,
                                                       signal_context, loop_ctx,
                                                       depth + 1);
            if (result.is_error())
            {
                return result;
            }
        }
    }

    return result;
}

Value execute_runtime_while_with_vm(const std::vector<Value>& items,
                                    const Environment& env,
                                    bool signal_context,
                                    const TemporalContext& ctx,
                                    int depth)
{
    if (items.size() < 3)
    {
        return Value::error();
    }

    Environment loop_env(env);
    TemporalContext loop_ctx = ctx;
    loop_env.set_temporal_context(&loop_ctx);

    Value result = Value::nil();
    for (int iteration = 0; iteration < kRuntimeVmWhileMaxIterations; ++iteration)
    {
        const Value condition =
            execute_runtime_expr_with_vm_impl(items[1], loop_env, signal_context,
                                              loop_ctx, depth + 1);
        if (condition.is_error())
        {
            return condition;
        }
        if (!is_truthy(condition))
        {
            return result;
        }

        for (size_t i = 2; i < items.size(); ++i)
        {
            result = execute_runtime_expr_with_vm_impl(items[i], loop_env,
                                                       signal_context, loop_ctx,
                                                       depth + 1);
            if (result.is_error())
            {
                return result;
            }
        }
    }

    return Value::error();
}

Value execute_runtime_expr_with_vm_impl(const Value& expr,
                                        const Environment& env,
                                        bool signal_context,
                                        const TemporalContext& ctx,
                                        int depth)
{
    if (depth > kRuntimeVmBridgeMaxDepth)
    {
        return Value::error();
    }

    if (expr.is_symbol())
    {
        return execute_runtime_symbol_with_vm(expr.as_atom(), env, signal_context,
                                              ctx, depth);
    }

    std::vector<Value> lambda_params;
    Value lambda_body;
    if (try_parse_lambda_expr(expr, lambda_params, lambda_body))
    {
        Environment lambda_env(env);
        TemporalContext lambda_ctx = ctx;
        lambda_env.set_temporal_context(&lambda_ctx);
        return Value(lambda_params, lambda_body, lambda_env);
    }

    if (expr.is_list())
    {
        const std::vector<Value> items = expr.as_list();
        if (!items.empty() && items[0].is_symbol())
        {
            const String op = items[0].as_atom();
            if (op == "for")
            {
                return execute_runtime_for_with_vm(items, env, signal_context, ctx,
                                                   depth);
            }
            if (op == "while")
            {
                return execute_runtime_while_with_vm(items, env, signal_context,
                                                     ctx, depth);
            }
        }
    }

    const NumericVmCompileResult compile_result =
        compile_numeric_program(expr, env, signal_context);
    if (!compile_result.ok)
    {
        return Value::error();
    }

    const TaggedVmExecutionResult execution_result =
        execute_tagged_program(compile_result.program, ctx);
    if (!execution_result.ok)
    {
        return Value::error();
    }

    return execution_result.value;
}
} // namespace

double tri_wave(double duty, double phase)
{
    duty = std::clamp(duty, 0.01, 0.99);
    if (phase > duty)
    {
        phase = duty - ((phase - duty) * (duty / (1.0 - duty)));
    }
    return phase / duty;
}

// Classify whether a compiled program uses only numeric-safe opcodes.
// Programs that avoid type-polymorphic opcodes (list/string/nil introspection,
// boolean logic on arbitrary types, list construction) can execute with an
// unboxed double register file for significantly lower overhead on the hot path.
bool classify_numeric_only(const NumericVmProgram& program)
{
    // All constants must be numeric
    for (const auto& c : program.constants)
    {
        if (!c.is_number())
        {
            return false;
        }
    }

    // Scan instructions for type-polymorphic opcodes
    for (const auto& insn : program.instructions)
    {
        switch (insn.opcode)
        {
        case NumericVmOpcode::IS_NIL:
        case NumericVmOpcode::IS_NUMBER:
        case NumericVmOpcode::IS_LIST:
        case NumericVmOpcode::IS_STRING:
        case NumericVmOpcode::NOT:
        case NumericVmOpcode::AND:
        case NumericVmOpcode::OR:
        case NumericVmOpcode::MAKE_LIST:
        case NumericVmOpcode::MAKE_VECTOR:
        case NumericVmOpcode::LIST_HEAD:
        case NumericVmOpcode::LIST_TAIL:
        case NumericVmOpcode::LIST_LENGTH:
            return false;
        default:
            break;
        }
    }

    // Recursively check sub-programs (callees)
    for (const auto& fn : program.functions)
    {
        if (fn && !classify_numeric_only(*fn))
        {
            return false;
        }
    }

    return true;
}

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
            result.diagnostics = m_diagnostics;
            return result;
        }

        NumericVmInstruction ret;
        ret.opcode = NumericVmOpcode::RET;
        ret.rs1 = static_cast<uint16_t>(out_reg);
        m_program.instructions.push_back(ret);
        m_program.register_count = m_next_register;

        // ok is false if any diagnostic is an error — even if compilation
        // "succeeded" via report_and_continue with placeholder values
        bool has_errors = false;
        for (const auto& d : m_diagnostics)
        {
            if (d.severity == DiagnosticSeverity::Error)
            {
                has_errors = true;
                break;
            }
        }
        result.ok = !has_errors;
        result.program = m_program;
        result.diagnostics = m_diagnostics;
        if (has_errors && m_error.length() == 0 && !m_diagnostics.empty())
        {
            result.error = m_diagnostics[0].message;
        }

        // Determine if the program is numeric-only (no type-polymorphic opcodes).
        // A numeric-only program can use unboxed double registers for faster execution.
        if (result.ok)
        {
            result.program.is_numeric_only = classify_numeric_only(result.program);
        }

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
                // Pin so the preserved register cannot be recycled
                pin_register(preserved);
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
            return compile_vector_literal(expr.as_vector(), transform);
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
                report(DiagnosticSeverity::Error, DiagnosticCategory::Syntax,
                       expr.span, "this expression can't be used inside an output",
                       "Use a built-in function or a named value",
                       "(a1 (sin beat))");
            }
            return -1;
        }

        const String op = items[0].as_atom();
        if (is_signal_side_effect_form(op))
        {
            if (op == "eval")
            {
                report(DiagnosticSeverity::Error, DiagnosticCategory::Boundary,
                       items[0].span, "can't use eval inside an output",
                       "Write the expression directly: (a1 (sin beat))");
            }
            else
            {
                report(DiagnosticSeverity::Error, DiagnosticCategory::Boundary,
                       items[0].span, "can't use " + op + " inside an output — " + op + " is a one-time action",
                       "Use " + op + " at the top level");
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
            if (items.size() == 3)
            {
                return compile_tri(items, transform);
            }
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
                return report_and_continue(DiagnosticCategory::Type,
                       items[0].span, "input needs a fixed channel number",
                       "Try: (input 0)");
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
            return compile_and(items, transform);
        }
        if (op == "or")
        {
            return compile_or(items, transform);
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
            report(DiagnosticSeverity::Error, DiagnosticCategory::Runtime,
                   SourceSpan{}, symbol + " refers back to itself — this would loop forever",
                   "Check for accidental recursion");
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

        // Symbol not found at compile time — emit late-bound fallback
        // and check for fuzzy matches to help catch typos
        {
            const String match = find_fuzzy_match(symbol, m_env);
            if (match.length() > 0)
            {
                report(DiagnosticSeverity::Hint, DiagnosticCategory::UndefinedName,
                       SourceSpan{},
                       "I don't recognise \"" + symbol + "\"",
                       "Did you mean " + match + "?");
            }
        }
        return emit_late_bound_symbol(symbol, transform);
    }

    int compile_time_warp(const String& op,
                          const std::vector<Value>& items,
                          const AffineTimeTransform& transform)
    {
        if (items.size() != 3)
        {
            return report_and_continue(DiagnosticCategory::Arity,
                   items[0].span, op + " takes 2 values, but got " + String(items.size() - 1),
                   "Try: (" + op + " 2 beat)");
        }

        const std::optional<double> factor_or_offset =
            try_resolve_numeric_constant(items[1]);
        if (!factor_or_offset.has_value())
        {
            return report_and_continue(DiagnosticCategory::Type,
                   items[0].span, op + " needs a fixed number as its first value",
                   "Try: (" + op + " 2 beat)");
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
            return report_and_continue(DiagnosticCategory::Arity,
                   items[0].span, "do needs at least one expression inside it",
                   "Try: (do (sin beat) (cos beat))");
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
            return report_and_continue(DiagnosticCategory::Arity,
                   items[0].span, "if needs exactly 3 parts: a condition, a then-value, and an else-value",
                   "Try: (if (> beat 0.5) 1 0)");
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

    int compile_and(const std::vector<Value>& items,
                    const AffineTimeTransform& transform)
    {
        // (and) with no args → truthy (1.0)
        if (items.size() == 1)
        {
            return emit_const(Value(1.0));
        }

        // (and x) with one arg → just return x
        if (items.size() == 2)
        {
            return compile_expr(items[1], transform);
        }

        // Variadic: (and a b c ...)
        // Evaluate left-to-right, short-circuit at the first falsy value.
        const int result_reg = allocate_register();
        std::vector<size_t> branch_to_end_indices;

        for (size_t i = 1; i < items.size(); ++i)
        {
            const int operand_reg = compile_expr(items[i], transform);
            if (operand_reg < 0)
            {
                return -1;
            }
            emit_mov(result_reg, operand_reg);

            // For all operands except the last, branch to end if falsy.
            if (i < items.size() - 1)
            {
                NumericVmInstruction branch;
                branch.opcode = NumericVmOpcode::BRANCH_UNLESS;
                branch.rs1 = static_cast<uint16_t>(result_reg);
                branch.imm = 0;
                branch_to_end_indices.push_back(m_program.instructions.size());
                m_program.instructions.push_back(branch);
            }
        }

        // Patch all forward branches to point here (past the last operand).
        const size_t end_index = m_program.instructions.size();
        for (const size_t idx : branch_to_end_indices)
        {
            patch_branch(idx, end_index);
        }
        return result_reg;
    }

    int compile_or(const std::vector<Value>& items,
                   const AffineTimeTransform& transform)
    {
        // (or) with no args → falsy (0.0)
        if (items.size() == 1)
        {
            return emit_const(Value(0.0));
        }

        // (or x) with one arg → just return x
        if (items.size() == 2)
        {
            return compile_expr(items[1], transform);
        }

        // Variadic: (or a b c ...)
        // Evaluate left-to-right, short-circuit at the first truthy value.
        const int result_reg = allocate_register();
        std::vector<size_t> branch_to_end_indices;

        for (size_t i = 1; i < items.size(); ++i)
        {
            const int operand_reg = compile_expr(items[i], transform);
            if (operand_reg < 0)
            {
                return -1;
            }
            emit_mov(result_reg, operand_reg);

            // For all operands except the last, branch to end if truthy.
            if (i < items.size() - 1)
            {
                NumericVmInstruction branch;
                branch.opcode = NumericVmOpcode::BRANCH_IF;
                branch.rs1 = static_cast<uint16_t>(result_reg);
                branch.imm = 0;
                branch_to_end_indices.push_back(m_program.instructions.size());
                m_program.instructions.push_back(branch);
            }
        }

        // Patch all forward branches to point here (past the last operand).
        const size_t end_index = m_program.instructions.size();
        for (const size_t idx : branch_to_end_indices)
        {
            patch_branch(idx, end_index);
        }
        return result_reg;
    }

    int compile_let(const std::vector<Value>& items,
                    const AffineTimeTransform& transform)
    {
        if (items.size() < 3)
        {
            return report_and_continue(DiagnosticCategory::Arity,
                   items[0].span, "let needs names and values paired together, plus a body",
                   "Try: (let (x 1 y 2) (+ x y))");
        }
        if (!items[1].is_sequential())
        {
            return report_and_continue(DiagnosticCategory::Type,
                   items[0].span, "let needs a list of names and values",
                   "Try: (let (x 1 y 2) (+ x y))");
        }

        const std::vector<Value> bindings = items[1].as_sequential();
        if (bindings.size() % 2 != 0)
        {
            return report_and_continue(DiagnosticCategory::Arity,
                   items[0].span, "let needs names and values paired together",
                   "Try: (let (x 1 y 2) (+ x y))");
        }

        push_local_scope();
        for (size_t i = 0; i < bindings.size(); i += 2)
        {
            if (!bindings[i].is_symbol())
            {
                pop_local_scope();
                return report_and_continue(DiagnosticCategory::Type,
                       bindings[i].span, "let names must be words, not numbers or expressions",
                       "Try: (let (x 1) x)");
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
            return report_and_continue(DiagnosticCategory::Arity,
                   items[0].span, "while needs a condition and at least one body expression",
                   "Try: (while (< x 10) (set! x (+ x 1)))");
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

    // Compile (for var collection body...) — unrolls at compile time when the
    // collection resolves to a constant sequence; otherwise falls back to the
    // tree-walker via emit_runtime_eval.
    int compile_for(const std::vector<Value>& items,
                    const AffineTimeTransform& transform)
    {
        // Form: (for var collection body...)
        if (items.size() < 4)
        {
            return report_and_continue(DiagnosticCategory::Arity,
                   items[0].span, "for needs a variable, a collection, and a body",
                   "Try: (for x [1 2 3] (* x 2))");
        }

        if (!items[1].is_symbol())
        {
            return report_and_continue(DiagnosticCategory::Type,
                   items[1].span, "for needs a variable name as its first argument",
                   "Try: (for x [1 2 3] (* x 2))");
        }

        const String var_name = items[1].as_atom();

        // Maximum unroll size to avoid code explosion.
        constexpr size_t MAX_FOR_UNROLL = 64;

        // --- Try to resolve collection as an all-numeric sequence ---
        std::optional<std::vector<double>> numeric_seq =
            try_resolve_numeric_sequence(items[2]);

        if (numeric_seq.has_value() && !numeric_seq->empty()
            && numeric_seq->size() <= MAX_FOR_UNROLL)
        {
            int result_reg = -1;

            for (size_t i = 0; i < numeric_seq->size(); ++i)
            {
                push_local_scope();
                int val_reg = emit_const(Value((*numeric_seq)[i]));
                bind_local_value(var_name, val_reg);

                // Compile all body expressions; last one is the iteration result.
                int body_result = -1;
                for (size_t j = 3; j < items.size(); ++j)
                {
                    body_result = compile_expr(items[j], transform);
                    if (body_result < 0)
                    {
                        pop_local_scope();
                        // Body failed to compile — fall back to runtime.
                        return emit_runtime_eval(Value(items), transform);
                    }
                }

                if (result_reg < 0)
                {
                    result_reg = allocate_register();
                }
                emit_mov(result_reg, body_result);
                pop_local_scope();
            }

            return result_reg;
        }

        // --- Try to resolve collection as a literal vector of constants ---
        if (items[2].is_vector())
        {
            const std::vector<Value> elements = items[2].as_vector();
            bool all_constant = true;
            for (const Value& elem : elements)
            {
                if (!try_resolve_numeric_constant(elem).has_value()
                    && !elem.is_string() && !elem.is_nil())
                {
                    all_constant = false;
                    break;
                }
            }

            if (all_constant && !elements.empty()
                && elements.size() <= MAX_FOR_UNROLL)
            {
                int result_reg = -1;

                for (const Value& elem : elements)
                {
                    push_local_scope();
                    int val_reg = emit_const(elem);
                    bind_local_value(var_name, val_reg);

                    int body_result = -1;
                    for (size_t j = 3; j < items.size(); ++j)
                    {
                        body_result = compile_expr(items[j], transform);
                        if (body_result < 0)
                        {
                            pop_local_scope();
                            return emit_runtime_eval(Value(items), transform);
                        }
                    }

                    if (result_reg < 0)
                    {
                        result_reg = allocate_register();
                    }
                    emit_mov(result_reg, body_result);
                    pop_local_scope();
                }

                return result_reg;
            }
        }

        // Can't resolve at compile time — fall back to tree-walker.
        return emit_runtime_eval(Value(items), transform);
    }

    int compile_vector_builtin(const std::vector<Value>& items,
                               const AffineTimeTransform& transform,
                               bool interpolate)
    {
        if (items.size() != 3)
        {
            return report_and_continue(DiagnosticCategory::Arity,
                   items[0].span, String(interpolate ? "interp" : "from-list") +
                   " needs a list and a timing signal",
                   String("Try: (") + (interpolate ? "interp" : "from-list") + " [0 0.5 1] beat)");
        }

        return compile_vector_lookup(items[1], items[2], transform, interpolate);
    }

    int compile_dm(const std::vector<Value>& items,
                   const AffineTimeTransform& transform)
    {
        if (items.size() != 4)
        {
            return report_and_continue(DiagnosticCategory::Arity,
                   items[0].span, "dm needs 3 values: a condition, a default, and a value",
                   "Try: (dm (> beat 0.5) 0 1)");
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
            return report_and_continue(DiagnosticCategory::Arity,
                   items[0].span, "euclid needs at least 3 values: hits, steps, and a timing signal",
                   "Try: (euclid 3 8 beat)",
                   "(euclid 3 8 beat)");
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
        return report_and_continue(DiagnosticCategory::Runtime,
               items[0].span, "something went wrong compiling euclid — try simplifying the expression",
               "Try: (euclid 3 8 beat)");
    }

    int compile_vector_literal(const std::vector<Value>& elements,
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
        insn.opcode = NumericVmOpcode::MAKE_VECTOR;
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

    // Check if a Value is a non-numeric constant that will fail in arithmetic
    void warn_if_non_numeric(const Value& arg, const String& op_name)
    {
        if (arg.is_string())
        {
            report(DiagnosticSeverity::Error, DiagnosticCategory::Type,
                   arg.span,
                   "can't do maths with text — " + op_name + " needs numbers",
                   "Remove the quotes if you meant a number");
        }
        else if (arg.is_list() && !arg.list.empty() && !arg.list[0].is_symbol())
        {
            report(DiagnosticSeverity::Error, DiagnosticCategory::Type,
                   arg.span,
                   op_name + " needs numbers, not a list",
                   "Use a number or an expression that produces a number");
        }
    }

    int compile_fold(const std::vector<Value>& items,
                     size_t arg_start,
                     NumericVmOpcode opcode,
                     const AffineTimeTransform& transform,
                     bool allow_single_arg_identity)
    {
        if (items.size() <= arg_start)
        {
            return report_and_continue(DiagnosticCategory::Arity,
                   items[0].span, items[0].as_atom() + " needs at least 2 values",
                   "Try: (" + items[0].as_atom() + " 1 2)");
        }

        if (items.size() == arg_start + 1)
        {
            if (allow_single_arg_identity)
            {
                return compile_expr(items[arg_start], transform);
            }
            return report_and_continue(DiagnosticCategory::Arity,
                   items[0].span, items[0].as_atom() + " needs at least 2 values",
                   "Try: (" + items[0].as_atom() + " 1 2)");
        }

        // Type-check arguments at compile time
        for (size_t i = arg_start; i < items.size(); ++i)
        {
            warn_if_non_numeric(items[i], items[0].as_atom());
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
            return report_and_continue(DiagnosticCategory::Arity,
                   items[0].span, items[0].as_atom() + " needs exactly 2 values",
                   "Try: (" + items[0].as_atom() + " 1 2)");
        }

        // Type-check arguments at compile time
        warn_if_non_numeric(items[1], items[0].as_atom());
        warn_if_non_numeric(items[2], items[0].as_atom());

        const int lhs = compile_expr(items[1], transform);
        const int rhs = compile_expr(items[2], transform);
        if (lhs < 0 || rhs < 0)
        {
            return -1;
        }
        return emit_binary(opcode, lhs, rhs);
    }

    int compile_tri(const std::vector<Value>& items,
                    const AffineTimeTransform& transform)
    {
        if (items.size() != 3)
        {
            return report_and_continue(DiagnosticCategory::Arity,
                   items[0].span, "tri needs a duty cycle and a signal",
                   "Try: (tri 0.5 beat)");
        }

        const int duty = compile_expr(items[1], transform);
        const int phase = compile_expr(items[2], transform);
        if (duty < 0 || phase < 0)
        {
            return -1;
        }

        NumericVmInstruction insn;
        insn.opcode = NumericVmOpcode::TRI;
        insn.rd = static_cast<uint16_t>(duty);
        insn.rs1 = static_cast<uint16_t>(duty);
        insn.rs2 = static_cast<uint16_t>(phase);
        m_program.instructions.push_back(insn);
        return duty;
    }

    int compile_unary(const std::vector<Value>& items,
                      NumericVmOpcode opcode,
                      const AffineTimeTransform& transform)
    {
        if (items.size() != 2)
        {
            return report_and_continue(DiagnosticCategory::Arity,
                   items[0].span, items[0].as_atom() + " needs a value to work with",
                   "Try: (" + items[0].as_atom() + " beat)");
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
            return report_and_continue(DiagnosticCategory::Arity,
                   items[0].span, "clamp needs exactly 3 values: a value, a minimum, and a maximum",
                   "Try: (clamp beat 0 1)");
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
            return report_and_continue(DiagnosticCategory::Arity,
                   items[0].span, items[0].as_atom() + " needs a value to convert",
                   "Try: (" + items[0].as_atom() + " beat)");
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

        return report_and_continue(DiagnosticCategory::Arity,
               items[0].span, "scale needs 3 or 5 values: output range and a signal",
               "Try: (scale 0 5 beat)",
               "(scale 0 5 beat)");
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
            return report_and_continue(DiagnosticCategory::Arity,
                   items[0].span, "lerp needs 3 or 5 values: start, end, and a position",
                   "Try: (lerp 0 1 beat)");
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
            [env = &m_env, symbol, signal_context = m_signal_context](const std::vector<Value>&,
                                   const TemporalContext& ctx) -> Value {
                Environment exec_env(*env);
                TemporalContext exec_ctx = ctx;
                exec_env.set_temporal_context(&exec_ctx);
                return execute_runtime_symbol_with_vm(symbol, exec_env, signal_context,
                                                      exec_ctx, 0);
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
                    report(DiagnosticSeverity::Error, DiagnosticCategory::Boundary,
                           expr.span, "can't use eval inside an output",
                           "Write the expression directly: (a1 (sin beat))");
                }
                else
                {
                    report(DiagnosticSeverity::Error, DiagnosticCategory::Boundary,
                           expr.span, "can't use " + *side_effect + " inside an output — " + *side_effect + " is a one-time action",
                           "Use " + *side_effect + " at the top level");
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
            [env = &m_env, runtime_expr = expr, transform, signal_context = m_signal_context, local_values,
             local_callables](const std::vector<Value>& args,
                              const TemporalContext& ctx) -> Value {
                TemporalContext exec_ctx = apply_time_transform(ctx, transform);
                Environment exec_env(*env);
                exec_env.set_temporal_context(&exec_ctx);

                for (size_t i = 0; i < local_values.size() && i < args.size(); ++i)
                {
                    exec_env.set(local_values[i].first, args[i]);
                    exec_env.unset_expr(local_values[i].first);
                }

                for (const auto& callable : local_callables)
                {
                    if (callable.second.type == Value::LAMBDA)
                    {
                        exec_env.set(callable.first, callable.second);
                        continue;
                    }

                    std::vector<Value> params;
                    Value body;
                    if (try_parse_lambda_expr(callable.second, params, body))
                    {
                        exec_env.set(callable.first, Value(params, body, exec_env));
                        continue;
                    }
                }

                return execute_runtime_expr_with_vm_impl(runtime_expr, exec_env,
                                                         signal_context, exec_ctx, 0);
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
        // rhs is dead after the binary op — result lives in lhs
        if (rhs != lhs)
        {
            release_register(rhs);
        }
        return lhs;
    }

    int allocate_register()
    {
        if (!m_free_registers.empty())
        {
            int reg = m_free_registers.back();
            m_free_registers.pop_back();
            return reg;
        }
        return m_next_register++;
    }

    void release_register(int reg)
    {
        if (reg < 0) return;
        if (m_pinned_registers.count(reg)) return;
        m_free_registers.push_back(reg);
    }

    void pin_register(int reg)
    {
        if (reg >= 0)
        {
            m_pinned_registers.insert(reg);
        }
    }

    size_t store_intrinsic(const TaggedVmIntrinsic& intrinsic)
    {
        m_program.intrinsics.push_back(intrinsic);
        return m_program.intrinsics.size() - 1;
    }

    size_t store_constant(const Value& value)
    {
        const uint64_t h = hash_value(value);
        auto it = m_constant_hash_index.find(h);
        if (it != m_constant_hash_index.end())
        {
            for (size_t idx : it->second)
            {
                if (m_program.constants[idx] == value)
                {
                    return idx;
                }
            }
        }

        const size_t new_idx = m_program.constants.size();
        m_program.constants.push_back(value);
        m_constant_hash_index[h].push_back(new_idx);
        return new_idx;
    }

    size_t store_data_segment(const std::vector<double>& values)
    {
        const uint64_t h = hash_double_vector(values);
        auto it = m_data_segment_hash_index.find(h);
        if (it != m_data_segment_hash_index.end())
        {
            for (size_t idx : it->second)
            {
                if (m_program.data_segments[idx] == values)
                {
                    return idx;
                }
            }
        }

        const size_t new_idx = m_program.data_segments.size();
        m_program.data_segments.push_back(values);
        m_data_segment_hash_index[h].push_back(new_idx);
        return new_idx;
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
        auto checked = [](double value) -> std::optional<double> {
            if (!std::isfinite(value))
            {
                return std::nullopt;
            }
            return value;
        };

        if (expr.is_number())
        {
            return expr.as_float();
        }

        if (expr.is_symbol())
        {
            const String symbol = expr.as_atom();
            // Local bindings (let, lambda params, closure captures) shadow
            // global definitions. If a local binding exists, the value is
            // in a register — not a compile-time constant.
            if (find_local_value_binding(symbol).has_value())
            {
                return std::nullopt;
            }
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
            return checked(fn(*arg));
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
            return checked(fn(*lhs, *rhs));
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
            if (items.size() != 3)
            {
                return std::nullopt;
            }
            const std::optional<double> rhs_value =
                try_resolve_numeric_constant(items[2]);
            if (!rhs_value.has_value() || *rhs_value == 0.0)
            {
                return std::nullopt;
            }
            return binary([](double lhs, double rhs) { return lhs / rhs; });
        }
        if (op == "%")
        {
            if (items.size() != 3)
            {
                return std::nullopt;
            }
            const std::optional<double> rhs_value =
                try_resolve_numeric_constant(items[2]);
            if (!rhs_value.has_value() || *rhs_value == 0.0)
            {
                return std::nullopt;
            }
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
            // ModuLisp convention: (pow exponent base) → base^exponent
            return binary([](double lhs, double rhs) { return std::pow(rhs, lhs); });
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
            if (items.size() != 3)
            {
                return std::nullopt;
            }
            const std::optional<double> duty =
                try_resolve_numeric_constant(items[1]);
            const std::optional<double> phase =
                try_resolve_numeric_constant(items[2]);
            if (!duty.has_value() || !phase.has_value())
            {
                return std::nullopt;
            }
            return checked(tri_wave(*duty, *phase));
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
            return std::nullopt;
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
            if (items.size() != 2)
            {
                return std::nullopt;
            }
            const std::optional<double> arg = try_resolve_numeric_constant(items[1]);
            if (!arg.has_value() || *arg < 0.0)
            {
                return std::nullopt;
            }
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
            report(DiagnosticSeverity::Error, DiagnosticCategory::Syntax,
                   SourceSpan{}, "empty parentheses () — did you forget the function name?",
                   "Try: (sin beat)");
            return false;
        }

        std::vector<Value> params;
        Value body;
        if (items[0].is_symbol())
        {
            const String op = items[0].as_atom();
            if (m_current_function_name.has_value() && op == *m_current_function_name)
            {
                return emit_recursive_self_call(
                    std::vector<Value>(items.begin() + 1, items.end()),
                    transform, out_reg);
            }
            if (const std::optional<Value> local_callable =
                    find_local_callable_binding(op))
            {
                if (parse_lambda_value(*local_callable, params, body))
                {
                    if (should_compile_lambda_as_function(std::nullopt,
                                                          *local_callable, body))
                    {
                        return emit_compiled_lambda_call(
                            std::nullopt, *local_callable, params, body,
                            std::vector<Value>(items.begin() + 1, items.end()),
                            transform, out_reg);
                    }
                    if (lambda_has_captured_scope(*local_callable))
                    {
                        out_reg = compile_closure_inline(
                            *local_callable, params, body,
                            std::vector<Value>(items.begin() + 1,
                                               items.end()),
                            transform);
                    }
                    else
                    {
                        out_reg = compile_inline_lambda(
                            params, body,
                            std::vector<Value>(items.begin() + 1,
                                               items.end()),
                            transform);
                    }
                    return out_reg >= 0;
                }
            }
            if (const std::optional<Value> global_callable = m_env.get(op))
            {
                if (parse_lambda_value(*global_callable, params, body))
                {
                    maybe_add_dependency(op);
                    const std::set<String> used_atoms = body.get_used_atoms();
                    const std::optional<String> function_name =
                        used_atoms.find(op) != used_atoms.end()
                            ? std::optional<String>(op)
                            : std::nullopt;
                    if (should_compile_lambda_as_function(function_name, *global_callable,
                                                          body))
                    {
                        return emit_compiled_lambda_call(
                            function_name, *global_callable, params, body,
                            std::vector<Value>(items.begin() + 1, items.end()),
                            transform, out_reg);
                    }
                    if (lambda_has_captured_scope(*global_callable))
                    {
                        out_reg = compile_closure_inline(
                            *global_callable, params, body,
                            std::vector<Value>(items.begin() + 1,
                                               items.end()),
                            transform);
                    }
                    else
                    {
                        out_reg = compile_inline_lambda(
                            params, body,
                            std::vector<Value>(items.begin() + 1,
                                               items.end()),
                            transform);
                    }
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
            Value callable_value(params, body, m_env);
            if (should_compile_lambda_as_function(std::nullopt, callable_value,
                                                  body))
            {
                return emit_compiled_lambda_call(
                    std::nullopt, callable_value, params, body,
                    std::vector<Value>(items.begin() + 1, items.end()),
                    transform, out_reg);
            }
            out_reg = compile_inline_lambda(params, body,
                                            std::vector<Value>(items.begin() + 1,
                                                               items.end()),
                                            transform);
            return out_reg >= 0;
        }

        return false;
    }

    // Compile a closure call by injecting captured bindings as locals,
    // then inlining the lambda body. Works because signal context has no
    // mutation — captured values are frozen at definition time.
    int compile_closure_inline(const Value& callable,
                               const std::vector<Value>& params,
                               const Value& body,
                               const std::vector<Value>& arg_exprs,
                               const AffineTimeTransform& transform)
    {
        push_local_scope();

        // Bind captured values as local constants or callables
        for (const auto& entry : callable.lambda_scope->get_defs())
        {
            if (entry.second.is_builtin())
            {
                continue;
            }

            if (entry.second.type == Value::LAMBDA)
            {
                bind_local_callable(entry.first, entry.second);
            }
            else
            {
                const int reg = emit_const(entry.second);
                bind_local_value(entry.first, reg);
            }
        }

        // Bind captured expression bindings (signal expressions that
        // reference time/inputs — compile them inline)
        for (const auto& entry : callable.lambda_scope->get_def_exprs())
        {
            // If it's a callable expression, bind as callable
            std::vector<Value> expr_params;
            Value expr_body;
            if (parse_lambda_value(entry.second, expr_params, expr_body))
            {
                bind_local_callable(entry.first, entry.second);
                continue;
            }

            const int reg = compile_expr(entry.second, transform);
            if (reg < 0)
            {
                pop_local_scope();
                return -1;
            }
            bind_local_value(entry.first, reg);
        }

        // Now inline as normal — captures are visible as locals
        const int result =
            compile_inline_lambda(params, body, arg_exprs, transform);
        pop_local_scope();
        return result;
    }

    size_t count_expr_nodes(const Value& expr) const
    {
        if (expr.is_list())
        {
            size_t total = 1;
            for (const Value& item : expr.as_list())
            {
                total += count_expr_nodes(item);
            }
            return total;
        }
        if (expr.is_vector())
        {
            size_t total = 1;
            for (const Value& item : expr.as_vector())
            {
                total += count_expr_nodes(item);
            }
            return total;
        }
        if (expr.type == Value::QUOTE && !expr.list.empty())
        {
            return 1 + count_expr_nodes(expr.list[0]);
        }
        return 1;
    }

    bool should_compile_lambda_as_function(const std::optional<String>& self_name,
                                           const Value& callable,
                                           const Value& body) const
    {
        const std::set<String> used_atoms = body.get_used_atoms();
        const bool recursive =
            self_name.has_value() && used_atoms.find(*self_name) != used_atoms.end();
        if (recursive)
        {
            return true;
        }

        if (count_expr_nodes(body) > kInlineLambdaNodeThreshold)
        {
            return true;
        }

        if (lambda_has_captured_scope(callable) &&
            count_expr_nodes(body) > (kInlineLambdaNodeThreshold / 2))
        {
            return true;
        }

        return false;
    }

    bool collect_function_capture_arguments(
        const Value& callable,
        const AffineTimeTransform& transform,
        std::vector<String>& capture_names,
        std::vector<int>& capture_regs,
        std::vector<std::pair<String, Value>>& callable_captures)
    {
        if (callable.type != Value::LAMBDA || !callable.lambda_scope)
        {
            return true;
        }

        for (const auto& entry : callable.lambda_scope->get_defs())
        {
            if (entry.second.is_builtin())
            {
                continue;
            }

            if (entry.second.type == Value::LAMBDA)
            {
                callable_captures.push_back(entry);
                continue;
            }

            capture_names.push_back(entry.first);
            capture_regs.push_back(emit_const(entry.second));
        }

        for (const auto& entry : callable.lambda_scope->get_def_exprs())
        {
            std::vector<Value> expr_params;
            Value expr_body;
            if (parse_lambda_value(entry.second, expr_params, expr_body))
            {
                callable_captures.push_back(entry);
                continue;
            }

            const int reg = compile_expr(entry.second, transform);
            if (reg < 0)
            {
                return false;
            }

            capture_names.push_back(entry.first);
            capture_regs.push_back(reg);
        }

        return true;
    }

    std::shared_ptr<NumericVmProgram> compile_lambda_function_program(
        const std::optional<String>& self_name,
        const std::vector<String>& capture_names,
        const std::vector<std::pair<String, Value>>& callable_captures,
        const std::vector<Value>& params,
        const Value& body,
        const AffineTimeTransform& transform)
    {
        auto function_program = std::make_shared<NumericVmProgram>();

        NumericVmCompiler child(m_env, m_signal_context);
        child.push_local_scope();

        int next_param_reg = 0;
        for (const String& capture_name : capture_names)
        {
            child.bind_local_value(capture_name, next_param_reg++);
        }
        for (const auto& callable_capture : callable_captures)
        {
            child.bind_local_callable(callable_capture.first, callable_capture.second);
        }
        for (const Value& param : params)
        {
            if (!param.is_symbol())
            {
                report(DiagnosticSeverity::Error, DiagnosticCategory::Type,
                       param.span,
                       "function parameter names must be words, not numbers or expressions",
                       "Try: (fn (x y) (+ x y))");
                return nullptr;
            }
            child.bind_local_value(param.as_atom(), next_param_reg++);
        }

        if (self_name.has_value())
        {
            child.m_current_function_name = *self_name;
            child.m_current_function_capture_names = capture_names;
        }

        child.m_next_register = std::max(child.m_next_register, next_param_reg);
        const int result_reg = child.compile_expr(body, transform);
        child.pop_local_scope();
        if (result_reg < 0)
        {
            if (m_error.length() == 0)
            {
                m_error = child.m_error;
            }
            m_diagnostics.insert(m_diagnostics.end(), child.m_diagnostics.begin(),
                                 child.m_diagnostics.end());
            return nullptr;
        }

        NumericVmInstruction ret;
        ret.opcode = NumericVmOpcode::RET;
        ret.rs1 = static_cast<uint16_t>(result_reg);
        child.m_program.instructions.push_back(ret);
        child.m_program.register_count = child.m_next_register;
        child.m_program.is_numeric_only = classify_numeric_only(child.m_program);

        *function_program = child.m_program;
        return function_program;
    }

    bool emit_compiled_lambda_call(const std::optional<String>& self_name,
                                   const Value& callable,
                                   const std::vector<Value>& params,
                                   const Value& body,
                                   const std::vector<Value>& arg_exprs,
                                   const AffineTimeTransform& transform,
                                   int& out_reg)
    {
        if (params.size() != arg_exprs.size())
        {
            out_reg = report_and_continue(
                DiagnosticCategory::Arity, body.span,
                "this function takes " + String(params.size()) + " values, but got " +
                    String(arg_exprs.size()),
                "Check that you're passing the right number of values");
            return true;
        }

        std::vector<String> capture_names;
        std::vector<int> capture_regs;
        std::vector<std::pair<String, Value>> callable_captures;
        if (!collect_function_capture_arguments(callable, transform, capture_names,
                                                capture_regs, callable_captures))
        {
            return false;
        }

        std::vector<int> arg_regs = capture_regs;
        for (const Value& arg_expr : arg_exprs)
        {
            const int arg_reg = compile_expr(arg_expr, transform);
            if (arg_reg < 0)
            {
                return false;
            }
            arg_regs.push_back(arg_reg);
        }

        auto function_program = compile_lambda_function_program(
            self_name, capture_names, callable_captures, params, body, transform);
        if (!function_program)
        {
            return false;
        }

        const int first_arg_reg = allocate_argument_window(arg_regs);
        const size_t function_index = m_program.functions.size();
        m_program.functions.push_back(function_program);

        out_reg = allocate_register();
        NumericVmInstruction insn;
        insn.opcode = NumericVmOpcode::CALL;
        insn.rd = static_cast<uint16_t>(out_reg);
        insn.rs1 = static_cast<uint16_t>(first_arg_reg >= 0 ? first_arg_reg : 0);
        insn.rs2 = static_cast<uint16_t>(arg_regs.size());
        insn.imm = static_cast<int32_t>(function_index);
        m_program.instructions.push_back(insn);
        return true;
    }

    bool emit_recursive_self_call(const std::vector<Value>& arg_exprs,
                                  const AffineTimeTransform& transform,
                                  int& out_reg)
    {
        if (!m_current_function_name.has_value())
        {
            return false;
        }

        std::vector<int> arg_regs;
        for (const String& capture_name : m_current_function_capture_names)
        {
            const std::optional<int> capture_reg =
                find_local_value_binding(capture_name);
            if (!capture_reg.has_value())
            {
                return false;
            }
            arg_regs.push_back(*capture_reg);
        }

        for (const Value& arg_expr : arg_exprs)
        {
            const int arg_reg = compile_expr(arg_expr, transform);
            if (arg_reg < 0)
            {
                return false;
            }
            arg_regs.push_back(arg_reg);
        }

        const int first_arg_reg = allocate_argument_window(arg_regs);
        out_reg = allocate_register();

        NumericVmInstruction insn;
        insn.opcode = NumericVmOpcode::CALL;
        insn.rd = static_cast<uint16_t>(out_reg);
        insn.rs1 = static_cast<uint16_t>(first_arg_reg >= 0 ? first_arg_reg : 0);
        insn.rs2 = static_cast<uint16_t>(arg_regs.size());
        insn.imm = -1;
        m_program.instructions.push_back(insn);
        return true;
    }

    int compile_inline_lambda(const std::vector<Value>& params,
                              const Value& body,
                              const std::vector<Value>& arg_exprs,
                              const AffineTimeTransform& transform)
    {
        if (params.size() != arg_exprs.size())
        {
            return report_and_continue(DiagnosticCategory::Arity,
                   body.span, "this function takes " + String(params.size()) + " values, but got " + String(arg_exprs.size()),
                   "Check that you're passing the right number of values");
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
                return report_and_continue(DiagnosticCategory::Type,
                       params[i].span, "function parameter names must be words, not numbers or expressions",
                       "Try: (fn (x y) (+ x y))");
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

        // Argument windows need consecutive registers, so always
        // allocate fresh from the top — the free list may have gaps.
        const int first = m_next_register++;
        emit_mov(first, arg_regs[0]);
        for (size_t i = 1; i < arg_regs.size(); ++i)
        {
            const int dst = m_next_register++;
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
        // Unpin registers bound to locals in the scope being popped
        if (!m_local_value_scopes.empty())
        {
            for (const auto& entry : m_local_value_scopes.back())
            {
                m_pinned_registers.erase(entry.second.source_register);
            }
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
                // Unpin CSE-preserved registers from entries being evicted,
                // since their cached values are no longer reachable.
                for (auto it = m_cse_cache.begin(); it != m_cse_cache.end(); ++it)
                {
                    m_pinned_registers.erase(it->second);
                }
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
        // Pin so the register cannot be recycled while the binding is live
        pin_register(source_register);
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

    void report(DiagnosticSeverity severity, DiagnosticCategory category,
                SourceSpan span, const String& message,
                const String& suggestion = "", const String& example = "")
    {
        m_diagnostics.push_back({severity, category, span, message, suggestion, example});
        if (severity == DiagnosticSeverity::Error && m_error.length() == 0)
            m_error = message;  // backward compat
    }

    int report_and_continue(DiagnosticCategory category, SourceSpan span,
                            const String& message, const String& suggestion = "",
                            const String& example = "")
    {
        report(DiagnosticSeverity::Error, category, span, message, suggestion, example);
        int reg = allocate_register();
        NumericVmInstruction insn;
        insn.opcode = NumericVmOpcode::LOAD_CONST;
        insn.rd = static_cast<uint16_t>(reg);
        insn.imm = static_cast<int32_t>(store_constant(Value(0.0)));
        m_program.instructions.push_back(insn);
        return reg;
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
        checkpoint.diagnostic_count = m_diagnostics.size();
        checkpoint.free_register_count = m_free_registers.size();
        checkpoint.pinned_register_count = m_pinned_registers.size();
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
        m_diagnostics.resize(checkpoint.diagnostic_count);
        // Restore free list to checkpointed state
        m_free_registers.resize(checkpoint.free_register_count);
        // Conservatively clear all pins added since checkpoint —
        // the pinned set doesn't support ordered rollback, so
        // rebuild from scratch if pins were added.
        if (m_pinned_registers.size() > checkpoint.pinned_register_count)
        {
            m_pinned_registers.clear();
        }
        // CSE entries added since checkpoint may reference rolled-back
        // registers/instructions — clear them conservatively.
        if (m_cse_cache.size() > checkpoint.cse_cache_size)
        {
            m_cse_cache.clear();
        }
        // Rebuild hash indices after truncation — they are append-only
        // so stale entries would point past the resized vectors.
        rebuild_constant_hash_index();
        rebuild_data_segment_hash_index();
    }

    void rebuild_constant_hash_index()
    {
        m_constant_hash_index.clear();
        for (size_t i = 0; i < m_program.constants.size(); ++i)
        {
            m_constant_hash_index[hash_value(m_program.constants[i])].push_back(i);
        }
    }

    void rebuild_data_segment_hash_index()
    {
        m_data_segment_hash_index.clear();
        for (size_t i = 0; i < m_program.data_segments.size(); ++i)
        {
            m_data_segment_hash_index[hash_double_vector(m_program.data_segments[i])]
                .push_back(i);
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
    std::vector<int> m_free_registers;
    std::set<int> m_pinned_registers;
    String m_error;
    std::vector<Diagnostic> m_diagnostics;
    std::vector<String> m_recursion_stack;
    std::vector<std::map<String, LocalValueBinding>> m_local_value_scopes;
    std::vector<std::map<String, Value>> m_local_callable_scopes;
    std::optional<String> m_current_function_name;
    std::vector<String> m_current_function_capture_names;
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

    // Hash-based indices for O(1) average-case constant/data-segment dedup.
    std::unordered_map<uint64_t, std::vector<size_t>> m_constant_hash_index;
    std::unordered_map<uint64_t, std::vector<size_t>> m_data_segment_hash_index;
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
        error = String(role) + " register out of range";
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
        error = "branch target out of range";
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
        error = "data segment index out of range";
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
        error = "source register out of range";
        return false;
    }

    const Value& value = registers[index];
    if (!value.is_number())
    {
        error = "vector lookup requires a numeric phasor";
        return false;
    }

    phasor = value.as_float();
    return true;
}
static constexpr int VM_MAX_CALL_DEPTH = 64;

// Forward declaration: the fast executor may need to fall back to the tagged
// executor for CALL instructions targeting non-numeric-only sub-programs.
TaggedVmExecutionResult execute_tagged_program_impl(const NumericVmProgram& program,
                                                    const TemporalContext& ctx,
                                                    const std::vector<Value>& initial_registers = {},
                                                    int call_depth = 0);
NumericVmExecutionResult execute_numeric_program_fast(
    const NumericVmProgram& program,
    const TemporalContext& ctx,
    const std::vector<double>& initial_registers = {},
    int call_depth = 0);

// --- Unboxed double executor for numeric-only programs ---
// This executor mirrors the tagged executor but uses std::vector<double> as the
// register file, eliminating all Value boxing/unboxing on the hot sampling path.
// It is only called when program.is_numeric_only is true (no type-polymorphic opcodes).

NumericVmExecutionResult execute_numeric_program_fast_impl(
    const NumericVmProgram& program,
    const TemporalContext& ctx,
    std::vector<double>& registers,
    int call_depth = 0)
{
    NumericVmExecutionResult result;
    if (call_depth > VM_MAX_CALL_DEPTH)
    {
        result.error = "too many nested function calls";
        result.error_category = DiagnosticCategory::Runtime;
        return result;
    }
    if (program.register_count == 0 || program.instructions.empty())
    {
        result.error = "program is empty";
        result.error_category = DiagnosticCategory::Runtime;
        return result;
    }
    if (registers.size() < program.register_count)
    {
        result.error = "register file is too small";
        result.error_category = DiagnosticCategory::Runtime;
        return result;
    }
    size_t backward_branch_count = 0;

#if defined(__GNUC__) && !defined(WASM_BUILD) && defined(USE_COMPUTED_GOTO)
#define VM_FAST_USE_COMPUTED_GOTO 1
#else
#define VM_FAST_USE_COMPUTED_GOTO 0
#endif

#if VM_FAST_USE_COMPUTED_GOTO
    // Dispatch table for numeric-only opcodes (same opcode indices as tagged executor).
    // Opcodes that should never appear in numeric-only programs (IS_NIL, etc.) fall
    // through to vm_fast_loop_exit via op_fast_INVALID.
    static const void* fast_dispatch_table[] = {
        &&op_fast_LOAD_CONST,    &&op_fast_LOAD_TIME,     &&op_fast_MOV,
        &&op_fast_VEC_INDEX,     &&op_fast_VEC_LERP,      &&op_fast_ADD,
        &&op_fast_SUB,           &&op_fast_MUL,           &&op_fast_DIV,
        &&op_fast_MOD,           &&op_fast_NEG,           &&op_fast_CMP_GT,
        &&op_fast_CMP_LT,       &&op_fast_CMP_GE,       &&op_fast_CMP_LE,
        &&op_fast_CMP_EQ,       &&op_fast_FLOOR,         &&op_fast_CEIL,
        &&op_fast_FRAC,          &&op_fast_ABS,           &&op_fast_MIN,
        &&op_fast_MAX,           &&op_fast_POW,           &&op_fast_SQRT,
        &&op_fast_CLAMP,         &&op_fast_SIN,           &&op_fast_COS,
        &&op_fast_TAN,           &&op_fast_BRANCH,        &&op_fast_BRANCH_IF,
        &&op_fast_BRANCH_UNLESS, &&op_fast_CALL,          &&op_fast_CALL_INTRINSIC,
        &&op_fast_RET,           &&op_fast_INVALID,       &&op_fast_INVALID,
        &&op_fast_INVALID,       &&op_fast_INVALID,       &&op_fast_INVALID,
        &&op_fast_INVALID,       &&op_fast_INVALID,       &&op_fast_INVALID,
        &&op_fast_INVALID,       &&op_fast_INVALID,       &&op_fast_INVALID,
        &&op_fast_INVALID,       &&op_fast_U_SIN,         &&op_fast_U_COS,
        &&op_fast_U_SIN_BI,      &&op_fast_U_COS_BI,      &&op_fast_TRI,
        &&op_fast_SQR,           &&op_fast_PULSE,         &&op_fast_LOAD_INPUT
    };
    static_assert(sizeof(fast_dispatch_table) / sizeof(fast_dispatch_table[0]) == 54,
                  "fast_dispatch_table must have one entry per NumericVmOpcode");
    #define VMF_DISPATCH() do { \
        if (pc >= program.instructions.size()) goto vm_fast_loop_exit; \
        insn = program.instructions[pc]; \
        if (static_cast<int>(insn.opcode) < 0 || \
            static_cast<int>(insn.opcode) >= 54) goto vm_fast_loop_exit; \
        goto *fast_dispatch_table[static_cast<int>(insn.opcode)]; \
    } while (0)
    #define VMF_CASE(op) op_fast_##op:
    #define VMF_NEXT() do { ++pc; VMF_DISPATCH(); } while (0)
    #define VMF_BREAK VMF_NEXT()
    #define VMF_BRANCH_DONE() VMF_DISPATCH()
#else
    #define VMF_DISPATCH() continue
    #define VMF_CASE(op) case NumericVmOpcode::op:
    #define VMF_NEXT() { ++pc; continue; }
    #define VMF_BREAK break
    #define VMF_BRANCH_DONE() break
#endif

    for (size_t pc = 0; pc < program.instructions.size();)
    {
#if VM_FAST_USE_COMPUTED_GOTO
        NumericVmInstruction insn{};
        VMF_DISPATCH();

        VMF_CASE(INVALID)
        {
            result.error = "unexpected opcode in numeric-only program";
            result.error_category = DiagnosticCategory::Runtime;
            return result;
        }
#else
        const NumericVmInstruction& insn = program.instructions[pc];
        switch (insn.opcode)
        {
#endif
        VMF_CASE(LOAD_CONST)
            if (insn.imm < 0 ||
                static_cast<size_t>(insn.imm) >= program.constants.size())
            {
                result.error = "constant index out of range";
                result.error_category = DiagnosticCategory::Runtime;
                return result;
            }
            registers[insn.rd] = program.constants[static_cast<size_t>(insn.imm)].as_float();
            VMF_NEXT();
        VMF_CASE(LOAD_TIME)
        {
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
            registers[insn.rd] = value;
            VMF_NEXT();
        }
        VMF_CASE(LOAD_INPUT)
        {
            registers[insn.rd] = ctx.input_at(static_cast<size_t>(std::max(insn.imm, 0)));
            VMF_NEXT();
        }
        VMF_CASE(MOV)
            registers[insn.rd] = registers[insn.rs1];
            VMF_NEXT();
        VMF_CASE(VEC_INDEX)
        VMF_CASE(VEC_LERP)
        {
            const std::vector<double>* data = nullptr;
            if (!load_data_segment(program, insn.imm, data, result.error))
            {
                return result;
            }
            if (!data || data->empty())
            {
                result.error = "data segment is empty";
                result.error_category = DiagnosticCategory::Runtime;
                return result;
            }

            const double phasor = registers[insn.rs1];
            const double clamped_phasor = std::clamp(phasor, 0.0, 1.0);
            const double scaled = clamped_phasor * static_cast<double>(data->size());
            const size_t index =
                std::min(static_cast<size_t>(std::floor(scaled)), data->size() - 1);

            if (insn.opcode == NumericVmOpcode::VEC_INDEX)
            {
                registers[insn.rd] = (*data)[index];
            }
            else
            {
                if (data->size() == 1)
                {
                    registers[insn.rd] = (*data)[0];
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
                        (*data)[base] +
                        (((*data)[base + 1] - (*data)[base]) * fraction);
                }
            }
            VMF_NEXT();
        }
        VMF_CASE(NEG)
        VMF_CASE(FLOOR)
        VMF_CASE(CEIL)
        VMF_CASE(FRAC)
        VMF_CASE(ABS)
        VMF_CASE(SQRT)
        VMF_CASE(SIN)
        VMF_CASE(COS)
        VMF_CASE(TAN)
        VMF_CASE(U_SIN)
        VMF_CASE(U_COS)
        VMF_CASE(U_SIN_BI)
        VMF_CASE(U_COS_BI)
        VMF_CASE(SQR)
        {
            const double left = registers[insn.rs1];
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
            case NumericVmOpcode::SQR:
                out = left < 0.5 ? 1.0 : 0.0;
                break;
            default:
                break;
            }
            registers[insn.rd] = out;
            VMF_NEXT();
        }
        VMF_CASE(ADD)
        VMF_CASE(SUB)
        VMF_CASE(MUL)
        VMF_CASE(DIV)
        VMF_CASE(MOD)
        VMF_CASE(CMP_GT)
        VMF_CASE(CMP_LT)
        VMF_CASE(CMP_GE)
        VMF_CASE(CMP_LE)
        VMF_CASE(CMP_EQ)
        VMF_CASE(MIN)
        VMF_CASE(MAX)
        VMF_CASE(POW)
        VMF_CASE(TRI)
        VMF_CASE(PULSE)
        {
            const double left = registers[insn.rs1];
            const double right = registers[insn.rs2];
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
                    result.error = "dividing by zero \xe2\x80\x94 the result is undefined";
                    result.error_category = DiagnosticCategory::Arithmetic;
                    return result;
                }
                out = left / right;
                break;
            case NumericVmOpcode::MOD:
                if (right == 0.0)
                {
                    result.error = "dividing by zero \xe2\x80\x94 the result is undefined";
                    result.error_category = DiagnosticCategory::Arithmetic;
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
                // ModuLisp convention: (pow exponent base) -> base^exponent
                out = std::pow(right, left);
                break;
            case NumericVmOpcode::TRI:
                out = tri_wave(left, right);
                break;
            case NumericVmOpcode::PULSE:
                out = left < right ? 1.0 : 0.0;
                break;
            default:
                break;
            }
            registers[insn.rd] = out;
            VMF_NEXT();
        }
        VMF_CASE(CLAMP)
        {
            const double val = registers[insn.rs1];
            const double low = registers[insn.rs2];
            const double high = registers[insn.rs3];
            registers[insn.rd] = std::fmin(std::fmax(val, low), high);
            VMF_NEXT();
        }
        VMF_CASE(BRANCH)
        {
            size_t target = 0;
            if (!resolve_branch_target(pc, insn.imm, program.instructions.size(), target,
                                       result.error))
            {
                return result;
            }
            if (target <= pc && ++backward_branch_count > VM_MAX_BACKWARD_BRANCHES)
            {
                result.error = "this loop ran too long and was stopped";
                result.error_category = DiagnosticCategory::Runtime;
                return result;
            }
            pc = target;
            VMF_BRANCH_DONE();
        }
        VMF_CASE(BRANCH_IF)
        VMF_CASE(BRANCH_UNLESS)
        {
            // In numeric-only mode, truthiness is simply reg != 0.0
            const bool cond = registers[insn.rs1] != 0.0;
            if ((insn.opcode == NumericVmOpcode::BRANCH_IF && cond) ||
                (insn.opcode == NumericVmOpcode::BRANCH_UNLESS && !cond))
            {
                size_t target = 0;
                if (!resolve_branch_target(pc, insn.imm, program.instructions.size(),
                                           target, result.error))
                {
                    return result;
                }
                if (target <= pc &&
                    ++backward_branch_count > VM_MAX_BACKWARD_BRANCHES)
                {
                    result.error = "this loop ran too long and was stopped";
                    result.error_category = DiagnosticCategory::Runtime;
                    return result;
                }
                pc = target;
            }
            else
            {
                ++pc;
            }
            VMF_BRANCH_DONE();
        }
        VMF_CASE(CALL)
        VMF_CASE(CALL_INTRINSIC)
        {
            const size_t arg_start = static_cast<size_t>(insn.rs1);
            const size_t arg_count = static_cast<size_t>(insn.rs2);
            if (arg_start + arg_count > registers.size())
            {
                result.error = "call arguments out of range";
                result.error_category = DiagnosticCategory::Runtime;
                return result;
            }

            if (insn.opcode == NumericVmOpcode::CALL)
            {
                const NumericVmProgram* callee = nullptr;
                if (insn.imm == -1)
                {
                    callee = &program;
                }
                else if (insn.imm >= 0 &&
                         static_cast<size_t>(insn.imm) < program.functions.size() &&
                         program.functions[static_cast<size_t>(insn.imm)])
                {
                    callee = program.functions[static_cast<size_t>(insn.imm)].get();
                }

                if (!callee)
                {
                    result.error = "function index out of range";
                    result.error_category = DiagnosticCategory::Runtime;
                    return result;
                }

                if (callee->is_numeric_only)
                {
                    // Fast path: callee is also numeric-only
                    std::vector<double> args(
                        registers.begin() + arg_start,
                        registers.begin() + arg_start + arg_count);
                    const NumericVmExecutionResult callee_result =
                        execute_numeric_program_fast(*callee, ctx, args,
                                                     call_depth + 1);
                    if (!callee_result.ok)
                    {
                        return callee_result;
                    }
                    registers[insn.rd] = callee_result.value;
                }
                else
                {
                    // Fallback: callee uses typed opcodes, use tagged executor
                    std::vector<Value> args;
                    args.reserve(arg_count);
                    for (size_t i = 0; i < arg_count; ++i)
                    {
                        args.push_back(Value(registers[arg_start + i]));
                    }
                    const TaggedVmExecutionResult callee_result =
                        execute_tagged_program_impl(*callee, ctx, args,
                                                    call_depth + 1);
                    if (!callee_result.ok)
                    {
                        result.error = callee_result.error;
                        result.error_category = callee_result.error_category;
                        return result;
                    }
                    if (!callee_result.value.is_number())
                    {
                        result.error = "function returned a non-numeric value in numeric context";
                        result.error_category = DiagnosticCategory::Type;
                        return result;
                    }
                    registers[insn.rd] = callee_result.value.as_float();
                }
            }
            else
            {
                // CALL_INTRINSIC: must bridge through Value for the intrinsic interface
                if (insn.imm < 0 ||
                    static_cast<size_t>(insn.imm) >= program.intrinsics.size() ||
                    !program.intrinsics[static_cast<size_t>(insn.imm)])
                {
                    result.error = "intrinsic index out of range";
                    result.error_category = DiagnosticCategory::Runtime;
                    return result;
                }
                std::vector<Value> args;
                args.reserve(arg_count);
                for (size_t i = 0; i < arg_count; ++i)
                {
                    args.push_back(Value(registers[arg_start + i]));
                }
                const Value intrinsic_result =
                    program.intrinsics[static_cast<size_t>(insn.imm)](args, ctx);
                if (intrinsic_result.is_error())
                {
                    result.error = "intrinsic function returned an error";
                    result.error_category = DiagnosticCategory::Runtime;
                    return result;
                }
                if (!intrinsic_result.is_number())
                {
                    result.error = "intrinsic returned a non-numeric value in numeric context";
                    result.error_category = DiagnosticCategory::Type;
                    return result;
                }
                registers[insn.rd] = intrinsic_result.as_float();
            }
            VMF_NEXT();
        }
        VMF_CASE(RET)
        {
            const double rv = registers[insn.rs1];
            if (!std::isfinite(rv))
            {
                result.error = "this produced an undefined number \xe2\x80\x94 check for division by zero or sqrt of a negative";
                result.error_category = DiagnosticCategory::Arithmetic;
                return result;
            }
            result.ok = true;
            result.value = rv;
            return result;
        }

        // Type-polymorphic opcodes should never appear in numeric-only programs.
        // Handle them gracefully in switch mode.
#if !VM_FAST_USE_COMPUTED_GOTO
        case NumericVmOpcode::IS_NIL:
        case NumericVmOpcode::IS_NUMBER:
        case NumericVmOpcode::IS_LIST:
        case NumericVmOpcode::IS_STRING:
        case NumericVmOpcode::NOT:
        case NumericVmOpcode::AND:
        case NumericVmOpcode::OR:
        case NumericVmOpcode::MAKE_LIST:
        case NumericVmOpcode::MAKE_VECTOR:
        case NumericVmOpcode::LIST_HEAD:
        case NumericVmOpcode::LIST_TAIL:
        case NumericVmOpcode::LIST_LENGTH:
            result.error = "unexpected opcode in numeric-only program";
            result.error_category = DiagnosticCategory::Runtime;
            return result;
        } // end switch
#endif
    } // end for

#if VM_FAST_USE_COMPUTED_GOTO
vm_fast_loop_exit:
#endif

    result.error = "program terminated without returning a value";
    result.error_category = DiagnosticCategory::Runtime;
    return result;

#undef VMF_DISPATCH
#undef VMF_CASE
#undef VMF_NEXT
#undef VMF_BREAK
#undef VMF_BRANCH_DONE
#undef VM_FAST_USE_COMPUTED_GOTO
}

NumericVmExecutionResult execute_numeric_program_fast(
    const NumericVmProgram& program,
    const TemporalContext& ctx,
    const std::vector<double>& initial_registers,
    int call_depth)
{
    std::vector<double> registers(program.register_count, 0.0);
    for (size_t i = 0; i < initial_registers.size() && i < registers.size(); ++i)
    {
        registers[i] = initial_registers[i];
    }

    return execute_numeric_program_fast_impl(program, ctx, registers, call_depth);
}

// --- End unboxed double executor ---

TaggedVmExecutionResult execute_tagged_program_impl(const NumericVmProgram& program,
                                                    const TemporalContext& ctx,
                                                    const std::vector<Value>& initial_registers,
                                                    int call_depth)
{
    TaggedVmExecutionResult result;
    if (call_depth > VM_MAX_CALL_DEPTH)
    {
        result.error = "too many nested function calls";
        result.error_category = DiagnosticCategory::Runtime;
        return result;
    }
    if (program.register_count == 0 || program.instructions.empty())
    {
        result.error = "program is empty";
        result.error_category = DiagnosticCategory::Runtime;
        return result;
    }

    std::vector<Value> registers(program.register_count, Value::nil());
    for (size_t i = 0; i < initial_registers.size() && i < registers.size(); ++i)
    {
        registers[i] = initial_registers[i];
    }
    size_t backward_branch_count = 0;
// Computed-goto dispatch: enabled for GCC/Clang on non-WASM builds when
// USE_COMPUTED_GOTO is defined.  Falls back to a normal switch otherwise.
#if defined(__GNUC__) && !defined(WASM_BUILD) && defined(USE_COMPUTED_GOTO)
#define VM_USE_COMPUTED_GOTO 1
#else
#define VM_USE_COMPUTED_GOTO 0
#endif

#if VM_USE_COMPUTED_GOTO
    // 54 opcodes: LOAD_CONST(0) through LOAD_INPUT(53)
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
        &&op_MAKE_VECTOR,   &&op_LIST_HEAD,     &&op_LIST_TAIL,
        &&op_LIST_LENGTH,   &&op_U_SIN,         &&op_U_COS,
        &&op_U_SIN_BI,      &&op_U_COS_BI,      &&op_TRI,
        &&op_SQR,           &&op_PULSE,         &&op_LOAD_INPUT
    };
    static_assert(sizeof(dispatch_table) / sizeof(dispatch_table[0]) == 54,
                  "dispatch_table must have one entry per NumericVmOpcode");
    #define VM_DISPATCH() do { \
        if (pc >= program.instructions.size()) goto vm_loop_exit; \
        insn = program.instructions[pc]; \
        if (static_cast<int>(insn.opcode) < 0 || \
            static_cast<int>(insn.opcode) >= 54) goto vm_loop_exit; \
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
                result.error = "constant index out of range";
                result.error_category = DiagnosticCategory::Runtime;
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
            registers[insn.rd] =
                Value(ctx.input_at(static_cast<size_t>(std::max(insn.imm, 0))));
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
                result.error = "data segment is empty";
                result.error_category = DiagnosticCategory::Runtime;
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
                result.error = "expected a number but got a different type";
                result.error_category = DiagnosticCategory::Type;
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
        VM_CASE(TRI)
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
                result.error = "expected numbers but got a different type";
                result.error_category = DiagnosticCategory::Type;
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
                    result.error = "dividing by zero \xe2\x80\x94 the result is undefined";
                    result.error_category = DiagnosticCategory::Arithmetic;
                    return result;
                }
                out = left / right;
                break;
            case NumericVmOpcode::MOD:
                if (right == 0.0)
                {
                    result.error = "dividing by zero \xe2\x80\x94 the result is undefined";
                    result.error_category = DiagnosticCategory::Arithmetic;
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
                // ModuLisp convention: (pow exponent base) → base^exponent
                out = std::pow(right, left);
                break;
            case NumericVmOpcode::TRI:
                out = tri_wave(left, right);
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
                result.error = "clamp expected numbers but got a different type";
                result.error_category = DiagnosticCategory::Type;
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
            if (target <= pc && ++backward_branch_count > VM_MAX_BACKWARD_BRANCHES)
            {
                result.error = "this loop ran too long and was stopped";
                result.error_category = DiagnosticCategory::Runtime;
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
                if (target <= pc &&
                    ++backward_branch_count > VM_MAX_BACKWARD_BRANCHES)
                {
                    result.error = "this loop ran too long and was stopped";
                    result.error_category = DiagnosticCategory::Runtime;
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
                result.error = "call arguments out of range";
                result.error_category = DiagnosticCategory::Runtime;
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
                const NumericVmProgram* callee = nullptr;
                if (insn.imm == -1)
                {
                    callee = &program;
                }
                else if (insn.imm >= 0 &&
                         static_cast<size_t>(insn.imm) < program.functions.size() &&
                         program.functions[static_cast<size_t>(insn.imm)])
                {
                    callee = program.functions[static_cast<size_t>(insn.imm)].get();
                }

                if (!callee)
                {
                    result.error = "function index out of range";
                    result.error_category = DiagnosticCategory::Runtime;
                    return result;
                }

                const TaggedVmExecutionResult callee_result =
                    execute_tagged_program_impl(*callee, ctx, args, call_depth + 1);
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
                    result.error = "intrinsic index out of range";
                    result.error_category = DiagnosticCategory::Runtime;
                    return result;
                }
                registers[insn.rd] =
                    program.intrinsics[static_cast<size_t>(insn.imm)](args, ctx);
                if (registers[insn.rd].is_error())
                {
                    result.error = "intrinsic function returned an error";
                    result.error_category = DiagnosticCategory::Runtime;
                    return result;
                }
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
        VM_CASE(MAKE_VECTOR)
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
                result.error = "list construction arguments out of range";
                result.error_category = DiagnosticCategory::Runtime;
                return result;
            }

            std::vector<Value> elements;
            elements.reserve(count);
            for (size_t i = 0; i < count; ++i)
            {
                elements.push_back(registers[start + i]);
            }
            registers[insn.rd] = insn.opcode == NumericVmOpcode::MAKE_VECTOR
                                     ? Value::vector(elements)
                                     : Value(elements);
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
            if (!src.is_sequential())
            {
                result.error = "head expected a list or vector";
                result.error_category = DiagnosticCategory::Type;
                return result;
            }

            const std::vector<Value> items = src.as_sequential();
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
            if (!src.is_sequential())
            {
                result.error = "tail expected a list or vector";
                result.error_category = DiagnosticCategory::Type;
                return result;
            }

            const std::vector<Value> items = src.as_sequential();
            if (items.size() <= 1)
            {
                registers[insn.rd] = src.is_vector() ? Value::vector({})
                                                     : Value(std::vector<Value>{});
            }
            else
            {
                const std::vector<Value> tail(items.begin() + 1, items.end());
                registers[insn.rd] =
                    src.is_vector() ? Value::vector(tail) : Value(tail);
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
            if (src.is_sequential())
            {
                registers[insn.rd] =
                    Value(static_cast<double>(src.as_sequential().size()));
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

    result.error = "program terminated without returning a value";
    result.error_category = DiagnosticCategory::Runtime;
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
    // Fast path: numeric-only programs use unboxed double registers
    if (program.is_numeric_only)
    {
        return execute_numeric_program_fast(program, ctx);
    }

    // Fallback: tagged execution with numeric extraction
    NumericVmExecutionResult result;
    const TaggedVmExecutionResult tagged_result = execute_tagged_program(program, ctx);
    if (!tagged_result.ok)
    {
        result.error = tagged_result.error;
        result.error_category = tagged_result.error_category;
        return result;
    }

    if (!tagged_result.value.is_number())
    {
        result.error = "expected a numeric result but got a different type";
        result.error_category = DiagnosticCategory::Type;
        return result;
    }

    const double numeric_value = tagged_result.value.as_float();
    if (!std::isfinite(numeric_value))
    {
        result.error = "this produced an undefined number \xe2\x80\x94 check for division by zero or sqrt of a negative";
        result.error_category = DiagnosticCategory::Arithmetic;
        return result;
    }

    result.ok = true;
    result.value = numeric_value;
    return result;
}

// ---------------------------------------------------------------------------
// Batch execution: run a compiled program at multiple time points
// ---------------------------------------------------------------------------
//
// Numeric-only programs can reuse a single unboxed register file for the whole
// batch. Typed programs still fall back to the tagged per-sample executor.
// ---------------------------------------------------------------------------

NumericVmBatchResult execute_numeric_program_batch(
    const NumericVmProgram& program,
    const TemporalContext& base_ctx,
    const double* time_points,
    double* results,
    size_t count)
{
    NumericVmBatchResult batch_result;

    if (count == 0)
    {
        batch_result.ok = true;
        return batch_result;
    }

    if (program.register_count == 0 || program.instructions.empty())
    {
        batch_result.error = "program is empty";
        for (size_t i = 0; i < count; ++i)
        {
            results[i] = 0.0;
        }
        return batch_result;
    }

    double last_valid = 0.0;
    const bool use_numeric_fast_path = program.is_numeric_only;
    std::vector<double> numeric_registers;
    if (use_numeric_fast_path)
    {
        numeric_registers.resize(program.register_count, 0.0);
    }

    for (size_t sample_idx = 0; sample_idx < count; ++sample_idx)
    {
        // Build per-sample temporal context -- only t and derived phasors change
        TemporalContext sample_ctx = base_ctx;
        sample_ctx.t = time_points[sample_idx];
        sample_ctx.beat = wrap_phase(time_points[sample_idx], base_ctx.beatDur);
        sample_ctx.bar = wrap_phase(time_points[sample_idx], base_ctx.barDur);
        sample_ctx.phrase = wrap_phase(time_points[sample_idx], base_ctx.phraseDur);
        sample_ctx.section = wrap_phase(time_points[sample_idx], base_ctx.sectionDur);
        sample_ctx.beatNum =
            static_cast<int>(time_to_count(time_points[sample_idx], base_ctx.beatDur));
        sample_ctx.barNum =
            static_cast<int>(time_to_count(time_points[sample_idx], base_ctx.barDur));

        if (use_numeric_fast_path)
        {
            std::fill(numeric_registers.begin(), numeric_registers.end(), 0.0);
            const NumericVmExecutionResult sample_result =
                execute_numeric_program_fast_impl(program, sample_ctx,
                                                  numeric_registers);
            if (!sample_result.ok)
            {
                for (size_t j = sample_idx; j < count; ++j)
                {
                    results[j] = last_valid;
                }
                batch_result.error = sample_result.error;
                batch_result.error_category = sample_result.error_category;
                batch_result.error_at_index = sample_idx;
                return batch_result;
            }

            if (std::isfinite(sample_result.value))
            {
                results[sample_idx] = sample_result.value;
                last_valid = sample_result.value;
            }
            else
            {
                results[sample_idx] = last_valid;
            }
            continue;
        }

        const TaggedVmExecutionResult sample_result =
            execute_tagged_program_impl(program, sample_ctx);

        if (!sample_result.ok)
        {
            for (size_t j = sample_idx; j < count; ++j)
            {
                results[j] = last_valid;
            }
            batch_result.error = sample_result.error;
            batch_result.error_category = sample_result.error_category;
            batch_result.error_at_index = sample_idx;
            return batch_result;
        }

        if (sample_result.value.is_number())
        {
            const double value = sample_result.value.as_float();
            if (std::isfinite(value))
            {
                results[sample_idx] = value;
                last_valid = value;
            }
            else
            {
                results[sample_idx] = last_valid;
            }
        }
        else
        {
            results[sample_idx] = last_valid;
        }
    }

    batch_result.ok = true;
    return batch_result;
}
