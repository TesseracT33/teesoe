#pragma once

#include "interface/mi.hpp"
#include "mips/recompiler.hpp"
#include "rdp/rdp.hpp"
#include "rsp/recompiler.hpp"
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
        c.mov(rax, &sp.status);
        c.or_(dword_ptr(rax), 3); // set halted, broke
        c.bt(dword_ptr(rax), 6); // test intbreak
        c.jnb(l_end);
        reg_alloc.Free(host_gpr_arg[0]);
        c.mov(host_gpr_arg[0].r32(), std::to_underlying(mi::InterruptType::SP));
        reg_alloc.Call(mi::RaiseInterrupt); // todo: do jmp with block epilogue
        c.bind(l_end);
        branched = true;
    }

    void j(u32 instr) const
    {
        take_branch((instr << 2) & 0xFFF);
        branch_hit = true;
    }

    void jal(u32 instr) const
    {
        take_branch((instr << 2) & 0xFFF);
        LinkJit(31);
        branch_hit = true;
    }

    void lb(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gpd ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        c.lea(eax, ptr(hs, imm)); // addr
        c.and_(eax, 0xFFF);
        c.mov(rcx, dmem);
        c.movsx(ht, byte_ptr(rcx, rax));
    }

    void lbu(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gpd ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        c.lea(eax, ptr(hs, imm)); // addr
        c.and_(eax, 0xFFF);
        c.mov(rcx, dmem);
        c.movzx(ht, byte_ptr(rcx, rax));
    }

    void lh(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gpd ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        c.lea(eax, ptr(hs, imm)); // addr
        c.and_(eax, 0xFFF);
        c.mov(rcx, dmem);
        c.mov(ht.r8Hi(), byte_ptr(rcx, rax));
        c.inc(eax);
        c.and_(eax, 0xFFF);
        c.mov(ht.r8Lo(), byte_ptr(rcx, rax));
        c.movsx(ht, ht.r16());
    }

    void lhu(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gpd ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        c.lea(eax, ptr(hs, imm)); // addr
        c.and_(eax, 0xFFF);
        c.mov(rcx, dmem);
        c.mov(ht.r8Hi(), byte_ptr(rcx, rax));
        c.inc(eax);
        c.and_(eax, 0xFFF);
        c.mov(ht.r8Lo(), byte_ptr(rcx, rax));
        c.movzx(ht, ht.r16());
    }

    void lw(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gpd ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        c.lea(eax, ptr(hs, imm)); // addr
        c.and_(eax, 0xFFF);
        c.mov(rcx, dmem);
        c.mov(ht.r8Hi(), byte_ptr(rcx, rax));
        c.inc(eax);
        c.and_(eax, 0xFFF);
        c.mov(ht.r8Lo(), byte_ptr(rcx, rax));
        c.shl(edx, 16);
        c.inc(eax);
        c.and_(eax, 0xFFF);
        c.mov(ht.r8Hi(), byte_ptr(rcx, rax));
        c.inc(eax);
        c.and_(eax, 0xFFF);
        c.mov(ht.r8Lo(), byte_ptr(rcx, rax));
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
    {
        reg_alloc.Free(host_gpr_arg[0]);
        reg_alloc.Free(host_gpr_arg[1]);
        c.mov(host_gpr_arg[0].r32(), (rd & 7) << 2);
        c.mov(host_gpr_arg[1].r32(), GetGpr(rt));
        reg_alloc.Call(rd & 8 ? rdp::WriteReg : rsp::WriteReg);
    }

    void sb(u32 rs, u32 rt, s16 imm) const
    {
        Gpd hs = GetGpr(rs), ht = GetGpr(rt);
        c.lea(eax, ptr(hs, imm)); // addr
        c.and_(eax, 0xFFF);
        c.mov(rcx, dmem);
        c.mov(byte_ptr(rcx, rax), ht.r8Lo());
    }

    void sh(u32 rs, u32 rt, s16 imm) const
    {
        Gpd hs = GetGpr(rs), ht = GetGpr(rt);
        c.lea(eax, ptr(hs, imm)); // addr
        c.and_(eax, 0xFFF);
        c.mov(rcx, dmem);
        c.mov(byte_ptr(rcx, rax), ht.r8Hi());
        c.inc(eax);
        c.and_(eax, 0xFFF);
        c.mov(byte_ptr(rcx, rax), ht.r8Lo());
    }

    void sub(u32 rs, u32 rt, u32 rd) const { subu(rs, rt, rd); }

    void sw(u32 rs, u32 rt, s16 imm) const
    {
        Gpd hs = GetGpr(rs), ht = GetGpr(rt);
        c.lea(eax, ptr(hs, imm)); // addr
        c.and_(eax, 0xFFF);
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
