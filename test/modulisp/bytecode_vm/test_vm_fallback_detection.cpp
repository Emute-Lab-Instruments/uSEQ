// test_vm_fallback_detection.cpp
// Detects unexpected runtime intrinsic calls by inspecting compiled bytecode
// programs for CALL_INTRINSIC opcodes.

#include "../../../uSEQ/src/modulisp/bytecode_vm.h"
#include "../../../uSEQ/src/modulisp/modulisp_interpreter.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
int g_passed = 0;
int g_failed = 0;

void report(const std::string& name, bool ok, const std::string& detail = "")
{
    if (ok)
    {
        std::cout << "  PASS: " << name << std::endl;
        ++g_passed;
    }
    else
    {
        std::cout << "  FAIL: " << name;
        if (!detail.empty())
        {
            std::cout << " -- " << detail;
        }
        std::cout << std::endl;
        ++g_failed;
    }
}

// Count CALL_INTRINSIC instructions in a compiled program
size_t count_intrinsic_calls(const NumericVmProgram& program)
{
    size_t count = 0;
    for (const auto& insn : program.instructions)
    {
        if (insn.opcode == NumericVmOpcode::CALL_INTRINSIC)
        {
            ++count;
        }
    }
    return count;
}

// Compile an expression using a pre-initialized interpreter's environment
NumericVmCompileResult compile_with_env(ModuLispInterpreter& interp,
                                        const std::string& code)
{
    Value expr = interp.get_parser()->parse(String(code.c_str()));
    return compile_numeric_program(expr, *interp.get_environment(), false);
}

// Assert that an expression compiles with zero CALL_INTRINSIC opcodes
void assert_zero_intrinsics(ModuLispInterpreter& interp,
                            const std::string& code,
                            const std::string& label)
{
    const auto result = compile_with_env(interp, code);
    if (!result.ok)
    {
        report(label, false, "compilation failed: " + std::string(result.error.c_str()));
        return;
    }
    const size_t count = count_intrinsic_calls(result.program);
    if (count != 0)
    {
        report(label, false,
               "expected 0 CALL_INTRINSIC but found " + std::to_string(count));
    }
    else
    {
        report(label, true);
    }
}

// Assert that an expression compiles with at least one CALL_INTRINSIC
void assert_has_intrinsic(ModuLispInterpreter& interp,
                          const std::string& code,
                          const std::string& label)
{
    const auto result = compile_with_env(interp, code);
    if (!result.ok)
    {
        report(label, false, "compilation failed: " + std::string(result.error.c_str()));
        return;
    }
    const size_t count = count_intrinsic_calls(result.program);
    if (count == 0)
    {
        report(label, false, "expected CALL_INTRINSIC but found none");
    }
    else
    {
        report(label, true);
    }
}

// ===== Part 1: Compilation introspection =====

void test_zero_intrinsic_expressions()
{
    std::cout << "\n=== Part 1a: expressions that must compile with zero "
                 "CALL_INTRINSIC ===\n";

    ModuLispInterpreter interp;
    interp.init();

    assert_zero_intrinsics(interp, "(+ 1 2)", "simple addition");
    assert_zero_intrinsics(interp, "(* beat 2)", "temporal * arithmetic");
    assert_zero_intrinsics(interp, "(if (> beat 0.5) 1 0)", "conditional with temporal");
    assert_zero_intrinsics(interp, "(let (x 5) (+ x 3))", "let binding");
    assert_zero_intrinsics(interp, "(floor (* beat 4))", "floor of arithmetic");
    assert_zero_intrinsics(interp, "(clamp beat 0 1)", "ternary clamp");
    assert_zero_intrinsics(interp, "(do 1 2 (+ 3 4))", "do block");
    assert_zero_intrinsics(interp, "(fast 2 beat)", "time warp fast");
    assert_zero_intrinsics(interp, "(sin beat)", "trig sin");
    assert_zero_intrinsics(interp, "(usin beat)", "UGen usin");
    assert_zero_intrinsics(interp, "(tri 0.5 beat)", "tri duty+phase");
    assert_zero_intrinsics(interp, "(not (> beat 0.5))", "logical not");
    assert_zero_intrinsics(interp, "(nil? 0)", "type predicate nil?");
    assert_zero_intrinsics(interp, "(min beat bar)", "binary min");
}

