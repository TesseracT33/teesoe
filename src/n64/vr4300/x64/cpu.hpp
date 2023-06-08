#pragma once

#include "mips/recompiler.hpp"
#include "vr4300/recompiler.hpp"

namespace n64::vr4300::x64 {

using namespace asmjit;
using namespace asmjit::x86;

struct Recompiler : public mips::Recompiler<s64, s64, u64> {
    using mips::Recompiler<s64, s64, u64>::Recompiler;

    void beq(u32 rs, u32 rt, s16 imm) const { branch<mips::Cond::Eq, false>(rs, rt, imm); }

    void beql(u32 rs, u32 rt, s16 imm) const { branch<mips::Cond::Eq, true>(rs, rt, imm); }

    void bgez(u32 rs, s16 imm) const { branch<mips::Cond::Ge, false>(rs, imm); }

    void bgezal(u32 rs, s16 imm) const
    {
        Label l_branch = c.newLabel(), l_link = c.newLabel(), l_no_delay_slot = c.newLabel(), l_end = c.newLabel();
        c.mov(ebx, ptr(in_branch_delay_slot_taken));
        c.or_(ebx, ptr(in_branch_delay_slot_not_taken));
        Gp hs = GetGpr(rs);
        c.test(hs, hs);
        c.jns(l_branch);
        OnBranchNotTakenJit();
        c.jmp(l_link);

        c.bind(l_branch);
        c.mov(rax, jit_pc + 4 + (imm << 2));
        TakeBranchJit(rax);

        Gpq h31 = GetGprMarkDirty(31);

        c.bind(l_link);
        c.test(ebx, ebx);
        c.je(l_no_delay_slot);
        c.mov(h31.r32(), 4);
        c.add(h31, ptr(jump_addr));
        c.jmp(l_end);

        c.bind(l_no_delay_slot);
        c.mov(h31, jit_pc + 8);

        c.bind(l_end);
        c.xor_(ebx, ebx);

        branch_hit = true;
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

    void break_() const { BlockEpilogWithJmp(BreakpointException); }

    void ddiv(u32 rs, u32 rt) const
    {
        if (!CheckDwordOpCondJit()) return;

        Label l_div = c.newLabel(), l_divzero = c.newLabel(), l_end = c.newLabel();
        Gpq hs = GetGpr(rs), ht = GetGpr(rt);
        c.test(ht, ht);
        c.je(l_divzero);
        c.mov(rax, hs);
        c.mov(rbx, ht);
        c.xor_(rax, 1LL << 63);
        c.not_(rbx);
        c.or_(rax, rbx);
        c.jne(l_div);
        c.mov(ptr(lo), hs);
        c.mov(ptr(hi), 0);
        c.jmp(l_end);

        c.bind(l_divzero);
        c.mov(rax, hs);
        c.mov(ptr(hi), rax);
        c.sar(rax, 63);
        c.and_(eax, 2);
        c.dec(rax);
        c.mov(ptr(lo), rax);
        c.jmp(l_end);

        c.bind(l_div);
        c.mov(rax, hs);

        Gpq t = ht;
        bool rdx_bound = reg_alloc.IsBound(rdx);
        if (rdx_bound) {
            c.mov(rbx, rdx);
            if (ht == rdx) {
                c.push(rcx);
                c.mov(rcx, ht);
                t = rcx;
            }
        }

        c.xor_(edx, edx);
        c.idiv(rax, t);
        c.mov(ptr(lo), rax);
        c.mov(ptr(hi), rdx);

        if (rdx_bound) {
            c.mov(rdx, rbx);
            if (ht == rdx) c.pop(rcx);
        }

        c.bind(l_end);
        c.xor_(ebx, ebx);
        block_cycles += 68;
    }

    void ddivu(u32 rs, u32 rt) const
    {
        if (!CheckDwordOpCondJit()) return;

        Label l_div = c.newLabel(), l_end = c.newLabel();
        Gpq hs = GetGpr(rs), ht = GetGpr(rt);
        c.test(ht, ht);
        c.jne(l_div);
        c.mov(ptr(lo), -1);
        c.mov(ptr(hi), hs);
        c.jmp(l_end);

        c.bind(l_div);
        c.mov(rax, hs);

        Gpq t = ht;
        bool rdx_bound = reg_alloc.IsBound(rdx);
        if (rdx_bound) {
            c.mov(rbx, rdx);
            if (ht == rdx) {
                c.push(rcx);
                c.mov(rcx, ht);
                t = rcx;
            }
        }

        c.xor_(edx, edx);
        c.div(rax, t);
        c.mov(ptr(lo), rax);
        c.mov(ptr(hi), rdx);

        if (rdx_bound) {
            c.mov(rdx, rbx);
            c.xor_(ebx, ebx);
            if (ht == rdx) c.pop(rcx);
        }

        c.bind(l_end);
        block_cycles += 68;
    }

    void div(u32 rs, u32 rt) const
    {
        Label l_div = c.newLabel(), l_divzero = c.newLabel(), l_end = c.newLabel();
        Gpd hs = GetGpr32(rs), ht = GetGpr32(rt);
        c.test(ht, ht);
        c.je(l_divzero);
        c.mov(eax, hs);
        c.mov(ebx, ht);
        c.xor_(eax, 1 << 31);
        c.not_(ebx);
        c.or_(eax, ebx);
        c.jne(l_div);
        c.movsxd(rax, hs);
        c.mov(ptr(lo), rax);
        c.mov(ptr(hi), 0);
        c.jmp(l_end);

        c.bind(l_divzero);
        c.mov(eax, hs);
        c.mov(ptr(hi), eax);
        c.sar(eax, 31);
        c.and_(eax, 2);
        c.dec(rax);
        c.mov(ptr(lo), eax);
        c.jmp(l_end);

        c.bind(l_div);
        c.mov(eax, hs);

        Gpd t = ht;
        bool rdx_bound = reg_alloc.IsBound(rdx);
        if (rdx_bound) {
            c.mov(rbx, rdx);
            if (ht == edx) {
                c.push(rcx);
                c.mov(ecx, ht);
                t = ecx;
            }
        }

        c.xor_(edx, edx);
        c.idiv(eax, t);
        c.cdqe(rax);
        c.mov(ptr(lo), rax);
        c.movsxd(rdx, edx);
        c.mov(ptr(hi), rdx);

        if (rdx_bound) {
            c.mov(rdx, rbx);
            if (ht == edx) c.pop(rcx);
        }

        c.bind(l_end);
        c.xor_(ebx, ebx);
        block_cycles += 36;
    }

    void divu(u32 rs, u32 rt) const
    {
        Label l_div = c.newLabel(), l_end = c.newLabel();
        Gpd hs = GetGpr32(rs), ht = GetGpr32(rt);
        c.test(ht, ht);
        c.jne(l_div);
        c.mov(ptr(lo), -1);
        c.movsxd(rax, hs);
        c.mov(ptr(hi), rax);
        c.jmp(l_end);

        c.bind(l_div);
        c.mov(eax, hs);

        Gpd t = ht;
        bool rdx_bound = reg_alloc.IsBound(rdx);
        if (rdx_bound) {
            c.mov(rbx, rdx);
            if (ht == edx) {
                c.push(rcx);
                c.mov(ecx, ht);
                t = ecx;
            }
        }

        c.xor_(edx, edx);
        c.div(eax, t);
        c.cdqe(rax);
        c.mov(ptr(lo), rax);
        c.movsxd(rax, edx);
        c.mov(ptr(hi), rax);

        if (rdx_bound) {
            c.mov(rdx, rbx);
            c.xor_(ebx, ebx);
            if (ht == edx) c.pop(rcx);
        }

        c.bind(l_end);
        block_cycles += 36;
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
            multiply64<true>(rt, rt);
        }
    }

