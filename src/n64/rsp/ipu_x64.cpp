#include "interface/mi.hpp"
#include "mips/types.hpp"
#include "numtypes.hpp"
#include "rdp/rdp.hpp"
#include "rsp/recompiler.hpp"
#include "rsp/register_allocator.hpp"
#include "rsp/rsp.hpp"

#include <utility>

namespace n64::rsp::x64 {

using namespace asmjit;
using namespace asmjit::x86;

template<mips::Cond cc> static void branch(u32 rs, u32 rt, s16 imm);
template<mips::Cond cc> static void branch(u32 rs, s16 imm);

static Gpd GetGpr(u32 gpr)
{
    return reg_alloc.GetGpr(gpr).r32();
}

static Gpd GetDirtyGpr(u32 gpr)
{
    return reg_alloc.GetDirtyGpr(gpr).r32();
}

void add(u32 rs, u32 rt, u32 rd)
{
    addu(rs, rt, rd);
}

void addi(u32 rs, u32 rt, s16 imm)
{
    addiu(rs, rt, imm);
}

void addiu(u32 rs, u32 rt, s16 imm)
{
    if (!rt) return;
    Gpd ht = GetDirtyGpr(rt), hs = GetGpr(rs);
    if (hs == ht) {
        c.add(ht, imm);
    } else {
        c.lea(ht, ptr(hs.r64(), imm));
    }
}

void addu(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpd hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    if (hs == hd) {
        c.add(hd, ht);
    } else {
        c.lea(hd, ptr(hs.r64(), ht.r64()));
    }
}

void and_(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpd hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    if (rs != rd) c.mov(hd, hs);
    c.and_(hd, ht);
}

void andi(u32 rs, u32 rt, u16 imm)
{
    if (!rt) return;
    Gpd ht = GetDirtyGpr(rt), hs = GetGpr(rs);
    if (rs != rt) c.mov(ht, hs);
    c.and_(ht, imm);
}

void beq(u32 rs, u32 rt, s16 imm)
{
    branch<mips::Cond::Eq>(rs, rt, imm);
}

void bgez(u32 rs, s16 imm)
{
    branch<mips::Cond::Ge>(rs, imm);
}

void bgezal(u32 rs, s16 imm)
{
    branch<mips::Cond::Ge>(rs, imm);
    EmitLink(31); // TODO: in vr4300,  we link before branch.
}

void bgtz(u32 rs, s16 imm)
{
    branch<mips::Cond::Gt>(rs, imm);
}

void blez(u32 rs, s16 imm)
{
    branch<mips::Cond::Le>(rs, imm);
}

void bltz(u32 rs, s16 imm)
{
    branch<mips::Cond::Lt>(rs, imm);
}

void bltzal(u32 rs, s16 imm)
{
    branch<mips::Cond::Lt>(rs, imm);
    EmitLink(31);
}

void bne(u32 rs, u32 rt, s16 imm)
{
    branch<mips::Cond::Ne>(rs, rt, imm);
}

void break_()
{
    reg_alloc.DestroyVolatile(host_gpr_arg[0]);
    Label l_end = c.newLabel();
    c.or_(JitPtr(sp.status), 3); // set halted, broke
    c.bt(JitPtr(sp.status), 6); // test intbreak
    c.jnc(l_end);
    c.mov(host_gpr_arg[0].r32(), mi::InterruptType::SP);
    BlockEpilogWithPcFlushAndJmp((void*)mi::RaiseInterrupt);
    c.bind(l_end);
}

void j(u32 instr)
{
    EmitBranchTaken(instr << 2 & 0xFFF);
    last_instr_was_branch = true;
}

void jal(u32 instr)
{
    EmitBranchTaken(instr << 2 & 0xFFF);
    EmitLink(31);
    last_instr_was_branch = true;
}

void jalr(u32 rs, u32 rd)
{
    EmitBranchTaken(GetGpr(rs));
    EmitLink(rd);
    last_instr_was_branch = true;
}

void jr(u32 rs)
{
    EmitBranchTaken(GetGpr(rs));
    last_instr_was_branch = true;
}

void lb(u32 rs, u32 rt, s16 imm)
{
    if (!rt) return;
    Gpd ht = GetDirtyGpr(rt);
    if (rs) {
        Gpd hs = GetGpr(rs);
        c.lea(eax, ptr(hs.r64(), imm)); // addr
        c.and_(eax, 0xFFF);
        c.movsx(ht, JitPtrOffset(dmem, rax, 1));
    } else {
        c.movsx(ht, JitPtrOffset(dmem, imm & 0xFFF, 1));
    }
}

void lbu(u32 rs, u32 rt, s16 imm)
{
    if (!rt) return;
    Gpd ht = GetDirtyGpr(rt);
    if (rs) {
        Gpd hs = GetGpr(rs);
        c.lea(eax, ptr(hs.r64(), imm)); // addr
        c.and_(eax, 0xFFF);
        c.movzx(ht, JitPtrOffset(dmem, rax, 1));
    } else {
        c.movzx(ht, JitPtrOffset(dmem, imm & 0xFFF, 1));
    }
}

void lh(u32 rs, u32 rt, s16 imm)
{
    if (!rt) return;
    Gpd ht = GetDirtyGpr(rt);
    imm &= 0xFFF;
    if (rs || imm == 0xFFF) {
        Gpd hs = GetGpr(rs);
        c.mov(rcx, dmem);
        c.lea(eax, ptr(hs.r64(), imm)); // addr
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

void lhu(u32 rs, u32 rt, s16 imm)
{
    if (!rt) return;
    Gpd ht = GetDirtyGpr(rt);
    imm &= 0xFFF;
    if (rs || imm == 0xFFF) {
        Gpd hs = GetGpr(rs);
        c.mov(rcx, dmem);
        c.lea(eax, ptr(hs.r64(), imm)); // addr
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

void lui(u32 rt, s16 imm)
{
    if (!rt) return;
    Gpd ht = GetDirtyGpr(rt);
    c.mov(ht, imm << 16);
}

void lw(u32 rs, u32 rt, s16 imm)
{
    if (!rt) return;
    Gpd ht = GetDirtyGpr(rt);
    imm &= 0xFFF;
    if (rs || imm > 0xFFC) {
        Gpd hs = GetGpr(rs);
        Label l_no_ov = c.newLabel(), l_end = c.newLabel();
        c.lea(eax, ptr(hs.r64(), imm)); // addr
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

void lwu(u32 rs, u32 rt, s16 imm)
{
    lw(rs, rt, imm);
}

void mfc0(u32 rt, u32 rd)
{
    reg_alloc.DestroyVolatile(host_gpr_arg[0]);
    c.mov(host_gpr_arg[0].r32(), (rd & 7) << 2);
    reg_alloc.Call(rd & 8 ? (void*)rdp::ReadReg : (void*)rsp::ReadReg);
    if (rt) {
        Gpd ht = GetDirtyGpr(rt);
        c.mov(ht, eax);
    }
}

void mtc0(u32 rt, u32 rd)
{ // TODO: possible to start DMA to IMEM and cause invalidation of the currently executed block
    reg_alloc.ReserveArgs(2);
    c.mov(host_gpr_arg[0].r32(), (rd & 7) << 2);
    c.mov(host_gpr_arg[1].r32(), GetGpr(rt));
    reg_alloc.Call(rd & 8 ? (void*)rdp::WriteReg : (void*)rsp::WriteReg);
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

void nor(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpd hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    if (rs != rd) c.mov(hd, hs);
    c.or_(hd, ht);
    c.not_(hd);
}

void or_(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpd hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    if (rs != rd) c.mov(hd, hs);
    c.or_(hd, ht);
}

void ori(u32 rs, u32 rt, u16 imm)
{
    if (!rt) return;
    Gpd ht = GetDirtyGpr(rt), hs = GetGpr(rs);
    if (rs != rt) c.mov(ht, hs);
    if (imm) c.or_(ht, imm);
}

void sb(u32 rs, u32 rt, s16 imm)
{
    Gpd ht = GetGpr(rt);
    if (rs) {
        Gpd hs = GetGpr(rs);
        c.lea(eax, ptr(hs.r64(), imm)); // addr
        c.and_(eax, 0xFFF);
        c.mov(JitPtrOffset(dmem, rax, 1), ht.r8());
    } else {
        c.mov(JitPtrOffset(dmem, imm & 0xFFF, 1), ht.r8());
    }
}

void sh(u32 rs, u32 rt, s16 imm)
{
    Gpd ht = GetGpr(rt);
    imm &= 0xFFF;
    if (rs || imm == 0xFFF) {
        Gpd hs = GetGpr(rs);
        c.mov(rcx, dmem);
        c.lea(eax, ptr(hs.r64(), imm)); // addr
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

void sll(u32 rt, u32 rd, u32 sa)
{
    if (!rd) return;
    Gpd hd = GetDirtyGpr(rd), ht = GetGpr(rt);
    if (rt != rd) c.mov(hd, ht);
    c.shl(hd, sa);
}

void sllv(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpd hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    c.shlx(hd, ht, hs);
}

void slt(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpd hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    c.cmp(hs, ht);
    c.setl(al);
    c.movzx(hd, al);
}

void slti(u32 rs, u32 rt, s16 imm)
{
    if (!rt) return;
    Gpd ht = GetDirtyGpr(rt), hs = GetGpr(rs);
    c.cmp(hs, imm);
    c.setl(al);
    c.movzx(ht, al);
}

void sltiu(u32 rs, u32 rt, s16 imm)
{
    if (!rt) return;
    Gpd ht = GetDirtyGpr(rt), hs = GetGpr(rs);
    c.cmp(hs, imm);
    c.setb(al);
    c.movzx(ht, al);
}

void sltu(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpd hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    c.cmp(hs, ht);
    c.setb(al);
    c.movzx(hd, al);
}
void sra(u32 rt, u32 rd, u32 sa)
{
    if (!rd) return;
    Gpd hd = GetDirtyGpr(rd), ht = GetGpr(rt);
    if (rt != rd) c.mov(hd, ht);
    c.sar(hd, sa);
}

void srav(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpd hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    c.sarx(hd, ht, hs);
}

void srl(u32 rt, u32 rd, u32 sa)
{
    if (!rd) return;
    Gpd hd = GetDirtyGpr(rd), ht = GetGpr(rt);
    if (rt != rd) c.mov(hd, ht);
    c.shr(hd, sa);
}

void srlv(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpd hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    c.shrx(hd, ht, hs);
}

void sub(u32 rs, u32 rt, u32 rd)
{
    subu(rs, rt, rd);
}

void subu(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpd hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    if (rt == rd) {
        c.neg(hd);
        c.add(hd, hs);
    } else {
        if (rs != rd) c.mov(hd, hs);
        c.sub(hd, ht);
    }
}

void sw(u32 rs, u32 rt, s16 imm)
{
    Gpd ht = GetGpr(rt);
    imm &= 0xFFF;
    if (rs || imm > 0xFFC) {
        Gpd hs = GetGpr(rs);
        Label l_no_ov = c.newLabel(), l_end = c.newLabel();
        c.lea(eax, ptr(hs.r64(), imm)); // addr
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

void xor_(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpd hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    if (rs != rd) c.mov(hd, hs);
    c.xor_(hd, ht);
}

void xori(u32 rs, u32 rt, u16 imm)
{
    if (!rt) return;
    Gpd ht = GetDirtyGpr(rt), hs = GetGpr(rs);
    if (rs != rt) c.mov(ht, hs);
    c.xor_(ht, imm);
}

template<mips::Cond cc> void branch(u32 rs, u32 rt, s16 imm)
{
    Label l_nobranch = c.newLabel();
    if (!rs) {
        Gpd ht = GetGpr(rt);
        c.test(ht, ht);
    } else if (!rt) {
        Gpd hs = GetGpr(rs);
        c.test(hs, hs);
    } else {
        Gpd hs = GetGpr(rs), ht = GetGpr(rt);
        c.cmp(hs, ht);
    }
    if constexpr (cc == mips::Cond::Eq) c.jne(l_nobranch);
    if constexpr (cc == mips::Cond::Ne) c.je(l_nobranch);
    EmitBranchTaken(jit_pc + 4 + (imm << 2));
    c.bind(l_nobranch);
    last_instr_was_branch = true;
}

template<mips::Cond cc> void branch(u32 rs, s16 imm)
{
    Label l_nobranch = c.newLabel();
    Gpd hs = GetGpr(rs);
    c.test(hs, hs);
    if constexpr (cc == mips::Cond::Ge) c.js(l_nobranch);
    if constexpr (cc == mips::Cond::Gt) c.jle(l_nobranch);
    if constexpr (cc == mips::Cond::Le) c.jg(l_nobranch);
    if constexpr (cc == mips::Cond::Lt) c.jns(l_nobranch);
    EmitBranchTaken(jit_pc + 4 + (imm << 2));
    c.bind(l_nobranch);
    last_instr_was_branch = true;
}

} // namespace n64::rsp::x64
