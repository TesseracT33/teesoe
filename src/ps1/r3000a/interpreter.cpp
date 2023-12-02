#include "interpreter.hpp"
#include "exceptions.hpp"
#include "memory.hpp"
#include "r3000a.hpp"

#include <limits>

namespace ps1::r3000a {

static u64 muldiv_delay;
static u64 time_last_muldiv_start;

static void add_muldiv_delay(u64 cycles)
{
    u64 time = get_time();
    if (time < time_last_muldiv_start + muldiv_delay) { // current mult/div hasn't finished yet
        u64 delta = time_last_muldiv_start + muldiv_delay - time;
        advance_pipeline(delta); // block until current mult/div finishes
        time_last_muldiv_start = time + delta;
    } else {
        time_last_muldiv_start = time;
    }
    muldiv_delay = cycles;
}

static void block_lohi_read()
{
    u64 time = get_time();
    if (time < time_last_muldiv_start + muldiv_delay) { // current mult/div hasn't finished yet
        advance_pipeline(time_last_muldiv_start + muldiv_delay - time); // block until current mult/div finishes
    }
}

void link(u32 reg)
{
    gpr.set(reg, pc + 8);
}

void reset_branch()
{
    in_branch_delay_slot_taken = in_branch_delay_slot_not_taken = false;
    branch_state = BranchState::NoBranch;
}

u32 run_interpreter(u32 cpu_cycles)
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

void take_branch(s32 target_address)
{
    in_branch_delay_slot_taken = true;
    in_branch_delay_slot_not_taken = false;
    branch_state = BranchState::DelaySlotTaken;
    jump_addr = target_address;
}

void Interpreter::break_() const
{
    breakpoint_exception();
}

void Interpreter::div(u32 rs, u32 rt) const
{
    s32 op1 = gpr[rs];
    s32 op2 = gpr[rt];
    if (op2 == 0) {
        lo = op1 >= 0 ? -1 : 1;
        hi = op1;
    } else if (op1 == std::numeric_limits<s32>::min() && op2 == -1) {
        lo = std::numeric_limits<s32>::min();
        hi = 0;
    } else [[likely]] {
        lo = op1 / op2;
        hi = op1 % op2;
    }
    add_muldiv_delay(36);
}

void Interpreter::divu(u32 rs, u32 rt) const
{
    u32 op1 = gpr[rs];
    u32 op2 = gpr[rt];
    if (op2 == 0) {
        lo = -1;
        hi = op1;
    } else {
        lo = op1 / op2;
        hi = op1 % op2;
    }
    add_muldiv_delay(36);
}

void Interpreter::lb(u32 rs, u32 rt, s16 imm) const
{
    s8 val = read<s8>(gpr[rs] + imm);
    if (!exception_occurred) {
        load_delay.insert(rt, val);
    }
}

void Interpreter::lbu(u32 rs, u32 rt, s16 imm) const
{
    u8 val = read<s8>(gpr[rs] + imm);
    if (!exception_occurred) {
        load_delay.insert(rt, val);
    }
}

void Interpreter::lh(u32 rs, u32 rt, s16 imm) const
{
    s16 val = read<s16>(gpr[rs] + imm);
    if (!exception_occurred) {
        load_delay.insert(rt, val);
    }
}

void Interpreter::lhu(u32 rs, u32 rt, s16 imm) const
{
    u16 val = read<s16>(gpr[rs] + imm);
    if (!exception_occurred) {
        load_delay.insert(rt, val);
    }
}

void Interpreter::lw(u32 rs, u32 rt, s16 imm) const
{
    s32 val = read<s32>(gpr[rs] + imm);
    if (!exception_occurred) {
        load_delay.insert(rt, val);
    }
}

void Interpreter::lwl(u32 rs, u32 rt, s16 imm) const
{
    s32 addr = gpr[rs] + imm;
    s32 val = read<s32, Alignment::Unaligned>(addr);
    if (!exception_occurred) {
        s32 load_mask = s32(0xFF00'0000) >> (8 * (addr & 3));
        load_delay.insert(rt, val & load_mask | gpr[rt] & ~load_mask);
    }
}

void Interpreter::lwr(u32 rs, u32 rt, s16 imm) const
{
    s32 addr = gpr[rs] + imm;
    s32 val = read<s32, Alignment::Unaligned>(addr);
    if (!exception_occurred) {
        s32 bits_offset = 8 * (addr & 3);
        u32 load_mask = 0xFFFF'FFFF >> bits_offset;
        val >>= bits_offset;
        load_delay.insert(val & load_mask | gpr[rt] & ~load_mask);
    }
}

void Interpreter::lwu(u32 rs, u32 rt, s16 imm) const
{
    u32 val = read<s32>(gpr[rs] + imm);
    if (!exception_occurred) {
        load_delay.insert(rt, val);
    }
}

void Interpreter::mfhi(u32 rd) const
{
    block_lohi_read();
    gpr.set(rd, hi);
}

void Interpreter::mflo(u32 rd) const
{
    block_lohi_read();
    gpr.set(rd, lo);
}

void Interpreter::mthi(u32 rs) const
{ // TODO: what happens if writing to lo/hi during mult/div?
    hi = gpr[rs];
}

void Interpreter::mtlo(u32 rs) const
{
    lo = gpr[rs];
}

void Interpreter::mult(u32 rs, u32 rt) const
{
    s32 op1 = s32(gpr[rs]), op2 = s32(gpr[rt]);
    s64 prod = s64(op1) * s64(op2);
    lo = u32(prod);
    hi = u32(prod >> 32);
    //  Fast  (6 cycles)   rs = 00000000h..000007FFh, or rs = FFFFF800h..FFFFFFFFh
    //  Med   (9 cycles)   rs = 00000800h..000FFFFFh, or rs = FFF00000h..FFFFF801h
    //  Slow  (13 cycles)  rs = 00100000h..7FFFFFFFh, or rs = 80000000h..FFF00001h
    u32 cycles;
    if (u32(op1) < 0x800 || u32(op1) >= 0xFFFF'F800) {
        cycles = 6;
    } else if (u32(op1) < 0x100000 || (u32)op1 >= 0xFFF0'0000) {
        cycles = 9;
    } else {
        cycles = 13;
    }
    add_muldiv_delay(cycles);
}

void Interpreter::multu(u32 rs, u32 rt) const
{
    u32 op1 = u32(gpr[rs]), op2 = u32(gpr[rt]);
    u64 prod = u64(op1) * u64(op2);
    lo = u32(prod);
    hi = u32(prod >> 32);
    //  Fast  (6 cycles)   rs = 00000000h..000007FFh
    //  Med   (9 cycles)   rs = 00000800h..000FFFFFh
    //  Slow  (13 cycles)  rs = 00100000h..FFFFFFFFh
    u32 cycles = 6;
    if (op1 > 0x7FF) cycles += 3;
    if (op1 > 0xFFFFF) cycles += 4;
    add_muldiv_delay(cycles);
}

void Interpreter::sb(u32 rs, u32 rt, s16 imm) const
{
    write<1>(gpr[rs] + imm, u8(gpr[rt]));
}

void Interpreter::sh(u32 rs, u32 rt, s16 imm) const
{
    write<2>(gpr[rs] + imm, u16(gpr[rt]));
}

void Interpreter::sw(u32 rs, u32 rt, s16 imm) const
{
    write<4>(gpr[rs] + imm, gpr[rt]);
}

void Interpreter::swl(u32 rs, u32 rt, s16 imm) const
{
}

void Interpreter::swr(u32 rs, u32 rt, s16 imm) const
{
}

void Interpreter::syscall() const
{
    syscall_exception();
}

} // namespace ps1::r3000a
