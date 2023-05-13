#pragma once

#include "asmjit/a64.h"
#include "asmjit/x86.h"
#include "exceptions.hpp"
#include "host.hpp"
#include "mips/recompiler.hpp"
#include "status.hpp"
#include "types.hpp"
#include "vr4300.hpp"

namespace n64::vr4300 {

Status InitRecompiler();
void Invalidate(u32 addr);
void InvalidateRange(u32 addr_lo, u32 addr_hi);
void OnBranchJit();
u64 RunRecompiler(u64 cpu_cycles);

inline std::conditional_t<arch.x64, asmjit::x86::Compiler, asmjit::a64::Compiler> compiler;

struct Recompiler : public mips::Recompiler<s64, s64, u64> {
    using mips::Recompiler<s64, s64, u64>::Recompiler;

    void beq(u32 rs, u32 rt, s16 imm) const;
    void beql(u32 rs, u32 rt, s16 imm) const;
    void bgez(u32 rs, s16 imm) const;
    void bgezal(u32 rs, s16 imm) const;
    void bgezall(u32 rs, s16 imm) const;
    void bgezl(u32 rs, s16 imm) const;
    void bgtz(u32 rs, s16 imm) const;
    void bgtzl(u32 rs, s16 imm) const;
    void blez(u32 rs, s16 imm) const;
    void blezl(u32 rs, s16 imm) const;
    void bltz(u32 rs, s16 imm) const;
    void bltzal(u32 rs, s16 imm) const;
    void bltzall(u32 rs, s16 imm) const;
    void bltzl(u32 rs, s16 imm) const;
    void bne(u32 rs, u32 rt, s16 imm) const;
    void bnel(u32 rs, u32 rt, s16 imm) const;
    void break_() const;
    void ddiv(u32 rs, u32 rt) const;
    void ddivu(u32 rs, u32 rt) const;
    void div(u32 rs, u32 rt) const;
    void divu(u32 rs, u32 rt) const;
    void dmult(u32 rs, u32 rt) const;
    void dmultu(u32 rs, u32 rt) const;
    void j(u32 instr) const;
    void jal(u32 instr) const;
    void jalr(u32 rs, u32 rd) const;
    void jr(u32 rs) const;
    void lb(u32 rs, u32 rt, s16 imm) const;
    void lbu(u32 rs, u32 rt, s16 imm) const;
    void ld(u32 rs, u32 rt, s16 imm) const;
    void ldl(u32 rs, u32 rt, s16 imm) const;
    void ldr(u32 rs, u32 rt, s16 imm) const;
    void lh(u32 rs, u32 rt, s16 imm) const;
    void lhu(u32 rs, u32 rt, s16 imm) const;
    void ll(u32 rs, u32 rt, s16 imm) const;
    void lld(u32 rs, u32 rt, s16 imm) const;
    void lw(u32 rs, u32 rt, s16 imm) const;
    void lwl(u32 rs, u32 rt, s16 imm) const;
    void lwr(u32 rs, u32 rt, s16 imm) const;
    void lwu(u32 rs, u32 rt, s16 imm) const;
    void mult(u32 rs, u32 rt) const;
    void multu(u32 rs, u32 rt) const;
    void sb(u32 rs, u32 rt, s16 imm) const;
    void sc(u32 rs, u32 rt, s16 imm) const;
    void scd(u32 rs, u32 rt, s16 imm) const;
    void sd(u32 rs, u32 rt, s16 imm) const;
    void sdl(u32 rs, u32 rt, s16 imm) const;
    void sdr(u32 rs, u32 rt, s16 imm) const;
    void sh(u32 rs, u32 rt, s16 imm) const;
    void sync() const;
    void syscall() const;
    void sw(u32 rs, u32 rt, s16 imm) const;
    void swl(u32 rs, u32 rt, s16 imm) const;
    void swr(u32 rs, u32 rt, s16 imm) const;

private:
    template<std::integral> void load(u32 rs, u32 rt, s16 imm) const;
    template<std::integral> void load_left(u32 rs, u32 rt, s16 imm) const;
    template<std::integral> void load_linked(u32 rs, u32 rt, s16 imm) const;
    template<std::integral> void load_right(u32 rs, u32 rt, s16 imm) const;
    template<bool unsig> void multiply32(u32 rs, u32 rt) const;
    template<bool unsig> void multiply64(u32 rs, u32 rt) const;
    template<std::integral> void store(u32 rs, u32 rt, s16 imm) const;
    template<std::integral> void store_conditional(u32 rs, u32 rt, s16 imm) const;
    template<std::integral> void store_left(u32 rs, u32 rt, s16 imm) const;
    template<std::integral> void store_right(u32 rs, u32 rt, s16 imm) const;
} inline constexpr cpu_recompiler{
    compiler,
    gpr,
    lo,
    hi,
    pc,
    can_execute_dword_instrs,
    [](u64) {},
    [](u32) {},
    OnBranchJit,
    IntegerOverflowException,
    ReservedInstructionException,
    TrapException,
};

} // namespace n64::vr4300
