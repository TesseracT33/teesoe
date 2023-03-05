#pragma once

#include "types.hpp"

namespace n64::si {

enum class StatusFlag : s32 {
    DmaBusy = 1 << 0, /* Set when a read or write DMA, or an IO write, is in progress. */
    IoBusy = 1 << 1, /* Set when either an IO read or write is in progress. */
    ReadPending = 1 << 2, /* Set when an IO read occurs, while an IO or DMA write is in progress. */
    DmaError = 1 << 3, /* Set when overlapping DMA requests occur. Can only be cleared with a power reset. */
    Interrupt = 1 << 12 /* Copy of SI interrupt flag from MIPS Interface, which is also seen in the RCP Interrupt Cause
                           register. Writing any value to STATUS clears this bit in all three locations. */
};

void ClearStatusFlag(StatusFlag);
void Initialize();
u32 ReadReg(u32 addr);
void SetStatusFlag(StatusFlag);
void WriteReg(u32 addr, u32 data);

} // namespace n64::si
