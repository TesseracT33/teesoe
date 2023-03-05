#pragma once

#include "types.hpp"

namespace n64::vr4300 {

enum class Cop2Instruction {
    CFC2,
    CTC2,
    MFC2,
    MTC2,
    DCFC2,
    DCTC2,
    DMFC2,
    DMTC2
};

template<Cop2Instruction> void Cop2Move(u32 rt);
void InitializeCop22();

} // namespace n64::vr4300
