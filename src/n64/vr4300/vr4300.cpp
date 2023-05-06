#include "vr4300.hpp"
#include "cache.hpp"
#include "cop0.hpp"
#include "cop1.hpp"
#include "disassembler.hpp"
#include "exceptions.hpp"
#include "jit/util.hpp"
#include "log.hpp"
#include "memory/rdram.hpp"
#include "mmu.hpp"
#include "n64_build_options.hpp"

#include <bit>
#include <cassert>
#include <cstring>
#include <format>
#include <utility>

namespace n64::vr4300 {

enum class BranchState {
    DelaySlotNotTaken,
    DelaySlotTaken,
    NoBranch,
    Perform,
} static branch_state{ BranchState::NoBranch };

static bool interrupt;

static void InitializeRegisters();
static void JitInstructionEpilogue();

void AddInitialEvents()
{
    ReloadCountCompareEvent<true>();
}

void AdvancePipeline(u64 cycles)
{
    p_cycle_counter += cycles;
    cop0.count += cycles;
}

void CheckInterrupts()
{
    bool prev_interrupt = interrupt;
    interrupt = cop0.status.ie & !cop0.status.exl & !cop0.status.erl & bool(cop0.cause.ip & cop0.status.im);
    if (interrupt && !prev_interrupt) {
        if constexpr (log_interrupts) {
            log(
              std::format("INTERRUPT; cause.ip = ${:02X}; status.im = ${:02X}", u8(cop0.cause.ip), u8(cop0.status.im)));
        }
        InterruptException();
    }
}

/* Devices external to the CPU (e.g. the console's reset button) use this function to tell the CPU about interrupts. */
void ClearInterruptPending(ExternalInterruptSource interrupt)
{
    cop0.cause.ip &= ~std::to_underlying(interrupt);
}

void DiscardBranch()
{
    pc += 4;
    in_branch_delay_slot_taken = in_branch_delay_slot_not_taken = false;
    branch_state = BranchState::NoBranch;
}

u64 GetElapsedCycles()
{
    return p_cycle_counter;
}

void InitializeRegisters()
{
    std::memset(&gpr, 0, sizeof(gpr));
    gpr.set(29, 0xFFFF'FFFF'A400'1FF0);
    cop0.SetRaw(Cop0Reg::index, 0x3F);
    cop0.SetRaw(Cop0Reg::config, 0x7006'E463);
    cop0.SetRaw(Cop0Reg::context, 0x007F'FFF0);
    cop0.SetRaw(Cop0Reg::bad_v_addr, s64(0xFFFF'FFFF'FFFF'FFFF));
    cop0.SetRaw(Cop0Reg::cause, s32(0xB000'007C));
    cop0.SetRaw(Cop0Reg::epc, s64(0xFFFF'FFFF'FFFF'FFFF));
    cop0.SetRaw(Cop0Reg::status, 0x3400'0000);
    cop0.SetRaw(Cop0Reg::ll_addr, s32(0xFFFF'FFFF));
    cop0.SetRaw(Cop0Reg::watch_lo, s32(0xFFFF'FFFB));
    cop0.SetRaw(Cop0Reg::watch_hi, 0xF);
    cop0.SetRaw(Cop0Reg::x_context, s64(0xFFFF'FFFF'FFFF'FFF0));
    cop0.SetRaw(Cop0Reg::error_epc, s64(0xFFFF'FFFF'FFFF'FFFF));
}

void InitRun(bool hle_pif)
{
    if (hle_pif) {
        /* https://github.com/Dillonb/n64-resources/blob/master/bootn64.html */
        gpr.set(20, 1);
        gpr.set(22, 0x3F);
        gpr.set(29, 0xA400'1FF0);
        cop0.SetRaw(Cop0Reg::status, 0x3400'0000);
        cop0.SetRaw(Cop0Reg::config, 0x7006'E463);
        for (u64 i = 0; i < 0x1000; i += 4) {
            u64 src_addr = 0xFFFF'FFFF'B000'0000 + i;
            u64 dst_addr = 0xFFFF'FFFF'A400'0000 + i;
            WriteVirtual<4>(dst_addr, ReadVirtual<s32>(src_addr));
        }
        pc = 0xFFFF'FFFF'A400'0040;
    } else {
        ColdResetException();
    }
}

void JitInstructionEpilogue()
{
    // TODO: optimize to only be done right before branch instruction or at the end of a block?
    jit.compiler.add(ptr(pc), 4);
}

void JitInstructionEpilogueFirstBlockInstruction()
{
    using namespace asmjit::x86;
    Compiler& c = jit.compiler;
    Gp v0 = c.newGpw();
    asmjit::Label l_exit = c.newLabel();
    c.cmp(Mem(std::bit_cast<u64>(&in_branch_delay_slot_taken)), 0);
    c.je(l_exit);
    Gp v1 = c.newGpq();
    c.mov(Mem(std::bit_cast<u64>(&in_branch_delay_slot_taken)), 0);
    c.mov(v1, Mem(std::bit_cast<u64>(&jump_addr)));
    c.mov(Mem(std::bit_cast<u64>(&pc)), v1);
    c.ret();
    c.bind(l_exit);
    c.add(Mem(pc), 4);
}

void JumpRecompiler()
{ // Assumption: target_address is in rax
  // TODO
  // using namespace asmjit::x86;
  // Compiler& c = jit.compiler;
  // c.mov(mem(jump_is_pending), 1);
  // c.
}

void NotifyIllegalInstrCode(u32 instr_code)
{
    log_error(std::format("Illegal CPU instruction code {:08X} encountered.\n", instr_code));
}

void OnBranchNotTaken()
{
    in_branch_delay_slot_not_taken = true;
    in_branch_delay_slot_taken = false;
    branch_state = BranchState::DelaySlotNotTaken;
}

void PowerOn()
{
    exception_occurred = false;
    InitializeRegisters();
    InitCop1();
    InitializeMMU();
    InitCache();
}

void Reset()
{
    exception_occurred = false;
    ResetBranch();
    SoftResetException();
}

void ResetBranch()
{
    in_branch_delay_slot_taken = in_branch_delay_slot_not_taken = false;
    branch_state = BranchState::NoBranch;
}

u64 RunInterpreter(u64 cpu_cycles)
{
    p_cycle_counter = 0;
    while (p_cycle_counter < cpu_cycles) {
        AdvancePipeline(1);
        exception_occurred = false;
        u32 instr = FetchInstruction(pc);
        if (exception_occurred) continue;
        disassembler::exec_cpu<CpuImpl::Interpreter>(instr);
        if (exception_occurred) continue;
        switch (branch_state) {
        case BranchState::DelaySlotNotTaken:
            pc += 4;
            branch_state = BranchState::NoBranch;
            break;
        case BranchState::DelaySlotTaken:
            pc += 4;
            branch_state = BranchState::Perform;
            break;
        case BranchState::NoBranch:
            pc += 4;
            in_branch_delay_slot_not_taken = false;
            break;
        case BranchState::Perform:
            pc = jump_addr;
            branch_state = BranchState::NoBranch;
            in_branch_delay_slot_taken = false;
            if (pc & 3) AddressErrorException<MemOp::InstrFetch>(pc);
            break;
        default: std::unreachable();
        }
    }
    return p_cycle_counter - cpu_cycles;
}

u64 RunRecompiler(u64 cpu_cycles)
{
    // TODO: made change in interpreter to increment pc after instr execution
    auto exec_block = [](Jit::Block* block) {
        block->func();
        AdvancePipeline(block->cycles);
    };

    p_cycle_counter = 0;
    while (p_cycle_counter < cpu_cycles) {
        u32 physical_pc = GetPhysicalPC();
        auto [block, compiled] = jit.get_block(physical_pc);
        assert(block);
        if (compiled) {
            exec_block(block);
        } else {
            auto one_instr = [addr = pc]() mutable {
                u32 instr = FetchInstruction(addr);
                addr += 4;
                disassembler::exec_cpu<CpuImpl::Recompiler>(instr);
                jit.cycles++;
            };
            one_instr();
            JitInstructionEpilogueFirstBlockInstruction();
            while (!jit.branched && (pc & 255)) {
                // If the branch delay slot instruction fits within the block boundary, include it before stopping
                jit.branched = jit.branch_hit;
                one_instr();
                JitInstructionEpilogue();
            }
            block->cycles = jit.cycles;
            jit.finalize_block(block);
            exec_block(block);
        }
    }
    return p_cycle_counter - cpu_cycles;
}

void SetInterruptPending(ExternalInterruptSource interrupt)
{
    cop0.cause.ip |= std::to_underlying(interrupt);
    CheckInterrupts();
}

void SignalInterruptFalse()
{
    interrupt = false;
}

void TakeBranch(u64 target_address)
{
    in_branch_delay_slot_taken = true;
    in_branch_delay_slot_not_taken = false;
    branch_state = BranchState::DelaySlotTaken;
    jump_addr = target_address;
}

} // namespace n64::vr4300
