#pragma once

#include "mips/gpr.hpp"
#include "types.hpp"

#include <array>

namespace n64::rsp {

enum class ScalarInstruction {
    /* Load instructions */
    LB,
    LBU,
    LH,
    LHU,
    LW,
    LWU,
    LL,
    /* Store instructions */
    SB,
    SH,
    SW,
    SC,
    /* ALU immediate instructions */
    ADDI,
    ADDIU,
    SLTI,
    SLTIU,
    ANDI,
    ORI,
    XORI,
    LUI,
    /* ALU three-operand instructions */
    ADD,
    ADDU,
    SUB,
    SUBU,
    SLT,
    SLTU,
    AND,
    OR,
    XOR,
    NOR,
    /* ALU shift instructions */
    SLL,
    SRL,
    SRA,
    SLLV,
    SRLV,
    SRAV,
    /* Jump instructions */
    J,
    JAL,
    JR,
    JALR,
    /* Branch instructions */
    BEQ,
    BNE,
    BLEZ,
    BGTZ,
    BLTZ,
    BGEZ,
    BLTZAL,
    BGEZAL,
    /* Special instructions */
    BREAK,
    /* COP0 Move instructions */
    MFC0,
    MTC0
};

/* Main processor instructions */
template<ScalarInstruction> void ScalarLoad(u32 instr_code);
template<ScalarInstruction> void ScalarStore(u32 instr_code);
template<ScalarInstruction> void AluImmediate(u32 instr_code);
template<ScalarInstruction> void AluThreeOperand(u32 instr_code);
template<ScalarInstruction> void Shift(u32 instr_code);
template<ScalarInstruction> void Jump(u32 instr_code);
template<ScalarInstruction> void Branch(u32 instr_code);
template<ScalarInstruction> void Move(u32 instr_code);
void Break();

inline mips::Gpr<s32> gpr;

inline bool ll_bit; /* Read from / written to by load linked and store conditional instructions. */

} // namespace n64::rsp
