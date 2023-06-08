#pragma once

#include "mips/recompiler.hpp"
#include "rsp.hpp"
#include "status.hpp"
#include "types.hpp"

namespace n64::rsp {

Status InitRecompiler();
void Invalidate(u32 addr);
void InvalidateRange(u32 addr_lo, u32 addr_hi);
void LinkJit(u32 reg);
void OnBranchJit();
u64 RunRecompiler(u64 cpu_cycles);
void TakeBranchJit(asmjit::x86::Gp target);

inline AsmjitCompiler compiler;
inline mips::RegisterAllocator reg_alloc{ gpr.view(), compiler };
inline u32 jit_pc;
inline bool branch_hit;

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
    compiler,
    reg_alloc,
    lo_dummy,
    hi_dummy,
    jit_pc,
    branch_hit,
    branch_hit, // DUMMY
    TakeBranchJit,
    LinkJit,
};

} // namespace n64::rsp
