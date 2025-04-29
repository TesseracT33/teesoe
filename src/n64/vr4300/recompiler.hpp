#pragma once

#include "jit_common.hpp"
#include "mips/register_allocator.hpp"
#include "numtypes.hpp"
#include "platform.hpp"
#include "status.hpp"
#include "vr4300.hpp"

#include <type_traits>

namespace n64::vr4300 {

void BlockEpilog();
void BlockEpilogWithJmp(void* func);
void BlockEpilogWithPcFlushAndJmp(void* func, int pc_offset = 0);
void BlockEpilogWithPcFlush(int pc_offset = 0);
void BlockProlog();
bool CheckDwordOpCondJit();
void EmitBranchDiscarded();
void EmitBranchNotTaken();
void EmitBranchTaken(u64 target);
void EmitBranchTaken(HostGpr64 target);
void EmitLink(u32 reg);
void FlushPc(int pc_offset = 0);
Status InitRecompiler();
void Invalidate(u32 paddr);
void InvalidateRange(u32 paddr_lo, u32 paddr_hi);
u32 RunRecompiler(u32 cpu_cycles);
void TearDownRecompiler();

inline constexpr std::array reg_alloc_volatile_gprs = [] {
    using namespace asmjit::x86;
    if constexpr (platform.abi.systemv) {
        return std::array<Gpq, 8>{ r11, r10, r9, r8, rcx, rdx, rsi, rdi };
    } else {
        return std::array<Gpq, 6>{ r11, r10, r9, r8, rdx, rcx };
    }
}();

inline constexpr std::array reg_alloc_nonvolatile_gprs = [] {
    using namespace asmjit::x86;
    if constexpr (platform.abi.systemv) {
        return std::array<Gpq, 5>{ rbx, r12, r13, r14, r15 };
    } else {
        return std::array<Gpq, 7>{ rbx, r12, r13, r14, r15, rdi, rsi };
    }
}();

constexpr HostGpr64 guest_gpr_base_ptr_reg = [] {
    if constexpr (platform.a64) return asmjit::a64::x17;
    if constexpr (platform.x64) return asmjit::x86::rbp;
}();

inline JitCompiler c;
inline mips::RegisterAllocator<s64, reg_alloc_volatile_gprs.size(), reg_alloc_nonvolatile_gprs.size()>
  reg_alloc{ gpr.view(), reg_alloc_volatile_gprs, reg_alloc_nonvolatile_gprs, guest_gpr_base_ptr_reg, c };
inline u64 jit_pc;
inline u32 block_cycles;
inline bool branched;

inline ptrdiff_t get_offset_to_guest_gpr_base_ptr(void const* obj)
{
    return static_cast<u8 const*>(obj) - reinterpret_cast<u8 const*>(&gpr) + sizeof(u64) * 16;
}

template<typename T> asmjit::x86::Mem JitPtr(T const& obj, u32 ptr_size = sizeof(std::remove_pointer_t<T>))
{
    auto obj_ptr = [&] {
        if constexpr (std::is_pointer_v<T>) return obj;
        else return &obj;
    }();
    ptrdiff_t diff = get_offset_to_guest_gpr_base_ptr(obj_ptr);
    return asmjit::x86::ptr(guest_gpr_base_ptr_reg, s32(diff), ptr_size);
}

template<typename T>
asmjit::x86::Mem JitPtrOffset(T const& obj, s32 offset, u32 ptr_size = sizeof(std::remove_pointer_t<T>))
{
    auto obj_ptr = [&] {
        if constexpr (std::is_pointer_v<T>) return obj;
        else return &obj;
    }();
    ptrdiff_t diff = get_offset_to_guest_gpr_base_ptr(obj_ptr) + offset;
    return asmjit::x86::ptr(guest_gpr_base_ptr_reg, s32(diff), ptr_size);
}

template<typename T>
asmjit::x86::Mem JitPtrOffset(T const& obj, asmjit::x86::Gp index, u32 ptr_size = sizeof(std::remove_pointer_t<T>))
{
    auto obj_ptr = [&] {
        if constexpr (std::is_pointer_v<T>) return obj;
        else return &obj;
    }();
    ptrdiff_t diff = reinterpret_cast<u8 const*>(obj_ptr) - reinterpret_cast<u8 const*>(&gpr);
    return asmjit::x86::ptr(guest_gpr_base_ptr_reg, index.r64(), 0u, s32(diff), ptr_size);
}

} // namespace n64::vr4300
