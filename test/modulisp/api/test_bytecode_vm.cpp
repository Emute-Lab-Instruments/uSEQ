#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../../../uSEQ/src/modulisp/bytecode_vm.h"
#include "../../../uSEQ/src/modulisp/modulisp_interpreter.h"

#include <algorithm>
#include <memory>

namespace
{
Value sum_intrinsic(const std::vector<Value>& args, const TemporalContext&)
{
    double total = 0.0;
    for (const Value& arg : args)
    {
        total += arg.as_float();
    }
    return Value(total);
}

bool has_opcode(const NumericVmProgram& program, NumericVmOpcode opcode)
{
    return std::any_of(program.instructions.begin(), program.instructions.end(),
                       [opcode](const NumericVmInstruction& insn) {
                           return insn.opcode == opcode;
                       });
}

bool has_dependency(const NumericVmProgram& program, const String& dependency)
{
    return std::any_of(program.dependencies.begin(), program.dependencies.end(),
                       [&dependency](const String& item) {
                           return item == dependency;
                       });
}
} // namespace

TEST_CASE("Numeric VM executes direct bytecode", "[modulisp][vm]")
{
    NumericVmProgram program;
    program.constants = { 2.0 };
    program.register_count = 2;
    program.instructions = {
        { NumericVmOpcode::LOAD_CONST, 0, 0, 0, 0, 0, 0.0, 0.0 },
        { NumericVmOpcode::LOAD_TIME, 1, 0, 0, 0,
          static_cast<int32_t>(NumericVmTemporalChannel::T), 1.0, 0.0 },
        { NumericVmOpcode::ADD, 0, 0, 1, 0, 0, 0.0, 0.0 },
        { NumericVmOpcode::RET, 0, 0, 0, 0, 0, 0.0, 0.0 }
    };

    TemporalContext ctx;
    ctx.t = 3.5;
    ctx.beatDur = 0.5;
    ctx.barDur = 2.0;
    ctx.phraseDur = 8.0;
    ctx.sectionDur = 32.0;

    const NumericVmExecutionResult result = execute_numeric_program(program, ctx);
    REQUIRE(result.ok);
    REQUIRE(result.value == Approx(5.5).epsilon(1e-9));
}

TEST_CASE("Tagged VM preserves vector constants", "[modulisp][vm]")
{
    NumericVmProgram program;
    program.constants = { Value::vector({ Value(1), Value(2), Value(3) }) };
    program.register_count = 1;
    program.instructions = {
        { NumericVmOpcode::LOAD_CONST, 0, 0, 0, 0, 0, 0.0, 0.0 },
        { NumericVmOpcode::RET, 0, 0, 0, 0, 0, 0.0, 0.0 }
    };

    TemporalContext ctx;
    const TaggedVmExecutionResult result = execute_tagged_program(program, ctx);
    REQUIRE(result.ok);
    REQUIRE(result.value == Value::vector({ Value(1), Value(2), Value(3) }));
}

TEST_CASE("Tagged VM executes branch opcodes", "[modulisp][vm]")
{
    NumericVmProgram program;
    program.constants = { Value::string("unreachable"),
                          Value::string("reachable") };
    program.register_count = 1;
    program.instructions = {
        { NumericVmOpcode::BRANCH, 0, 0, 0, 0, 2, 0.0, 0.0 },
        { NumericVmOpcode::LOAD_CONST, 0, 0, 0, 0, 0, 0.0, 0.0 },
        { NumericVmOpcode::RET, 0, 0, 0, 0, 0, 0.0, 0.0 },
        { NumericVmOpcode::LOAD_CONST, 0, 0, 0, 0, 1, 0.0, 0.0 },
        { NumericVmOpcode::RET, 0, 0, 0, 0, 0, 0.0, 0.0 }
    };

    TemporalContext ctx;
    const TaggedVmExecutionResult result = execute_tagged_program(program, ctx);
    REQUIRE(result.ok);
    REQUIRE(result.value == Value::string("reachable"));
}

