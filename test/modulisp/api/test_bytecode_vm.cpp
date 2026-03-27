#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../../../uSEQ/src/modulisp/bytecode_vm.h"
#include "../../../uSEQ/src/modulisp/modulisp_interpreter.h"

#include <algorithm>
#include <iterator>
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

TEST_CASE("Numeric VM reads LOAD_INPUT from the temporal snapshot",
          "[modulisp][vm]")
{
    NumericVmProgram program;
    program.register_count = 2;
    program.instructions = {
        { NumericVmOpcode::LOAD_INPUT, 0, 0, 0, 0, 2, 0.0, 0.0 },
        { NumericVmOpcode::LOAD_TIME, 1, 0, 0, 0,
          static_cast<int32_t>(NumericVmTemporalChannel::T), 1.0, 0.0 },
        { NumericVmOpcode::ADD, 0, 0, 1, 0, 0, 0.0, 0.0 },
        { NumericVmOpcode::RET, 0, 0, 0, 0, 0, 0.0, 0.0 }
    };

    double inputs[ModuLispInterpreter::kInputSlotCount] = {};
    inputs[2] = 7.25;

    TemporalContext ctx;
    ctx.t = 1.5;
    ctx.beatDur = 0.5;
    ctx.barDur = 2.0;
    ctx.phraseDur = 8.0;
    ctx.sectionDur = 32.0;
    ctx.input_values = inputs;
    ctx.input_count = ModuLispInterpreter::kInputSlotCount;

    const NumericVmExecutionResult result = execute_numeric_program(program, ctx);
    REQUIRE(result.ok);
    REQUIRE(result.value == Approx(8.75).epsilon(1e-9));
}

