#include "cop0.hpp"
#include "exceptions.hpp"
#include "interpreter.hpp"
#include "mmu.hpp"
#include "vr4300.hpp"

#include <limits>

namespace n64::vr4300 {

void add(u32 rs, u32 rt, u32 rd)
{
    s32 sum;
    if (__builtin_add_overflow(s32(gpr[rs]), s32(gpr[rt]), &sum)) {
        IntegerOverflowException();
    } else {
        gpr.set(rd, sum);
    }
}

void addi(u32 rs, u32 rt, s16 imm)
{
    s32 sum;
    if (__builtin_add_overflow(s32(gpr[rs]), imm, &sum)) {
        IntegerOverflowException();
    } else {
        gpr.set(rt, sum);
    }
}

void addiu(u32 rs, u32 rt, s16 imm)
{
    gpr.set(rt, s32(gpr[rs]) + imm);
}

void addu(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, s32(gpr[rs]) + s32(gpr[rt]));
}

void and_(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, gpr[rs] & gpr[rt]);
}

void andi(u32 rs, u32 rt, u16 imm)
{
    gpr.set(rt, gpr[rs] & imm);
}

void beq(u32 rs, u32 rt, s16 imm)
{
    if (gpr[rs] == gpr[rt]) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        OnBranchNotTaken();
    }
}

void beql(u32 rs, u32 rt, s16 imm)
{
    if (gpr[rs] == gpr[rt]) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        DiscardBranch();
    }
}

void bgez(u32 rs, s16 imm)
{
    if (gpr[rs] >= 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        OnBranchNotTaken();
    }
}

void bgezal(u32 rs, s16 imm)
{
    bool in_delay_slot = branch_state == BranchState::DelaySlotTaken || branch_state == BranchState::DelaySlotNotTaken;
    if (gpr[rs] >= 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        OnBranchNotTaken();
    }
    gpr.set(31, 4 + (in_delay_slot ? jump_addr : pc + 4));
}

void bgezall(u32 rs, s16 imm)
{
    gpr.set(31, pc + 8);
    if (gpr[rs] >= 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        DiscardBranch();
    }
}

void bgezl(u32 rs, s16 imm)
{
    if (gpr[rs] >= 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        DiscardBranch();
    }
}

void bgtz(u32 rs, s16 imm)
{
    if (gpr[rs] > 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        OnBranchNotTaken();
    }
}

void bgtzl(u32 rs, s16 imm)
{
    if (gpr[rs] > 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        DiscardBranch();
    }
}

void blez(u32 rs, s16 imm)
{
    if (gpr[rs] <= 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        OnBranchNotTaken();
    }
}

void blezl(u32 rs, s16 imm)
{
    if (gpr[rs] <= 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        DiscardBranch();
    }
}

void bltz(u32 rs, s16 imm)
{
    if (gpr[rs] < 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        OnBranchNotTaken();
    }
}

void bltzal(u32 rs, s16 imm)
{
    gpr.set(31, pc + 8);
    if (gpr[rs] < 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        OnBranchNotTaken();
    }
}

void bltzall(u32 rs, s16 imm)
{
    gpr.set(31, pc + 8);
    if (gpr[rs] < 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        DiscardBranch();
    }
}

void bltzl(u32 rs, s16 imm)
{
    if (gpr[rs] < 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        DiscardBranch();
    }
}

void bne(u32 rs, u32 rt, s16 imm)
{
    if (gpr[rs] != gpr[rt]) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        OnBranchNotTaken();
    }
}

void bnel(u32 rs, u32 rt, s16 imm)
{
    if (gpr[rs] != gpr[rt]) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        DiscardBranch();
    }
}

void break_()
{
    BreakpointException();
}

void dadd(u32 rs, u32 rt, u32 rd)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    s64 sum;
    if (__builtin_add_overflow(gpr[rs], gpr[rt], &sum)) {
        IntegerOverflowException();
    } else {
        gpr.set(rd, sum);
    }
}