TEST_CASE("Tagged VM supports call and intrinsic substrate", "[modulisp][vm]")
{
    auto callee = std::make_shared<NumericVmProgram>();
    callee->register_count = 2;
    callee->instructions = {
        { NumericVmOpcode::ADD, 0, 0, 1, 0, 0, 0.0, 0.0 },
        { NumericVmOpcode::RET, 0, 0, 0, 0, 0, 0.0, 0.0 }
    };

    NumericVmProgram program;
    program.constants = { Value(2.0), Value(3.0), Value(4.0) };
    program.functions = { callee };
    program.intrinsics = { sum_intrinsic };
    program.register_count = 4;
    program.instructions = {
        { NumericVmOpcode::LOAD_CONST, 0, 0, 0, 0, 0, 0.0, 0.0 },
        { NumericVmOpcode::LOAD_CONST, 1, 0, 0, 0, 1, 0.0, 0.0 },
        { NumericVmOpcode::CALL, 2, 0, 2, 0, 0, 0.0, 0.0 },
        { NumericVmOpcode::LOAD_CONST, 3, 0, 0, 0, 2, 0.0, 0.0 },
        { NumericVmOpcode::CALL_INTRINSIC, 2, 2, 2, 0, 0, 0.0, 0.0 },
        { NumericVmOpcode::RET, 0, 2, 0, 0, 0, 0.0, 0.0 }
    };

    TemporalContext ctx;
    const NumericVmExecutionResult result = execute_numeric_program(program, ctx);
    REQUIRE(result.ok);
    REQUIRE(result.value == Approx(9.0).epsilon(1e-9));
}

TEST_CASE("Tagged VM reads numeric data segments", "[modulisp][vm]")
{
    NumericVmProgram program;
    program.data_segments = { { 10.0, 20.0, 30.0 } };
    program.register_count = 1;
    program.instructions = {
        { NumericVmOpcode::LOAD_CONST, 0, 0, 0, 0, 0, 0.0, 0.0 },
        { NumericVmOpcode::VEC_INDEX, 0, 0, 0, 0, 0, 0.0, 0.0 },
        { NumericVmOpcode::RET, 0, 0, 0, 0, 0, 0.0, 0.0 }
    };
    program.constants = { Value(0.5) };

    TemporalContext ctx;
    const NumericVmExecutionResult result = execute_numeric_program(program, ctx);
    REQUIRE(result.ok);
    REQUIRE(result.value == Approx(20.0).epsilon(1e-9));
}

TEST_CASE("Numeric VM compiler lowers temporal arithmetic", "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    const auto compile_result =
        compile_numeric_program(interp.get_parser()->parse("(* t 2)"),
                                *interp.get_environment());

    REQUIRE(compile_result.ok);
    REQUIRE(compile_result.program.instructions.size() == 4);
    REQUIRE(compile_result.program.instructions[0].opcode == NumericVmOpcode::LOAD_TIME);
    REQUIRE(compile_result.program.instructions[1].opcode == NumericVmOpcode::LOAD_CONST);
    REQUIRE(compile_result.program.instructions[2].opcode == NumericVmOpcode::MUL);
    REQUIRE(compile_result.program.instructions[3].opcode == NumericVmOpcode::RET);
}

TEST_CASE("Numeric VM compiler folds constant arithmetic subtrees", "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    const auto compile_result =
        compile_numeric_program(interp.get_parser()->parse("(+ (* 2 3) 4)"),
                                *interp.get_environment());

    REQUIRE(compile_result.ok);
    REQUIRE(compile_result.program.instructions.size() == 2);
    REQUIRE(compile_result.program.register_count == 1);
    REQUIRE(compile_result.program.instructions[0].opcode == NumericVmOpcode::LOAD_CONST);
    REQUIRE(compile_result.program.constants.size() == 1);
    REQUIRE(compile_result.program.constants[0].is_number());
    REQUIRE(compile_result.program.constants[0].as_float() == Approx(10.0).epsilon(1e-9));
    REQUIRE(compile_result.program.instructions[1].opcode == NumericVmOpcode::RET);
}

