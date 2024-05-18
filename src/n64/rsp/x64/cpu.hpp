#pragma once

#include "interface/mi.hpp"
#include "mips/recompiler_x64.hpp"
#include "rdp/rdp.hpp"
#include "rsp/recompiler.hpp"
#include "rsp/register_allocator.hpp"
#include "rsp/rsp.hpp"

namespace n64::rsp::x64 {

using namespace asmjit;
using namespace asmjit::x86;

struct Recompiler : public mips::RecompilerX64<s32, u32, RegisterAllocator> {
    using mips::RecompilerX64<s32, u32, RegisterAllocator>::RecompilerX64;

    void add(u32 rs, u32 rt, u32 rd) const { addu(rs, rt, rd); }

    void addi(u32 rs, u32 rt, s16 imm) const { addiu(rs, rt, imm); }

    void break_() const
    {
        reg_alloc.FlushAll();
        Label l_end = c.newLabel();
        c.or_(JitPtr(sp.status), 3); // set halted, broke
        c.bt(JitPtr(sp.status), 6); // test intbreak
        c.jnc(l_end);
        c.mov(host_gpr_arg[0].r32(), std::to_underlying(mi::InterruptType::SP));
        BlockEpilogWithPcFlushAndJmp(mi::RaiseInterrupt);
        c.bind(l_end);
        branched = true;
    }

    void j(u32 instr) const
    {
        TakeBranchJit(instr << 2 & 0xFFF);
        branch_hit = true;
    }

    void jal(u32 instr) const
    {
        TakeBranchJit(instr << 2 & 0xFFF);
        LinkJit(31);
        branch_hit = true;
    }

    void lb(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gpd ht = GetDirtyGpr(rt);
        if (rs) {
            Gpd hs = GetGpr(rs);
            c.lea(eax, ptr(hs, imm)); // addr
            c.and_(eax, 0xFFF);
            c.movsx(ht, JitPtrOffset(dmem, rax, 1));
        } else {
            c.movsx(ht, JitPtrOffset(dmem, imm & 0xFFF, 1));
        }
    }

    void lbu(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gpd ht = GetDirtyGpr(rt);
        if (rs) {
            Gpd hs = GetGpr(rs);
            c.lea(eax, ptr(hs, imm)); // addr
            c.and_(eax, 0xFFF);
            c.movzx(ht, JitPtrOffset(dmem, rax, 1));
        } else {
            c.movzx(ht, JitPtrOffset(dmem, imm & 0xFFF, 1));
        }
    }

    void lh(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gpd ht = GetDirtyGpr(rt);
        imm &= 0xFFF;
        if (rs || imm == 0xFFF) {
            Gpd hs = GetGpr(rs);
            c.mov(rcx, dmem);
            c.lea(eax, ptr(hs, imm)); // addr
            c.and_(eax, 0xFFF);
            c.mov(dh, byte_ptr(rcx, rax));
            c.inc(eax);
            c.and_(eax, 0xFFF);
            c.mov(dl, byte_ptr(rcx, rax));
            c.movsx(ht, dx);
        } else {
            c.movbe(ax, JitPtrOffset(dmem, imm, 2));
            c.movsx(ht, ax);
        }
    }

    void lhu(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gpd ht = GetDirtyGpr(rt);
        imm &= 0xFFF;
        if (rs || imm == 0xFFF) {
            Gpd hs = GetGpr(rs);
            c.mov(rcx, dmem);
            c.lea(eax, ptr(hs, imm)); // addr
            c.and_(eax, 0xFFF);
            c.mov(dh, byte_ptr(rcx, rax));
            c.inc(eax);
            c.and_(eax, 0xFFF);
            c.mov(dl, byte_ptr(rcx, rax));
            c.movzx(ht, dx);
        } else {
            c.movbe(ax, JitPtrOffset(dmem, imm, 2));
            c.movzx(ht, ax);
        }
    }

    void lw(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gpd ht = GetDirtyGpr(rt);
        imm &= 0xFFF;
        if (rs || imm > 0xFFC) {
            Gpd hs = GetGpr(rs);
            Label l_no_ov = c.newLabel(), l_end = c.newLabel();
            c.lea(eax, ptr(hs, imm)); // addr
            c.and_(eax, 0xFFF);
            c.cmp(eax, 0xFFC);
            c.jbe(l_no_ov);

            c.mov(rcx, dmem);
            c.mov(dh, byte_ptr(rcx, rax));
            c.inc(eax);
            c.and_(eax, 0xFFF);
            c.mov(dl, byte_ptr(rcx, rax));
            c.shl(edx, 16);
            c.inc(eax);
            c.and_(eax, 0xFFF);
            c.mov(dh, byte_ptr(rcx, rax));
            c.inc(eax);
            c.and_(eax, 0xFFF);
            c.mov(dl, byte_ptr(rcx, rax));
            c.mov(ht, edx);
            c.jmp(l_end);

            c.bind(l_no_ov);
            c.movbe(ht, JitPtrOffset(dmem, rax, 4));

            c.bind(l_end);
        } else {
            c.movbe(ht, JitPtrOffset(dmem, imm, 4));
        }
    }

