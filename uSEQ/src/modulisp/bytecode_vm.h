#pragma once

#include "../utils/string.h"
#include "lisp/environment.h"
#include "lisp/value.h"
#include "temporal_context.h"
#include <cstddef>
#include <cstdint>
#include <vector>

enum class NumericVmOpcode
{
    LOAD_CONST,
    LOAD_TIME,
    MOV,
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    NEG,
    CMP_GT,
    CMP_LT,
    CMP_GE,
    CMP_LE,
    CMP_EQ,
    FLOOR,
    CEIL,
    FRAC,
    ABS,
    MIN,
    MAX,
    POW,
    SQRT,
    CLAMP,
    SIN,
    COS,
    TAN,
    RET
};

enum class NumericVmTemporalChannel
{
    T,
    TIME,
    BEAT,
    BAR,
    PHRASE,
    SECTION,
    BEAT_NUM,
    BAR_NUM
};

struct NumericVmInstruction
{
    NumericVmOpcode opcode = NumericVmOpcode::RET;
    uint16_t rd = 0;
    uint16_t rs1 = 0;
    uint16_t rs2 = 0;
    uint16_t rs3 = 0;
    int32_t imm = 0;
    double imm0 = 0.0;
    double imm1 = 0.0;
};

struct NumericVmProgram
{
    std::vector<NumericVmInstruction> instructions;
    std::vector<double> constants;
    size_t register_count = 0;
};

struct NumericVmCompileResult
{
    bool ok = false;
    NumericVmProgram program;
    String error;
};

struct NumericVmExecutionResult
{
    bool ok = false;
    double value = 0.0;
    String error;
};

NumericVmCompileResult compile_numeric_program(const Value& expr,
                                               const Environment& env);
NumericVmExecutionResult execute_numeric_program(const NumericVmProgram& program,
                                                 const TemporalContext& ctx);
