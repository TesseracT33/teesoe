#pragma once

#include "mips/recompiler_x64.hpp"
#include "vr4300/recompiler.hpp"

namespace n64::vr4300::x64 {

using namespace asmjit;
using namespace asmjit::x86;

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
    reg_alloc.Reserve(rcx);
    Gpq hs = GetGpr(rs), h31 = GetDirtyGpr(31);
    c.mov(cl, JitPtr(in_branch_delay_slot_taken));
    c.or_(cl, JitPtr(in_branch_delay_slot_not_taken));
    c.test(hs, hs);
    c.jns(l_branch);
    OnBranchNotTakenJit();
    c.jmp(l_link);

    c.bind(l_branch);
    c.mov(rax, jit_pc + 4 + (imm << 2));
    TakeBranchJit(rax);

    c.bind(l_link);
    c.test(cl, cl);
    c.je(l_no_delay_slot);
    c.mov(h31.r32(), 4);
    c.add(h31, JitPtr(jump_addr));
    c.jmp(l_end);

    c.bind(l_no_delay_slot);
    c.mov(h31, jit_pc + 8);

    c.bind(l_end);

    branch_hit = true;
    reg_alloc.Free(rcx);
}

void bgezall(u32 rs, s16 imm)
{
    branch_and_link<mips::Cond::Ge, true>(rs, imm);
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
    branch_and_link<mips::Cond::Lt, false>(rs, imm);
}

void bltzall(u32 rs, s16 imm)
{
    branch_and_link<mips::Cond::Lt, true>(rs, imm);
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
    BlockEpilogWithPcFlushAndJmp(BreakpointException);
    branched = true;
}

void ddiv(u32 rs, u32 rt)
{
    if (!CheckDwordOpCondJit()) return;

    Label l_div = c.newLabel(), l_divzero = c.newLabel(), l_end = c.newLabel();
    reg_alloc.Reserve(rdx);
    Gpq hs = GetGpr(rs), ht = GetGpr(rt);
    c.test(ht, ht);
    c.je(l_divzero);
    c.mov(rax, hs);
    c.mov(rdx, ht);
    c.btc(rax, 63);
    c.not_(rdx);
    c.or_(rax, rdx);
    c.jne(l_div);
    c.mov(JitPtr(lo), hs);
    c.mov(JitPtr(hi), 0);
    c.jmp(l_end);

    c.bind(l_divzero);
    c.mov(rax, hs);
    c.mov(JitPtr(hi), rax);
    c.sar(rax, 63);
    c.and_(eax, 2);
    c.dec(rax);
    c.mov(JitPtr(lo), rax);
    c.jmp(l_end);

    c.bind(l_div);
    c.mov(rax, hs);
    c.cqo(rdx, rax);
    c.idiv(rdx, rax, ht);
    c.mov(JitPtr(lo), rax);
    c.mov(JitPtr(hi), rdx);

    c.bind(l_end);
    block_cycles += 68;
    reg_alloc.Free(rdx);
}

void ddivu(u32 rs, u32 rt)
{
    if (!CheckDwordOpCondJit()) return;

    Label l_div = c.newLabel(), l_end = c.newLabel();
    reg_alloc.Reserve(rdx);
    Gpq hs = GetGpr(rs), ht = GetGpr(rt);
    c.test(ht, ht);
    c.jne(l_div);
    c.mov(JitPtr(lo), -1);
    c.mov(JitPtr(hi), hs);
    c.jmp(l_end);

    c.bind(l_div);
    c.mov(rax, hs);
    c.xor_(edx, edx);
    c.div(rdx, rax, ht);
    c.mov(JitPtr(lo), rax);
    c.mov(JitPtr(hi), rdx);

    c.bind(l_end);
    block_cycles += 68;
    reg_alloc.Free(rdx);
}

void div(u32 rs, u32 rt)
{
    Label l_div = c.newLabel(), l_divzero = c.newLabel(), l_end = c.newLabel();
    reg_alloc.Reserve(rdx);
    Gpd hs = GetGpr32(rs), ht = GetGpr32(rt);
    c.test(ht, ht);
    c.je(l_divzero);
    c.mov(eax, hs);
    c.mov(edx, ht);
    c.btc(eax, 31);
    c.not_(edx);
    c.or_(eax, edx);
    c.jne(l_div);
    c.movsxd(rax, hs);
    c.mov(JitPtr(lo), rax);
    c.mov(JitPtr(hi), 0);
    c.jmp(l_end);

    c.bind(l_divzero);
    c.movsxd(rax, hs);
    c.mov(JitPtr(hi), rax);
    c.sar(eax, 31);
    c.and_(eax, 2);
    c.dec(rax);
    c.mov(JitPtr(lo), rax);
    c.jmp(l_end);

    c.bind(l_div);
    c.mov(eax, hs);
    c.cdq(edx, eax);
    c.idiv(edx, eax, ht);
    c.cdqe(rax);
    c.mov(JitPtr(lo), rax);
    c.movsxd(rax, edx);
    c.mov(JitPtr(hi), rax);

    c.bind(l_end);
    block_cycles += 36;
    reg_alloc.Free(rdx);
}

