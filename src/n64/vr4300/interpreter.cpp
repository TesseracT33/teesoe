#include "interpreter.hpp"
#include "cop0.hpp"
#include "decoder.hpp"
#include "exceptions.hpp"
#include "mmu.hpp"
#include "platform.hpp"

#include <array>
#include <limits>

#if defined _MSC_VER && !defined __clang__
#include <intrin.h>
#endif

using mips::BranchState;

namespace n64::vr4300 {

void DiscardBranch()
{
    pc += 4;
    in_branch_delay_slot_taken = in_branch_delay_slot_not_taken = false;
    branch_state = BranchState::NoBranch;
}

void Link(u32 reg)
{
    gpr.set(reg, pc + 8);
}

void OnBranchNotTaken()
{
    in_branch_delay_slot_not_taken = true;
    in_branch_delay_slot_taken = false;
    branch_state = BranchState::DelaySlotNotTaken;
}

void ResetBranch()
{
    in_branch_delay_slot_taken = in_branch_delay_slot_not_taken = false;
    branch_state = BranchState::NoBranch;
}

u32 RunInterpreter(u32 cpu_cycles)
{
    cycle_counter = 0;
    while (cycle_counter < cpu_cycles) {
        AdvancePipeline(1);
        exception_occurred = false;
        u32 instr = FetchInstruction(pc);
        if (exception_occurred) continue;
        decoder::exec_cpu<CpuImpl::Interpreter>(instr);
        if (exception_occurred) continue;
        if (branch_state == BranchState::Perform) {
            PerformBranch();
        } else {
            in_branch_delay_slot_not_taken &= branch_state != BranchState::NoBranch;
            branch_state = branch_state == BranchState::DelaySlotTaken ? BranchState::Perform : BranchState::NoBranch;
            pc += 4;
        }
    }
    return cycle_counter - cpu_cycles;
}

void TakeBranch(u64 target_address)
{
    in_branch_delay_slot_taken = true;
    in_branch_delay_slot_not_taken = false;
    branch_state = BranchState::DelaySlotTaken;
    jump_addr = target_address;
}

void Interpreter::beq(u32 rs, u32 rt, s16 imm) const
{
    if (gpr[rs] == gpr[rt]) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        OnBranchNotTaken();
    }
}

void Interpreter::beql(u32 rs, u32 rt, s16 imm) const
{
    if (gpr[rs] == gpr[rt]) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        DiscardBranch();
    }
}

void Interpreter::bgez(u32 rs, s16 imm) const
{
    if (gpr[rs] >= 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        OnBranchNotTaken();
    }
}

void Interpreter::bgezal(u32 rs, s16 imm) const
{
    bool in_delay_slot = in_branch_delay_slot_taken || in_branch_delay_slot_not_taken;
    if (gpr[rs] >= 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        OnBranchNotTaken();
    }
    gpr.set(31, 4 + (in_delay_slot ? jump_addr : pc + 4));
}

void Interpreter::bgezall(u32 rs, s16 imm) const
{
    gpr.set(31, pc + 8);
    if (gpr[rs] >= 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        DiscardBranch();
    }
}

void Interpreter::bgezl(u32 rs, s16 imm) const
{
    if (gpr[rs] >= 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        DiscardBranch();
    }
}

void Interpreter::bgtz(u32 rs, s16 imm) const
{
    if (gpr[rs] > 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        OnBranchNotTaken();
    }
}

void Interpreter::bgtzl(u32 rs, s16 imm) const
{
    if (gpr[rs] > 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        DiscardBranch();
    }
}

void Interpreter::blez(u32 rs, s16 imm) const
{
    if (gpr[rs] <= 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        OnBranchNotTaken();
    }
}

void Interpreter::blezl(u32 rs, s16 imm) const
{
    if (gpr[rs] <= 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        DiscardBranch();
    }
}

void Interpreter::bltz(u32 rs, s16 imm) const
{
    if (gpr[rs] < 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        OnBranchNotTaken();
    }
}

void Interpreter::bltzal(u32 rs, s16 imm) const
{
    gpr.set(31, pc + 8);
    if (gpr[rs] < 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        OnBranchNotTaken();
    }
}

void Interpreter::bltzall(u32 rs, s16 imm) const
{
    gpr.set(31, pc + 8);
    if (gpr[rs] < 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        DiscardBranch();
    }
}

