#pragma once

#include "types.hpp"

namespace n64::ai {

void Initialize();
u32 ReadReg(u32 addr);
void Step(u64 cpu_cycles);
void WriteReg(u32 addr, u32 data);

} // namespace n64::ai
