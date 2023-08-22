#pragma once

#include "mips/recompiler_x64.hpp"
#include "vr4300/recompiler.hpp"

namespace n64::vr4300::x64 {

using namespace asmjit;
using namespace asmjit::x86;

using RegAllocator = mips::RegisterAllocator<s64, reg_alloc_volatile_gprs.size(), reg_alloc_nonvolatile_gprs.size()>;

struct Recompiler : public mips::RecompilerX64<s64, u64, RegAllocator> {
    using mips::RecompilerX64<s64, u64, RegAllocator>::RecompilerX64;

    void beq(u32 rs, u32 rt, s16 imm) const { branch<mips::Cond::Eq, false>(rs, rt, imm); }

    void beql(u32 rs, u32 rt, s16 imm) const { branch<mips::Cond::Eq, true>(rs, rt, imm); }

    void bgez(u32 rs, s16 imm) const { branch<mips::Cond::Ge, false>(rs, imm); }

    void bgezal(u32 rs, s16 imm) const
    {
        Label l_branch = c.newLabel(), l_link = c.newLabel(), l_no_delay_slot = c.newLabel(), l_end = c.newLabel();
        reg_alloc.Reserve(rcx);
        Gpq hs = GetGpr(rs), h31 = GetDirtyGpr(31);
        c.mov(cl, GlobalVarPtr(in_branch_delay_slot_taken));
        c.or_(cl, GlobalVarPtr(in_branch_delay_slot_not_taken));
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
        c.add(h31, GlobalVarPtr(jump_addr));
        c.jmp(l_end);

        c.bind(l_no_delay_slot);
        c.mov(h31, jit_pc + 8);

        c.bind(l_end);

        branch_hit = true;
        reg_alloc.Free(rcx);
    }

    void bgezall(u32 rs, s16 imm) const { branch_and_link<mips::Cond::Ge, true>(rs, imm); }

    void bgezl(u32 rs, s16 imm) const { branch<mips::Cond::Ge, true>(rs, imm); }

    void bgtz(u32 rs, s16 imm) const { branch<mips::Cond::Gt, false>(rs, imm); }

    void bgtzl(u32 rs, s16 imm) const { branch<mips::Cond::Gt, true>(rs, imm); }

    void blez(u32 rs, s16 imm) const { branch<mips::Cond::Le, false>(rs, imm); }

    void blezl(u32 rs, s16 imm) const { branch<mips::Cond::Le, true>(rs, imm); }

    void bltz(u32 rs, s16 imm) const { branch<mips::Cond::Lt, false>(rs, imm); }

    void bltzal(u32 rs, s16 imm) const { branch_and_link<mips::Cond::Lt, false>(rs, imm); }

    void bltzall(u32 rs, s16 imm) const { branch_and_link<mips::Cond::Lt, true>(rs, imm); }

    void bltzl(u32 rs, s16 imm) const { branch<mips::Cond::Lt, true>(rs, imm); }

    void bne(u32 rs, u32 rt, s16 imm) const { branch<mips::Cond::Ne, false>(rs, rt, imm); }

    void bnel(u32 rs, u32 rt, s16 imm) const { branch<mips::Cond::Ne, true>(rs, rt, imm); }

    void break_() const
    {
        BlockEpilogWithPcFlushAndJmp(BreakpointException);
        branched = true;
    }

    void ddiv(u32 rs, u32 rt) const
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
        c.mov(GlobalVarPtr(lo), hs);
        c.mov(GlobalVarPtr(hi), 0);
        c.jmp(l_end);

        c.bind(l_divzero);
        c.mov(rax, hs);
        c.mov(GlobalVarPtr(hi), rax);
        c.sar(rax, 63);
        c.and_(eax, 2);
        c.dec(rax);
        c.mov(GlobalVarPtr(lo), rax);
        c.jmp(l_end);

        c.bind(l_div);
        c.mov(rax, hs);
        c.cqo(rdx, rax);
        c.idiv(rdx, rax, ht);
        c.mov(GlobalVarPtr(lo), rax);
        c.mov(GlobalVarPtr(hi), rdx);

        c.bind(l_end);
        block_cycles += 68;
        reg_alloc.Free(rdx);
    }