TEST_CASE("Numeric VM compiler reuses accumulator registers for arithmetic",
          "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    const auto compile_result =
        compile_numeric_program(interp.get_parser()->parse("(+ t 2)"),
                                *interp.get_environment());

    REQUIRE(compile_result.ok);
    REQUIRE(compile_result.program.instructions.size() == 4);
    REQUIRE(compile_result.program.register_count == 2);
    REQUIRE(compile_result.program.instructions[0].opcode == NumericVmOpcode::LOAD_TIME);
    REQUIRE(compile_result.program.instructions[1].opcode == NumericVmOpcode::LOAD_CONST);
    REQUIRE(compile_result.program.instructions[2].opcode == NumericVmOpcode::ADD);
    REQUIRE(compile_result.program.instructions[2].rd ==
            compile_result.program.instructions[2].rs1);
    REQUIRE(compile_result.program.instructions[3].opcode == NumericVmOpcode::RET);
}

TEST_CASE("Numeric VM compiler resolves numeric symbols and affine warps",
          "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    interp.eval("(define warped (offset 1 (slow 2 (fast 4 t))))");

    const auto compile_result =
        compile_numeric_program(interp.get_parser()->parse("warped"),
                                *interp.get_environment());

    REQUIRE(compile_result.ok);
    REQUIRE(compile_result.program.instructions.size() == 2);
    REQUIRE(compile_result.program.instructions[0].opcode == NumericVmOpcode::LOAD_TIME);
    REQUIRE(compile_result.program.instructions[0].imm0 == Approx(2.0).epsilon(1e-9));
    REQUIRE(compile_result.program.instructions[0].imm1 == Approx(2.0).epsilon(1e-9));
    REQUIRE(compile_result.program.instructions[1].opcode == NumericVmOpcode::RET);
    REQUIRE(compile_result.program.dependencies.size() == 1);
    REQUIRE(compile_result.program.dependencies[0] == "warped");
}

TEST_CASE("Numeric VM compiler preserves unary identity for arithmetic",
          "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    const auto plus_result =
        compile_numeric_program(interp.get_parser()->parse("(+ t)"),
                                *interp.get_environment());
    REQUIRE(plus_result.ok);
    REQUIRE(plus_result.program.instructions.size() == 2);
    REQUIRE(plus_result.program.register_count == 1);
    REQUIRE(plus_result.program.instructions[0].opcode == NumericVmOpcode::LOAD_TIME);
    REQUIRE(plus_result.program.instructions[1].opcode == NumericVmOpcode::RET);

    const auto product_result =
        compile_numeric_program(interp.get_parser()->parse("(* t)"),
                                *interp.get_environment());
    REQUIRE(product_result.ok);
    REQUIRE(product_result.program.instructions.size() == 2);
    REQUIRE(product_result.program.register_count == 1);
    REQUIRE(product_result.program.instructions[0].opcode == NumericVmOpcode::LOAD_TIME);
    REQUIRE(product_result.program.instructions[1].opcode == NumericVmOpcode::RET);
}

