#pragma once

#include "exceptions.hpp"
#include "jit_util.hpp"
#include "mips/recompiler.hpp"
#include "mips/register_allocator.hpp"
#include "numtypes.hpp"
#include "platform.hpp"
#include "status.hpp"
#include "vr4300.hpp"

#include <concepts>

namespace n64::vr4300 {

void BlockEpilog();
void BlockEpilogWithJmp(void* func);
void BlockEpilogWithPcFlushAndJmp(void* func, int pc_offset = 0);
void BlockEpilogWithPcFlush(int pc_offset = 0);
void BlockProlog();
void BlockRecordCycles();
void DiscardBranchJit();
bool CheckDwordOpCondJit();
void FlushPc(int pc_offset = 0);
Status InitRecompiler();
void Invalidate(u32 addr);
void InvalidateRange(u32 addr_lo, u32 addr_hi);
void LinkJit(u32 reg);
void OnBranchNotTakenJit();
u32 RunRecompiler(u32 cpu_cycles);
void TearDownRecompiler();

inline bool compiler_exception_occurred;

inline constexpr std::array reg_alloc_volatile_gprs = [] {
    using namespace asmjit::x86;
    if constexpr (os.linux) {
        return std::array<Gpq, 8>{ r11, r10, r9, r8, rcx, rdx, rsi, rdi };
    } else {
        return std::array<Gpq, 6>{ r11, r10, r9, r8, rdx, rcx };
    }
}();

inline constexpr std::array reg_alloc_nonvolatile_gprs = [] {
    using namespace asmjit::x86;
    if constexpr (os.linux) {
        return std::array<Gpq, 5>{ rbx, r12, r13, r14, r15 };
    } else {
        return std::array<Gpq, 7>{ rbx, r12, r13, r14, r15, rdi, rsi };
    }
}();

inline constexpr HostGpr reg_alloc_base_gpr_ptr_reg = [] {
    if constexpr (arch.a64) return asmjit::a64::x0;
    if constexpr (arch.x64) return asmjit::x86::rbp;
}();

inline AsmjitCompiler compiler;
inline mips::RegisterAllocator<s64, reg_alloc_volatile_gprs.size(), reg_alloc_nonvolatile_gprs.size()>
  reg_alloc{ gpr.view(), reg_alloc_volatile_gprs, reg_alloc_nonvolatile_gprs, reg_alloc_base_gpr_ptr_reg, compiler };
inline u64 jit_pc;
inline u32 block_cycles;
inline bool branch_hit, branched;

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
    c.mov(JitPtr(in_branch_delay_slot_taken), 1);
    c.mov(JitPtr(in_branch_delay_slot_not_taken), 0);
    c.mov(JitPtr(branch_state), std::to_underlying(mips::BranchState::Perform));
    if constexpr (std::integral<decltype(target)>) {
        c.mov(rax, target);
        c.mov(JitPtr(jump_addr), rax);
    } else {
        c.mov(JitPtr(jump_addr), target.r64());
    }
}

} // namespace n64::vr4300