    void lwu(u32 rs, u32 rt, s16 imm) const { lw(rs, rt, imm); }

    void mfc0(u32 rt, u32 rd) const
    {
        reg_alloc.ReserveArgs(1);
        c.mov(host_gpr_arg[0].r32(), (rd & 7) << 2);
        reg_alloc.Call(rd & 8 ? rdp::ReadReg : rsp::ReadReg);
        reg_alloc.FreeArgs(1);
        if (rt) c.mov(GetDirtyGpr(rt), eax);
    }

    void mtc0(u32 rt, u32 rd) const
    { // TODO: possible to start DMA to IMEM and cause invalidation of the currently executed block
        reg_alloc.ReserveArgs(2);
        c.mov(host_gpr_arg[0].r32(), (rd & 7) << 2);
        c.mov(host_gpr_arg[1].r32(), GetGpr(rt));
        reg_alloc.Call(rd & 8 ? rdp::WriteReg : rsp::WriteReg);
        reg_alloc.FreeArgs(2);
        if ((rd & 7) == 4) { // SP_STATUS
            Label l_no_halt = c.newLabel(), l_no_sstep = c.newLabel();
            c.bt(JitPtr(sp.status), 0); // halted
            c.jnc(l_no_halt);
            BlockEpilogWithPcFlush(4);

            c.bind(l_no_halt);
            c.bt(JitPtr(sp.status), 5); // sstep
            c.jnc(l_no_sstep);
            BlockEpilogWithPcFlush(4);

            c.bind(l_no_sstep);
        }
    }

    void sb(u32 rs, u32 rt, s16 imm) const
    {
        Gpd ht = GetGpr(rt);
        if (rs) {
            Gpd hs = GetGpr(rs);
            c.lea(eax, ptr(hs, imm)); // addr
            c.and_(eax, 0xFFF);
            c.mov(JitPtrOffset(dmem, rax, 1), ht.r8());
        } else {
            c.mov(JitPtrOffset(dmem, imm & 0xFFF, 1), ht.r8());
        }
    }

    void sh(u32 rs, u32 rt, s16 imm) const
    {
        Gpd ht = GetGpr(rt);
        imm &= 0xFFF;
        if (rs || imm == 0xFFF) {
            Gpd hs = GetGpr(rs);
            c.mov(rcx, dmem);
            c.lea(eax, ptr(hs, imm)); // addr
            c.and_(eax, 0xFFF);
            c.mov(edx, ht);
            c.mov(byte_ptr(rcx, rax), dh);
            c.inc(eax);
            c.and_(eax, 0xFFF);
            c.mov(byte_ptr(rcx, rax), dl);
        } else {
            c.movbe(JitPtrOffset(dmem, imm, 2), ht.r16());
        }
    }

    void sub(u32 rs, u32 rt, u32 rd) const { subu(rs, rt, rd); }

    void sw(u32 rs, u32 rt, s16 imm) const
    {
        Gpd ht = GetGpr(rt);
        imm &= 0xFFF;
        if (rs || imm > 0xFFC) {
            Gpd hs = GetGpr(rs);
            Label l_no_ov = c.newLabel(), l_end = c.newLabel();
            c.lea(eax, ptr(hs, imm)); // addr
            c.and_(eax, 0xFFF);
            c.cmp(eax, 0xFFC);
            c.jbe(l_no_ov);

            c.mov(rcx, dmem);
            c.mov(edx, ht);
            c.bswap(edx);
            c.mov(byte_ptr(rcx, rax), dl);
            c.inc(eax);
            c.and_(eax, 0xFFF);
            c.mov(byte_ptr(rcx, rax), dh);
            c.shr(edx, 16);
            c.inc(eax);
            c.and_(eax, 0xFFF);
            c.mov(byte_ptr(rcx, rax), dl);
            c.inc(eax);
            c.and_(eax, 0xFFF);
            c.mov(byte_ptr(rcx, rax), dh);
            c.jmp(l_end);

            c.bind(l_no_ov);
            c.movbe(JitPtrOffset(dmem, rax, 4), ht);

            c.bind(l_end);
        } else {
            c.movbe(JitPtrOffset(dmem, imm, 4), ht);
        }
    }

} inline constexpr cpu_recompiler{
    compiler,
    reg_alloc,
    jit_pc,
    branch_hit,
    branched,
    [] { return JitPtr(lo_dummy); },
    [] { return JitPtr(hi_dummy); },
    [](u32 target) { TakeBranchJit(target); },
    [](HostGpr32 target) { TakeBranchJit(target); },
    LinkJit,
};

} // namespace n64::rsp::x64
