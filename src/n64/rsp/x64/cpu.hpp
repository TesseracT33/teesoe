#pragma once

#include "interface/mi.hpp"
#include "mips/recompiler.hpp"
#include "rdp/rdp.hpp"
#include "rsp/recompiler.hpp"
#include "rsp/register_allocator.hpp"
#include "rsp/rsp.hpp"

namespace n64::rsp::x64 {

using namespace asmjit;
using namespace asmjit::x86;

struct Recompiler : public mips::Recompiler<s32, s32, u32, RegisterAllocator> {
    using mips::Recompiler<s32, s32, u32, RegisterAllocator>::Recompiler;

    void add(u32 rs, u32 rt, u32 rd) const { addu(rs, rt, rd); }

    void addi(u32 rs, u32 rt, s16 imm) const { addiu(rs, rt, imm); }

    void break_() const
    {
        Label l_end = c.newLabel();
        c.or_(GlobalVarPtr(sp.status), 3); // set halted, broke
        c.bt(GlobalVarPtr(sp.status), 6); // test intbreak
        c.jnc(l_end);
        reg_alloc.Free(host_gpr_arg[0]);
        c.mov(host_gpr_arg[0].r32(), std::to_underlying(mi::InterruptType::SP));
        reg_alloc.Call(mi::RaiseInterrupt); // todo: do jmp with block epilogue
        c.bind(l_end);
        branched = true;
    }

    void j(u32 instr) const
    {
        take_branch(instr << 2 & 0xFFF);
        branch_hit = true;
    }

    void jal(u32 instr) const
    {
        take_branch(instr << 2 & 0xFFF);
        LinkJit(31);
        branch_hit = true;
    }

    void lb(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gpd ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        c.lea(eax, ptr(hs, imm)); // addr
        c.and_(eax, 0xFFF);
        c.movsx(ht, GlobalArrPtrWithRegOffset(dmem, rax, 1));
    }

    void lbu(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gpd ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        c.lea(eax, ptr(hs, imm)); // addr
        c.and_(eax, 0xFFF);
        c.movzx(ht, GlobalArrPtrWithRegOffset(dmem, rax, 1));
    }

    void lh(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gpd ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        c.mov(rcx, dmem);
        c.lea(eax, ptr(hs, imm)); // addr
        c.and_(eax, 0xFFF);
        c.mov(dh, byte_ptr(rcx, rax));
        c.inc(eax);
        c.and_(eax, 0xFFF);
        c.mov(dl, byte_ptr(rcx, rax));
        c.movsx(ht, dx);
    }

    void lhu(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gpd ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        c.mov(rcx, dmem);
        c.lea(eax, ptr(hs, imm)); // addr
        c.and_(eax, 0xFFF);
        c.mov(dh, byte_ptr(rcx, rax));
        c.inc(eax);
        c.and_(eax, 0xFFF);
        c.mov(dl, byte_ptr(rcx, rax));
        c.movzx(ht, dx);
    }

    void lw(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gpd ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        Label l_no_ov = c.newLabel(), l_end = c.newLabel();
        c.mov(rcx, dmem);
        c.lea(eax, ptr(hs, imm)); // addr
        c.and_(eax, 0xFFF);
        c.cmp(eax, 0xFFC);
        c.jbe(l_no_ov);

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
        c.movbe(ht, dword_ptr(rcx, rax));

        c.bind(l_end);
    }

    void lwu(u32 rs, u32 rt, s16 imm) const { lw(rs, rt, imm); }

    void mfc0(u32 rt, u32 rd) const
    {
        reg_alloc.Free(host_gpr_arg[0]);
        c.mov(host_gpr_arg[0].r32(), (rd & 7) << 2);
        reg_alloc.Call(rd & 8 ? rdp::ReadReg : rsp::ReadReg);
        if (rt) c.mov(GetDirtyGpr(rt), eax);
    }

    void mtc0(u32 rt, u32 rd) const
    { // TODO: possible to start DMA to IMEM and cause invalidation of the currently executed block
        reg_alloc.Free<2>({ host_gpr_arg[0], host_gpr_arg[1] });
        c.mov(host_gpr_arg[0].r32(), (rd & 7) << 2);
        c.mov(host_gpr_arg[1].r32(), GetGpr(rt));
        reg_alloc.Call(rd & 8 ? rdp::WriteReg : rsp::WriteReg);
        if ((rd & 7) == 4) { // SP_STATUS
            Label l_no_halt = c.newLabel(), l_no_sstep = c.newLabel();
            c.bt(GlobalVarPtr(sp.status), 0); // halted
            c.jnc(l_no_halt);
            BlockEpilogWithPcFlush(4);

            c.bind(l_no_halt);
            c.bt(GlobalVarPtr(sp.status), 5); // sstep
            c.jnc(l_no_sstep);
            BlockEpilogWithPcFlush(4);

            c.bind(l_no_sstep);
        }
    }

    void sb(u32 rs, u32 rt, s16 imm) const
    {
        Gpd hs = GetGpr(rs), ht = GetGpr(rt);
        c.lea(eax, ptr(hs, imm)); // addr
        c.and_(eax, 0xFFF);
        c.mov(GlobalArrPtrWithRegOffset(dmem, rax, 1), ht.r8());
    }

    void sh(u32 rs, u32 rt, s16 imm) const
    {
        Gpd hs = GetGpr(rs), ht = GetGpr(rt);
        c.mov(rcx, dmem);
        c.lea(eax, ptr(hs, imm)); // addr
        c.and_(eax, 0xFFF);
        c.mov(edx, ht);
        c.mov(byte_ptr(rcx, rax), dh);
        c.inc(eax);
        c.and_(eax, 0xFFF);
        c.mov(byte_ptr(rcx, rax), dl);
    }

    void sub(u32 rs, u32 rt, u32 rd) const { subu(rs, rt, rd); }

    void sw(u32 rs, u32 rt, s16 imm) const
    {
        Gpd hs = GetGpr(rs), ht = GetGpr(rt);
        Label l_no_ov = c.newLabel(), l_end = c.newLabel();
        c.mov(rcx, dmem);
        c.lea(eax, ptr(hs, imm)); // addr
        c.and_(eax, 0xFFF);
        c.cmp(eax, 0xFFC);
        c.jbe(l_no_ov);

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
        c.movbe(dword_ptr(rcx, rax), ht);

        c.bind(l_end);
    }

} inline constexpr cpu_recompiler{
    compiler,
    reg_alloc,
    lo_dummy,
    hi_dummy,
    jit_pc,
    branch_hit,
    branched,
    [](u32 target) { TakeBranchJit(target); },
    [](asmjit::x86::Gp target) { TakeBranchJit(target); },
    LinkJit,
};

} // namespace n64::rsp::x64