void Interpreter::bltzl(u32 rs, s16 imm) const
{
    if (gpr[rs] < 0) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        DiscardBranch();
    }
}

void Interpreter::bne(u32 rs, u32 rt, s16 imm) const
{
    if (gpr[rs] != gpr[rt]) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        OnBranchNotTaken();
    }
}

void Interpreter::bnel(u32 rs, u32 rt, s16 imm) const
{
    if (gpr[rs] != gpr[rt]) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        DiscardBranch();
    }
}

void Interpreter::break_() const
{
    BreakpointException();
}

void Interpreter::ddiv(u32 rs, u32 rt) const
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

void Interpreter::ddivu(u32 rs, u32 rt) const
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    u64 op1 = u64(gpr[rs]);
    u64 op2 = u64(gpr[rt]);
    if (op2 == 0) {
        lo = -1;
        hi = op1;
    } else {
        lo = op1 / op2;
        hi = op1 % op2;
    }
    AdvancePipeline(68);
}

void Interpreter::div(u32 rs, u32 rt) const
{
    s32 op1 = s32(gpr[rs]);
    s32 op2 = s32(gpr[rt]);
    if (op2 == 0) {
        lo = op1 >= 0 ? -1 : 1;
        hi = op1;
    } else if (op1 == std::numeric_limits<s32>::min() && op2 == -1) {
        lo = op1;
        hi = 0;
    } else [[likely]] {
        lo = op1 / op2;
        hi = op1 % op2;
    }
    AdvancePipeline(36);
}

void Interpreter::divu(u32 rs, u32 rt) const
{
    u32 op1 = u32(gpr[rs]);
    u32 op2 = u32(gpr[rt]);
    if (op2 == 0) {
        lo = -1;
        hi = s32(op1);
    } else {
        lo = s32(op1 / op2);
        hi = s32(op1 % op2);
    }
    AdvancePipeline(36);
}

void Interpreter::dmult(u32 rs, u32 rt) const
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
#if INT128_AVAILABLE
    s128 prod = s128(gpr[rs]) * s128(gpr[rt]);
    lo = s64(prod);
    hi = prod >> 64;
#elif defined _MSC_VER
    lo = _mul128(gpr[rs], gpr[rt], &hi);
#else
#error DMULT unimplemented on targets where INT128 or MSVC _mul128 is unavailable
#endif
    AdvancePipeline(7);
}

void Interpreter::dmultu(u32 rs, u32 rt) const
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
#if INT128_AVAILABLE
    u128 prod = u128(gpr[rs]) * u128(gpr[rt]);
    lo = s64(prod);
    hi = prod >> 64;
#elif defined _MSC_VER
    lo = _umul128(gpr[rs], gpr[rt], reinterpret_cast<u64*>(&hi));
#else
#error DMULTU unimplemented on targets where UINT128 or MSVC _umul128 is unavailable
#endif
    AdvancePipeline(7);
}

void Interpreter::j(u32 instr) const
{
    if (!in_branch_delay_slot_taken) {
        TakeBranch((pc + 4) & 0xFFFF'FFFF'F000'0000 | instr << 2 & 0xFFF'FFFF);
    }
}

void Interpreter::jal(u32 instr) const
{
    if (in_branch_delay_slot_taken) {
        gpr.set(31, jump_addr + 4);
    } else {
        gpr.set(31, pc + 8);
        TakeBranch((pc + 4) & 0xFFFF'FFFF'F000'0000 | instr << 2 & 0xFFF'FFFF);
    }
}

void Interpreter::jalr(u32 rs, u32 rd) const
{
    if (in_branch_delay_slot_taken) {
        gpr.set(rd, jump_addr + 4);
    } else {
        s64 target = gpr[rs];
        gpr.set(rd, pc + 8);
        TakeBranch(target);
    }
}

void Interpreter::jr(u32 rs) const
{
    if (!in_branch_delay_slot_taken) {
        TakeBranch(gpr[rs]);
    }
}

