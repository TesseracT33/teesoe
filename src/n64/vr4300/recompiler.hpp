#pragma once

#include "exceptions.hpp"
#include "host.hpp"
#include "jit_util.hpp"
#include "mips/recompiler.hpp"
#include "mips/register_allocator.hpp"
#include "status.hpp"
#include "types.hpp"
#include "vr4300.hpp"

namespace n64::vr4300 {

void BlockEpilog();
void BlockEpilogWithJmp(void* func);
void BlockProlog();
void BlockRecordCycles();
void DiscardBranchJit();
bool CheckDwordOpCondJit();
Status InitRecompiler();
void Invalidate(u32 addr);
void InvalidateRange(u32 addr_lo, u32 addr_hi);
void LinkJit(u32 reg);
void OnBranchNotTakenJit();
u64 RunRecompiler(u64 cpu_cycles);
void TakeBranchJit(asmjit::x86::Gp reg);

inline AsmjitCompiler compiler;
inline mips::RegisterAllocator reg_alloc{ gpr.view(), compiler };
inline u64 jit_pc;
inline u64 block_cycles;
inline bool branch_hit, branched;

void JitCallInterpreterImpl(auto impl)
{
    reg_alloc.Call(impl);
}

template<typename Arg, typename... Args> void JitCallInterpreterImpl(auto impl, Arg first_arg, Args... remaining_args)
{
    static_assert(1 + sizeof...(remaining_args) <= host_gpr_arg.size(),
      "This function does not support passing arguments by the stack");
    static int r_idx{};
    if (r_idx == 0) {
        reg_alloc.FlushAllVolatile();
    }
    if constexpr (sizeof(first_arg) <= 4) {
        compiler.mov(host_gpr_arg[r_idx].r32(), first_arg);
    } else {
        compiler.mov(host_gpr_arg[r_idx].r64(), first_arg);
    }
    if constexpr (sizeof...(remaining_args)) {
        r_idx++;
    } else {
        r_idx = 0;
    }
    JitCallInterpreterImpl(impl, remaining_args...);
}

} // namespace n64::vr4300
