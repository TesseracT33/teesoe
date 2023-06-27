#pragma once

#include "mips/recompiler.hpp"
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
inline u32 jit_pc;
inline bool branch_hit, branched;
inline u32 block_cycles;

void BlockEpilogWithPcFlush(int pc_offset);

template<typename T> auto GlobalVarPtr(T const& obj)
{
    if constexpr (std::is_pointer_v<T>) {
        return jit_mem_global_var(asmjit::x86::rbp, gpr.ptr(0), obj);
    } else {
        return jit_mem_global_var(asmjit::x86::rbp, gpr.ptr(0), &obj);
    }
}

template<typename T> auto GlobalArrPtrWithImmOffset(T const& obj, u32 index, size_t ptr_size)
{
    if constexpr (std::is_pointer_v<T>) {
        return jit_mem_global_arr_with_imm_index(asmjit::x86::rbp, index, gpr.ptr(0), obj, ptr_size);
    } else {
        return jit_mem_global_arr_with_imm_index(asmjit::x86::rbp, index, gpr.ptr(0), &obj, ptr_size);
    }
}

template<typename T> auto GlobalArrPtrWithRegOffset(T const& obj, asmjit::x86::Gp index, size_t ptr_size)
{
    if constexpr (std::is_pointer_v<T>) {
        return jit_mem_global_arr_with_reg_index(asmjit::x86::rbp, index.r64(), gpr.ptr(0), obj, ptr_size);
    } else {
        return jit_mem_global_arr_with_reg_index(asmjit::x86::rbp, index.r64(), gpr.ptr(0), &obj, ptr_size);
    }
}

inline void TakeBranchJit(auto target)
{
    using namespace asmjit::x86;
    auto& c = compiler;
    auto jump_addr_ptr = GlobalVarPtr(jump_addr);
    if constexpr (std::integral<decltype(target)>) {
        c.mov(jump_addr_ptr, s32(target & 0xFFC));
    } else {
        if (target.r32() != eax) c.mov(eax, target.r32());
        c.and_(eax, 0xFFC);
        c.mov(jump_addr_ptr, eax);
    }
    c.mov(GlobalVarPtr(in_branch_delay_slot), 1);
    c.mov(GlobalVarPtr(jump_is_pending), 0);
}

} // namespace n64::rsp
