#pragma once

#include "mips/recompiler_x64.hpp"
#include "vr4300/recompiler.hpp"

namespace n64::vr4300::x64 {

using namespace asmjit;
using namespace asmjit::x86;

using RegAllocator = mips::RegisterAllocator<s64, reg_alloc_volatile_gprs.size(), reg_alloc_nonvolatile_gprs.size()>;

struct Recompiler : public mips::RecompilerX64<s64, s64, u64, RegAllocator> {
    using mips::RecompilerX64<s64, s64, u64, RegAllocator>::RecompilerX64;

    void beq(u32 rs, u32 rt, s16 imm) const { branch<mips::Cond::Eq, false>(rs, rt, imm); }

    void beql(u32 rs, u32 rt, s16 imm) const { branch<mips::Cond::Eq, true>(rs, rt, imm); }

    void bgez(u32 rs, s16 imm) const { branch<mips::Cond::Ge, false>(rs, imm); }

    void bgezal(u32 rs, s16 imm) const
    {
        Label l_branch = c.newLabel(), l_link = c.newLabel(), l_no_delay_slot = c.newLabel(), l_end = c.newLabel();
        Gpq hs = GetGpr(rs), h31 = GetDirtyGpr(31);
        reg_alloc.Free(rcx);
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
        reg_alloc.Free(rdx);
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
        c.xor_(edx, edx);
        c.idiv(rax, ht);
        c.mov(GlobalVarPtr(lo), rax);
        c.mov(GlobalVarPtr(hi), rdx);

        c.bind(l_end);
        block_cycles += 68;
    }

    void ddivu(u32 rs, u32 rt) const
    {
        if (!CheckDwordOpCondJit()) return;

        Label l_div = c.newLabel(), l_end = c.newLabel();
        Gpq hs = GetGpr(rs), ht = GetGpr(rt);
        reg_alloc.Free(rdx);
        c.test(ht, ht);
        c.jne(l_div);
        c.mov(GlobalVarPtr(lo), -1);
        c.mov(GlobalVarPtr(hi), hs);
        c.jmp(l_end);

        c.bind(l_div);
        c.mov(rax, hs);
        c.xor_(edx, edx);
        c.div(rax, ht);
        c.mov(GlobalVarPtr(lo), rax);
        c.mov(GlobalVarPtr(hi), rdx);

        c.bind(l_end);
        block_cycles += 68;
    }

    void div(u32 rs, u32 rt) const
    {
        Label l_div = c.newLabel(), l_divzero = c.newLabel(), l_end = c.newLabel();
        Gpd hs = GetGpr32(rs), ht = GetGpr32(rt);
        reg_alloc.Free(rdx);
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
        c.xor_(edx, edx);
        c.idiv(eax, ht);
        c.cdqe(rax);
        c.mov(GlobalVarPtr(lo), rax);
        c.movsxd(rax, edx);
        c.mov(GlobalVarPtr(hi), rax);

        c.bind(l_end);
        block_cycles += 36;
    }

    void divu(u32 rs, u32 rt) const
    {
        Label l_div = c.newLabel(), l_end = c.newLabel();
        Gpd hs = GetGpr32(rs), ht = GetGpr32(rt);
        reg_alloc.Free(rdx);
        c.test(ht, ht);
        c.jne(l_div);
        c.mov(GlobalVarPtr(lo), -1);
        c.movsxd(rax, hs);
        c.mov(GlobalVarPtr(hi), rax);
        c.jmp(l_end);

        c.bind(l_div);
        c.mov(eax, hs);
        c.xor_(edx, edx);
        c.div(eax, ht);
        c.cdqe(rax);
        c.mov(GlobalVarPtr(lo), rax);
        c.movsxd(rax, edx);
        c.mov(GlobalVarPtr(hi), rax);

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
        reg_alloc.Free<2>({ host_gpr_arg[0], host_gpr_arg[1] });
        Gpq hs = GetGpr(rs), ht = GetGpr(rt);
        c.lea(host_gpr_arg[0], ptr(hs, imm));
        c.lea(eax, ptr(host_gpr_arg[0], 3u));
        c.sarx(host_gpr_arg[1], ht, rax);
        reg_alloc.Call(WriteVirtual<8, Alignment::UnalignedLeft>);
        c.cmp(GlobalVarPtr(exception_occurred), 0);
        c.je(l_end);

        BlockEpilog();

        c.bind(l_end);
    }

