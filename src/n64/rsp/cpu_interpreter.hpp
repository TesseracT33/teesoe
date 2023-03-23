#pragma once

#include "mips/interpreter.hpp"
#include "rsp.hpp"
#include "types.hpp"

#include <cassert>

namespace n64::rsp {

struct Interpreter : public mips::Interpreter<s32, u32, u32> {
    using mips::Interpreter<s32, u32, u32>::Interpreter;

    void add(u32 rs, u32 rt, u32 rd) const;
    void addi(u32 rs, u32 rt, s16 imm) const;
    void break_() const;
    void j(u32 instr) const;
    void jal(u32 instr) const;
    void lb(u32 rs, u32 rt, s16 imm) const;
    void lbu(u32 rs, u32 rt, s16 imm) const;
    void lh(u32 rs, u32 rt, s16 imm) const;
    void lhu(u32 rs, u32 rt, s16 imm) const;
    void ll(u32 rs, u32 rt, s16 imm) const;
    void lw(u32 rs, u32 rt, s16 imm) const;
    void lwu(u32 rs, u32 rt, s16 imm) const;
    void sb(u32 rs, u32 rt, s16 imm) const;
    void sc(u32 rs, u32 rt, s16 imm) const;
    void sh(u32 rs, u32 rt, s16 imm) const;
    void sub(u32 rs, u32 rt, u32 rd) const;
    void sw(u32 rs, u32 rt, s16 imm) const;
} inline constexpr cpu_interpreter{
    gpr,
    pc, // LO dummy
    pc, // HI dummy
    pc,
    in_branch_delay_slot,
    Jump,
    Link,
};

} // namespace n64::rsp
