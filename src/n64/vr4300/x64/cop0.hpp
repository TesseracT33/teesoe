#pragma once

#include "vr4300/cop0.hpp"
#include "vr4300/recompiler.hpp"

#include <concepts>

namespace n64::vr4300::x64 {

using namespace asmjit;
using namespace asmjit::x86;

inline bool can_exec_cop0_instrs; // FIXME

inline AsmjitCompiler& c = compiler;

inline void CheckCop0UsableJit()
{
    Label l_end = c.newLabel();
    c.cmp(ptr(can_exec_cop0_instrs), 1);
    c.je(l_end);
    reg_alloc.FlushAllVolatile();
    c.xor_(host_gpr_arg[0].r32(), host_gpr_arg[0].r32());
    BlockEpilogWithJmp(CoprocessorUnusableException);
    c.bind(l_end);
}

template<size_t size> inline void ReadCop0Jit(Gpq dst, u32 idx)
{
    auto Read = [dst](auto const& reg) {
        if constexpr (sizeof(reg) == 4) {
            if constexpr (size == 4) {
                c.movsxd(dst, dword_ptr(reg));
            } else {
                c.mov(dst.r32(), dword_ptr(reg));
            }
        } else if constexpr (sizeof(reg) == 8) {
            if constexpr (size == 4) {
                // c.movsxd(dst, dword_ptr(reg));
            } else {
                c.mov(dst, qword_ptr(reg));
            }
        } else {
            static_assert(always_false<sizeof(reg)>, "Register must be either 4 or 8 bytes.");
        }
    };

    switch (idx & 31) {
    case Cop0Reg::index: Read(cop0.index); break;
    case Cop0Reg::random: c.xor_(dst.r32(), dst.r32()); break; // TODO
    case Cop0Reg::entry_lo_0: Read(cop0.entry_lo[0]); break;
    case Cop0Reg::entry_lo_1: Read(cop0.entry_lo[1]); break;
    case Cop0Reg::context: Read(cop0.context); break;
    case Cop0Reg::page_mask: Read(cop0.page_mask); break;
    case Cop0Reg::wired: Read(cop0.wired); break;
    case Cop0Reg::bad_v_addr: Read(cop0.bad_v_addr); break;
    case Cop0Reg::count: Read(u32((cop0.count + block_cycles) >> 1)); break;
    case Cop0Reg::entry_hi: Read(cop0.entry_hi); break;
    case Cop0Reg::compare: /* See the declaration of 'compare' */
        c.mov(rax, ptr(cop0.compare));
        c.shr(rax, 1);
        if constexpr (size == 4) {
            c.movsxd(dst, eax);
        } else {
            c.mov(dst.r32(), eax);
        }
        break;
    case Cop0Reg::status: Read(cop0.status); break;
    case Cop0Reg::cause: Read(cop0.cause); break;
    case Cop0Reg::epc: Read(cop0.epc); break;
    case Cop0Reg::pr_id: Read(cop0.pr_id); break;
    case Cop0Reg::config: Read(cop0.config); break;
    case Cop0Reg::ll_addr: Read(cop0.ll_addr); break;
    case Cop0Reg::watch_lo: Read(cop0.watch_lo); break;
    case Cop0Reg::watch_hi: Read(cop0.watch_hi); break;
    case Cop0Reg::x_context: Read(cop0.x_context); break;
    case Cop0Reg::parity_error: Read(cop0.parity_error); break;
    case Cop0Reg::cache_error: Read(cop0.cache_error); break;
    case Cop0Reg::tag_lo: Read(cop0.tag_lo); break;
    case Cop0Reg::tag_hi: Read(cop0.tag_hi); break;
    case Cop0Reg::error_epc: Read(cop0.error_epc); break;
    case 7:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 31: Read(last_cop0_write); break;
    default: c.xor_(dst.r32(), dst.r32());
    }
}

inline void dmfc0(u32 rt, u32 rd)
{
    CheckCop0UsableJit();
    if (!CheckDwordOpCondJit()) return;
    if (!rt) return;
    Gpq ht = reg_alloc.GetHostGprMarkDirty(rt);
    ReadCop0Jit<8>(ht, rd);
}

inline void dmtc0(u32 rt, u32 rd)
{
    /*
    CheckCop0UsableJit();
    if (!CheckDwordOpCondJit()) return;
    Gpq ht = get_gpr(rt);

    auto Write = [ht](auto& reg) {
        if constexpr (sizeof(reg) == 4) {
            c.mov(ptr(reg), ht.r32());
        } else {
            c.mov(ptr(reg), ht.r64());
        }
    };

    auto WriteMasked = [ht](auto& reg, std::integral auto mask) {
        static_assert(sizeof(reg) == sizeof(mask));
        if constexpr (sizeof(reg) == 4) {
            c.mov(eax, ht.r32());
            c.and_(eax, mask);
            c.and_(ptr(reg), ~mask);
            c.or_(ptr(reg), eax);
        } else {
            c.mov(rax, ht.r64());
            c.mov(rbx, mask);
            c.and_(rax, rbx);
            c.not_(rbx);
            c.and_(ptr(reg), rbx);
            c.or_(ptr(reg), rax);
            c.xor_(ebx, ebx);
        }
    };

    switch (rd & 31) {
    case Cop0Reg::index: WriteMasked(cop0.index, 0x8000'003F); break;

    case Cop0Reg::random: WriteMasked(cop0.random, 0x20); break;

    case Cop0Reg::entry_lo_0: WriteMasked(cop0.entry_lo[0], 0x3FFF'FFFF); break;

    case Cop0Reg::entry_lo_1: WriteMasked(cop0.entry_lo[1], 0x3FFF'FFFF); break;

    case Cop0Reg::context: WriteMasked(cop0.context, 0xFFFF'FFFF'FF80'0000); break;

    case Cop0Reg::page_mask: WriteMasked(cop0.page_mask, 0x01FF'E000); break;

    case Cop0Reg::wired:
        WriteMasked(cop0.wired, 0x3F);
        OnWriteToWired();
        break;

    case Cop0Reg::bad_v_addr: break;

    case Cop0Reg::count:
        // c.lea(rax, ptr(ht, ht)); // [ht + ht]
        c.mov(ptr(cop0.count), rax);
        OnWriteToCount();
        break;

    case Cop0Reg::entry_hi: WriteMasked(cop0.entry_hi, 0xC000'00FF'FFFF'E0FF); break;

    case Cop0Reg::compare:
        // c.lea(rax, ptr(ht, ht)); // [ht + ht]
        c.mov(ptr(cop0.compare), rax);
        OnWriteToCompare();
        break;

    case Cop0Reg::status:
        WriteMasked(cop0.status, 0xFF57'FFFF);
        OnWriteToStatus();
        break;

    case Cop0Reg::cause:
        WriteMasked(cop0.cause, 0x300);
        OnWriteToCause();
        break;

    case Cop0Reg::epc: Write(cop0.epc); break;

    case Cop0Reg::config: WriteMasked(cop0.config, 0xF00'800F); break;

    case Cop0Reg::ll_addr: Write(cop0.ll_addr); break;

    case Cop0Reg::watch_lo: WriteMasked(cop0.watch_lo, 0xFFFF'FFFB); break;

    case Cop0Reg::watch_hi: Write(cop0.watch_hi); break;

    case Cop0Reg::x_context: WriteMasked(cop0.x_context, 0xFFFF'FFFE'0000'0000); break;

    case Cop0Reg::parity_error: WriteMasked(cop0.parity_error, 0xFF); break;

    case Cop0Reg::tag_lo: WriteMasked(cop0.tag_lo, 0x0FFF'FFC0); break;

    case Cop0Reg::error_epc: Write(cop0.error_epc); break;
    }

    c.mov(ptr(last_cop0_write), ht.r32());
    */
}

inline void eret()
{
    CheckCop0UsableJit();

    Label l0 = c.newLabel(), l1 = c.newLabel();
    c.cmp(ptr(cop0.status.erl), 1);
    c.je(l0);
    c.mov(rax, cop0.epc);
    c.mov(ptr(pc), rax);
    c.mov(ptr(cop0.status.exl), 0);
    c.jmp(l1);

    c.bind(l0);
    c.mov(rax, cop0.error_epc);
    c.mov(ptr(pc), rax);
    c.mov(ptr(cop0.status.erl), 0);

    c.bind(l1);
    c.mov(ptr(ll_bit), 0);
}

inline void mfc0(u32 rt, u32 rd)
{
    CheckCop0UsableJit();
    if (!rt) return;
    Gpq ht = reg_alloc.GetHostGprMarkDirty(rt);
    ReadCop0Jit<4>(ht, rd);
}

inline void mtc0(u32 rt, u32 rd)
{
    CheckCop0UsableJit();
    // Gpq ht = get_gpr(rt);
    switch (rd & 31) {
    }
}

inline void tlbr()
{
    CheckCop0UsableJit();
    JitCallInterpreterImpl(vr4300::tlbr);
}

inline void tlbwi()
{
    CheckCop0UsableJit();
    JitCallInterpreterImpl(vr4300::tlbwi);
}

inline void tlbwr()
{
    CheckCop0UsableJit();
    JitCallInterpreterImpl(vr4300::tlbwr);
}

inline void tlbp()
{
    CheckCop0UsableJit();
    JitCallInterpreterImpl(vr4300::tlbp);
}

} // namespace n64::vr4300::x64