void daddi(u32 rs, u32 rt, s16 imm)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    s64 sum;
    if (__builtin_add_overflow(gpr[rs], imm, &sum)) {
        IntegerOverflowException();
    } else {
        gpr.set(rt, sum);
    }
}

void daddiu(u32 rs, u32 rt, s16 imm)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    gpr.set(rt, gpr[rs] + imm);
}

void daddu(u32 rs, u32 rt, u32 rd)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    gpr.set(rd, gpr[rs] + gpr[rt]);
}

void ddiv(u32 rs, u32 rt)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    s64 op1 = gpr[rs];
    s64 op2 = gpr[rt];
    if (op2 == 0) {
        lo = op1 >= 0 ? -1 : 1;
        hi = op1;
    } else if (op1 == std::numeric_limits<s64>::min() && op2 == -1) {
        lo = op1;
        hi = 0;
    } else [[likely]] {
        lo = op1 / op2;
        hi = op1 % op2;
    }
    AdvancePipeline(68);
}

void ddivu(u32 rs, u32 rt)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    u64 op1 = u64(gpr[rs]);
    u64 op2 = u64(gpr[rt]);
    if (op2 == 0) {
        lo = -1;
        hi = op1;
    } else [[likely]] {
        lo = op1 / op2;
        hi = op1 % op2;
    }
    AdvancePipeline(68);
}

void div(u32 rs, u32 rt)
{
    s64 op1 = gpr[rs];
    s64 op2 = gpr[rt];
    if (op2 == 0) {
        lo = op1 < 0 ? 1 : -1;
        hi = op1;
    } else [[likely]] {
        lo = s32(s64(s32(op1)) / op2);
        hi = s32(s64(s32(op1)) % op2);
    }
    AdvancePipeline(36);
}

void divu(u32 rs, u32 rt)
{
    u32 op1 = u32(gpr[rs]);
    u32 op2 = u32(gpr[rt]);
    if (op2 == 0) {
        lo = -1;
        hi = s32(op1);
    } else [[likely]] {
        lo = s32(op1 / op2);
        hi = s32(op1 % op2);
    }
    AdvancePipeline(36);
}

void dmult(u32 rs, u32 rt)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    s128 prod = s128(gpr[rs]) * s128(gpr[rt]);
    lo = s64(prod);
    hi = s64(prod >> 64);
    AdvancePipeline(7);
}

void dmultu(u32 rs, u32 rt)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    u128 prod = u128(gpr[rs]) * u128(gpr[rt]);
    lo = s64(prod);
    hi = s64(prod >> 64);
    AdvancePipeline(7);
}

void dsll(u32 rt, u32 rd, u32 sa)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    gpr.set(rd, gpr[rt] << sa);
}

void dsll32(u32 rt, u32 rd, u32 sa)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    gpr.set(rd, gpr[rt] << (sa + 32));
}

void dsllv(u32 rs, u32 rt, u32 rd)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    gpr.set(rd, gpr[rt] << (gpr[rs] & 63));
}

void dsra(u32 rt, u32 rd, u32 sa)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    gpr.set(rd, gpr[rt] >> sa);
}

void dsra32(u32 rt, u32 rd, u32 sa)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    gpr.set(rd, gpr[rt] >> (sa + 32));
}

void dsrav(u32 rs, u32 rt, u32 rd)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    gpr.set(rd, gpr[rt] >> (gpr[rs] & 63));
}

void dsrl(u32 rt, u32 rd, u32 sa)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    gpr.set(rd, u64(gpr[rt]) >> sa);
}

void dsrl32(u32 rt, u32 rd, u32 sa)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    gpr.set(rd, u64(gpr[rt]) >> (sa + 32));
}

void dsrlv(u32 rs, u32 rt, u32 rd)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    gpr.set(rd, u64(gpr[rt]) >> (gpr[rs] & 63));
}

