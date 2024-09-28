#pragma once

#include "asmjit/x86.h"
#include "numtypes.hpp"

namespace n64::vr4300::x86 {

asmjit::x86::Mem GprPtr32(u32 gpr_idx);
asmjit::x86::Mem GprPtr64(u32 gpr_idx);

} // namespace n64::vr4300::x86