    void ddivu(u32 rs, u32 rt) const
    {
        if (!CheckDwordOpCondJit()) return;

        Label l_div = c.newLabel(), l_end = c.newLabel();
        reg_alloc.Reserve(rdx);
        Gpq hs = GetGpr(rs), ht = GetGpr(rt);
        c.test(ht, ht);
        c.jne(l_div);
        c.mov(GlobalVarPtr(lo), -1);
        c.mov(GlobalVarPtr(hi), hs);
        c.jmp(l_end);

        c.bind(l_div);
        c.mov(rax, hs);
        c.xor_(edx, edx);
        c.div(rdx, rax, ht);
        c.mov(GlobalVarPtr(lo), rax);
        c.mov(GlobalVarPtr(hi), rdx);

        c.bind(l_end);
        block_cycles += 68;
        reg_alloc.Free(rdx);
    }

    void div(u32 rs, u32 rt) const
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
        c.mov(GlobalVarPtr(lo), rax);
        c.mov(GlobalVarPtr(hi), 0);
        c.jmp(l_end);

        c.bind(l_divzero);
        c.movsxd(rax, hs);
        c.mov(GlobalVarPtr(hi), rax);
        c.sar(eax, 31);
        c.and_(eax, 2);
        c.dec(rax);
        c.mov(GlobalVarPtr(lo), rax);
        c.jmp(l_end);

        c.bind(l_div);
        c.mov(eax, hs);
        c.cdq(edx, eax);
        c.idiv(edx, eax, ht);
        c.cdqe(rax);
        c.mov(GlobalVarPtr(lo), rax);
        c.movsxd(rax, edx);
        c.mov(GlobalVarPtr(hi), rax);