void dsub(u32 rs, u32 rt, u32 rd)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    s64 sum;
    if (__builtin_sub_overflow(gpr[rs], gpr[rt], &sum)) {
        IntegerOverflowException();
    } else {
        gpr.set(rd, sum);
    }
}

void dsubu(u32 rs, u32 rt, u32 rd)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    gpr.set(rd, gpr[rs] - gpr[rt]);
}

void j(u32 instr)
{
    if (branch_state != BranchState::DelaySlotTaken) {
        TakeBranch((pc + 4) & 0xFFFF'FFFF'F000'0000 | instr << 2 & 0xFFF'FFFF);
    }
}

void jal(u32 instr)
{
    if (branch_state == BranchState::DelaySlotTaken) {
        gpr.set(31, jump_addr + 4);
    } else {
        gpr.set(31, pc + 8);
        TakeBranch((pc + 4) & 0xFFFF'FFFF'F000'0000 | instr << 2 & 0xFFF'FFFF);
    }
}

void jalr(u32 rs, u32 rd)
{
    if (branch_state == BranchState::DelaySlotTaken) {
        gpr.set(rd, jump_addr + 4);
    } else {
        s64 target = gpr[rs];
        gpr.set(rd, pc + 8);
        TakeBranch(target);
    }
}

void jr(u32 rs)
{
    if (branch_state != BranchState::DelaySlotTaken) {
        TakeBranch(gpr[rs]);
    }
}

void lb(u32 rs, u32 rt, s16 imm)
{
    s8 val = ReadVirtual<s8>(gpr[rs] + imm);
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void lbu(u32 rs, u32 rt, s16 imm)
{
    u8 val = ReadVirtual<s8>(gpr[rs] + imm);
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void ld(u32 rs, u32 rt, s16 imm)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    s64 val = ReadVirtual<s64>(gpr[rs] + imm);
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void ldl(u32 rs, u32 rt, s16 imm)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    s64 addr = gpr[rs] + imm;
    s64 val = ReadVirtual<s64, Alignment::UnalignedLeft>(addr);
    if (!exception_occurred) {
        u32 bits_from_last_boundary = (u32(addr) & 7) << 3;
        val <<= bits_from_last_boundary;
        s64 untouched_gpr = gpr[rt] & ((1ll << bits_from_last_boundary) - 1);
        gpr.set(rt, val | untouched_gpr);
    }
}

void ldr(u32 rs, u32 rt, s16 imm)
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    s64 addr = gpr[rs] + imm;
    u64 val = ReadVirtual<s64, Alignment::UnalignedRight>(addr);
    if (!exception_occurred) {
        u32 bits_from_last_boundary = (u32(addr) & 7) << 3;
        val >>= 56 - bits_from_last_boundary;
        s64 untouched_gpr = gpr[rt] & 0xFFFF'FFFF'FFFF'FF00 << bits_from_last_boundary;
        gpr.set(rt, val | untouched_gpr);
    }
}