TEST_CASE("Numeric VM compiler lowers if into branches", "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    const auto compile_result =
        compile_numeric_program(interp.get_parser()->parse("(if (> t 0) 1 2)"),
                                *interp.get_environment());

    REQUIRE(compile_result.ok);
    REQUIRE(std::any_of(compile_result.program.instructions.begin(),
                        compile_result.program.instructions.end(),
                        [](const NumericVmInstruction& insn) {
                            return insn.opcode == NumericVmOpcode::BRANCH ||
                                   insn.opcode == NumericVmOpcode::BRANCH_IF ||
                                   insn.opcode == NumericVmOpcode::BRANCH_UNLESS;
                        }));

    TemporalContext ctx_false;
    ctx_false.t = 0.0;
    ctx_false.beatDur = 0.5;
    ctx_false.barDur = 2.0;
    ctx_false.phraseDur = 8.0;
    ctx_false.sectionDur = 32.0;

    const NumericVmExecutionResult false_result =
        execute_numeric_program(compile_result.program, ctx_false);
    REQUIRE(false_result.ok);
    REQUIRE(false_result.value == Approx(2.0).epsilon(1e-9));

    TemporalContext ctx_true = ctx_false;
    ctx_true.t = 1.0;
    const NumericVmExecutionResult true_result =
        execute_numeric_program(compile_result.program, ctx_true);
    REQUIRE(true_result.ok);
    REQUIRE(true_result.value == Approx(1.0).epsilon(1e-9));
}

TEST_CASE("Numeric VM compiler folds constant if conditions at compile time",
          "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    const auto compile_result =
        compile_numeric_program(interp.get_parser()->parse("(if 1 (+ t 1) (+ t 2))"),
                                *interp.get_environment());

    REQUIRE(compile_result.ok);
    REQUIRE_FALSE(has_opcode(compile_result.program, NumericVmOpcode::BRANCH));
    REQUIRE_FALSE(has_opcode(compile_result.program, NumericVmOpcode::BRANCH_IF));
    REQUIRE_FALSE(has_opcode(compile_result.program, NumericVmOpcode::BRANCH_UNLESS));

    TemporalContext ctx;
    ctx.t = 4.0;
    ctx.beatDur = 0.5;
    ctx.barDur = 2.0;
    ctx.phraseDur = 8.0;
    ctx.sectionDur = 32.0;

    const NumericVmExecutionResult result =
        execute_numeric_program(compile_result.program, ctx);
    REQUIRE(result.ok);
    REQUIRE(result.value == Approx(5.0).epsilon(1e-9));
}

TEST_CASE("Numeric VM compiler lowers do blocks to the last form", "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    const auto compile_result =
        compile_numeric_program(interp.get_parser()->parse("(do (+ 1 2) (+ t 3))"),
                                *interp.get_environment());

    REQUIRE(compile_result.ok);
    REQUIRE(compile_result.program.instructions.back().opcode ==
            NumericVmOpcode::RET);

    TemporalContext ctx;
    ctx.t = 2.0;
    ctx.beatDur = 0.5;
    ctx.barDur = 2.0;
    ctx.phraseDur = 8.0;
    ctx.sectionDur = 32.0;

    const NumericVmExecutionResult result =
        execute_numeric_program(compile_result.program, ctx);
    REQUIRE(result.ok);
    REQUIRE(result.value == Approx(5.0).epsilon(1e-9));
}

TEST_CASE("Numeric VM compiler expands interpolated vectors without intrinsics",
          "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    const auto compile_result =
        compile_numeric_program(interp.get_parser()->parse("(interp [10 20 30] beat)"),
                                *interp.get_environment());

    REQUIRE(compile_result.ok);
    REQUIRE(compile_result.program.data_segments.size() == 1);
    REQUIRE(has_opcode(compile_result.program, NumericVmOpcode::VEC_LERP));
    REQUIRE_FALSE(has_opcode(compile_result.program, NumericVmOpcode::CALL_INTRINSIC));
}

TEST_CASE("Numeric VM compiler tracks transitive dependencies across aliases",
          "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    interp.eval("(define leaf (+ t 1))");
    interp.eval("(define alias leaf)");

    const auto compile_result =
        compile_numeric_program(interp.get_parser()->parse("alias"),
                                *interp.get_environment());

    REQUIRE(compile_result.ok);
    REQUIRE(has_dependency(compile_result.program, "alias"));
    REQUIRE(has_dependency(compile_result.program, "leaf"));
}

