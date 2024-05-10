#pragma once

#include "mips/recompiler.hpp"
#include "numtypes.hpp"
#include "rsp.hpp"
#include "status.hpp"
#include "vu.hpp"

namespace n64::rsp {

void BlockEpilog();
void BlockEpilogWithPcFlushAndJmp(void* func, int pc_offset = 0);
void BlockEpilogWithPcFlush(int pc_offset);
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

template<typename T> asmjit::x86::Mem JitPtr(T const& obj, u32 ptr_size = sizeof(T))
{
    std::ptrdiff_t diff = reinterpret_cast<u8 const*>(&obj) - reinterpret_cast<u8 const*>(gpr.ptr(0));
    return asmjit::x86::ptr(asmjit::x86::rbp, s32(diff), ptr_size);
}

template<typename T> asmjit::x86::Mem JitPtrOffset(T const& obj, u32 index, size_t ptr_size = sizeof(T))
{
    std::ptrdiff_t diff = reinterpret_cast<u8 const*>(&obj) + index - reinterpret_cast<u8 const*>(gpr.ptr(0));
    return asmjit::x86::ptr(asmjit::x86::rbp, s32(diff), ptr_size);
}

template<typename T> asmjit::x86::Mem JitPtrOffset(T const& obj, asmjit::x86::Gp index, size_t ptr_size = sizeof(T))
{
    std::ptrdiff_t diff = reinterpret_cast<u8 const*>(&obj) - reinterpret_cast<u8 const*>(gpr.ptr(0));
    return asmjit::x86::ptr(asmjit::x86::rbp, index.r64(), 0u, s32(diff), ptr_size);
}

inline void TakeBranchJit(auto target)
{
    using namespace asmjit::x86;
    auto& c = compiler;
    if constexpr (std::integral<decltype(target)>) {
        c.mov(JitPtr(jump_addr), s32(target & 0xFFC));
    } else {
        if (target.r32() != eax) c.mov(eax, target.r32());
        c.and_(eax, 0xFFC);
        c.mov(JitPtr(jump_addr), eax);
    }
    c.mov(JitPtr(jump_is_pending), 1);
}

} // namespace n64::rsp
