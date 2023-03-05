#pragma once

#include "types.hpp"

#include <concepts>

namespace n64::vr4300 {

template<typename T>
concept FpuNumericType = std::same_as<f32, T> || std::same_as<f64, T> || std::same_as<s32, T> || std::same_as<s64, T>;

enum class Cop1Instruction {
    /* Load/store/transfer instructions */
    LWC1,
    SWC1,
    LDC1,
    SDC1,
    MTC1,
    MFC1,
    CTC1,
    CFC1,
    DMTC1,
    DMFC1,
    DCFC1,
    DCTC1,

    /* Conversion instructions */
    CVT_S,
    CVT_D,
    CVT_L,
    CVT_W,
    ROUND_L,
    ROUND_W,
    TRUNC_L,
    TRUNC_W,
    CEIL_L,
    CEIL_W,
    FLOOR_L,
    FLOOR_W,

    /* Computational instructions */
    ADD,
    SUB,
    MUL,
    DIV,
    ABS,
    MOV,
    NEG,
    SQRT,

    /* Branch instructions */
    BC1T,
    BC1F,
    BC1TL,
    BC1FL,

    /* Compare */
    C
};

/* COP1/FPU instructions */
template<Cop1Instruction> void FpuLoad(u32 instr_code);
template<Cop1Instruction> void FpuStore(u32 instr_code);
template<Cop1Instruction> void FpuMove(u32 instr_code);
template<Cop1Instruction> void FpuConvert(u32 instr_code);
template<Cop1Instruction> void FpuCompute(u32 instr_code);
template<Cop1Instruction> void FpuBranch(u32 instr_code);
void FpuCompare(u32 instr_code);
void InitializeFpu();

} // namespace n64::vr4300