TEST_CASE("Numeric VM compiler inlines defn callables without CALL opcodes",
          "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    interp.eval("(defn add1 (x) (+ x 1))");

    const auto compile_result =
        compile_numeric_program(interp.get_parser()->parse("(add1 t)"),
                                *interp.get_environment());

    REQUIRE(compile_result.ok);
    REQUIRE_FALSE(has_opcode(compile_result.program, NumericVmOpcode::CALL));
    REQUIRE_FALSE(has_opcode(compile_result.program, NumericVmOpcode::CALL_INTRINSIC));
    REQUIRE(has_dependency(compile_result.program, "add1"));
}

TEST_CASE("Numeric VM compiler lowers unsupported numeric builtins via CALL_INTRINSIC",
          "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    const auto compile_result =
        compile_numeric_program(interp.get_parser()->parse("(tri 0.5 beat)"),
                                *interp.get_environment());

    REQUIRE(compile_result.ok);
    REQUIRE(has_opcode(compile_result.program, NumericVmOpcode::CALL_INTRINSIC));

    TemporalContext ctx;
    ctx.t = 0.125;
    ctx.beatDur = 0.5;
    ctx.barDur = 2.0;
    ctx.phraseDur = 8.0;
    ctx.sectionDur = 32.0;

    const NumericVmExecutionResult result =
        execute_numeric_program(compile_result.program, ctx);
    REQUIRE(result.ok);
    REQUIRE(result.value == Approx(0.5).epsilon(1e-9));
}

TEST_CASE("Numeric VM compiler flattens slow into LOAD_TIME scale", "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    const auto compile_result =
        compile_numeric_program(interp.get_parser()->parse("(slow 2 t)"),
                                *interp.get_environment());

    REQUIRE(compile_result.ok);
    REQUIRE(compile_result.program.instructions.size() == 2);
    REQUIRE(compile_result.program.instructions[0].opcode == NumericVmOpcode::LOAD_TIME);
    REQUIRE(compile_result.program.instructions[0].imm0 == Approx(0.5).epsilon(1e-9));
    REQUIRE(compile_result.program.instructions[0].imm1 == Approx(0.0).epsilon(1e-9));
}

TEST_CASE("Numeric VM compiler lowers vector lookup into data segments",
          "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    const auto compile_result =
        compile_numeric_program(interp.get_parser()->parse("(from-list [10 20 30] beat)"),
                                *interp.get_environment());

    REQUIRE(compile_result.ok);
    REQUIRE(compile_result.program.data_segments.size() == 1);
    REQUIRE(compile_result.program.data_segments[0] ==
            std::vector<double>({ 10.0, 20.0, 30.0 }));
    REQUIRE(compile_result.program.instructions.size() == 3);
    REQUIRE(compile_result.program.instructions[0].opcode == NumericVmOpcode::LOAD_TIME);
    REQUIRE(compile_result.program.instructions[1].opcode == NumericVmOpcode::VEC_INDEX);
    REQUIRE(compile_result.program.instructions[2].opcode == NumericVmOpcode::RET);
}

TEST_CASE("Output sampling falls back for let expressions", "[modulisp][vm][outputs]")
{
    ModuLispInterpreter interp(nullptr, nullptr, nullptr, 8, 8, 8);
    interp.init();

    interp.eval("(a1 (let [x 1 y 2] (+ x y)))");

    bool ok = false;
    const double value = interp.eval_output_at_time("a1", 0.0, &ok);
    REQUIRE(ok);
    REQUIRE(value == Approx(3.0).epsilon(1e-9));
}

TEST_CASE("Output sampling falls back for defn and lambda callables",
          "[modulisp][vm][outputs]")
{
    ModuLispInterpreter interp(nullptr, nullptr, nullptr, 8, 8, 8);
    interp.init();

    interp.eval("(defn add1 (x) (+ x 1))");
    interp.eval("(a1 (add1 t))");

    bool ok = false;
    const double first_value = interp.eval_output_at_time("a1", 2.0, &ok);
    REQUIRE(ok);
    REQUIRE(first_value == Approx(3.0).epsilon(1e-9));
}

