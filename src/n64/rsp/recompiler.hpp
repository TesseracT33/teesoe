#pragma once

#include "mips/recompiler.hpp"
#include "register_allocator.hpp"
#include "rsp.hpp"
#include "status.hpp"
#include "types.hpp"
#include "vu.hpp"

namespace n64::rsp {

void BlockEpilog();
Status InitRecompiler();
void Invalidate(u32 addr);
void InvalidateRange(u32 addr_lo, u32 addr_hi);
void LinkJit(u32 reg);
u32 RunRecompiler(u32 cpu_cycles);
void TearDownRecompiler();

inline AsmjitCompiler compiler;
inline RegisterAllocator reg_alloc{ gpr.view(),
    reg_alloc_volatile_gprs,
    reg_alloc_nonvolatile_gprs,
    reg_alloc_base_gpr_ptr_reg,
    compiler };
inline u32 jit_pc;
inline bool branch_hit, branched;
inline u32 block_cycles;

inline void TakeBranchJit(auto target)
{
    using namespace asmjit::x86;
    auto& c = compiler;
    if constexpr (std::integral<decltype(target)>) {
        c.mov(eax, s32(target));
    } else {
        if (target.r32() != eax) { // TODO: handle this better
            c.mov(eax, target.r32());
        }
    }
    c.mov(ptr(jump_addr), eax);
    c.mov(eax, 1);
    c.mov(ptr(in_branch_delay_slot), al);
    c.xor_(eax, eax);
    c.mov(ptr(jump_is_pending), al);
}

} // namespace n64::rsp
