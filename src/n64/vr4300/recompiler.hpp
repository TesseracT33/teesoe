#pragma once

#include "exceptions.hpp"
#include "host.hpp"
#include "jit_util.hpp"
#include "mips/recompiler.hpp"
#include "mips/register_allocator.hpp"
#include "status.hpp"
#include "types.hpp"
#include "vr4300.hpp"

#include <concepts>

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
u32 RunRecompiler(u32 cpu_cycles);
void TearDownRecompiler();

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

template<typename T> auto GlobalVarPtr(T const& obj, u32 ptr_size = sizeof(T))
{
    if constexpr (std::is_pointer_v<T>) {
        return jit_mem_global_var(asmjit::x86::rbp, gpr.ptr(0), obj, ptr_size);
    } else {
        return jit_mem_global_var(asmjit::x86::rbp, gpr.ptr(0), &obj, ptr_size);
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

inline void JitCallInterpreterImpl(auto impl)
{
    reg_alloc.Call(impl);
}

template<typename Arg, typename... Args>
inline void JitCallInterpreterImpl(auto impl, Arg first_arg, Args... remaining_args)
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

inline void TakeBranchJit(auto target)
{
    using namespace asmjit::x86;
    auto& c = compiler;
    c.mov(GlobalVarPtr(in_branch_delay_slot_taken), 1);
    c.mov(GlobalVarPtr(in_branch_delay_slot_not_taken), 0);
    c.mov(GlobalVarPtr(branch_state), std::to_underlying(mips::BranchState::DelaySlotTaken));
    if constexpr (std::integral<decltype(target)>) {
        c.mov(rax, target);
        c.mov(GlobalVarPtr(jump_addr), rax);
    } else {
        c.mov(GlobalVarPtr(jump_addr), target.r64());
    }
}

} // namespace n64::vr4300
