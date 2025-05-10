#include "interpreter.hpp"
#include "cop0.hpp"
#include "decoder.hpp"
#include "exceptions.hpp"
#include "mmu.hpp"
#include "vr4300.hpp"

namespace n64::vr4300 {

void Cop3()
{
    if (cop0.status.cu3) {
        ReservedInstructionException();
    } else {
        CoprocessorUnusableException(3);
    }
}

void DiscardBranch()
{
    pc += 4;
    branch_state = BranchState::NoBranch;
}

void Link(u32 reg)
{
    gpr.set(reg, pc + 8);
}

void OnBranchNotTaken()
{
    last_instr_was_branch = true;
    branch_state = BranchState::DelaySlotNotTaken;
}

void ResetBranch()
{
    branch_state = BranchState::NoBranch;
}

u32 RunInterpreter(u32 cpu_cycles)
{
    cycle_counter = 0;
    while (cycle_counter < cpu_cycles) {
        AdvancePipeline(1);
        exception_occurred = false;
        last_instr_was_branch = false;
        u32 instr = FetchInstruction(pc);
        if (exception_occurred) continue;
        decode_and_interpret_cpu(instr);
        if (exception_occurred) continue;
        if (last_instr_was_branch) {
            pc += 4;
        } else {
            if (branch_state == BranchState::DelaySlotTaken) {
                PerformBranch();
            } else {
                branch_state = BranchState::NoBranch;
                pc += 4;
            }
        }
    }
    return cycle_counter - cpu_cycles;
}

void TakeBranch(u64 target_address)
{
    last_instr_was_branch = true;
    branch_state = BranchState::DelaySlotTaken;
    jump_addr = target_address;
}

} // namespace n64::vr4300