    void j(u32 instr) const
    {
        Label l_end = c.newLabel();
        c.cmp(ptr(in_branch_delay_slot_taken), 1);
        c.je(l_end);
        c.mov(rax, (jit_pc + 4) & 0xFFFF'FFFF'F000'0000 | instr << 2 & 0xFFF'FFFF);
        TakeBranchJit(rax);
        c.bind(l_end);
        branch_hit = true;
    }

    void jal(u32 instr) const
    {
        Label l_jump = c.newLabel(), l_end = c.newLabel();
        Gpq h31 = GetGprMarkDirty(31);
        c.cmp(ptr(in_branch_delay_slot_taken), 0);
        c.je(l_jump);
        c.mov(h31.r32(), 4);
        c.add(h31, ptr(jump_addr));
        c.jmp(l_end);
        c.bind(l_jump);
        c.mov(h31, jit_pc + 8);
        c.mov(rax, (jit_pc + 4) & 0xFFFF'FFFF'F000'0000 | instr << 2 & 0xFFF'FFFF);
        TakeBranchJit(rax);
        c.bind(l_end);
        branch_hit = true;
    }

    void jalr(u32 rs, u32 rd) const
    {
        Label l_jump = c.newLabel(), l_end = c.newLabel();
        Gpq hd = GetGprMarkDirty(rd);
        c.cmp(ptr(in_branch_delay_slot_taken), 0);
        c.je(l_jump);
        c.mov(hd.r32(), 4);
        c.add(hd, ptr(jump_addr));
        c.jmp(l_end);
        c.bind(l_jump);
        if (rs == rd) {
            c.mov(rax, GetGpr(rs));
            c.mov(hd, jit_pc + 8);
            TakeBranchJit(rax);
        } else {
            c.mov(hd, jit_pc + 8);
            TakeBranchJit(GetGpr(rs));
        }
        c.bind(l_end);
        branch_hit = true;
    }

