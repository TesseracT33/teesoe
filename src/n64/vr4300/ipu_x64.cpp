#include "mips/types.hpp"
#include "numtypes.hpp"
#include "vr4300/cop0.hpp"
#include "vr4300/exceptions.hpp"
#include "vr4300/recompiler.hpp"
#include "vr4300/vr4300.hpp"

#include <concepts>
#include <utility>

namespace n64::vr4300::x64 {

using namespace asmjit;
using namespace asmjit::x86;

template<mips::Cond cc, bool likely> static void branch(u32 rs, u32 rt, s16 imm);
template<mips::Cond cc, bool likely> static void branch(u32 rs, s16 imm);
template<std::integral Int, bool linked> static void load(u32 rs, u32 rt, s16 imm);
template<std::integral Int> static void store(u32 rs, u32 rt, s16 imm);
template<std::integral Int> static void store_conditional(u32 rs, u32 rt, s16 imm);
template<mips::Cond cc> static void trap(u32 rs, s16 imm);
template<mips::Cond cc> static void trap(u32 rs, u32 rt);

static Gpq GetGpr(u32 gpr)
{
    return reg_alloc.GetGpr(gpr).r64();
}

static Gpq GetDirtyGpr(u32 gpr)
{
    return reg_alloc.GetDirtyGpr(gpr).r64();
}

void add(u32 rs, u32 rt, u32 rd)
{
    Label l_noexception = c.newLabel();
    Gpq hs = GetGpr(rs), ht = GetGpr(rt);
    c.mov(eax, hs.r32());
    c.add(eax, ht.r32());
    c.jno(l_noexception);
    BlockEpilogWithPcFlushAndJmp((void*)IntegerOverflowException);
    c.bind(l_noexception);
    if (rd) {
        Gpq hd = GetDirtyGpr(rd);
        c.movsxd(hd, eax);
    }
}

void addi(u32 rs, u32 rt, s16 imm)
{
    Label l_noexception = c.newLabel();
    Gpq hs = GetGpr(rs);
    c.mov(eax, hs.r32());
    c.add(eax, imm);
    c.jno(l_noexception);
    BlockEpilogWithPcFlushAndJmp((void*)IntegerOverflowException);
    c.bind(l_noexception);
    if (rt) {
        Gpq ht = GetDirtyGpr(rt);
        c.movsxd(ht, eax);
    }
}

void addiu(u32 rs, u32 rt, s16 imm)
{
    if (!rt) return;
    Gpq ht = GetDirtyGpr(rt), hs = GetGpr(rs);
    if (rs == rt) {
        c.add(ht.r32(), imm);
    } else {
        c.lea(ht.r32(), ptr(hs, imm));
    }
    c.movsxd(ht, ht.r32());
}

void addu(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    if (rs == rd) {
        c.add(hd.r32(), ht.r32());
    } else {
        c.lea(hd.r32(), ptr(hs, ht));
    }
    c.movsxd(hd, hd.r32());
}

void and_(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    if (rs != rd) c.mov(hd, hs);
    c.and_(hd, ht);
}

void andi(u32 rs, u32 rt, u16 imm)
{
    if (!rt) return;
    Gpq ht = GetDirtyGpr(rt), hs = GetGpr(rs);
    if (rs != rt) c.mov(ht.r32(), hs.r32());
    c.and_(ht.r32(), imm);
}

void beq(u32 rs, u32 rt, s16 imm)
{
    branch<mips::Cond::Eq, false>(rs, rt, imm);
}

void beql(u32 rs, u32 rt, s16 imm)
{
    branch<mips::Cond::Eq, true>(rs, rt, imm);
}

void bgez(u32 rs, s16 imm)
{
    branch<mips::Cond::Ge, false>(rs, imm);
}

void bgezal(u32 rs, s16 imm)
{
    Label l_branch = c.newLabel(), l_link = c.newLabel(), l_no_delay_slot = c.newLabel(), l_end = c.newLabel();
    Gpq hs = GetGpr(rs), h31 = GetDirtyGpr(31);
    c.mov(al, JitPtr(branch_state));
    c.push(rax);
    c.test(hs, hs);
    c.jns(l_branch);
    EmitBranchNotTaken();
    c.jmp(l_link);

    c.bind(l_branch);
    EmitBranchTaken(jit_pc + 4 + (imm << 2));

    c.bind(l_link);
    // TODO: not correct
    c.pop(rax);
    c.cmp(al, std::to_underlying(BranchState::DelaySlotTaken) | std::to_underlying(BranchState::DelaySlotNotTaken));
    c.je(l_no_delay_slot);
    c.mov(h31.r32(), 4);
    c.add(h31, JitPtr(jump_addr));
    c.jmp(l_end);

    c.bind(l_no_delay_slot);
    c.mov(h31, jit_pc + 8);

    c.bind(l_end);

    last_instr_was_branch = true;
}

void bgezall(u32 rs, s16 imm)
{
    EmitLink(31);
    branch<mips::Cond::Ge, true>(rs, imm);
}

void bgezl(u32 rs, s16 imm)
{
    branch<mips::Cond::Ge, true>(rs, imm);
}

void bgtz(u32 rs, s16 imm)
{
    branch<mips::Cond::Gt, false>(rs, imm);
}

void bgtzl(u32 rs, s16 imm)
{
    branch<mips::Cond::Gt, true>(rs, imm);
}

void blez(u32 rs, s16 imm)
{
    branch<mips::Cond::Le, false>(rs, imm);
}

void blezl(u32 rs, s16 imm)
{
    branch<mips::Cond::Le, true>(rs, imm);
}

void bltz(u32 rs, s16 imm)
{
    branch<mips::Cond::Lt, false>(rs, imm);
}

void bltzal(u32 rs, s16 imm)
{
    EmitLink(31);
    branch<mips::Cond::Lt, false>(rs, imm);
}

void bltzall(u32 rs, s16 imm)
{
    EmitLink(31);
    branch<mips::Cond::Lt, true>(rs, imm);
}

void bltzl(u32 rs, s16 imm)
{
    branch<mips::Cond::Lt, true>(rs, imm);
}

void bne(u32 rs, u32 rt, s16 imm)
{
    branch<mips::Cond::Ne, false>(rs, rt, imm);
}

void bnel(u32 rs, u32 rt, s16 imm)
{
    branch<mips::Cond::Ne, true>(rs, rt, imm);
}

void break_()
{
    BlockEpilogWithPcFlushAndJmp((void*)BreakpointException);
    branched = true;
}

void dadd(u32 rs, u32 rt, u32 rd)
{
    if (!CheckDwordOpCondJit()) return;
    Label l_noexception = c.newLabel();
    Gpq hs = GetGpr(rs), ht = GetGpr(rt);
    c.mov(rax, hs);
    c.add(rax, ht);
    c.jno(l_noexception);
    BlockEpilogWithPcFlushAndJmp((void*)IntegerOverflowException);
    c.bind(l_noexception);
    if (rd) {
        Gpq hd = GetDirtyGpr(rd);
        c.mov(hd, rax);
    }
}

void daddi(u32 rs, u32 rt, s16 imm)
{
    if (!CheckDwordOpCondJit()) return;
    Label l_noexception = c.newLabel();
    Gpq hs = GetGpr(rs);
    c.mov(rax, hs);
    c.add(rax, imm);
    c.jno(l_noexception);
    BlockEpilogWithPcFlushAndJmp((void*)IntegerOverflowException);
    c.bind(l_noexception);
    if (rt) {
        Gpq ht = GetDirtyGpr(rt);
        c.mov(ht, rax);
    }
}

void daddiu(u32 rs, u32 rt, s16 imm)
{
    if (!CheckDwordOpCondJit()) return;
    if (!rt) return;
    Gpq ht = GetDirtyGpr(rt), hs = GetGpr(rs);
    if (rs == rt) {
        c.add(ht, imm);
    } else {
        c.lea(ht, ptr(hs, imm));
    }
}

void daddu(u32 rs, u32 rt, u32 rd)
{
    if (!CheckDwordOpCondJit()) return;
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    if (rs == rd) {
        c.add(hd, ht);
    } else {
        c.lea(hd, ptr(hs, ht));
    }
}

void ddiv(u32 rs, u32 rt)
{
    if (!CheckDwordOpCondJit()) return;

    reg_alloc.Reserve(rdx);
    Label l_div = c.newLabel(), l_divzero = c.newLabel(), l_end = c.newLabel();
    Gpq hs = GetGpr(rs), ht = GetGpr(rt);
    c.test(ht, ht);
    c.jz(l_divzero);
    c.mov(rax, hs);
    c.mov(rdx, ht);
    c.btc(rax, 63);
    c.not_(rdx);
    c.or_(rax, rdx);
    c.jne(l_div);
    c.mov(rax, hs);
    c.xor_(edx, edx);
    c.jmp(l_end);

    c.bind(l_divzero);
    c.mov(rax, hs);
    c.mov(rdx, rax);
    c.sar(rax, 63);
    c.and_(eax, 2);
    c.dec(rax);
    c.jmp(l_end);

    c.bind(l_div);
    c.mov(rax, hs);
    c.cqo(rdx, rax);
    c.idiv(rdx, rax, ht);

    c.bind(l_end);
    c.mov(JitPtr(lo), rax);
    c.mov(JitPtr(hi), rdx);

    block_cycles += 68;
    reg_alloc.Free(rdx);
}

void ddivu(u32 rs, u32 rt)
{
    if (!CheckDwordOpCondJit()) return;

    Label l_div = c.newLabel(), l_end = c.newLabel();
    Gpq hs = GetGpr(rs), ht = GetGpr(rt);
    c.push(rdx);
    c.test(ht, ht);
    c.jnz(l_div);
    c.xor_(eax, eax);
    c.dec(rax);
    c.mov(rdx, hs);
    c.jmp(l_end);

    c.bind(l_div);
    c.mov(rax, hs);
    c.xor_(edx, edx);
    if (ht == rdx) {
        c.div(rdx, rax, qword_ptr(x86::rsp));
    } else {
        c.div(rdx, rax, ht);
    }

    c.bind(l_end);
    c.mov(JitPtr(lo), rax);
    c.mov(JitPtr(hi), rdx);
    c.pop(rdx);

    block_cycles += 68;
}

void div(u32 rs, u32 rt)
{
    Label l_div = c.newLabel(), l_end = c.newLabel();
    Gpq hs = GetGpr(rs), ht = GetGpr(rt);
    c.push(rdx);
    c.test(ht, ht);
    c.jnz(l_div);
    c.movsxd(rdx, hs.r32());
    c.mov(eax, edx);
    c.sar(eax, 31);
    c.and_(eax, 2);
    c.dec(rax);
    c.jmp(l_end);

    c.bind(l_div);
    c.movsxd(rax, hs.r32());
    c.cqo(rdx, rax);
    if (ht == rdx) {
        c.idiv(rdx, rax, qword_ptr(x86::rsp));
    } else {
        c.idiv(rdx, rax, ht);
    }
    c.cdqe(rax);
    c.movsxd(rdx, edx);

    c.bind(l_end);
    c.mov(JitPtr(lo), rax);
    c.mov(JitPtr(hi), rdx);
    c.pop(rdx);

    block_cycles += 36;
}

void divu(u32 rs, u32 rt)
{
    Label l_div = c.newLabel(), l_end = c.newLabel();
    Gpq hs = GetGpr(rs), ht = GetGpr(rt);
    c.push(rdx);
    c.test(ht.r32(), ht.r32());
    c.jnz(l_div);
    c.xor_(eax, eax);
    c.dec(rax);
    c.movsxd(rdx, hs.r32());
    c.jmp(l_end);

    c.bind(l_div);
    c.mov(eax, hs.r32());
    c.xor_(edx, edx);
    if (ht == rdx) {
        c.div(edx, eax, dword_ptr(x86::rsp));
    } else {
        c.div(edx, eax, ht.r32());
    }
    c.cdqe(rax);
    c.movsxd(rdx, edx);

    c.bind(l_end);
    c.mov(JitPtr(lo), rax);
    c.mov(JitPtr(hi), rdx);
    c.pop(rdx);

    block_cycles += 36;
}

void dmult(u32 rs, u32 rt)
{
    if (!CheckDwordOpCondJit()) return;
    Gpq hs = GetGpr(rs), ht = GetGpr(rt);
    c.push(rdx);
    c.mov(rax, hs);
    c.xor_(edx, edx);
    if (ht == rdx) {
        c.imul(rdx, rax, qword_ptr(x86::rsp));
    } else {
        c.imul(rdx, rax, ht);
    }
    c.mov(JitPtr(lo), rax);
    c.mov(JitPtr(hi), rdx);
    c.pop(rdx);
    block_cycles += 7;
}

void dmultu(u32 rs, u32 rt)
{
    Gpq hs = GetGpr(rs), ht = GetGpr(rt);
    c.push(rdx);
    c.mov(rax, hs);
    c.xor_(edx, edx);
    if (ht == rdx) {
        c.mul(rdx, rax, qword_ptr(x86::rsp));
    } else {
        c.mul(rdx, rax, ht);
    }
    c.mov(JitPtr(lo), rax);
    c.mov(JitPtr(hi), rdx);
    c.pop(rdx);
    block_cycles += 7;
}

void dsll(u32 rt, u32 rd, u32 sa)
{
    if (!CheckDwordOpCondJit()) return;
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), ht = GetGpr(rt);
    if (rt != rd) c.mov(hd, ht);
    c.shl(hd, sa);
}

