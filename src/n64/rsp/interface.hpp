#pragma once

#include "types.hpp"

namespace n64::rsp {

struct Sp {
    u32 dma_spaddr, dma_ramaddr, dma_rdlen, dma_wrlen;
    struct {
        u32 halted   : 1;
        u32 broke    : 1;
        u32 dma_busy : 1;
        u32 dma_full : 1;
        u32 io_busy  : 1;
        u32 sstep    : 1;
        u32 intbreak : 1;
        u32 sig      : 8;
        u32          : 17;
    } status;
    u32 dma_full, dma_busy, semaphore;
} inline sp;

u32 ReadReg(u32 addr);
void WriteReg(u32 addr, u32 data);

} // namespace n64::rsp