    void jr(u32 rs) const
    {
        Label l_end = c.newLabel();
        c.cmp(ptr(in_branch_delay_slot_taken), 1);
        c.je(l_end);
        TakeBranchJit(GetGpr(rs));
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
        if (CheckDwordOpCondJit()) {
            load_left<s64>(rs, rt, imm);
        }
    }

    void ldr(u32 rs, u32 rt, s16 imm) const
    {
        if (CheckDwordOpCondJit()) {
            load_right<s64>(rs, rt, imm);
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

    void lwl(u32 rs, u32 rt, s16 imm) const { load_left<s32>(rs, rt, imm); }

    void lwr(u32 rs, u32 rt, s16 imm) const { load_right<s32>(rs, rt, imm); }

    void lwu(u32 rs, u32 rt, s16 imm) const { load<u32, false>(rs, rt, imm); }

    void mult(u32 rs, u32 rt) const
    {
        c.movsxd(rax, GetGpr32(rs));
        c.movsxd(rbx, GetGpr32(rt));
        c.imul(rax, rbx);
        c.movsxd(rbx, eax);
        c.mov(ptr(lo), rbx);
        c.sar(rax, 32);
        c.mov(ptr(hi), rax);
        c.xor_(ebx, ebx);

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
        reg_alloc.Flush(host_gpr_arg[0]);
        reg_alloc.Flush(host_gpr_arg[1]);
        Gpq hs = GetGpr(rs);
        if (imm) c.lea(host_gpr_arg[0], Mem(hs, imm)); // [HS + imm]
        else if (host_gpr_arg[0] != hs) c.mov(host_gpr_arg[0], hs);
        c.lea(eax, Mem(host_gpr_arg[0], 3u)); // [8 * GP]
        c.sarx(host_gpr_arg[1], GetGpr(rt), rax);
        reg_alloc.Call(WriteVirtual<8, Alignment::UnalignedLeft>);
        c.cmp(ptr(exception_occurred), 0);
        c.je(l_end);

        BlockEpilog();

        c.bind(l_end);
    }

    void sdr(u32 rs, u32 rt, s16 imm) const
    {
        if (!CheckDwordOpCondJit()) return;
        Label l_end = c.newLabel();
        reg_alloc.Flush(host_gpr_arg[0]);
        reg_alloc.Flush(host_gpr_arg[1]);
        c.mov(rax, GetGpr(rs));
        if (imm) c.lea(host_gpr_arg[0], Mem(rax, imm)); // [rax + imm]
        else c.mov(host_gpr_arg[0], rax);
        c.imul(eax, 56);
        c.shlx(host_gpr_arg[1], GetGpr(rt), rax);
        reg_alloc.Call(WriteVirtual<8, Alignment::UnalignedRight>);
        c.cmp(ptr(exception_occurred), 0);
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

    void syscall() const { BlockEpilogWithJmp(SyscallException); }

    void sw(u32 rs, u32 rt, s16 imm) const { store<s32>(rs, rt, imm); }

    void swl(u32 rs, u32 rt, s16 imm) const
    {
        Label l_end = c.newLabel();
        reg_alloc.Flush(host_gpr_arg[0]);
        reg_alloc.Flush(host_gpr_arg[1]);
        Gpq hs = GetGpr(rs);
        if (imm) c.lea(host_gpr_arg[0], Mem(hs, imm)); // [HS + imm]
        else if (host_gpr_arg[0] != hs) c.mov(host_gpr_arg[0], hs);
        c.lea(eax, Mem(host_gpr_arg[0], 3u)); // [8 * GP]
        c.and_(al, 24);
        c.shrx(host_gpr_arg[1], GetGpr(rt), rax);
        reg_alloc.Call(WriteVirtual<4, Alignment::UnalignedLeft>);
        c.cmp(ptr(exception_occurred), 0);
        c.je(l_end);

        BlockEpilog();

        c.bind(l_end);
    }

    void swr(u32 rs, u32 rt, s16 imm) const
    {
        Label l_end = c.newLabel();
        reg_alloc.Flush(host_gpr_arg[0]);
        reg_alloc.Flush(host_gpr_arg[1]);
        Gpq hs = GetGpr(rs);
        if (imm) c.lea(host_gpr_arg[0], Mem(hs, imm)); // [HS + imm]
        else if (host_gpr_arg[0] != hs) c.mov(host_gpr_arg[0], hs);
        c.lea(eax, Mem(host_gpr_arg[0], 3u)); // [8 * GP]
        c.not_(eax);
        c.and_(al, 24);
        c.shlx(host_gpr_arg[1], GetGpr(rt), rax);
        reg_alloc.Call(WriteVirtual<4, Alignment::UnalignedRight>);
        c.cmp(ptr(exception_occurred), 0);
        c.je(l_end);

        BlockEpilog();

        c.bind(l_end);
    }

private:
    template<mips::Cond cc, bool likely> void branch(u32 rs, u32 rt, s16 imm) const
    {
        Label l_branch = c.newLabel(), l_end = c.newLabel();
        Gpq hs = GetGpr(rs), ht = GetGpr(rt);
        c.cmp(hs, ht);
        if constexpr (cc == mips::Cond::Eq) c.je(l_branch);
        if constexpr (cc == mips::Cond::Ne) c.jne(l_branch);
        likely ? DiscardBranchJit() : OnBranchNotTakenJit();
        c.jmp(l_end);
        c.bind(l_branch);
        c.mov(rax, jit_pc + 4 + (imm << 2));
        TakeBranchJit(rax);
        c.bind(l_end);
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
        c.mov(rax, jit_pc + 4 + (imm << 2));
        TakeBranchJit(rax);
        c.bind(l_end);
        branch_hit = true;
    }

    template<mips::Cond cc, bool likely> void branch_and_link(auto... args) const
    {
        branch<cc, likely>(args...);
        link(31);
    }

    template<std::integral Int, bool linked> void load(u32 rs, u32 rt, s16 imm) const
    {
        Label l_noexception = c.newLabel();

        reg_alloc.Flush(host_gpr_arg[0]);
        Gpq hs = GetGpr(rs);
        if (imm) c.lea(host_gpr_arg[0], Mem(hs, imm));
        else if (host_gpr_arg[0] != hs) c.mov(host_gpr_arg[0], hs);

        reg_alloc.Call(ReadVirtual<std::make_signed_t<Int>>);

        if constexpr (linked) {
            c.mov(ecx, ptr(last_physical_address_on_load));
            c.shr(ecx, 4);
            c.mov(ptr(cop0.ll_addr), ecx);
            c.mov(ptr(ll_bit), 1);
        }

        c.cmp(ptr(exception_occurred), 0);
        c.je(l_noexception);

        BlockEpilog();

        c.bind(l_noexception);
        if (rt) {
            Gpq ht = GetGprMarkDirty(rt);
            if constexpr (std::same_as<Int, s8>) c.movsx(ht, al);
            if constexpr (std::same_as<Int, u8>) c.movzx(ht.r32(), al);
            if constexpr (std::same_as<Int, s16>) c.movsx(ht, ax);
            if constexpr (std::same_as<Int, u16>) c.movzx(ht.r32(), ax);
            if constexpr (std::same_as<Int, s32>) c.movsx(ht, eax);
            if constexpr (std::same_as<Int, u32>) c.mov(ht.r32(), eax);
            if constexpr (sizeof(Int) == 8) c.mov(ht, rax);
        }
    }

    template<std::integral Int> void load_left(u32 rs, u32 rt, s16 imm) const
    {
        Label l_noexception = c.newLabel();

        reg_alloc.Flush(host_gpr_arg[0]);
        Gpq hs = GetGpr(rs);
        if (imm) c.lea(host_gpr_arg[0], Mem(hs, imm));
        else if (host_gpr_arg[0] != hs) c.mov(host_gpr_arg[0], hs);
        c.mov(ebx, host_gpr_arg[0].r32());

        reg_alloc.Call(ReadVirtual<std::make_signed_t<Int>, Alignment::UnalignedLeft>);
        c.cmp(ptr(exception_occurred), 0);
        c.je(l_noexception);

        BlockEpilog();

        c.bind(l_noexception);
        if (rt) {
            if constexpr (sizeof(Int) == 4) {
                c.lea(ecx, Mem(ebx, 3u)); // [8 * ebx]
                c.and_(ecx, 24);
                c.shl(eax, cl);
                c.mov(ebx, 1);
                c.shl(ebx, cl);
                c.dec(ebx);
                Gpd ht = GetGpr32MarkDirty(rt);
                c.and_(ht, ebx);
                c.or_(ht, eax);
                c.movsxd(ht.r64(), ht);
            } else {
                c.lea(ecx, Mem(ebx, 3u)); // [8 * ebx]
                c.and_(ecx, 56);
                c.shl(rax, cl);
                c.mov(ebx, 1);
                c.shl(rbx, cl);
                c.dec(rbx);
                Gpq ht = GetGprMarkDirty(rt);
                c.and_(ht, rbx);
                c.or_(ht, rax);
            }
        }
        c.xor_(ebx, ebx);
    }

    template<std::integral Int> void load_right(u32 rs, u32 rt, s16 imm) const
    {
        Label l_noexception = c.newLabel();

        reg_alloc.Flush(host_gpr_arg[0]);
        Gpq hs = GetGpr(rs);
        if (imm) c.lea(host_gpr_arg[0], Mem(hs, imm));
        else if (host_gpr_arg[0] != hs) c.mov(host_gpr_arg[0], hs);
        c.mov(ebx, host_gpr_arg[0].r32());

        reg_alloc.Call(ReadVirtual<std::make_signed_t<Int>, Alignment::UnalignedRight>);
        c.cmp(ptr(exception_occurred), 0);
        c.je(l_noexception);

        BlockEpilog();

        c.bind(l_noexception);
        if (rt) {
            if constexpr (sizeof(Int) == 4) {
                c.lea(ecx, Mem(ebx, 3u)); // [8 * ebx]
                c.and_(ecx, 24);
                c.mov(ebx, 0xFFFF'FF00);
                c.shl(ebx, cl);
                c.xor_(ecx, 24);
                c.shr(eax, cl);
                Gpd ht = GetGpr32MarkDirty(rt);
                c.and_(ht, ebx);
                c.or_(ht, eax);
                c.movsxd(ht.r64(), ht);
            } else {
                c.lea(ecx, Mem(ebx, 3u)); // [8 * ebx]
                c.and_(ecx, 56);
                c.mov(rbx, 0xFFFF'FF00);
                c.shl(rbx, cl);
                c.xor_(ecx, 56);
                c.shr(rax, cl);
                Gpq ht = GetGprMarkDirty(rt);
                c.and_(ht, rbx);
                c.or_(ht, rax);
            }
        }
        c.xor_(ebx, ebx);
    }

    template<bool unsig> void multiply64(u32 rs, u32 rt) const
    {
        c.mov(rax, GetGpr(rs));
        Gpq ht = GetGpr(rt);
        bool rdx_bound = reg_alloc.IsBound(rdx);
        if (rdx_bound) c.mov(rbx, rdx);
        unsig ? c.mul(rax, ht) : c.imul(rax, ht);
        c.mov(ptr(lo), rax);
        c.mov(ptr(hi), rdx);
        if (rdx_bound) {
            c.mov(rdx, rbx);
            c.xor_(ebx, ebx);
        }

        block_cycles += 7;
    }

    template<std::integral Int> void store(u32 rs, u32 rt, s16 imm) const
    {
        Label l_end = c.newLabel();

        reg_alloc.Flush(host_gpr_arg[0]);
        reg_alloc.Flush(host_gpr_arg[1]);
        Gpq hs = GetGpr(rs), ht = GetGpr(rt);
        if (imm) c.lea(host_gpr_arg[0], Mem(hs, imm));
        else if (host_gpr_arg[0] != hs) c.mov(host_gpr_arg[0], hs);
        if (host_gpr_arg[1] != ht) c.mov(host_gpr_arg[1], ht);

        reg_alloc.Call(WriteVirtual<sizeof(Int)>);
        c.cmp(ptr(exception_occurred), 0);
        c.je(l_end);

        BlockEpilog();

        c.bind(l_end);
    }

    template<std::integral Int> void store_conditional(u32 rs, u32 rt, s16 imm) const
    {
        Label l_store, l_end = c.newLabel();
        reg_alloc.FlushAllVolatile();
        c.cmp(ptr(ll_bit), 1);
        c.je(l_store);

        if (rt) {
            Gpd ht = GetGpr32MarkDirty(rt);
            c.xor_(ht, ht);
        }
        c.jmp(l_end);

        c.bind(l_store);
        reg_alloc.Flush(host_gpr_arg[0]);
        reg_alloc.Flush(host_gpr_arg[1]);
        Gpq hs = GetGpr(rs), ht = GetGpr(rt);
        if (imm) c.lea(host_gpr_arg[0], Mem(hs, imm));
        else if (host_gpr_arg[0] != hs) c.mov(host_gpr_arg[0], hs);
        if (host_gpr_arg[1] != ht) c.mov(host_gpr_arg[1], ht);
        reg_alloc.Call(WriteVirtual<sizeof(Int)>);
        if (rt) c.mov(GetGpr32MarkDirty(rt), 1);
        c.cmp(ptr(exception_occurred), 0);
        c.je(l_end);

        BlockEpilog();

        c.bind(l_end);
    }
} inline constexpr cpu_recompiler{
    compiler,
    reg_alloc,
    lo,
    hi,
    jit_pc,
    branch_hit,
    branched,
    TakeBranchJit,
    LinkJit,
    BlockEpilog,
    BlockEpilogWithJmp,
    IntegerOverflowException,
    TrapException,
    CheckDwordOpCondJit,
};

} // namespace n64::vr4300::x64