void dsll32(u32 rt, u32 rd, u32 sa)
{
    if (!CheckDwordOpCondJit()) return;
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), ht = GetGpr(rt);
    if (rt != rd) c.mov(hd.r32(), ht.r32());
    c.shl(hd, sa + 32);
}

void dsllv(u32 rs, u32 rt, u32 rd)
{
    if (!CheckDwordOpCondJit()) return;
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    c.shlx(hd, ht, hs);
}

void dsra(u32 rt, u32 rd, u32 sa)
{
    if (!CheckDwordOpCondJit()) return;
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), ht = GetGpr(rt);
    if (rt != rd) c.mov(hd, ht);
    c.sar(hd, sa);
}

void dsra32(u32 rt, u32 rd, u32 sa)
{
    if (!CheckDwordOpCondJit()) return;
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), ht = GetGpr(rt);
    if (rt != rd) c.mov(hd, ht);
    c.sar(hd, sa + 32);
}

void dsrav(u32 rs, u32 rt, u32 rd)
{
    if (!CheckDwordOpCondJit()) return;
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    c.sarx(hd, ht, hs);
}

void dsrl(u32 rt, u32 rd, u32 sa)
{
    if (!CheckDwordOpCondJit()) return;
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), ht = GetGpr(rt);
    if (rt != rd) c.mov(hd, ht);
    c.shr(hd, sa);
}