TEST_CASE("Tagged VM reads LOAD_INPUT from the temporal snapshot",
          "[modulisp][vm]")
{
    NumericVmProgram program;
    program.register_count = 1;
    program.instructions = {
        { NumericVmOpcode::LOAD_INPUT, 0, 0, 0, 0, 5, 0.0, 0.0 },
        { NumericVmOpcode::RET, 0, 0, 0, 0, 0, 0.0, 0.0 }
    };

    double inputs[ModuLispInterpreter::kInputSlotCount] = {};
    inputs[5] = 3.5;

    TemporalContext ctx;
    ctx.input_values = inputs;
    ctx.input_count = ModuLispInterpreter::kInputSlotCount;

    const TaggedVmExecutionResult result = execute_tagged_program(program, ctx);
    REQUIRE(result.ok);
    REQUIRE(result.value.is_number());
    REQUIRE(result.value.as_float() == Approx(3.5).epsilon(1e-9));
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

TEST_CASE("Tagged VM unary opcodes ignore unrelated source registers",
          "[modulisp][vm]")
{
    NumericVmProgram program;
    program.constants = { Value::string("sentinel"), Value(-2.0) };
    program.register_count = 2;
    program.instructions = {
        { NumericVmOpcode::LOAD_CONST, 0, 0, 0, 0, 0, 0.0, 0.0 },
        { NumericVmOpcode::LOAD_CONST, 1, 0, 0, 0, 1, 0.0, 0.0 },
        { NumericVmOpcode::ABS, 1, 1, 0, 0, 0, 0.0, 0.0 },
        { NumericVmOpcode::RET, 0, 1, 0, 0, 0, 0.0, 0.0 }
    };

    TemporalContext ctx;
    const TaggedVmExecutionResult result = execute_tagged_program(program, ctx);
    REQUIRE(result.ok);
    REQUIRE(result.value.is_number());
    REQUIRE(result.value.as_float() == Approx(2.0).epsilon(1e-9));
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

TEST_CASE("Numeric VM compiler lowers tri using language builtin semantics",
          "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    const auto compile_result =
        compile_numeric_program(interp.get_parser()->parse("(tri 0.5 beat)"),
                                *interp.get_environment());

    REQUIRE(compile_result.ok);
    REQUIRE(has_opcode(compile_result.program, NumericVmOpcode::TRI));
    REQUIRE_FALSE(has_opcode(compile_result.program, NumericVmOpcode::CALL_INTRINSIC));

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

TEST_CASE("Numeric VM compiler lowers input to LOAD_INPUT instead of folding",
          "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    const auto compile_result =
        compile_numeric_program(interp.get_parser()->parse("(input 2)"),
                                *interp.get_environment());

    REQUIRE(compile_result.ok);
    REQUIRE(has_opcode(compile_result.program, NumericVmOpcode::LOAD_INPUT));
    REQUIRE_FALSE((compile_result.program.instructions.size() == 2 &&
                   compile_result.program.instructions[0].opcode ==
                       NumericVmOpcode::LOAD_CONST));
}

TEST_CASE("Numeric VM reads LOAD_INPUT from temporal context", "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    const auto compile_result =
        compile_numeric_program(interp.get_parser()->parse("(input 2)"),
                                *interp.get_environment());

    REQUIRE(compile_result.ok);

    double input_values[ModuLispInterpreter::kInputSlotCount] = {};
    input_values[2] = 0.75;

    TemporalContext ctx;
    ctx.input_values = input_values;
    ctx.input_count = ModuLispInterpreter::kInputSlotCount;

    const NumericVmExecutionResult result =
        execute_numeric_program(compile_result.program, ctx);
    REQUIRE(result.ok);
    REQUIRE(result.value == Approx(0.75).epsilon(1e-9));
}

TEST_CASE("Numeric VM batch execution matches single-sample execution",
          "[modulisp][vm][batch]")
{
    ModuLispInterpreter interp;
    interp.init();

    const auto compile_result =
        compile_numeric_program(interp.get_parser()->parse("(+ (* t 2) 1)"),
                                *interp.get_environment());

    REQUIRE(compile_result.ok);
    REQUIRE(compile_result.program.is_numeric_only);

    constexpr double time_points[] = {0.0, 0.25, 0.5, 0.75, 1.0};
    double batch_results[std::size(time_points)] = {};

    TemporalContext base_ctx;
    base_ctx.beatDur = 0.5;
    base_ctx.barDur = 2.0;
    base_ctx.phraseDur = 8.0;
    base_ctx.sectionDur = 32.0;

    const NumericVmBatchResult batch =
        execute_numeric_program_batch(compile_result.program, base_ctx,
                                      time_points, batch_results,
                                      std::size(time_points));

    REQUIRE(batch.ok);

    for (size_t i = 0; i < std::size(time_points); ++i)
    {
        TemporalContext sample_ctx = base_ctx;
        sample_ctx.t = time_points[i];
        sample_ctx.beat = std::fmod(time_points[i] / base_ctx.beatDur, 1.0);
        sample_ctx.bar = std::fmod(time_points[i] / base_ctx.barDur, 1.0);
        sample_ctx.phrase = std::fmod(time_points[i] / base_ctx.phraseDur, 1.0);
        sample_ctx.section = std::fmod(time_points[i] / base_ctx.sectionDur, 1.0);
        sample_ctx.beatNum = static_cast<int>(time_points[i] / base_ctx.beatDur);
        sample_ctx.barNum = static_cast<int>(time_points[i] / base_ctx.barDur);

        const NumericVmExecutionResult single =
            execute_numeric_program(compile_result.program, sample_ctx);
        REQUIRE(single.ok);
        REQUIRE(batch_results[i] == Approx(single.value).epsilon(1e-9));
    }
}

TEST_CASE("Numeric VM batch execution reports error_at_index and holds last valid value",
          "[modulisp][vm][batch]")
{
    ModuLispInterpreter interp;
    interp.init();

    const auto compile_result =
        compile_numeric_program(interp.get_parser()->parse("(/ 1 t)"),
                                *interp.get_environment());

    REQUIRE(compile_result.ok);
    REQUIRE(compile_result.program.is_numeric_only);

    constexpr double time_points[] = {1.0, 0.5, 0.0, 0.25};
    double batch_results[std::size(time_points)] = {};

    TemporalContext base_ctx;
    base_ctx.beatDur = 0.5;
    base_ctx.barDur = 2.0;
    base_ctx.phraseDur = 8.0;
    base_ctx.sectionDur = 32.0;

    const NumericVmBatchResult batch =
        execute_numeric_program_batch(compile_result.program, base_ctx,
                                      time_points, batch_results,
                                      std::size(time_points));

    REQUIRE_FALSE(batch.ok);
    REQUIRE(batch.error_at_index == 2);
    REQUIRE(batch_results[0] == Approx(1.0).epsilon(1e-9));
    REQUIRE(batch_results[1] == Approx(2.0).epsilon(1e-9));
    REQUIRE(batch_results[2] == Approx(2.0).epsilon(1e-9));
    REQUIRE(batch_results[3] == Approx(2.0).epsilon(1e-9));
}

TEST_CASE("Numeric VM rejects invalid constant folds for runtime math errors",
          "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    const auto div_result =
        compile_numeric_program(interp.get_parser()->parse("(/ 1 0)"),
                                *interp.get_environment());
    REQUIRE(div_result.ok);
    REQUIRE(has_opcode(div_result.program, NumericVmOpcode::DIV));
    REQUIRE_FALSE((div_result.program.instructions.size() == 2 &&
                   div_result.program.instructions[0].opcode ==
                       NumericVmOpcode::LOAD_CONST));

    const auto mod_result =
        compile_numeric_program(interp.get_parser()->parse("(% 1 0)"),
                                *interp.get_environment());
    REQUIRE(mod_result.ok);
    REQUIRE(has_opcode(mod_result.program, NumericVmOpcode::MOD));
    REQUIRE_FALSE((mod_result.program.instructions.size() == 2 &&
                   mod_result.program.instructions[0].opcode ==
                       NumericVmOpcode::LOAD_CONST));

    const auto sqrt_result =
        compile_numeric_program(interp.get_parser()->parse("(sqrt -1)"),
                                *interp.get_environment());
    REQUIRE(sqrt_result.ok);
    REQUIRE(has_opcode(sqrt_result.program, NumericVmOpcode::SQRT));
    REQUIRE_FALSE((sqrt_result.program.instructions.size() == 2 &&
                   sqrt_result.program.instructions[0].opcode ==
                       NumericVmOpcode::LOAD_CONST));
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

TEST_CASE("Tagged VM preserves runtime vector literals", "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    const auto compile_result =
        compile_numeric_program(interp.get_parser()->parse("[1 2 3]"),
                                *interp.get_environment());

    REQUIRE(compile_result.ok);
    REQUIRE(has_opcode(compile_result.program, NumericVmOpcode::MAKE_VECTOR));

    TemporalContext ctx;
    const TaggedVmExecutionResult result =
        execute_tagged_program(compile_result.program, ctx);
    REQUIRE(result.ok);
    REQUIRE(result.value == Value::vector({ Value(1), Value(2), Value(3) }));
}

TEST_CASE("Tagged VM aborts infinite while loops after iteration budget",
          "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    const auto compile_result =
        compile_numeric_program(interp.get_parser()->parse("(while 1 1)"),
                                *interp.get_environment());

    REQUIRE(compile_result.ok);

    TemporalContext ctx;
    const TaggedVmExecutionResult result =
        execute_tagged_program(compile_result.program, ctx);
    REQUIRE_FALSE(result.ok);
    REQUIRE(result.error.indexOf("loop ran too long") >= 0);
}

TEST_CASE("Output sampling handles let expressions through the VM path",
          "[modulisp][vm][outputs]")
{
    ModuLispInterpreter interp(nullptr, nullptr, nullptr, 8, 8, 8);
    interp.init();

    interp.eval("(a1 (let [x 1 y 2] (+ x y)))");

    bool ok = false;
    const double value = interp.eval_output_at_time("a1", 0.0, &ok);
    REQUIRE(ok);
    REQUIRE(value == Approx(3.0).epsilon(1e-9));
}

TEST_CASE("Output sampling handles defn and lambda callables through the VM path",
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

TEST_CASE("Output sampling reads input snapshots through the VM path",
          "[modulisp][vm][outputs]")
{
    ModuLispInterpreter interp(nullptr, nullptr, nullptr, 8, 8, 8);
    interp.init();

    interp.set_input_value(2, 4.25);
    interp.eval("(a1 (input 2))");

    bool ok = false;
    const double value = interp.eval_output_at_time("a1", 0.0, &ok);
    REQUIRE(ok);
    REQUIRE(value == Approx(4.25).epsilon(1e-9));
}

TEST_CASE("Public eval_v handles lambda callables directly", "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    const Value result = interp.eval_v("((lambda [x] (+ x 3)) 2)");
    REQUIRE(result.is_number());
    REQUIRE(result.as_float() == Approx(5.0).epsilon(1e-9));
}

TEST_CASE("Compiled eval preserves lambda_scope for captured closures",
          "[modulisp][vm]")
{
    ModuLispInterpreter interp;
    interp.init();

    interp.eval("(define make-adder (let [x 2] (lambda [y] (+ x y))))");
    interp.eval("(define x 99)");

    const Value result = interp.eval_v("(make-adder 3)");
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

TEST_CASE("Compiled output sampling preserves lambda_scope for captured closures",
          "[modulisp][vm][outputs]")
{
    ModuLispInterpreter interp(nullptr, nullptr, nullptr, 8, 8, 8);
    interp.init();

    interp.eval("(define make-adder (let [x 2] (lambda [y] (+ x y))))");
    interp.eval("(define x 99)");
    interp.eval("(a1 (make-adder t))");

    auto results = interp.eval_outputs(0.0, 1.0, 2, { "a1" });
    REQUIRE(results.find("a1") != results.end());
    REQUIRE(results["a1"].size() == 2);
    REQUIRE(results["a1"][0] == Approx(2.0).epsilon(1e-6));
    REQUIRE(results["a1"][1] == Approx(3.0).epsilon(1e-6));
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
    REQUIRE(define_result.error.indexOf("inside an output") >= 0);

    const auto eval_result =
        compile_numeric_program(interp.get_parser()->parse("(eval \"(+ t 1)\")"),
                                *interp.get_environment());
    REQUIRE_FALSE(eval_result.ok);
    REQUIRE(eval_result.error.indexOf("inside an output") >= 0);
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

// ===== Closure compilation edge cases =====

TEST_CASE("Closure captures multiple variables", "[modulisp][vm][closures]")
{
    ModuLispInterpreter interp;
    interp.init();

    interp.eval("(define f (let [a 10 b 20] (lambda [x] (+ a b x))))");
    interp.eval("(define a 999)");
    interp.eval("(define b 999)");

    const Value result = interp.eval_v("(f 3)");
    REQUIRE(result.is_number());
    REQUIRE(result.as_float() == Approx(33.0).epsilon(1e-9));
}

TEST_CASE("Closure captures a vector", "[modulisp][vm][closures]")
{
    ModuLispInterpreter interp;
    interp.init();

    interp.eval("(define lookup (let [tbl [10 20 30]] (lambda [i] (tbl i))))");

    const Value result = interp.eval_v("(lookup 0.5)");
    REQUIRE(result.is_number());
    REQUIRE(result.as_float() == Approx(20.0).epsilon(1e-6));
}

TEST_CASE("Nested let shadowing does not leak through closure capture",
          "[modulisp][vm][closures]")
{
    ModuLispInterpreter interp;
    interp.init();

    // Inner let shadows outer let; closure should capture inner x=5
    interp.eval("(define f (let [x 1] (let [x 5] (lambda [y] (+ x y)))))");
    interp.eval("(define x 999)");

    const Value result = interp.eval_v("(f 2)");
    REQUIRE(result.is_number());
    REQUIRE(result.as_float() == Approx(7.0).epsilon(1e-9));
}

TEST_CASE("Let binding shadows global during constant folding",
          "[modulisp][vm][closures]")
{
    // Regression: try_resolve_numeric_constant used to resolve symbols
    // through the global env without checking local scopes first.
    ModuLispInterpreter interp;
    interp.init();

    interp.eval("(define x 100)");

    const Value result = interp.eval_v("(let [x 3] (+ x 1))");
    REQUIRE(result.is_number());
    REQUIRE(result.as_float() == Approx(4.0).epsilon(1e-9));
}

TEST_CASE("Closure in output sampling with time-dependent argument",
          "[modulisp][vm][closures][outputs]")
{
    ModuLispInterpreter interp(nullptr, nullptr, nullptr, 8, 8, 8);
    interp.init();

    interp.eval("(define scale-sig (let [k 3] (lambda [x] (* k x))))");
    interp.eval("(define k 999)");
    interp.eval("(a1 (scale-sig t))");

    bool ok = false;
    const double value = interp.eval_output_at_time("a1", 2.0, &ok);
    REQUIRE(ok);
    REQUIRE(value == Approx(6.0).epsilon(1e-6));
}

TEST_CASE("Closure capturing another closure (nested call)",
          "[modulisp][vm][closures]")
{
    ModuLispInterpreter interp;
    interp.init();

    // inner captures add1; outer captures inner
    interp.eval("(define add1 (let [n 1] (lambda [x] (+ x n))))");
    interp.eval("(define double-add1 (let [f add1] (lambda [x] (f (f x)))))");
    interp.eval("(define n 999)");

    const Value result = interp.eval_v("(double-add1 5)");
    REQUIRE(result.is_number());
    REQUIRE(result.as_float() == Approx(7.0).epsilon(1e-9));
}

TEST_CASE("Closure compiles without CALL_INTRINSIC fallback",
          "[modulisp][vm][closures]")
{
    ModuLispInterpreter interp;
    interp.init();

    interp.eval("(define f (let [k 2] (lambda [x] (* k x))))");

    const auto result =
        compile_numeric_program(interp.get_parser()->parse("(f 5)"),
                                *interp.get_environment(), false);
    REQUIRE(result.ok);
    REQUIRE_FALSE(has_opcode(result.program, NumericVmOpcode::CALL_INTRINSIC));
}

TEST_CASE("Closure captures eagerly evaluated let binding",
          "[modulisp][vm][closures]")
{
    // let evaluates eagerly: (let [b base] ...) captures the value of
    // base at eval time, not the expression. At eval time t=0, so
    // base=(* 0 2)=0, and f(10) = 0 + 10 = 10.
    ModuLispInterpreter interp;
    interp.init();

    interp.eval("(define base (* t 2))");
    interp.eval("(define f (let [b base] (lambda [x] (+ b x))))");

    const Value result = interp.eval_v("(f 10)");
    REQUIRE(result.is_number());
    REQUIRE(result.as_float() == Approx(10.0).epsilon(1e-9));
}

TEST_CASE("Immediate closure application compiles natively",
          "[modulisp][vm][closures]")
{
    // ((let [x 5] (lambda [y] (+ x y))) 3) — closure created and called inline
    ModuLispInterpreter interp;
    interp.init();

    interp.eval("(define x 999)");

    const Value result = interp.eval_v("((let [x 5] (lambda [y] (+ x y))) 3)");
    REQUIRE(result.is_number());
    REQUIRE(result.as_float() == Approx(8.0).epsilon(1e-9));
}
