#pragma once

#include "mips/recompiler.hpp"
#include "rsp.hpp"
#include "types.hpp"

#include <cassert>

namespace n64::rsp {

struct Recompiler : public mips::Recompiler<s32, s32, u32> {
    using mips::Recompiler<s32, s32, u32>::Recompiler;

    void add(u32 rs, u32 rt, u32 rd) const;
    void addi(u32 rs, u32 rt, s16 imm) const;
    void break_() const;
    void j(u32 instr) const;
    void jal(u32 instr) const;
    void lb(u32 rs, u32 rt, s16 imm) const;
    void lbu(u32 rs, u32 rt, s16 imm) const;
    void lh(u32 rs, u32 rt, s16 imm) const;
    void lhu(u32 rs, u32 rt, s16 imm) const;
    void lw(u32 rs, u32 rt, s16 imm) const;
    void lwu(u32 rs, u32 rt, s16 imm) const;
    void sb(u32 rs, u32 rt, s16 imm) const;
    void mfc0(u32 rt, u32 rd) const;
    void mtc0(u32 rt, u32 rd) const;
    void sh(u32 rs, u32 rt, s16 imm) const;
    void sub(u32 rs, u32 rt, u32 rd) const;
    void sw(u32 rs, u32 rt, s16 imm) const;

protected:
    template<std::integral> void load(u32 rs, u32 rt, s16 imm) const;
    template<std::integral> void store(u32 rs, u32 rt, s16 imm) const;
} inline constexpr cpu_recompiler{
    jit,
    gpr,
    lo_dummy,
    hi_dummy,
    pc,
    can_execute_dword_instrs_dummy,
    Jump,
};

} // namespace n64::rsp