void dsrl32(u32 rt, u32 rd, u32 sa)
{
    if (!CheckDwordOpCondJit()) return;
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), ht = GetGpr(rt);
    if (rt != rd) c.mov(hd, ht);
    c.shr(hd, sa + 32);
}

void dsrlv(u32 rs, u32 rt, u32 rd)
{
    if (!CheckDwordOpCondJit()) return;
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    c.shrx(hd, ht, hs);
}

void dsub(u32 rs, u32 rt, u32 rd)
{
    if (!CheckDwordOpCondJit()) return;
    Label l_noexception = c.newLabel();
    Gpq hs = GetGpr(rs), ht = GetGpr(rt);
    c.mov(rax, hs);
    c.sub(rax, ht);
    c.jno(l_noexception);
    BlockEpilogWithPcFlushAndJmp((void*)IntegerOverflowException);
    c.bind(l_noexception);
    if (rd) {
        Gpq hd = GetDirtyGpr(rd);
        c.mov(hd, rax);
    }
}

void dsubu(u32 rs, u32 rt, u32 rd)
{
    if (!CheckDwordOpCondJit()) return;
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    if (rt == rd) {
        c.neg(hd);
        c.add(hd, hs);
    } else {
        if (rs != rd) c.mov(hd, hs);
        c.sub(hd, ht);
    }
}