        c.bind(l_end);
        block_cycles += 36;
        reg_alloc.Free(rdx);
    }

    void divu(u32 rs, u32 rt) const
    {
        Label l_div = c.newLabel(), l_end = c.newLabel();
        reg_alloc.Reserve(rdx);
        Gpd hs = GetGpr32(rs), ht = GetGpr32(rt);
        c.test(ht, ht);
        c.jne(l_div);
        c.mov(GlobalVarPtr(lo), -1);
        c.movsxd(rax, hs);
        c.mov(GlobalVarPtr(hi), rax);
        c.jmp(l_end);

        c.bind(l_div);
        c.mov(eax, hs);
        c.xor_(edx, edx);
        c.div(edx, eax, ht);
        c.cdqe(rax);
        c.mov(GlobalVarPtr(lo), rax);
        c.movsxd(rax, edx);
        c.mov(GlobalVarPtr(hi), rax);

        c.bind(l_end);
        block_cycles += 36;
        reg_alloc.Free(rdx);
    }

    void dmult(u32 rs, u32 rt) const
    {
        if (CheckDwordOpCondJit()) {
            multiply64<false>(rs, rt);
        }
    }

    void dmultu(u32 rs, u32 rt) const
    {
        if (CheckDwordOpCondJit()) {
            multiply64<true>(rs, rt);
        }
    }

    void j(u32 instr) const
    {
        Label l_end = c.newLabel();
        c.cmp(GlobalVarPtr(in_branch_delay_slot_taken), 1);
        c.je(l_end);
        TakeBranchJit((jit_pc + 4) & 0xFFFF'FFFF'F000'0000 | instr << 2 & 0xFFF'FFFF);

        c.bind(l_end);
        branch_hit = true;
    }

    void jal(u32 instr) const
    {
        Label l_jump = c.newLabel(), l_end = c.newLabel();
        Gpq h31 = GetDirtyGpr(31);
        c.cmp(GlobalVarPtr(in_branch_delay_slot_taken), 0);
        c.je(l_jump);
        c.mov(h31.r32(), 4);
        c.add(h31, GlobalVarPtr(jump_addr));
        c.jmp(l_end);

        c.bind(l_jump);
        c.mov(h31, jit_pc + 8);
        TakeBranchJit((jit_pc + 4) & 0xFFFF'FFFF'F000'0000 | instr << 2 & 0xFFF'FFFF);

        c.bind(l_end);
        branch_hit = true;
    }

    void jalr(u32 rs, u32 rd) const
    {
        Label l_jump = c.newLabel(), l_end = c.newLabel();
        Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs);
        c.cmp(GlobalVarPtr(in_branch_delay_slot_taken), 0);
        c.je(l_jump);
        c.mov(hd.r32(), 4);
        c.add(hd, GlobalVarPtr(jump_addr));
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

    void jr(u32 rs) const
    {
        Label l_end = c.newLabel();
        Gpq hs = GetGpr(rs);
        c.cmp(GlobalVarPtr(in_branch_delay_slot_taken), 1);
        c.je(l_end);
        TakeBranchJit(hs);

        c.bind(l_end);
        branch_hit = true;
    }

    void lb(u32 rs, u32 rt, s16 imm) const { load<s8, false>(rs, rt, imm); }

    void lbu(u32 rs, u32 rt, s16 imm) const { load<u8, false>(rs, rt, imm); }

    void ld(u32 rs, u32 rt, s16 imm) const
    {
        if (CheckDwordOpCondJit()) {
            load<s64, false>(rs, rt, imm);
        }
    }

    void ldl(u32 rs, u32 rt, s16 imm) const
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
        c.cmp(GlobalVarPtr(exception_occurred), 0);
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

    void ldr(u32 rs, u32 rt, s16 imm) const
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
        c.cmp(GlobalVarPtr(exception_occurred), 0);
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

    void lh(u32 rs, u32 rt, s16 imm) const { load<s16, false>(rs, rt, imm); }

    void lhu(u32 rs, u32 rt, s16 imm) const { load<u16, false>(rs, rt, imm); }

    void ll(u32 rs, u32 rt, s16 imm) const { load<s32, true>(rs, rt, imm); }

    void lld(u32 rs, u32 rt, s16 imm) const
    {
        if (CheckDwordOpCondJit()) {
            load<s64, true>(rs, rt, imm);
        }
    }

    void lw(u32 rs, u32 rt, s16 imm) const { load<s32, false>(rs, rt, imm); }

    void lwl(u32 rs, u32 rt, s16 imm) const
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
        c.cmp(GlobalVarPtr(exception_occurred), 0);
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

    void lwr(u32 rs, u32 rt, s16 imm) const
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
        c.cmp(GlobalVarPtr(exception_occurred), 0);
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

    void lwu(u32 rs, u32 rt, s16 imm) const { load<u32, false>(rs, rt, imm); }

    void mult(u32 rs, u32 rt) const
    {
        Gpq hs = GetGpr(rs), ht = GetGpr(rt), t = reg_alloc.GetTemporary().r64();
        c.movsxd(t, hs.r32());
        c.movsxd(rax, ht.r32());
        c.imul(t, rax);
        c.movsxd(rax, t.r32());
        c.mov(GlobalVarPtr(lo), rax);
        c.sar(t, 32);
        c.mov(GlobalVarPtr(hi), t);
        block_cycles += 4;
    }

    void multu(u32 rs, u32 rt) const { mult(rs, rt); }

    void sb(u32 rs, u32 rt, s16 imm) const { store<s8>(rs, rt, imm); }

    void sc(u32 rs, u32 rt, s16 imm) const { store_conditional<s32>(rs, rt, imm); }

    void scd(u32 rs, u32 rt, s16 imm) const
    {
        if (CheckDwordOpCondJit()) {
            store_conditional<s64>(rs, rt, imm);
        }
    }

    void sd(u32 rs, u32 rt, s16 imm) const
    {
        if (CheckDwordOpCondJit()) {
            store<s64>(rs, rt, imm);
        }
    }

    void sdl(u32 rs, u32 rt, s16 imm) const
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
        c.cmp(GlobalVarPtr(exception_occurred), 0);
        c.je(l_end);

        BlockEpilog();

        c.bind(l_end);
    }

    void sdr(u32 rs, u32 rt, s16 imm) const
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
        c.cmp(GlobalVarPtr(exception_occurred), 0);
        c.je(l_end);

        BlockEpilog();

        c.bind(l_end);
    }

    void sh(u32 rs, u32 rt, s16 imm) const { store<s16>(rs, rt, imm); }

    void sync() const
    {
        /* Completes the Load/store instruction currently in the pipeline before the new
           load/store instruction is executed. Is executed as a NOP on the VR4300. */
    }

    void syscall() const
    {
        BlockEpilogWithPcFlushAndJmp(SyscallException);
        branched = true;
    }

    void sw(u32 rs, u32 rt, s16 imm) const { store<s32>(rs, rt, imm); }

    void swl(u32 rs, u32 rt, s16 imm) const
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
        c.cmp(GlobalVarPtr(exception_occurred), 0);
        c.je(l_end);

        BlockEpilog();

        c.bind(l_end);
    }

    void swr(u32 rs, u32 rt, s16 imm) const
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
        c.cmp(GlobalVarPtr(exception_occurred), 0);
        c.je(l_end);

        BlockEpilog();

        c.bind(l_end);
    }

