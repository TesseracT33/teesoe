#pragma once

#include "vr4300/cop0.hpp"
#include "vr4300/recompiler.hpp"

#include <concepts>

namespace n64::vr4300::x64 {

using namespace asmjit;
using namespace asmjit::x86;

inline AsmjitCompiler& c = compiler;

inline void OnCop0Unusable()
{
    reg_alloc.Free(host_gpr_arg[0]);
    c.xor_(host_gpr_arg[0].r32(), host_gpr_arg[0].r32());
    BlockEpilogWithJmp(CoprocessorUnusableException);
    branched = true;
}

template<size_t size> inline void ReadCop0(Gpq dst, u32 idx)
{
    auto Read = [dst](auto const& cop0_reg) {
        if constexpr (size == 4) {
            c.movsxd(dst, GlobalVarPtr(cop0_reg, 4));
        } else if constexpr (sizeof(cop0_reg) == 4) {
            c.mov(dst.r32(), GlobalVarPtr(cop0_reg));
        } else {
            c.mov(dst, GlobalVarPtr(cop0_reg));
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
        c.mov(rax, GlobalVarPtr(cop0.compare));
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

template<size_t size> inline void WriteCop0(Gpq src, u32 idx)
{
    auto Write = [src](auto& cop0_reg) {
        if constexpr (sizeof(cop0_reg) == 4) {
            c.mov(GlobalVarPtr(cop0_reg), src.r32());
        } else if constexpr (size == 4) {
            c.movsxd(rax, src.r32());
            c.mov(GlobalVarPtr(cop0_reg), rax);
        } else {
            c.mov(GlobalVarPtr(cop0_reg), src);
        }
    };

    auto WriteMasked = [src](auto& cop0_reg, std::integral auto mask) {
        static_assert(sizeof(cop0_reg) == sizeof(mask));
        if constexpr (sizeof(cop0_reg) == 4) {
            c.mov(eax, src.r32());
            c.and_(eax, mask);
            c.and_(GlobalVarPtr(cop0_reg), ~mask);
            c.or_(GlobalVarPtr(cop0_reg), eax);
        } else {
            reg_alloc.Free(rcx);
            size == 8 ? c.mov(rax, src) : c.movsxd(rax, src.r32());
            c.mov(rcx, mask);
            c.and_(rax, rcx);
            c.not_(rcx);
            c.and_(GlobalVarPtr(cop0_reg), rcx);
            c.or_(GlobalVarPtr(cop0_reg), rax);
        }
    };

    switch (idx & 31) {
    case Cop0Reg::index: WriteMasked(cop0.index, 0x8000'003F); break;
    case Cop0Reg::random: WriteMasked(cop0.random, 0x20); break;
    case Cop0Reg::entry_lo_0: WriteMasked(cop0.entry_lo[0], 0x3FFF'FFFF); break;
    case Cop0Reg::entry_lo_1: WriteMasked(cop0.entry_lo[1], 0x3FFF'FFFF); break;
    case Cop0Reg::context: WriteMasked(cop0.context, 0xFFFF'FFFF'FF80'0000); break;
    case Cop0Reg::page_mask: WriteMasked(cop0.page_mask, 0x01FF'E000); break;
    case Cop0Reg::wired:
        WriteMasked(cop0.wired, 0x3F);
        cop0.OnWriteToWired();
        break;
    case Cop0Reg::bad_v_addr: break;
    case Cop0Reg::count:
        c.lea(rax, ptr(src, src));
        c.mov(GlobalVarPtr(cop0.count), rax);
        cop0.OnWriteToCount();
        break;
    case Cop0Reg::entry_hi: WriteMasked(cop0.entry_hi, 0xC000'00FF'FFFF'E0FF); break;
    case Cop0Reg::compare:
        c.lea(rax, ptr(src, src));
        c.mov(GlobalVarPtr(cop0.compare), rax);
        cop0.OnWriteToCompare();
        break;
    case Cop0Reg::status:
        WriteMasked(cop0.status, 0xFF57'FFFF);
        branched = true;
        cop0.OnWriteToStatus();
        break;
    case Cop0Reg::cause:
        WriteMasked(cop0.cause, 0x300);
        cop0.OnWriteToCause();
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

    c.mov(GlobalVarPtr(last_cop0_write), src.r32());
}

inline void cache(u32 rs, u32 rt, s16 imm)
{
    reg_alloc.Free<3>({ host_gpr_arg[0], host_gpr_arg[1], host_gpr_arg[2] });
    c.mov(host_gpr_arg[0].r32(), rs);
    c.mov(host_gpr_arg[1].r32(), rt);
    c.mov(host_gpr_arg[2].r32(), imm);
    JitCallInterpreterImpl(vr4300::cache);
}

inline void dmfc0(u32 rt, u32 rd)
{
    if (can_exec_cop0_instrs) {
        if (!rd) return;
        Gpq hd = reg_alloc.GetHostGprMarkDirty(rd);
        ReadCop0<8>(hd, rt);
    } else {
        OnCop0Unusable();
    }
}

inline void dmtc0(u32 rt, u32 rd)
{
    if (can_exec_cop0_instrs) {
        Gpq ht = reg_alloc.GetHostGpr(rt);
        WriteCop0<8>(ht, rd);
    } else {
        OnCop0Unusable();
    }
}

inline void eret()
{
    if (can_exec_cop0_instrs) {
        Label l0 = c.newLabel(), l1 = c.newLabel();
        c.bt(GlobalVarPtr(cop0.status), 2); // erl
        c.jc(l0);
        c.mov(rax, GlobalVarPtr(cop0.epc));
        c.mov(GlobalVarPtr(pc), rax);
        c.btr(GlobalVarPtr(cop0.status), 1); // exl
        c.jmp(l1);

        c.bind(l0);
        c.mov(rax, GlobalVarPtr(cop0.error_epc));
        c.mov(GlobalVarPtr(pc), rax);
        c.btr(GlobalVarPtr(cop0.status), 2); // erl

        c.bind(l1);
        c.mov(GlobalVarPtr(ll_bit), 0);
        BlockEpilog();
        branched = true;
    } else {
        OnCop0Unusable();
    }
}

inline void mfc0(u32 rt, u32 rd)
{
    if (can_exec_cop0_instrs) {
        if (!rd) return;
        Gpq hd = reg_alloc.GetHostGprMarkDirty(rd);
        ReadCop0<4>(hd, rt);
    } else {
        OnCop0Unusable();
    }
}

inline void mtc0(u32 rt, u32 rd)
{
    if (can_exec_cop0_instrs) {
        Gpq ht = reg_alloc.GetHostGpr(rt);
        WriteCop0<4>(ht, rd);
    } else {
        OnCop0Unusable();
    }
}

inline void tlbr()
{
    if (can_exec_cop0_instrs) {
        JitCallInterpreterImpl(vr4300::tlbr);
    } else {
        OnCop0Unusable();
    }
}

inline void tlbwi()
{
    if (can_exec_cop0_instrs) {
        JitCallInterpreterImpl(vr4300::tlbwi);
    } else {
        OnCop0Unusable();
    }
}

inline void tlbwr()
{
    if (can_exec_cop0_instrs) {
        JitCallInterpreterImpl(vr4300::tlbwr);
    } else {
        OnCop0Unusable();
    }
}

inline void tlbp()
{
    if (can_exec_cop0_instrs) {
        JitCallInterpreterImpl(vr4300::tlbp);
    } else {
        OnCop0Unusable();
    }
}

} // namespace n64::vr4300::x64