void j(u32 instr)
{
    Label l_end = c.newLabel();
    c.cmp(JitPtr(branch_state), BranchState::DelaySlotTaken);
    c.je(l_end);
    EmitBranchTaken((jit_pc + 4) & 0xFFFF'FFFF'F000'0000 | instr << 2 & 0xFFF'FFFF);

    c.bind(l_end);
    last_instr_was_branch = true;
}

void jal(u32 instr)
{
    Label l_jump = c.newLabel(), l_end = c.newLabel();
    Gpq h31 = GetDirtyGpr(31);
    c.cmp(JitPtr(branch_state), BranchState::DelaySlotTaken);
    c.jne(l_jump);
    c.mov(h31, JitPtr(jump_addr));
    c.add(h31, 4);
    c.jmp(l_end);

    c.bind(l_jump);
    c.mov(h31, jit_pc + 8);
    EmitBranchTaken((jit_pc + 4) & 0xFFFF'FFFF'F000'0000 | instr << 2 & 0xFFF'FFFF);

    c.bind(l_end);
    last_instr_was_branch = true;
}

void jalr(u32 rs, u32 rd)
{
    Label l_jump = c.newLabel(), l_end = c.newLabel();
    Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs);
    c.cmp(JitPtr(branch_state), BranchState::DelaySlotTaken);
    c.jne(l_jump);
    c.mov(hd, JitPtr(jump_addr));
    c.add(hd, 4);
    c.jmp(l_end);

    c.bind(l_jump);
    if (rs == rd) {
        c.mov(rax, hs);
        c.mov(hd, jit_pc + 8);
        EmitBranchTaken(rax);
    } else {
        c.mov(hd, jit_pc + 8);
        EmitBranchTaken(hs);
    }

    c.bind(l_end);
    last_instr_was_branch = true;
}

void jr(u32 rs)
{
    Label l_end = c.newLabel();
    Gpq hs = GetGpr(rs);
    c.cmp(JitPtr(branch_state), BranchState::DelaySlotTaken);
    c.je(l_end);
    EmitBranchTaken(hs);

    c.bind(l_end);
    last_instr_was_branch = true;
}

void lb(u32 rs, u32 rt, s16 imm)
{
    load<s8, false>(rs, rt, imm);
}

void lbu(u32 rs, u32 rt, s16 imm)
{
    load<u8, false>(rs, rt, imm);
}

void ld(u32 rs, u32 rt, s16 imm)
{
    if (CheckDwordOpCondJit()) {
        load<s64, false>(rs, rt, imm);
    }
}

void ldl(u32 rs, u32 rt, s16 imm)
{
    if (!CheckDwordOpCondJit()) return;

    Label l_noexception = c.newLabel();
    FlushPc();
    reg_alloc.ReserveArgs(1);
    Gpq hs = GetGpr(rs);
    c.lea(host_gpr_arg[0], ptr(hs, imm));
    c.push(host_gpr_arg[0]);
    reg_alloc.CallWithStackAlignment((void*)ReadVirtual<s64, Alignment::UnalignedLeft>);
    reg_alloc.FreeArgs(1);
    c.pop(rcx);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_noexception);

    BlockEpilog();

    c.bind(l_noexception);
    if (rt) {
        reg_alloc.Reserve(rcx, rdx);
        Gpq ht = GetDirtyGpr(rt);
        c.shl(ecx, 3);
        c.mov(edx, 1);
        c.shl(rax, cl);
        c.shl(rdx, cl);
        c.dec(rdx);
        c.and_(ht, rdx); // todo: does not work if in the future we make the optimization where we don't always load the
                         // register on GetDirtyGpr
        c.or_(ht, rax);
        reg_alloc.Free(rcx, rdx);
    }
}

void ldr(u32 rs, u32 rt, s16 imm)
{
    if (!CheckDwordOpCondJit()) return;

    Label l_noexception = c.newLabel();
    FlushPc();
    reg_alloc.ReserveArgs(1);
    Gpq hs = GetGpr(rs);
    c.lea(host_gpr_arg[0], ptr(hs, imm));
    c.push(host_gpr_arg[0]);
    reg_alloc.CallWithStackAlignment((void*)ReadVirtual<s64, Alignment::UnalignedRight>);
    reg_alloc.FreeArgs(1);
    c.pop(rcx);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_noexception);

    BlockEpilog();

    c.bind(l_noexception);
    if (rt) {
        reg_alloc.Reserve(rcx, rdx);
        Gpq ht = GetDirtyGpr(rt);
        c.shl(rcx, 3);
        c.mov(rdx, 0xFFFF'FF00);
        c.shl(rdx, cl);
        c.xor_(ecx, 56);
        c.shr(rax, cl);
        c.and_(ht, rdx); // todo: does not work if in the future we make the optimization where we don't always load the
                         // register on GetDirtyGpr
        c.or_(ht, rax);
        reg_alloc.Free(rcx, rdx);
    }
}