TEST_CASE("Public eval_v handles lambda callables directly", "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    const Value result = interp.eval_v("((lambda [x] (+ x 3)) 2)");
    REQUIRE(result.is_number());
    REQUIRE(result.as_float() == Approx(5.0).epsilon(1e-9));
}

TEST_CASE("Output sampling uses numeric VM vector lookup expressions",
          "[modulisp][vm][outputs]")
{
    ModuLispInterpreter interp(nullptr, nullptr, nullptr, 8, 8, 8);
    interp.init();
    interp.set_bpm(120.0, 0.0);

    interp.eval("(a1 (from-list [10 20 30] beat))");

    bool ok = false;
    const double value = interp.eval_output_at_time("a1", 0.25, &ok);
    REQUIRE(ok);
    REQUIRE(value == Approx(20.0).epsilon(1e-6));
}

TEST_CASE("Output sampling uses numeric VM-compatible expressions", "[modulisp][vm][outputs]")
{
    ModuLispInterpreter interp(nullptr, nullptr, nullptr, 8, 8, 8);
    interp.init();
    interp.set_bpm(120.0, 0.0);

    interp.eval("(define foo (slow 2 (+ t 1)))");
    interp.eval("(a1 foo)");

    bool ok = false;
    const double value = interp.eval_output_at_time("a1", 9.0, &ok);
    REQUIRE(ok);
    REQUIRE(value == Approx(5.5).epsilon(1e-6));
}

TEST_CASE("Compiled output invalidates after dependency redefinition",
          "[modulisp][vm][outputs]")
{
    ModuLispInterpreter interp(nullptr, nullptr, nullptr, 8, 8, 8);
    interp.init();
    interp.set_bpm(120.0, 0.0);

    interp.eval("(define scale 2)");
    interp.eval("(a1 (* scale t))");

    bool ok = false;
    const double first_value = interp.eval_output_at_time("a1", 1.5, &ok);
    REQUIRE(ok);
    REQUIRE(first_value == Approx(3.0).epsilon(1e-6));

    interp.eval("(define scale 5)");

    ok = false;
    const double second_value = interp.eval_output_at_time("a1", 1.5, &ok);
    REQUIRE(ok);
    REQUIRE(second_value == Approx(7.5).epsilon(1e-6));
}

TEST_CASE("Numeric VM rejects side-effectful signal forms at compile time",
          "[modulisp][vm][signals]")
{
    ModuLispInterpreter interp;
    interp.init();

    const auto define_result =
        compile_numeric_program(interp.get_parser()->parse("(do (define x 1) (+ t 1))"),
                                *interp.get_environment());
    REQUIRE_FALSE(define_result.ok);
    REQUIRE(define_result.error.indexOf("Signal VM rejects side-effectful form") >= 0);

    const auto eval_result =
        compile_numeric_program(interp.get_parser()->parse("(eval \"(+ t 1)\")"),
                                *interp.get_environment());
    REQUIRE_FALSE(eval_result.ok);
    REQUIRE(eval_result.error.indexOf("Signal VM rejects eval") >= 0);
}

TEST_CASE("Compile-time rejection keeps the current active output graph",
          "[modulisp][vm][outputs]")
{
    ModuLispInterpreter interp(nullptr, nullptr, nullptr, 8, 8, 8);
    interp.init();

    interp.eval("(a1 (+ t 1))");

    bool ok = false;
    const double first_value = interp.eval_output_at_time("a1", 2.0, &ok);
    REQUIRE(ok);
    REQUIRE(first_value == Approx(3.0).epsilon(1e-6));

    interp.eval("(a1 (do (define x 1) (+ t 2)))");

    ok = false;
    const double second_value = interp.eval_output_at_time("a1", 2.0, &ok);
    REQUIRE(ok);
    REQUIRE(second_value == Approx(3.0).epsilon(1e-6));
}
