#pragma once

#include "numtypes.hpp"

#define VR4300_INSTRUCTIONS_NAMESPACE n64::vr4300
#include "instructions.hpp"
#undef VR4300_INSTRUCTIONS_NAMESPACE

namespace n64::vr4300 {

void Cop3();
void DiscardBranch();
void Link(u32 reg);
void OnBranchNotTaken();
void ResetBranch();
u32 RunInterpreter(u32 cpu_cycles);
void TakeBranch(u64 target_address);

} // namespace n64::vr4300