void lh(u32 rs, u32 rt, s16 imm)
{
    load<s16, false>(rs, rt, imm);
}

void lhu(u32 rs, u32 rt, s16 imm)
{
    load<u16, false>(rs, rt, imm);
}

void ll(u32 rs, u32 rt, s16 imm)
{
    load<s32, true>(rs, rt, imm);
}

void lld(u32 rs, u32 rt, s16 imm)
{
    if (CheckDwordOpCondJit()) {
        load<s64, true>(rs, rt, imm);
    }
}

void lui(u32 rt, s16 imm)
{
    if (!rt) return;
    Gpq ht = GetDirtyGpr(rt);
    c.mov(ht, imm << 16);
}

void lw(u32 rs, u32 rt, s16 imm)
{
    load<s32, false>(rs, rt, imm);
}

void lwl(u32 rs, u32 rt, s16 imm)
{
    Label l_noexception = c.newLabel();

    FlushPc();
    reg_alloc.ReserveArgs(1);
    Gpq hs = GetGpr(rs);
    c.lea(host_gpr_arg[0], ptr(hs, imm));
    c.push(host_gpr_arg[0]);
    reg_alloc.CallWithStackAlignment((void*)ReadVirtual<s32, Alignment::UnalignedLeft>);
    reg_alloc.FreeArgs(1);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_noexception);

    BlockEpilog();

    c.bind(l_noexception);
    if (rt) {
        reg_alloc.Reserve(rcx, rdx);
        Gpq ht = GetDirtyGpr(rt);
        c.shl(ecx, 3);
        c.mov(edx, 1);
        c.shl(eax, cl);
        c.shl(edx, cl);
        c.dec(edx);
        c.and_(ht.r32(), edx); // todo: does not work if in the future we make the optimization where we don't always
                               // load the register on GetDirtyGpr
        c.or_(ht.r32(), eax);
        c.movsxd(ht, ht.r32());
        reg_alloc.Free(rcx, rdx);
    }
}

void lwr(u32 rs, u32 rt, s16 imm)
{
    Label l_noexception = c.newLabel();

    FlushPc();
    reg_alloc.ReserveArgs(1);
    Gpq hs = GetGpr(rs);
    c.lea(host_gpr_arg[0], ptr(hs, imm));
    c.push(host_gpr_arg[0]);
    reg_alloc.CallWithStackAlignment((void*)ReadVirtual<s32, Alignment::UnalignedRight>);
    reg_alloc.FreeArgs(1);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_noexception);

    BlockEpilog();

    c.bind(l_noexception);
    if (rt) {
        reg_alloc.Reserve(rcx, rdx);
        Gpq ht = GetDirtyGpr(rt);
        c.shl(ecx, 3);
        c.mov(edx, 0xFFFF'FF00);
        c.shl(edx, cl);
        c.xor_(ecx, 24);
        c.shr(eax, cl);
        c.and_(ht.r32(), edx); // todo: does not work if in the future we make the optimization where we don't always
                               // load the register on GetDirtyGpr
        c.or_(ht.r32(), eax);
        c.movsxd(ht, ht.r32());
        reg_alloc.Free(rcx, rdx);
    }
}

void lwu(u32 rs, u32 rt, s16 imm)
{
    load<u32, false>(rs, rt, imm);
}

void mfhi(u32 rd)
{
    if (rd) {
        Gpq hd = GetDirtyGpr(rd);
        c.mov(hd, JitPtr(hi));
    }
}

void mflo(u32 rd)
{
    if (rd) {
        Gpq hd = GetDirtyGpr(rd);
        c.mov(hd, JitPtr(lo));
    }
}

void mthi(u32 rs)
{
    Gpq hs = GetGpr(rs);
    c.mov(JitPtr(hi), hs);
}

void mtlo(u32 rs)
{
    Gpq hs = GetGpr(rs);
    c.mov(JitPtr(lo), hs);
}

void mult(u32 rs, u32 rt)
{
    Gpq hs = GetGpr(rs), ht = GetGpr(rt);
    c.push(hs);
    c.mov(rax, ht);
    c.shl(rax, 29);
    c.sar(rax, 29);
    c.imul(hs, rax);
    c.movsxd(rax, hs.r32());
    c.sar(hs, 32);
    c.mov(JitPtr(lo), rax);
    c.mov(JitPtr(hi), hs);
    c.pop(hs);
    block_cycles += 4;
}

void multu(u32 rs, u32 rt)
{
    Gpq hs = GetGpr(rs), ht = GetGpr(rt);
    c.push(hs);
    c.mov(eax, ht.r32());
    c.mov(hs.r32(), hs.r32());
    c.imul(hs, rax);
    c.movsxd(rax, hs.r32());
    c.sar(hs, 32);
    c.mov(JitPtr(lo), rax);
    c.mov(JitPtr(hi), hs);
    c.pop(hs);
    block_cycles += 4;
}

void nor(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    if (rs != rd) c.mov(hd, hs);
    c.or_(hd, ht);
    c.not_(hd);
}