void test_expected_intrinsic_expressions()
{
    std::cout << "\n=== Part 1b: expressions that SHOULD use CALL_INTRINSIC ===\n";

    ModuLispInterpreter interp;
    interp.init();

    // A builtin function symbol looked up bare (not called inline) should
    // produce CALL_INTRINSIC because the compiler cannot inline it without
    // knowing the call form.
    assert_has_intrinsic(interp, "sin",
                         "bare builtin symbol resolves via intrinsic");

    assert_has_intrinsic(interp, "(tri beat)",
                         "tri wrong arity falls back to builtin arity checks");

    // A symbol not in the environment at all should produce CALL_INTRINSIC
    // for late binding.
    assert_has_intrinsic(interp, "some_undefined_symbol_xyz",
                         "undefined symbol produces late-bound intrinsic");
}

// ===== Part 2: Edge-case execution tests =====

void test_edge_case_execution()
{
    std::cout << "\n=== Part 2: edge-case execution through the VM ===\n";

    ModuLispInterpreter interp;
    interp.init();

    TemporalContext ctx;
    ctx.t = 0.0;
    ctx.beatDur = 0.5;
    ctx.barDur = 2.0;
    ctx.phraseDur = 8.0;
    ctx.sectionDur = 32.0;

    // (/ 1 0) -- division by zero should not crash
    {
        const auto result = compile_with_env(interp, "(/ 1 0)");
        if (result.ok)
        {
            const auto exec = execute_numeric_program(result.program, ctx);
            // The result might be inf or an error, but it must not crash.
            // We simply check that execution did not segfault (reaching this
            // line is success).
            bool no_crash = true;
            if (exec.ok && std::isinf(exec.value))
            {
                no_crash = true; // inf is acceptable for 1/0
            }
            report("(/ 1 0) does not crash", no_crash);
        }
        else
        {
            // Compile-time constant fold might produce an error or inf
            report("(/ 1 0) does not crash", true, "rejected at compile time");
        }
    }

    // (frac -0.3) should be 0.7 (frac = x - floor(x), -0.3 - (-1) = 0.7)
    {
        const auto result = compile_with_env(interp, "(frac -0.3)");
        if (!result.ok)
        {
            report("(frac -0.3) = 0.7", false,
                   "compilation failed: " + std::string(result.error.c_str()));
        }
        else
        {
            const auto exec = execute_numeric_program(result.program, ctx);
            bool ok = exec.ok && std::fabs(exec.value - 0.7) < 1e-9;
            report("(frac -0.3) = 0.7", ok,
                   ok ? "" : "got " + std::to_string(exec.value));
        }
    }

    // (frac 0.0) should be 0.0
    {
        const auto result = compile_with_env(interp, "(frac 0.0)");
        if (!result.ok)
        {
            report("(frac 0.0) = 0.0", false,
                   "compilation failed: " + std::string(result.error.c_str()));
        }
        else
        {
            const auto exec = execute_numeric_program(result.program, ctx);
            bool ok = exec.ok && std::fabs(exec.value) < 1e-9;
            report("(frac 0.0) = 0.0", ok,
                   ok ? "" : "got " + std::to_string(exec.value));
        }
    }

    // (abs -0) should be 0.0 (no negative zero leak)
    {
        const auto result = compile_with_env(interp, "(abs -0)");
        if (!result.ok)
        {
            // -0 might be parsed as symbol, try (abs (- 0))
            const auto result2 = compile_with_env(interp, "(abs (- 0))");
            if (!result2.ok)
            {
                report("(abs -0) = 0.0", false,
                       "compilation failed");
            }
            else
            {
                const auto exec = execute_numeric_program(result2.program, ctx);
                bool ok = exec.ok && exec.value == 0.0;
                report("(abs -0) = 0.0", ok,
                       ok ? "" : "got " + std::to_string(exec.value));
            }
        }
        else
        {
            const auto exec = execute_numeric_program(result.program, ctx);
            bool ok = exec.ok && exec.value == 0.0;
            report("(abs -0) = 0.0", ok,
                   ok ? "" : "got " + std::to_string(exec.value));
        }
    }

    // (sqrt -1) should not crash; result may be NaN, 0, or error
    // depending on how the constant folder handles non-finite values.
    {
        const auto result = compile_with_env(interp, "(sqrt -1)");
        bool no_crash = true;
        if (result.ok)
        {
            const auto exec = execute_numeric_program(result.program, ctx);
            // Reaching here without a segfault is the core assertion.
            // NaN, 0, or non-ok are all acceptable -- the key property
            // is that the VM handles the degenerate case gracefully.
            (void)exec;
        }
        report("(sqrt -1) does not crash", no_crash);
    }
}

} // namespace

int main()
{
    std::cout << "=== VM Fallback Detection Tests ===" << std::endl;

    test_zero_intrinsic_expressions();
    test_expected_intrinsic_expressions();
    test_edge_case_execution();

    std::cout << "\n=== Summary: " << g_passed << " passed, " << g_failed
              << " failed ===" << std::endl;

    return g_failed > 0 ? 1 : 0;
}
