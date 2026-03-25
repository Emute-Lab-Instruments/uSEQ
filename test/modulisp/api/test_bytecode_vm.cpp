#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../../../uSEQ/src/modulisp/bytecode_vm.h"
#include "../../../uSEQ/src/modulisp/modulisp_interpreter.h"

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