void divu(u32 rs, u32 rt)
{
    Label l_div = c.newLabel(), l_end = c.newLabel();
    reg_alloc.Reserve(rdx);
    Gpd hs = GetGpr32(rs), ht = GetGpr32(rt);
    c.test(ht, ht);
    c.jne(l_div);
    c.mov(JitPtr(lo), -1);
    c.movsxd(rax, hs);
    c.mov(JitPtr(hi), rax);
    c.jmp(l_end);

    c.bind(l_div);
    c.mov(eax, hs);
    c.xor_(edx, edx);
    c.div(edx, eax, ht);
    c.cdqe(rax);
    c.mov(JitPtr(lo), rax);
    c.movsxd(rax, edx);
    c.mov(JitPtr(hi), rax);

    c.bind(l_end);
    block_cycles += 36;
    reg_alloc.Free(rdx);
}

void dmult(u32 rs, u32 rt)
{
    if (CheckDwordOpCondJit()) {
        multiply64<false>(rs, rt);
    }
}

void dmultu(u32 rs, u32 rt)
{
    if (CheckDwordOpCondJit()) {
        multiply64<true>(rs, rt);
    }
}

void j(u32 instr)
{
    Label l_end = c.newLabel();
    c.cmp(JitPtr(in_branch_delay_slot_taken), 1);
    c.je(l_end);
    TakeBranchJit((jit_pc + 4) & 0xFFFF'FFFF'F000'0000 | instr << 2 & 0xFFF'FFFF);

    c.bind(l_end);
    branch_hit = true;
}

void jal(u32 instr)
{
    Label l_jump = c.newLabel(), l_end = c.newLabel();
    Gpq h31 = GetDirtyGpr(31);
    c.cmp(JitPtr(in_branch_delay_slot_taken), 0);
    c.je(l_jump);
    c.mov(h31.r32(), 4);
    c.add(h31, JitPtr(jump_addr));
    c.jmp(l_end);

    c.bind(l_jump);
    c.mov(h31, jit_pc + 8);
    TakeBranchJit((jit_pc + 4) & 0xFFFF'FFFF'F000'0000 | instr << 2 & 0xFFF'FFFF);

    c.bind(l_end);
    branch_hit = true;
}

void jalr(u32 rs, u32 rd)
{
    Label l_jump = c.newLabel(), l_end = c.newLabel();
    Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs);
    c.cmp(JitPtr(in_branch_delay_slot_taken), 0);
    c.je(l_jump);
    c.mov(hd.r32(), 4);
    c.add(hd, JitPtr(jump_addr));
    c.jmp(l_end);

    c.bind(l_jump);
    if (rs == rd) {
        c.mov(rax, hs);
        c.mov(hd, jit_pc + 8);
        TakeBranchJit(rax);
    } else {
        c.mov(hd, jit_pc + 8);
        TakeBranchJit(hs);
    }

    c.bind(l_end);
    branch_hit = true;
}