void Interpreter::lb(u32 rs, u32 rt, s16 imm) const
{
    s8 val = ReadVirtual<s8>(gpr[rs] + imm);
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void Interpreter::lbu(u32 rs, u32 rt, s16 imm) const
{
    u8 val = ReadVirtual<s8>(gpr[rs] + imm);
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void Interpreter::ld(u32 rs, u32 rt, s16 imm) const
{
    if (!can_execute_dword_instrs) return ReservedInstructionException();
    s64 val = ReadVirtual<s64>(gpr[rs] + imm);
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void Interpreter::ldl(u32 rs, u32 rt, s16 imm) const
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

void Interpreter::ldr(u32 rs, u32 rt, s16 imm) const
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

void Interpreter::lh(u32 rs, u32 rt, s16 imm) const
{
    s16 val = ReadVirtual<s16>(gpr[rs] + imm);
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void Interpreter::lhu(u32 rs, u32 rt, s16 imm) const
{
    u16 val = ReadVirtual<s16>(gpr[rs] + imm);
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void Interpreter::ll(u32 rs, u32 rt, s16 imm) const
{
    s32 val = ReadVirtual<s32>(gpr[rs] + imm);
    cop0.ll_addr = last_paddr_on_load >> 4;
    ll_bit = 1;
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void Interpreter::lld(u32 rs, u32 rt, s16 imm) const
{
    s64 val = ReadVirtual<s64>(gpr[rs] + imm);
    cop0.ll_addr = last_paddr_on_load >> 4;
    ll_bit = 1;
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void Interpreter::lw(u32 rs, u32 rt, s16 imm) const
{
    s32 val = ReadVirtual<s32>(gpr[rs] + imm);
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void Interpreter::lwl(u32 rs, u32 rt, s16 imm) const
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

void Interpreter::lwr(u32 rs, u32 rt, s16 imm) const
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

void Interpreter::lwu(u32 rs, u32 rt, s16 imm) const
{
    u32 val = ReadVirtual<s32>(gpr[rs] + imm);
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void Interpreter::mult(u32 rs, u32 rt) const
{
    s64 prod = s64(s32(gpr[rs])) * s64(s32(gpr[rt]));
    lo = s32(prod);
    hi = prod >> 32;
    AdvancePipeline(4);
}

void Interpreter::multu(u32 rs, u32 rt) const
{
    mult(rs, rt);
}

void Interpreter::sb(u32 rs, u32 rt, s16 imm) const
{
    WriteVirtual<1>(gpr[rs] + imm, gpr[rt]);
}

void Interpreter::sc(u32 rs, u32 rt, s16 imm) const
{
    if (ll_bit) {
        WriteVirtual<4>(gpr[rs] + imm, gpr[rt]);
    }
    gpr.set(rt, ll_bit);
}

void Interpreter::scd(u32 rs, u32 rt, s16 imm) const
{
    if (ll_bit) {
        WriteVirtual<8>(gpr[rs] + imm, gpr[rt]);
    }
    gpr.set(rt, ll_bit);
}

void Interpreter::sd(u32 rs, u32 rt, s16 imm) const
{
    WriteVirtual<8>(gpr[rs] + imm, gpr[rt]);
}

void Interpreter::sdl(u32 rs, u32 rt, s16 imm) const
{
    s64 addr = gpr[rs] + imm;
    WriteVirtual<8, Alignment::UnalignedLeft>(addr, gpr[rt] >> (8 * (addr & 7)));
}

void Interpreter::sdr(u32 rs, u32 rt, s16 imm) const
{
    s64 addr = gpr[rs] + imm;
    WriteVirtual<8, Alignment::UnalignedRight>(addr, gpr[rt] << (8 * (~addr & 7)));
}

void Interpreter::sh(u32 rs, u32 rt, s16 imm) const
{
    WriteVirtual<2>(gpr[rs] + imm, gpr[rt]);
}

void Interpreter::sync() const
{
    /* Completes the Load/store instruction currently in the pipeline before the new
       load/store instruction is executed. Is executed as a NOP on the VR4300. */
}

void Interpreter::syscall() const
{
    SyscallException();
}

void Interpreter::sw(u32 rs, u32 rt, s16 imm) const
{
    WriteVirtual<4>(gpr[rs] + imm, gpr[rt]);
}

void Interpreter::swl(u32 rs, u32 rt, s16 imm) const
{
    s64 addr = gpr[rs] + imm;
    WriteVirtual<4, Alignment::UnalignedLeft>(addr, u64(gpr[rt]) >> (8 * (addr & 3)));
}

void Interpreter::swr(u32 rs, u32 rt, s16 imm) const
{
    s64 addr = gpr[rs] + imm;
    WriteVirtual<4, Alignment::UnalignedRight>(addr, gpr[rt] << (8 * (~addr & 3)));
}

} // namespace n64::vr4300