private:
    template<mips::Cond cc, bool likely> void branch(u32 rs, u32 rt, s16 imm) const
    {
        if (!rs && !rt) {
            if constexpr (cc == mips::Cond::Eq) TakeBranchJit(jit_pc + 4 + (imm << 2));
            if constexpr (cc == mips::Cond::Ne) likely ? DiscardBranchJit() : OnBranchNotTakenJit();
        } else {
            Label l_branch = c.newLabel(), l_end = c.newLabel();
            if (!rs) {
                Gp ht = GetGpr(rt);
                c.test(ht, ht);
            } else if (!rt) {
                Gp hs = GetGpr(rs);
                c.test(hs, hs);
            } else {
                Gp hs = GetGpr(rs), ht = GetGpr(rt);
                c.cmp(hs, ht);
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

    template<mips::Cond cc, bool likely> void branch(u32 rs, s16 imm) const
    {
        Label l_branch = c.newLabel(), l_end = c.newLabel();
        Gpq hs = GetGpr(rs);
        c.test(hs, hs);
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

    template<mips::Cond cc, bool likely> void branch_and_link(auto... args) const
    {
        LinkJit(31);
        branch<cc, likely>(args...);
    }

    template<std::integral Int, bool linked> void load(u32 rs, u32 rt, s16 imm) const
    {
        Label l_noexception = c.newLabel();

        FlushPc();
        reg_alloc.ReserveArgs(2);
        Gpq hs = GetGpr(rs);
        c.lea(host_gpr_arg[0], ptr(hs, imm));
        reg_alloc.Call(ReadVirtual<std::make_signed_t<Int>>);
        reg_alloc.FreeArgs(2);

        if constexpr (linked) {
            c.mov(ecx, GlobalVarPtr(last_paddr_on_load));
            c.shr(ecx, 4);
            c.mov(GlobalVarPtr(cop0.ll_addr), ecx);
            c.mov(GlobalVarPtr(ll_bit), 1);
        }

        c.cmp(GlobalVarPtr(exception_occurred), 0);
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

    template<bool unsig> void multiply64(u32 rs, u32 rt) const
    {
        Gpq hs = GetGpr(rs), ht = GetGpr(rt);
        reg_alloc.Reserve(rdx);
        c.mov(rax, hs);
        c.xor_(edx, edx);
        if constexpr (unsig) {
            c.mul(rdx, rax, ht);
        } else {
            c.imul(rdx, rax, ht);
        }
        c.mov(GlobalVarPtr(lo), rax);
        c.mov(GlobalVarPtr(hi), rdx);
        block_cycles += 7;
        reg_alloc.Free(rdx);
    }

    template<std::integral Int> void store(u32 rs, u32 rt, s16 imm) const
    {
        Label l_end = c.newLabel();
        FlushPc();
        reg_alloc.ReserveArgs(2);
        Gpq hs = GetGpr(rs), ht = GetGpr(rt);
        c.lea(host_gpr_arg[0], ptr(hs, imm));
        c.mov(host_gpr_arg[1], ht);
        reg_alloc.Call(WriteVirtual<sizeof(Int)>);
        reg_alloc.FreeArgs(2);
        c.cmp(GlobalVarPtr(exception_occurred), 0);
        c.je(l_end);

        BlockEpilog();

        c.bind(l_end);
    }

    template<std::integral Int> void store_conditional(u32 rs, u32 rt, s16 imm) const
    {
        Label l_store = c.newLabel(), l_end = c.newLabel();
        c.cmp(GlobalVarPtr(ll_bit), 1);
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
        c.cmp(GlobalVarPtr(exception_occurred), 0);
        c.je(l_end);

        BlockEpilog();

        c.bind(l_end);
    }
} inline constexpr cpu_recompiler{
    compiler,
    reg_alloc,
    jit_pc,
    branch_hit,
    branched,
    [] { return GlobalVarPtr(lo); },
    [] { return GlobalVarPtr(hi); },
    [](u64 target) { TakeBranchJit(target); },
    [](HostGpr target) { TakeBranchJit(target); },
    LinkJit,
    BlockEpilogWithPcFlushAndJmp,
    IntegerOverflowException,
    TrapException,
    CheckDwordOpCondJit,
};

} // namespace n64::vr4300::x64