void or_(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    if (rs != rd) c.mov(hd, hs);
    c.or_(hd, ht);
}

void ori(u32 rs, u32 rt, u16 imm)
{
    if (!rt) return;
    Gpq ht = GetDirtyGpr(rt), hs = GetGpr(rs);
    if (rs != rt) c.mov(ht, hs);
    c.or_(ht, imm);
}

void sb(u32 rs, u32 rt, s16 imm)
{
    store<s8>(rs, rt, imm);
}

void sc(u32 rs, u32 rt, s16 imm)
{
    store_conditional<s32>(rs, rt, imm);
}

void scd(u32 rs, u32 rt, s16 imm)
{
    if (CheckDwordOpCondJit()) {
        store_conditional<s64>(rs, rt, imm);
    }
}

void sd(u32 rs, u32 rt, s16 imm)
{
    if (CheckDwordOpCondJit()) {
        store<s64>(rs, rt, imm);
    }
}

void sdl(u32 rs, u32 rt, s16 imm)
{
    if (!CheckDwordOpCondJit()) return;
    Label l_end = c.newLabel();
    FlushPc();
    reg_alloc.ReserveArgs(2);
    Gpq hs = GetGpr(rs), ht = GetGpr(rt);
    c.lea(host_gpr_arg[0], ptr(hs, imm));
    c.lea(eax, ptr(0, host_gpr_arg[0], 3u));
    c.sarx(host_gpr_arg[1], ht, rax);
    reg_alloc.Call((void*)WriteVirtual<8, Alignment::UnalignedLeft>);
    reg_alloc.FreeArgs(2);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_end);

    BlockEpilog();

    c.bind(l_end);
}

void sdr(u32 rs, u32 rt, s16 imm)
{
    if (!CheckDwordOpCondJit()) return;
    Label l_end = c.newLabel();
    FlushPc();
    reg_alloc.ReserveArgs(2);
    Gpq hs = GetGpr(rs), ht = GetGpr(rt);
    c.lea(host_gpr_arg[0], ptr(hs, imm));
    c.lea(eax, ptr(0, host_gpr_arg[0], 3u));
    c.xor_(al, 56);
    c.shlx(host_gpr_arg[1], ht, rax);
    reg_alloc.Call((void*)WriteVirtual<8, Alignment::UnalignedRight>);
    reg_alloc.FreeArgs(2);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_end);

    BlockEpilog();

    c.bind(l_end);
}

void sh(u32 rs, u32 rt, s16 imm)
{
    store<s16>(rs, rt, imm);
}

void sll(u32 rt, u32 rd, u32 sa)
{
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), ht = GetGpr(rt);
    if (rt == rd) {
        c.shl(hd.r32(), sa);
        c.movsxd(hd, hd.r32());
    } else {
        c.mov(eax, ht.r32());
        c.shl(eax, sa);
        c.movsxd(hd, eax);
    }
}

void sllv(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    c.shlx(hd.r32(), ht.r32(), hs.r32());
    c.movsxd(hd, hd.r32());
}

void slt(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    c.cmp(hs, ht);
    c.setl(al);
    c.movzx(hd.r32(), al);
}

void slti(u32 rs, u32 rt, s16 imm)
{
    if (!rt) return;
    Gpq ht = GetDirtyGpr(rt), hs = GetGpr(rs);
    c.cmp(hs, imm);
    c.setl(al);
    c.movzx(ht.r32(), al);
}

void sltiu(u32 rs, u32 rt, s16 imm)
{
    if (!rt) return;
    Gpq ht = GetDirtyGpr(rt), hs = GetGpr(rs);
    c.cmp(hs, imm);
    c.setb(al);
    c.movzx(ht.r32(), al);
}

void sltu(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    c.cmp(hs, ht);
    c.setb(al);
    c.movzx(hd.r32(), al);
}

void sra(u32 rt, u32 rd, u32 sa)
{
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), ht = GetGpr(rt);
    if (rt != rd) c.mov(hd, ht);
    c.sar(hd, sa);
    c.movsxd(hd, hd.r32());
}

void srav(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    c.mov(eax, hs.r32());
    c.and_(al, 31);
    c.sarx(hd, ht, rax);
    c.movsxd(hd, hd.r32());
}

void srl(u32 rt, u32 rd, u32 sa)
{
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), ht = GetGpr(rt);
    if (sa) {
        if (rt != rd) c.mov(hd.r32(), ht.r32());
        c.shr(hd.r32(), sa);
    } else {
        c.movsxd(hd, ht.r32());
    }
}

void srlv(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    c.shrx(hd.r32(), ht.r32(), hs.r32());
    c.movsxd(hd, hd.r32());
}

void sub(u32 rs, u32 rt, u32 rd)
{
    Label l_noexception = c.newLabel();
    Gpq hs = GetGpr(rs), ht = GetGpr(rt);
    c.mov(eax, hs.r32());
    c.sub(eax, ht.r32());
    c.jno(l_noexception);
    BlockEpilogWithPcFlushAndJmp((void*)IntegerOverflowException);
    c.bind(l_noexception);
    if (rd) {
        Gpq hd = GetDirtyGpr(rd);
        c.movsxd(hd, eax);
    }
}

