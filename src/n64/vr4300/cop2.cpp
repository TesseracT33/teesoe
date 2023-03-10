#include "cop2.hpp"
#include "cop0.hpp"
#include "cpu.hpp"
#include "exceptions.hpp"
#include "n64_build_options.hpp"
#include "util.hpp"
#include "vr4300.hpp"

#include <format>

namespace n64::vr4300 {

static u64 cop2_latch;

void InitializeCOP2()
{
    cop2_latch = 0;
}

template<Cop2Instruction instr> void Cop2Move(u32 rt)
{
    AdvancePipeline(1);

    if constexpr (log_cpu_instructions) {
        current_instr_log_output = std::format("{} {}", current_instr_name, rt);
    }

    if (!cop0.status.cu2) {
        SignalCoprocessorUnusableException(2);
        return;
    }

    using enum Cop2Instruction;

    if constexpr (one_of(instr, CFC2, MFC2)) gpr.set(rt, s32(cop2_latch));
    else if constexpr (instr == DMFC2) gpr.set(rt, cop2_latch);
    else if constexpr (one_of(instr, CTC2, DMTC2, MTC2)) cop2_latch = gpr[rt];
    else if constexpr (one_of(instr, DCFC2, DCTC2)) SignalException<Exception::ReservedInstructionCop2>();
    else static_assert(always_false<instr>);
}

template void Cop2Move<Cop2Instruction::CFC2>(u32);
template void Cop2Move<Cop2Instruction::CTC2>(u32);
template void Cop2Move<Cop2Instruction::MFC2>(u32);
template void Cop2Move<Cop2Instruction::MTC2>(u32);
template void Cop2Move<Cop2Instruction::DCFC2>(u32);
template void Cop2Move<Cop2Instruction::DCTC2>(u32);
template void Cop2Move<Cop2Instruction::DMFC2>(u32);
template void Cop2Move<Cop2Instruction::DMTC2>(u32);
} // namespace n64::vr4300
