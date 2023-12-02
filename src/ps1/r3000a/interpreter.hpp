#pragma once

#include "exceptions.hpp"
#include "mips/interpreter.hpp"
#include "r3000a.hpp"

namespace ps1::r3000a {

inline constexpr bool can_execute_dword_instrs_dummy{ false };

void link(u32 reg);
u32 run_interpreter(u32 cycles);
void take_branch(s32 target);

struct Interpreter : public mips::Interpreter<s32, s32, s32> {
    using mips::Interpreter<s32, s32, s32>::Interpreter;

    void break_() const;
    void div(u32 rs, u32 rt) const;
    void divu(u32 rs, u32 rt) const;
    void lb(u32 rs, u32 rt, s16 imm) const;
    void lbu(u32 rs, u32 rt, s16 imm) const;
    void lh(u32 rs, u32 rt, s16 imm) const;
    void lhu(u32 rs, u32 rt, s16 imm) const;
    void lw(u32 rs, u32 rt, s16 imm) const;
    void lwl(u32 rs, u32 rt, s16 imm) const;
    void lwr(u32 rs, u32 rt, s16 imm) const;
    void lwu(u32 rs, u32 rt, s16 imm) const;
    void mfhi(u32 rd) const;
    void mflo(u32 rd) const;
    void mthi(u32 rs) const;
    void mtlo(u32 rs) const;
    void mult(u32 rs, u32 rt) const;
    void multu(u32 rs, u32 rt) const;
    void sb(u32 rs, u32 rt, s16 imm) const;
    void sc(u32 rs, u32 rt, s16 imm) const;
    void sh(u32 rs, u32 rt, s16 imm) const;
    void syscall() const;
    void sw(u32 rs, u32 rt, s16 imm) const;
    void swl(u32 rs, u32 rt, s16 imm) const;
    void swr(u32 rs, u32 rt, s16 imm) const;
} inline constexpr cpu_interpreter{
    gpr,
    lo,
    hi,
    pc,
    can_execute_dword_instrs_dummy,
    take_branch,
    link,
    integer_overflow_exception,
    reserved_instruction_exception,
};

} // namespace ps1::r3000a