void lh(u32 rs, u32 rt, s16 imm)
{
    s16 val = ReadVirtual<s16>(gpr[rs] + imm);
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void lhu(u32 rs, u32 rt, s16 imm)
{
    u16 val = ReadVirtual<s16>(gpr[rs] + imm);
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void ll(u32 rs, u32 rt, s16 imm)
{
    s32 val = ReadVirtual<s32>(gpr[rs] + imm);
    cop0.ll_addr = last_paddr_on_load >> 4;
    ll_bit = 1;
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void lld(u32 rs, u32 rt, s16 imm)
{
    s64 val = ReadVirtual<s64>(gpr[rs] + imm);
    cop0.ll_addr = last_paddr_on_load >> 4;
    ll_bit = 1;
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void lui(u32 rt, s16 imm)
{
    gpr.set(rt, imm << 16);
}

void lw(u32 rs, u32 rt, s16 imm)
{
    s32 val = ReadVirtual<s32>(gpr[rs] + imm);
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void lwl(u32 rs, u32 rt, s16 imm)
{
    s64 addr = gpr[rs] + imm;
    s32 val = ReadVirtual<s32, Alignment::UnalignedLeft>(addr);
    if (!exception_occurred) {
        u32 bits_from_last_boundary = (u32(addr) & 3) << 3;
        val <<= bits_from_last_boundary;
        s32 untouched_gpr = s32(gpr[rt] & ((1 << bits_from_last_boundary) - 1));
        gpr.set(rt, val | untouched_gpr);
    }
}

void lwr(u32 rs, u32 rt, s16 imm)
{
    s64 addr = gpr[rs] + imm;
    u32 val = ReadVirtual<s32, Alignment::UnalignedRight>(addr);
    if (!exception_occurred) {
        u32 bits_from_last_boundary = (u32(addr) & 3) << 3;
        val >>= 24 - bits_from_last_boundary;
        s32 untouched_gpr = s32(gpr[rt] & 0xFFFF'FF00 << bits_from_last_boundary);
        gpr.set(rt, s32(val) | untouched_gpr);
    }
}

void lwu(u32 rs, u32 rt, s16 imm)
{
    u32 val = ReadVirtual<s32>(gpr[rs] + imm);
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void mfhi(u32 rd)
{
    gpr.set(rd, hi);
}

void mflo(u32 rd)
{
    gpr.set(rd, lo);
}

void mthi(u32 rs)
{
    hi = gpr[rs];
}

void mtlo(u32 rs)
{
    lo = gpr[rs];
}

void mult(u32 rs, u32 rt)
{
    s64 prod = gpr[rs] * (gpr[rt] << 29 >> 29);
    lo = s32(prod);
    hi = s32(prod >> 32);
    AdvancePipeline(4);
}

void multu(u32 rs, u32 rt)
{
    s64 prod = u64(u32(gpr[rs])) * u64(u32(gpr[rt]));
    lo = s32(prod);
    hi = s32(prod >> 32);
    AdvancePipeline(4);
}

void nor(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, ~(gpr[rs] | gpr[rt]));
}

void or_(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, gpr[rs] | gpr[rt]);
}

void ori(u32 rs, u32 rt, u16 imm)
{
    gpr.set(rt, gpr[rs] | imm);
}

void sb(u32 rs, u32 rt, s16 imm)
{
    WriteVirtual<1>(gpr[rs] + imm, gpr[rt]);
}

void sc(u32 rs, u32 rt, s16 imm)
{
    if (ll_bit) {
        WriteVirtual<4>(gpr[rs] + imm, gpr[rt]);
    }
    gpr.set(rt, ll_bit);
}

void scd(u32 rs, u32 rt, s16 imm)
{
    if (ll_bit) {
        WriteVirtual<8>(gpr[rs] + imm, gpr[rt]);
    }
    gpr.set(rt, ll_bit);
}

void sd(u32 rs, u32 rt, s16 imm)
{
    WriteVirtual<8>(gpr[rs] + imm, gpr[rt]);
}

void sdl(u32 rs, u32 rt, s16 imm)
{
    s64 addr = gpr[rs] + imm;
    WriteVirtual<8, Alignment::UnalignedLeft>(addr, gpr[rt] >> (8 * (addr & 7)));
}

void sdr(u32 rs, u32 rt, s16 imm)
{
    s64 addr = gpr[rs] + imm;
    WriteVirtual<8, Alignment::UnalignedRight>(addr, gpr[rt] << (8 * (~addr & 7)));
}

void sh(u32 rs, u32 rt, s16 imm)
{
    WriteVirtual<2>(gpr[rs] + imm, gpr[rt]);
}

void sll(u32 rt, u32 rd, u32 sa)
{
    gpr.set(rd, s32(gpr[rt]) << sa);
}

void sllv(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, s32(gpr[rt]) << (gpr[rs] & 31));
}

void slt(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, gpr[rs] < gpr[rt]);
}

void slti(u32 rs, u32 rt, s16 imm)
{
    gpr.set(rt, gpr[rs] < imm);
}

void sltiu(u32 rs, u32 rt, s16 imm)
{
    gpr.set(rt, u64(gpr[rs]) < u64(imm));
}

void sltu(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, u64(gpr[rs]) < u64(gpr[rt]));
}

void sra(u32 rt, u32 rd, u32 sa)
{
    gpr.set(rd, s32(gpr[rt] >> sa));
}

void srav(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, s32(gpr[rt] >> (gpr[rs] & 31)));
}

void srl(u32 rt, u32 rd, u32 sa)
{
    gpr.set(rd, s32(u32(gpr[rt]) >> sa));
}

void srlv(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, s32(u32(gpr[rt]) >> (gpr[rs] & 31)));
}