void subu(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    if (rt == rd) {
        c.neg(hd.r32());
        c.add(hd.r32(), hs.r32());
    } else {
        if (rs != rd) c.mov(hd.r32(), hs.r32());
        c.sub(hd.r32(), ht.r32());
    }
    c.movsxd(hd, hd.r32());
}

void syscall()
{
    BlockEpilogWithPcFlushAndJmp((void*)SyscallException);
    branched = true;
}

void sync()
{
    /* Completes the Load/store instruction currently in the pipeline before the new
       load/store instruction is executed. Is executed as a NOP on the VR4300. */
}

void sw(u32 rs, u32 rt, s16 imm)
{
    store<s32>(rs, rt, imm);
}

void swl(u32 rs, u32 rt, s16 imm)
{
    Label l_end = c.newLabel();
    FlushPc();
    reg_alloc.ReserveArgs(2);
    Gpq hs = GetGpr(rs), ht = GetGpr(rt);
    c.lea(host_gpr_arg[0], ptr(hs, imm));
    c.lea(eax, ptr(0, host_gpr_arg[0], 3u));
    c.and_(al, 24);
    c.shrx(host_gpr_arg[1], ht, rax);
    reg_alloc.Call((void*)WriteVirtual<4, Alignment::UnalignedLeft>);
    reg_alloc.FreeArgs(2);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_end);

    BlockEpilog();

    c.bind(l_end);
}

void swr(u32 rs, u32 rt, s16 imm)
{
    Label l_end = c.newLabel();
    FlushPc();
    reg_alloc.ReserveArgs(2);
    Gpq hs = GetGpr(rs), ht = GetGpr(rt);
    c.lea(host_gpr_arg[0], ptr(hs, imm));
    c.lea(eax, ptr(0, host_gpr_arg[0], 3u));
    c.xor_(al, 24);
    c.shlx(host_gpr_arg[1], ht, rax);
    reg_alloc.Call((void*)WriteVirtual<4, Alignment::UnalignedRight>);
    reg_alloc.FreeArgs(2);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_end);

    BlockEpilog();

    c.bind(l_end);
}

void teq(u32 rs, u32 rt)
{
    trap<mips::Cond::Eq>(rs, rt);
}

void teqi(u32 rs, s16 imm)
{
    trap<mips::Cond::Eq>(rs, imm);
}

void tge(u32 rs, u32 rt)
{
    trap<mips::Cond::Ge>(rs, rt);
}

void tgei(u32 rs, s16 imm)
{
    trap<mips::Cond::Ge>(rs, imm);
}

void tgeu(u32 rs, u32 rt)
{
    trap<mips::Cond::Geu>(rs, rt);
}

void tgeiu(u32 rs, s16 imm)
{
    trap<mips::Cond::Geu>(rs, imm);
}

void tlt(u32 rs, u32 rt)
{
    trap<mips::Cond::Lt>(rs, rt);
}

void tlti(u32 rs, s16 imm)
{
    trap<mips::Cond::Lt>(rs, imm);
}

void tltu(u32 rs, u32 rt)
{
    trap<mips::Cond::Ltu>(rs, rt);
}

void tltiu(u32 rs, s16 imm)
{
    trap<mips::Cond::Ltu>(rs, imm);
}

void tne(u32 rs, u32 rt)
{
    trap<mips::Cond::Ne>(rs, rt);
}

void tnei(u32 rs, s16 imm)
{
    trap<mips::Cond::Ne>(rs, imm);
}

void xor_(u32 rs, u32 rt, u32 rd)
{
    if (!rd) return;
    Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
    if (rs != rd) c.mov(hd, hs);
    c.xor_(hd, ht);
}

void xori(u32 rs, u32 rt, u16 imm)
{
    if (!rt) return;
    Gpq ht = GetDirtyGpr(rt), hs = GetGpr(rs);
    if (rs != rt) c.mov(ht, hs);
    c.xor_(ht, imm);
}

template<mips::Cond cc, bool likely> void branch(u32 rs, u32 rt, s16 imm)
{
    Label l_branch = c.newLabel(), l_end = c.newLabel();
    if (!rs) {
        Gpq ht = GetGpr(rt);
        c.test(ht, ht);
    } else if (!rt) {
        Gpq hs = GetGpr(rs);
        c.test(hs, hs);
    } else {
        Gpq hs = GetGpr(rs), ht = GetGpr(rt);
        c.cmp(hs, ht);
    }
    if constexpr (cc == mips::Cond::Eq) c.je(l_branch);
    if constexpr (cc == mips::Cond::Ne) c.jne(l_branch);
    likely ? EmitBranchDiscarded() : EmitBranchNotTaken();
    c.jmp(l_end);
    c.bind(l_branch);
    EmitBranchTaken(jit_pc + 4 + (imm << 2));
    c.bind(l_end);
    last_instr_was_branch = true;
}

template<mips::Cond cc, bool likely> void branch(u32 rs, s16 imm)
{
    Label l_branch = c.newLabel(), l_end = c.newLabel();
    Gpq hs = GetGpr(rs);
    c.test(hs, hs);
    if constexpr (cc == mips::Cond::Ge) c.jns(l_branch);
    if constexpr (cc == mips::Cond::Gt) c.jg(l_branch);
    if constexpr (cc == mips::Cond::Le) c.jle(l_branch);
    if constexpr (cc == mips::Cond::Lt) c.js(l_branch);
    likely ? EmitBranchDiscarded() : EmitBranchNotTaken();
    c.jmp(l_end);
    c.bind(l_branch);
    EmitBranchTaken(jit_pc + 4 + (imm << 2));
    c.bind(l_end);
    last_instr_was_branch = true;
}