void jr(u32 rs)
{
    Label l_end = c.newLabel();
    Gpq hs = GetGpr(rs);
    c.cmp(JitPtr(in_branch_delay_slot_taken), 1);
    c.je(l_end);
    TakeBranchJit(hs);

    c.bind(l_end);
    branch_hit = true;
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
    reg_alloc.CallWithStackAlignment(ReadVirtual<s64, Alignment::UnalignedLeft>);
    reg_alloc.FreeArgs(1);
    c.pop(rcx);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_noexception);

    BlockEpilog();

    c.bind(l_noexception);
    if (rt) {
        Gpq ht = GetDirtyGpr(rt); // TODO: assumption below that ecx and edx are unused
        c.shl(rcx, 3);
        c.shl(rax, cl);
        c.mov(edx, 1);
        c.shl(rdx, cl);
        c.dec(rdx);
        c.and_(ht, rdx);
        c.or_(ht, rax);
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
    reg_alloc.CallWithStackAlignment(ReadVirtual<s64, Alignment::UnalignedRight>);
    reg_alloc.FreeArgs(1);
    c.pop(rcx);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_noexception);

    BlockEpilog();

    c.bind(l_noexception);
    if (rt) {
        Gpq ht = GetDirtyGpr(rt); // TODO: assumption below that ecx and edx are unused
        c.shl(rcx, 3);
        c.mov(rdx, 0xFFFF'FF00);
        c.shl(rdx, cl);
        c.xor_(ecx, 56);
        c.shr(rax, cl);
        c.and_(ht, rdx);
        c.or_(ht, rax);
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
    reg_alloc.CallWithStackAlignment(ReadVirtual<s32, Alignment::UnalignedLeft>);
    reg_alloc.FreeArgs(1);
    c.pop(rcx);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_noexception);

    BlockEpilog();

    c.bind(l_noexception);
    if (rt) {
        Gpq ht = GetDirtyGpr(rt); // TODO: assumption below that ecx and edx are unused
        c.shl(ecx, 3);
        c.shl(eax, cl);
        c.mov(edx, 1);
        c.shl(edx, cl);
        c.dec(edx);
        c.and_(ht.r32(), edx);
        c.or_(ht.r32(), eax);
        c.movsxd(ht.r64(), ht.r32());
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
    reg_alloc.CallWithStackAlignment(ReadVirtual<s32, Alignment::UnalignedRight>);
    reg_alloc.FreeArgs(1);
    c.pop(rcx);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_noexception);

    BlockEpilog();

    c.bind(l_noexception);
    if (rt) {
        Gpq ht = GetDirtyGpr(rt); // TODO: assumption below that ecx and edx are unused
        c.shl(ecx, 3);
        c.mov(edx, 0xFFFF'FF00);
        c.shl(edx, cl);
        c.xor_(ecx, 24);
        c.shr(eax, cl);
        c.and_(ht.r32(), edx);
        c.or_(ht.r32(), eax);
        c.movsxd(ht.r64(), ht.r32());
    }
}

void lwu(u32 rs, u32 rt, s16 imm)
{
    load<u32, false>(rs, rt, imm);
}

void mult(u32 rs, u32 rt)
{
    c.movsxd(rax, GprPtr32(rs));
    if (rs == rt) {
        c.imul(rax, rax);
    } else {
        c.movsxd(rcx, GprPtr32(rt));
        c.imul(rax, rcx);
    }
    c.movsxd(rcx, eax);
    c.sar(rax, 32);
    c.mov(JitPtr(lo), rcx);
    c.mov(JitPtr(hi), rax);
    block_cycles += 4;
}

void multu(u32 rs, u32 rt)
{
    mult(rs, rt);
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
    if (!CheckDwordOpCondJit()) {
        return;
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
    reg_alloc.Call(WriteVirtual<8, Alignment::UnalignedLeft>);
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
    reg_alloc.Call(WriteVirtual<8, Alignment::UnalignedRight>);
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

void sync()
{
    /* Completes the Load/store instruction currently in the pipeline before the new
       load/store instruction is executed. Is executed as a NOP on the VR4300. */
}

void syscall()
{
    BlockEpilogWithPcFlushAndJmp(SyscallException);
    branched = true;
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
    reg_alloc.Call(WriteVirtual<4, Alignment::UnalignedLeft>);
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
    reg_alloc.Call(WriteVirtual<4, Alignment::UnalignedRight>);
    reg_alloc.FreeArgs(2);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_end);

    BlockEpilog();

    c.bind(l_end);
}

private:
template<mips::Cond cc, bool likely> void branch(u32 rs, u32 rt, s16 imm)
{
    if (!rs && !rt) {
        if constexpr (cc == mips::Cond::Eq) TakeBranchJit(jit_pc + 4 + (imm << 2));
        if constexpr (cc == mips::Cond::Ne) likely ? DiscardBranchJit() : OnBranchNotTakenJit();
    } else {
        Label l_branch = c.newLabel(), l_end = c.newLabel();
        if (!rs) {
            c.mov(rax, GprPtr64(rt));
            c.test(rax, rax);
        } else if (!rt) {
            c.mov(rax, GprPtr64(rs));
            c.test(rax, rax);
        } else {
            c.mov(rax, GprPtr64(rs));
            c.cmp(rax, GprPtr64(rt));
        }
        if constexpr (cc == mips::Cond::Eq) c.je(l_branch);
        if constexpr (cc == mips::Cond::Ne) c.jne(l_branch);
        likely ? DiscardBranchJit() : OnBranchNotTakenJit();
        c.jmp(l_end);
        c.bind(l_branch);
        TakeBranchJit(jit_pc + 4 + (imm << 2));
        c.bind(l_end);
    }
    branch_hit = true;
}

template<mips::Cond cc, bool likely> void branch(u32 rs, s16 imm)
{
    Label l_branch = c.newLabel(), l_end = c.newLabel();
    c.mov(rax, GprPtr64(rs));
    c.test(rax, rax);
    if constexpr (cc == mips::Cond::Ge) c.jns(l_branch);
    if constexpr (cc == mips::Cond::Gt) c.jg(l_branch);
    if constexpr (cc == mips::Cond::Le) c.jle(l_branch);
    if constexpr (cc == mips::Cond::Lt) c.js(l_branch);
    likely ? DiscardBranchJit() : OnBranchNotTakenJit();
    c.jmp(l_end);
    c.bind(l_branch);
    TakeBranchJit(jit_pc + 4 + (imm << 2));
    c.bind(l_end);
    branch_hit = true;
}

template<mips::Cond cc, bool likely> void branch_and_link(auto... args)
{
    LinkJit(31);
    branch<cc, likely>(args...);
}

template<std::integral Int, bool linked> void load(u32 rs, u32 rt, s16 imm)
{
    FlushPc();

    Label l_noexception = c.newLabel();
    if constexpr (os.linux) {
        c.mov(rdi, GprPtr64(rs));
        if (imm) {
            c.add(rdi, imm);
        }
        c.call(ReadVirtual<std::make_signed_t<Int>>);
    } else {
        c.sub(rsp, 32);
        c.mov(rcx, GprPtr64(rs));
        if (imm) {
            c.add(rcx, imm);
        }
        c.call(ReadVirtual<std::make_signed_t<Int>>);
        c.add(rsp, 32);
    }

    if constexpr (linked) {
        c.mov(ecx, JitPtr(last_paddr_on_load));
        c.shr(ecx, 4);
        c.mov(JitPtr(cop0.ll_addr), ecx);
        c.mov(JitPtr(ll_bit), 1);
    }

    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_noexception);

    BlockEpilog();

    c.bind(l_noexception);
    if (rt) {
        if constexpr (std::same_as<Int, s8>) c.movsx(rax, al);
        if constexpr (std::same_as<Int, u8>) c.movzx(eax, al);
        if constexpr (std::same_as<Int, s16>) c.movsx(rax, ax);
        if constexpr (std::same_as<Int, u16>) c.movzx(eax, ax);
        if constexpr (std::same_as<Int, s32>) c.cdqe(rax);
        if constexpr (std::same_as<Int, u32>) c.mov(eax, eax);
        c.mov(GprPtr64(rt), rax);
    }
}

template<bool unsig> void multiply64(u32 rs, u32 rt)
{
    c.mov(rax, GprPtr64(rs));
    c.xor_(edx, edx);
    if constexpr (unsig) {
        if (rs == rt) {
            c.mul(rdx, rax, rax);
        } else {
            c.mul(rdx, rax, GprPtr64(rt));
        }
    } else {
        if (rs == rt) {
            c.imul(rdx, rax, rax);
        } else {
            c.imul(rdx, rax, GprPtr64(rt));
        }
    }
    c.mov(JitPtr(lo), rax);
    c.mov(JitPtr(hi), rdx);
    block_cycles += 7;
}

template<std::integral Int> void store(u32 rs, u32 rt, s16 imm)
{
    FlushPc();

    Label l_end = c.newLabel();
    if constexpr (os.linux) {
        c.mov(rdi, GprPtr64(rs));
        if (imm) {
            c.add(rdi, imm);
        }
        if constexpr (sizeof(Int) < 8) {
            c.mov(esi, GprPtr32(rt));
        } else {
            c.mov(rsi, GprPtr64(rt));
        }
        c.call(WriteVirtual<sizeof(Int)>);
    } else {
        c.sub(rsp, 32);
        c.mov(rcx, GprPtr64(rs));
        if (imm) {
            c.add(rcx, imm);
        }
        if constexpr (sizeof(Int) < 8) {
            c.mov(edx, GprPtr32(rt));
        } else {
            c.mov(rdx, GprPtr64(rt));
        }
        c.call(WriteVirtual<sizeof(Int)>);
        c.add(rsp, 32);
    }

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
        Gpd ht = GetDirtyGpr32(rt);
        c.xor_(ht, ht);
    }
    c.jmp(l_end);

    c.bind(l_store);
    FlushPc();
    reg_alloc.ReserveArgs(2);
    Gpq hs = GetGpr(rs), ht = GetGpr(rt);
    c.lea(host_gpr_arg[0], ptr(hs, imm));
    c.mov(host_gpr_arg[1], ht);
    reg_alloc.Call(WriteVirtual<sizeof(Int)>);
    reg_alloc.FreeArgs(2);
    if (rt) c.mov(GetDirtyGpr32(rt), 1);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_end);

    BlockEpilog();

    c.bind(l_end);
}

} // namespace n64::vr4300::x64