void sub(u32 rs, u32 rt, u32 rd)
{
    s32 sum;
    if (__builtin_sub_overflow(s32(gpr[rs]), s32(gpr[rt]), &sum)) {
        IntegerOverflowException();
    } else {
        gpr.set(rd, sum);
    }
}

void subu(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, s32(gpr[rs]) - s32(gpr[rt]));
}

void sync()
{
    /* Completes the Load/store instruction currently in the pipeline before the new
       load/store instruction is executed. Is executed as a NOP on the VR4300. */
}

void syscall()
{
    SyscallException();
}

void sw(u32 rs, u32 rt, s16 imm)
{
    WriteVirtual<4>(gpr[rs] + imm, gpr[rt]);
}

void swl(u32 rs, u32 rt, s16 imm)
{
    s64 addr = gpr[rs] + imm;
    WriteVirtual<4, Alignment::UnalignedLeft>(addr, u64(gpr[rt]) >> (8 * (addr & 3)));
}

void swr(u32 rs, u32 rt, s16 imm)
{
    s64 addr = gpr[rs] + imm;
    WriteVirtual<4, Alignment::UnalignedRight>(addr, gpr[rt] << (8 * (~addr & 3)));
}

void teq(u32 rs, u32 rt)
{
    if (gpr[rs] == gpr[rt]) {
        TrapException();
    }
}

void teqi(u32 rs, s16 imm)
{
    if (gpr[rs] == imm) {
        TrapException();
    }
}

void tge(u32 rs, u32 rt)
{
    if (gpr[rs] >= gpr[rt]) {
        TrapException();
    }
}

void tgei(u32 rs, s16 imm)
{
    if (gpr[rs] >= imm) {
        TrapException();
    }
}

void tgeu(u32 rs, u32 rt)
{
    if (u64(gpr[rs]) >= u64(gpr[rt])) {
        TrapException();
    }
}

void tgeiu(u32 rs, s16 imm)
{
    if (u64(gpr[rs]) >= u64(imm)) {
        TrapException();
    }
}

void tlt(u32 rs, u32 rt)
{
    if (gpr[rs] < gpr[rt]) {
        TrapException();
    }
}

void tlti(u32 rs, s16 imm)
{
    if (gpr[rs] < imm) {
        TrapException();
    }
}

void tltu(u32 rs, u32 rt)
{
    if (u64(gpr[rs]) < u64(gpr[rt])) {
        TrapException();
    }
}

void tltiu(u32 rs, s16 imm)
{
    if (u64(gpr[rs]) < u64(imm)) {
        TrapException();
    }
}

void tne(u32 rs, u32 rt)
{
    if (gpr[rs] != gpr[rt]) {
        TrapException();
    }
}

void tnei(u32 rs, s16 imm)
{
    if (gpr[rs] != imm) {
        TrapException();
    }
}

void xor_(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, gpr[rs] ^ gpr[rt]);
}

void xori(u32 rs, u32 rt, u16 imm)
{
    gpr.set(rt, gpr[rs] ^ imm);
}

} // namespace n64::vr4300
