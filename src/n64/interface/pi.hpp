#pragma once

#include "types.hpp"

namespace n64::pi {

enum class StatusFlag : s32 {
    DmaBusy = 1 << 0,
    IoBusy = 1 << 1,
    DmaError = 1 << 2,
    DmaCompleted = 1 << 3
};

void ClearStatusFlag(StatusFlag);
void Initialize();
s32 ReadReg(u32 addr);
void SetStatusFlag(StatusFlag);
void WriteReg(u32 addr, s32 data);

} // namespace n64::pi