template<std::integral Int, bool linked> void load(u32 rs, u32 rt, s16 imm)
{
    Label l_noexception = c.newLabel();

    FlushPc();
    reg_alloc.ReserveArgs(1);
    Gpq hs = GetGpr(rs);
    c.lea(host_gpr_arg[0], ptr(hs, imm));
    reg_alloc.Call((void*)ReadVirtual<std::make_signed_t<Int>>);
    reg_alloc.FreeArgs(1);

    if constexpr (linked) {
        c.mov(ecx, JitPtr(last_paddr_on_load));
        c.mov(JitPtr(ll_bit), 1);
        c.shr(ecx, 4);
        c.mov(JitPtr(cop0.ll_addr), ecx);
    }

    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_noexception);

    BlockEpilog();

    c.bind(l_noexception);
    if (rt) {
        Gpq ht = GetDirtyGpr(rt);
        if constexpr (std::same_as<Int, s8>) c.movsx(ht, al);
        if constexpr (std::same_as<Int, u8>) c.movzx(ht.r32(), al);
        if constexpr (std::same_as<Int, s16>) c.movsx(ht, ax);
        if constexpr (std::same_as<Int, u16>) c.movzx(ht.r32(), ax);
        if constexpr (std::same_as<Int, s32>) c.movsxd(ht, eax);
        if constexpr (std::same_as<Int, u32>) c.mov(ht.r32(), eax);
        if constexpr (sizeof(Int) == 8) c.mov(ht, rax);
    }
}

template<std::integral Int> void store(u32 rs, u32 rt, s16 imm)
{
    Label l_end = c.newLabel();
    FlushPc();
    reg_alloc.ReserveArgs(2);
    Gpq hs = reg_alloc.GetGpr(rs), ht = reg_alloc.GetGpr(rt);
    c.lea(host_gpr_arg[0], ptr(hs, imm));
    c.mov(host_gpr_arg[1], ht);
    reg_alloc.Call((void*)WriteVirtual<sizeof(Int)>);
    reg_alloc.FreeArgs(2);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_end);

    BlockEpilog();

    c.bind(l_end);
}

template<std::integral Int> void store_conditional(u32 rs, u32 rt, s16 imm)
{
    Label l_store = c.newLabel(), l_end = c.newLabel();
    c.cmp(JitPtr(ll_bit), 1);
    c.je(l_store);

    if (rt) {
        Gpq ht = GetDirtyGpr(rt);
        c.xor_(ht.r32(), ht.r32());
    }
    c.jmp(l_end);

    c.bind(l_store);
    FlushPc();
    reg_alloc.ReserveArgs(2);
    Gpq hs = GetGpr(rs), ht = GetGpr(rt);
    c.lea(host_gpr_arg[0], ptr(hs, imm));
    c.mov(host_gpr_arg[1], ht);
    reg_alloc.Call((void*)WriteVirtual<sizeof(Int)>);
    reg_alloc.FreeArgs(2);
    if (rt) {
        Gpq ht = GetDirtyGpr(rt);
        c.mov(ht.r32(), 1);
    }
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_end);

    BlockEpilog();

    c.bind(l_end);
}

template<mips::Cond cc> void trap(u32 rs, s16 imm)
{
    Label l_notrap = c.newLabel();
    Gpq hs = GetGpr(rs);
    c.cmp(hs, imm);
    if constexpr (cc == mips::Cond::Eq) c.jne(l_notrap);
    if constexpr (cc == mips::Cond::Ge) c.jl(l_notrap);
    if constexpr (cc == mips::Cond::Geu) c.jb(l_notrap);
    if constexpr (cc == mips::Cond::Lt) c.jge(l_notrap);
    if constexpr (cc == mips::Cond::Ltu) c.jae(l_notrap);
    if constexpr (cc == mips::Cond::Ne) c.je(l_notrap);
    BlockEpilogWithPcFlushAndJmp((void*)TrapException);
    c.bind(l_notrap);
    branched = true;
}

template<mips::Cond cc> void trap(u32 rs, u32 rt)
{
    Label l_notrap = c.newLabel();
    Gpq hs = GetGpr(rs), ht = GetGpr(rt);
    c.cmp(hs, ht);
    if constexpr (cc == mips::Cond::Eq) c.jne(l_notrap);
    if constexpr (cc == mips::Cond::Ge) c.jl(l_notrap);
    if constexpr (cc == mips::Cond::Geu) c.jb(l_notrap);
    if constexpr (cc == mips::Cond::Lt) c.jge(l_notrap);
    if constexpr (cc == mips::Cond::Ltu) c.jae(l_notrap);
    if constexpr (cc == mips::Cond::Ne) c.je(l_notrap);
    BlockEpilogWithPcFlushAndJmp((void*)TrapException);
    c.bind(l_notrap);
    branched = true;
}

} // namespace n64::vr4300::x64
