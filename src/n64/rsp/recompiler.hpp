#pragma once

#include "mips/recompiler.hpp"
#include "numtypes.hpp"
#include "rsp.hpp"
#include "status.hpp"

namespace n64::rsp {

void BlockEpilog();
void BlockEpilogWithPcFlushAndJmp(void* func, int pc_offset = 0);
void BlockEpilogWithPcFlush(int pc_offset);
void EmitBranchTaken(u32 target);
void EmitBranchTaken(HostGpr32 target);
Status InitRecompiler();
void Invalidate(u32 addr);
void InvalidateRange(u32 addr_lo, u32 addr_hi);
void EmitLink(u32 reg);
u32 RunRecompiler(u32 cpu_cycles);
void TearDownRecompiler();

inline JitCompiler c;
inline u32 jit_pc;
inline u32 block_cycles;
inline bool last_instr_was_branch;
inline bool branched;

template<typename T> asmjit::x86::Mem JitPtr(T const& obj, u32 ptr_size = sizeof(T))
{
    std::ptrdiff_t diff = reinterpret_cast<u8 const*>(&obj) - reinterpret_cast<u8 const*>(gpr.ptr(0));
    return asmjit::x86::ptr(asmjit::x86::rbp, s32(diff), ptr_size);
}

template<typename T> asmjit::x86::Mem JitPtr(T const* obj, u32 ptr_size = sizeof(T))
{
    std::ptrdiff_t diff = reinterpret_cast<u8 const*>(obj) - reinterpret_cast<u8 const*>(gpr.ptr(0));
    return asmjit::x86::ptr(asmjit::x86::rbp, s32(diff), ptr_size);
}

template<typename T> asmjit::x86::Mem JitPtrOffset(T const& obj, u32 index, u32 ptr_size = sizeof(T))
{
    std::ptrdiff_t diff = reinterpret_cast<u8 const*>(&obj) + index - reinterpret_cast<u8 const*>(gpr.ptr(0));
    return asmjit::x86::ptr(asmjit::x86::rbp, s32(diff), ptr_size);
}

template<typename T> asmjit::x86::Mem JitPtrOffset(T const* obj, u32 index, u32 ptr_size = sizeof(T))
{
    std::ptrdiff_t diff = reinterpret_cast<u8 const*>(obj) + index - reinterpret_cast<u8 const*>(gpr.ptr(0));
    return asmjit::x86::ptr(asmjit::x86::rbp, s32(diff), ptr_size);
}

template<typename T> asmjit::x86::Mem JitPtrOffset(T const& obj, asmjit::x86::Gp index, u32 ptr_size = sizeof(T))
{
    std::ptrdiff_t diff = reinterpret_cast<u8 const*>(&obj) - reinterpret_cast<u8 const*>(gpr.ptr(0));
    return asmjit::x86::ptr(asmjit::x86::rbp, index.r64(), 0u, s32(diff), ptr_size);
}

template<typename T> asmjit::x86::Mem JitPtrOffset(T const* obj, asmjit::x86::Gp index, u32 ptr_size = sizeof(T))
{
    std::ptrdiff_t diff = reinterpret_cast<u8 const*>(obj) - reinterpret_cast<u8 const*>(gpr.ptr(0));
    return asmjit::x86::ptr(asmjit::x86::rbp, index.r64(), 0u, s32(diff), ptr_size);
}

} // namespace n64::rsp
