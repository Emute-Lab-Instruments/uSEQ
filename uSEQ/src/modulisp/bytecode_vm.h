#pragma once

#include "../utils/string.h"
#include "lisp/environment.h"
#include "lisp/value.h"
#include "temporal_context.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

enum class NumericVmOpcode
{
    LOAD_CONST,
    LOAD_TIME,
    MOV,
    VEC_INDEX,
    VEC_LERP,
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
    BRANCH,
    BRANCH_IF,
    BRANCH_UNLESS,
    CALL,
    CALL_INTRINSIC,
    RET,
    IS_NIL,
    IS_NUMBER,
    IS_LIST,
    IS_STRING,
    NOT,
    AND,
    OR,
    MAKE_LIST,
    LIST_HEAD,
    LIST_TAIL,
    LIST_LENGTH,
    U_SIN,
    U_COS,
    U_SIN_BI,
    U_COS_BI,
    TRI,
    SQR,
    PULSE,
    LOAD_INPUT
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

using TaggedVmIntrinsic = std::function<Value(const std::vector<Value>&,
                                              const TemporalContext&)>;

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
    std::vector<Value> constants;
    std::vector<std::vector<double>> data_segments;
    std::vector<String> dependencies;
    std::vector<std::shared_ptr<NumericVmProgram>> functions;
    std::vector<TaggedVmIntrinsic> intrinsics;
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

struct TaggedVmExecutionResult
{
    bool ok = false;
    Value value;
    String error;
};

NumericVmCompileResult compile_numeric_program(const Value& expr,
                                               const Environment& env,
                                               bool signal_context = false);
TaggedVmExecutionResult execute_tagged_program(const NumericVmProgram& program,
                                               const TemporalContext& ctx);
NumericVmExecutionResult execute_numeric_program(const NumericVmProgram& program,
                                                 const TemporalContext& ctx);