    void sdr(u32 rs, u32 rt, s16 imm) const
    {
        if (!CheckDwordOpCondJit()) return;
        Label l_end = c.newLabel();
        reg_alloc.Free<2>({ host_gpr_arg[0], host_gpr_arg[1] });
        Gpq hs = GetGpr(rs), ht = GetGpr(rt);
        c.mov(rax, hs);
        c.lea(host_gpr_arg[0], ptr(rax, imm));
        c.imul(eax, 56);
        c.shlx(host_gpr_arg[1], ht, rax);
        reg_alloc.Call(WriteVirtual<8, Alignment::UnalignedRight>);
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

    void syscall() const { BlockEpilogWithJmp(SyscallException); }

    void sw(u32 rs, u32 rt, s16 imm) const { store<s32>(rs, rt, imm); }

    void swl(u32 rs, u32 rt, s16 imm) const
    {
        Label l_end = c.newLabel();
        reg_alloc.Free<2>({ host_gpr_arg[0], host_gpr_arg[1] });
        Gpq hs = GetGpr(rs), ht = GetGpr(rt);
        c.lea(host_gpr_arg[0], ptr(hs, imm));
        c.lea(eax, ptr(host_gpr_arg[0], 3u));
        c.and_(al, 24);
        c.shrx(host_gpr_arg[1], ht, rax);
        reg_alloc.Call(WriteVirtual<4, Alignment::UnalignedLeft>);
        c.cmp(GlobalVarPtr(exception_occurred), 0);
        c.je(l_end);

        BlockEpilog();

        c.bind(l_end);
    }

    void swr(u32 rs, u32 rt, s16 imm) const
    {
        Label l_end = c.newLabel();
        reg_alloc.Free<2>({ host_gpr_arg[0], host_gpr_arg[1] });
        Gpq hs = GetGpr(rs), ht = GetGpr(rt);
        c.lea(host_gpr_arg[0], ptr(hs, imm));
        c.lea(eax, ptr(host_gpr_arg[0], 3u));
        c.not_(eax);
        c.and_(al, 24);
        c.shlx(host_gpr_arg[1], ht, rax);
        reg_alloc.Call(WriteVirtual<4, Alignment::UnalignedRight>);
        c.cmp(GlobalVarPtr(exception_occurred), 0);
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

        reg_alloc.Free(host_gpr_arg[0]);
        Gpq hs = GetGpr(rs);
        c.lea(host_gpr_arg[0], ptr(hs, imm));

        reg_alloc.Call(ReadVirtual<std::make_signed_t<Int>>);

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

    template<std::integral Int> void load_left(u32 rs, u32 rt, s16 imm) const
    {
        Label l_noexception = c.newLabel();

        reg_alloc.Free<2>({ rbx, host_gpr_arg[0] });
        Gpq hs = GetGpr(rs);
        c.lea(host_gpr_arg[0], ptr(hs, imm));
        c.mov(rbx, host_gpr_arg[0]);

        reg_alloc.Call(ReadVirtual<std::make_signed_t<Int>, Alignment::UnalignedLeft>);
        c.cmp(GlobalVarPtr(exception_occurred), 0);
        c.je(l_noexception);

        BlockEpilog();

        c.bind(l_noexception);
        if (rt) {
            Gpq ht = GetDirtyGpr(rt);
            if constexpr (sizeof(Int) == 4) {
                c.lea(ecx, ptr(ebx, 3u)); // [8 * ebx]
                c.and_(ecx, 24);
                c.shl(eax, cl);
                c.mov(ebx, 1);
                c.shl(ebx, cl);
                c.dec(ebx);
                c.and_(ht.r32(), ebx);
                c.or_(ht.r32(), eax);
                c.movsxd(ht.r64(), ht.r32());
            } else {
                c.lea(ecx, ptr(ebx, 3u)); // [8 * ebx]
                c.and_(ecx, 56);
                c.shl(rax, cl);
                c.mov(ebx, 1);
                c.shl(rbx, cl);
                c.dec(rbx);
                c.and_(ht, rbx);
                c.or_(ht, rax);
            }
        }
    }

    template<std::integral Int> void load_right(u32 rs, u32 rt, s16 imm) const
    {
        Label l_noexception = c.newLabel();

        reg_alloc.Free<2>({ rbx, host_gpr_arg[0] });
        Gpq hs = GetGpr(rs);
        c.lea(host_gpr_arg[0], ptr(hs, imm));
        c.mov(rbx, host_gpr_arg[0]);

        reg_alloc.Call(ReadVirtual<std::make_signed_t<Int>, Alignment::UnalignedRight>);
        c.cmp(GlobalVarPtr(exception_occurred), 0);
        c.je(l_noexception);

        BlockEpilog();

        c.bind(l_noexception);
        if (rt) {
            Gpq ht = GetDirtyGpr(rt);
            if constexpr (sizeof(Int) == 4) {
                c.lea(ecx, ptr(ebx, 3u)); // [8 * ebx]
                c.and_(ecx, 24);
                c.mov(ebx, 0xFFFF'FF00);
                c.shl(ebx, cl);
                c.xor_(ecx, 24);
                c.shr(eax, cl);
                c.and_(ht.r32(), ebx);
                c.or_(ht.r32(), eax);
                c.movsxd(ht.r64(), ht.r32());
            } else {
                c.lea(ecx, ptr(ebx, 3u)); // [8 * ebx]
                c.and_(ecx, 56);
                c.mov(rbx, 0xFFFF'FF00);
                c.shl(rbx, cl);
                c.xor_(ecx, 56);
                c.shr(rax, cl);
                c.and_(ht, rbx);
                c.or_(ht, rax);
            }
        }
    }

    template<bool unsig> void multiply64(u32 rs, u32 rt) const
    {
        Gpq hs = GetGpr(rs), ht = GetGpr(rt);
        reg_alloc.Free(rdx);
        c.mov(rax, hs);
        c.xor_(rdx, rdx);
        unsig ? c.mul(rax, ht) : c.imul(rax, ht);
        c.mov(GlobalVarPtr(lo), rax);
        c.mov(GlobalVarPtr(hi), rdx);
        block_cycles += 7;
    }

    template<std::integral Int> void store(u32 rs, u32 rt, s16 imm) const
    {
        Label l_end = c.newLabel();

        reg_alloc.Free<2>({ host_gpr_arg[0], host_gpr_arg[1] });
        Gpq hs = GetGpr(rs), ht = GetGpr(rt);
        c.lea(host_gpr_arg[0], ptr(hs, imm));
        if (host_gpr_arg[1] != ht) c.mov(host_gpr_arg[1], ht);

        reg_alloc.Call(WriteVirtual<sizeof(Int)>);
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
        reg_alloc.Free<2>({ host_gpr_arg[0], host_gpr_arg[1] });
        Gpq hs = GetGpr(rs), ht = GetGpr(rt);
        c.lea(host_gpr_arg[0], ptr(hs, imm));
        if (host_gpr_arg[1] != ht) c.mov(host_gpr_arg[1], ht);
        reg_alloc.Call(WriteVirtual<sizeof(Int)>);
        if (rt) c.mov(GetDirtyGpr32(rt), 1);
        c.cmp(GlobalVarPtr(exception_occurred), 0);
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
    [](u64 target) { TakeBranchJit(target); },
    [](HostGpr target) { TakeBranchJit(target); },
    LinkJit,
    BlockEpilog,
    BlockEpilogWithJmp,
    IntegerOverflowException,
    TrapException,
    CheckDwordOpCondJit,
};

} // namespace n64::vr4300::x64
