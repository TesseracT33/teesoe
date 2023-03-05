#pragma once

#include "types.hpp"

namespace n64::vr4300::recompiler {

void BreakupBlock();
bool Initialize();
u64 Run(u64 cpu_cycles_to_run);
bool Terminate();

} // namespace n64::vr4300::recompiler
