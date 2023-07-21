#pragma once

#include <bit>
#include <concepts>

#include "jit_util.hpp"
#include "mips/types.hpp"

namespace mips {

template<std::signed_integral GprInt, std::integral PcInt, typename RegisterAllocator> struct Recompiler {
    using BlockEpilogWithPcFlushAndJmpHandler = void (*)(void*, int);
    using CheckCanExecDwordInstrHandler = bool (*)();
    using ExceptionHandler = void (*)();
    using GetLoHiPtrHandler = std::conditional_t<arch.a64, asmjit::a64::Mem, asmjit::x86::Mem> (*)();
    using IndirectJumpHandler = void (*)(SizeToHostReg<sizeof(PcInt)>::type target);
    using LinkHandler = void (*)(u32 reg);
    using TakeBranchHandler = void (*)(PcInt target);

    consteval Recompiler(AsmjitCompiler& compiler,
      RegisterAllocator& reg_alloc,
      PcInt& jit_pc,
      bool& branch_hit,
      bool& branched,
      GetLoHiPtrHandler get_lo_ptr_handler,
      GetLoHiPtrHandler get_hi_ptr_handler,
      TakeBranchHandler take_branch_handler,
      IndirectJumpHandler indirect_jump_handler,
      LinkHandler link_handler,
      BlockEpilogWithPcFlushAndJmpHandler block_epilog_with_pc_flush_and_jmp =
        nullptr, // only for cpus supporting exceptions
      ExceptionHandler integer_overflow_exception = nullptr,
      ExceptionHandler trap_exception = nullptr,
      CheckCanExecDwordInstrHandler check_can_exec_dword_instr = nullptr) // MIPS64 only
      : c(compiler),
        reg_alloc(reg_alloc),
        jit_pc(jit_pc),
        branch_hit(branch_hit),
        branched(branched),
        get_lo_ptr(get_lo_ptr_handler),
        get_hi_ptr(get_hi_ptr_handler),
        take_branch(take_branch_handler),
        indirect_jump(indirect_jump_handler),
        link(link_handler),
        check_can_exec_dword_instr(check_can_exec_dword_instr),
        block_epilog_with_pc_flush_and_jmp(block_epilog_with_pc_flush_and_jmp),
        integer_overflow_exception(integer_overflow_exception),
        trap_exception(trap_exception)
    {
    }

    AsmjitCompiler& c;
    RegisterAllocator& reg_alloc;
    PcInt& jit_pc;
    bool& branch_hit;
    bool& branched;
    GetLoHiPtrHandler const get_lo_ptr, get_hi_ptr;
    TakeBranchHandler const take_branch;
    IndirectJumpHandler const indirect_jump;
    LinkHandler const link;
    CheckCanExecDwordInstrHandler const check_can_exec_dword_instr;
    BlockEpilogWithPcFlushAndJmpHandler const block_epilog_with_pc_flush_and_jmp;
    ExceptionHandler const integer_overflow_exception, trap_exception;

    static constexpr bool mips32 = sizeof(GprInt) == 4;
    static constexpr bool mips64 = sizeof(GprInt) == 8;

protected:
    auto GetGpr(u32 idx) const
    {
        auto r = reg_alloc.GetHostGpr(idx);
        if constexpr (arch.a64) {
            if constexpr (mips32) {
                return r.w();
            } else {
                return r.x();
            }
        } else {
            if constexpr (mips32) {
                return r.r32();
            } else {
                return r.r64();
            }
        }
    }

    HostGpr32 GetGpr32(u32 idx) const
    {
        if constexpr (arch.a64) {
            return reg_alloc.GetHostGpr(idx).w();
        } else {
            return reg_alloc.GetHostGpr(idx).r32();
        }
    }

    auto GetDirtyGpr(u32 idx) const
    {
        auto r = reg_alloc.GetHostGprMarkDirty(idx);
        if constexpr (arch.a64) {
            if constexpr (mips32) {
                return r.w();
            } else {
                return r.x();
            }
        } else {
            if constexpr (mips32) {
                return r.r32();
            } else {
                return r.r64();
            }
        }
    }

    HostGpr32 GetDirtyGpr32(u32 idx) const
    {
        if constexpr (arch.a64) {
            return reg_alloc.GetHostGprMarkDirty(idx).w();
        } else {
            return reg_alloc.GetHostGprMarkDirty(idx).r32();
        }
    }
};

} // namespace mips
