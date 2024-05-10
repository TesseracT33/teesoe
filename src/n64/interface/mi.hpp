#pragma once

#include "numtypes.hpp"

namespace n64::mi {

enum class InterruptType : s32 {
    SP = 1 << 0, /* Set by the RSP when requested by a write to the SP status register, and optionally when the RSP
                    halts */
    SI = 1 << 1, /* Set when a SI DMA to/from PIF RAM finishes */
    AI = 1 << 2, /* Set when no more samples remain in an audio stream */
    VI = 1 << 3, /* Set when VI_V_CURRENT == VI_V_INTR */
    PI = 1 << 4, /* Set when a PI DMA finishes */
    DP = 1 << 5 /* Set when a full sync completes */
};

void ClearInterrupt(InterruptType);
void Initialize();
u32 ReadReg(u32 addr);
void RaiseInterrupt(InterruptType);
void WriteReg(u32 addr, u32 data);

} // namespace n64::mi
