#pragma once

#include "jit_common.hpp"
#include "numtypes.hpp"
#include "platform.hpp"
#include "register_allocator.hpp"
#include "rsp.hpp"
#include "status.hpp"

#if PLATFORM_A64
#    define RSP_INSTRUCTIONS_NAMESPACE n64::rsp::a64
#elif PLATFORM_X64
#    define RSP_INSTRUCTIONS_NAMESPACE n64::rsp::x64
#else
#    error "Unrecognized platform"
#endif
#include "instructions.hpp"
#undef RSP_INSTRUCTIONS_NAMESPACE

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
inline RegisterAllocator reg_alloc{ c, gpr.view(), std::span<m128i const, 32>{ vpr } };
inline u32 jit_pc;
inline u32 block_cycles;
inline bool last_instr_was_branch;
inline bool branched;

inline ptrdiff_t get_offset_to_guest_gpr_base_ptr(void const* obj)
{
    return static_cast<u8 const*>(obj) - reinterpret_cast<u8 const*>(&gpr) + sizeof(gpr[0]) * 16;
}

template<typename T> asmjit::x86::Mem JitPtr(T const& obj, u32 ptr_size = sizeof(std::remove_pointer_t<T>))
{
    auto obj_ptr = [&] {
        if constexpr (std::is_pointer_v<T>) return obj;
        else return &obj;
    }();
    ptrdiff_t diff = get_offset_to_guest_gpr_base_ptr(obj_ptr);
    return asmjit::x86::ptr(guest_gpr_mid_ptr_reg, s32(diff), ptr_size);
}

template<typename T>
asmjit::x86::Mem JitPtrOffset(T const& obj, s32 offset, u32 ptr_size = sizeof(std::remove_pointer_t<T>))
{
    auto obj_ptr = [&] {
        if constexpr (std::is_pointer_v<T>) return obj;
        else return &obj;
    }();
    ptrdiff_t diff = get_offset_to_guest_gpr_base_ptr(obj_ptr) + offset;
    return asmjit::x86::ptr(guest_gpr_mid_ptr_reg, s32(diff), ptr_size);
}

template<typename T>
asmjit::x86::Mem JitPtrOffset(T const& obj, asmjit::x86::Gp index, u32 ptr_size = sizeof(std::remove_pointer_t<T>))
{
    auto obj_ptr = [&] {
        if constexpr (std::is_pointer_v<T>) return obj;
        else return &obj;
    }();
    ptrdiff_t diff = reinterpret_cast<u8 const*>(obj_ptr) - reinterpret_cast<u8 const*>(&gpr);
    return asmjit::x86::ptr(guest_gpr_mid_ptr_reg, index.r64(), 0u, s32(diff), ptr_size);
}

} // namespace n64::rsp
