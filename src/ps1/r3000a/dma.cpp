#include "dma.hpp"

namespace ps1::r300a::dma {

static s32 dpcr;

void init()
{
    dpcr = 0x0765'4321;
}

s32 read(s32 addr)
{
}
void write(s32 addr, s32 data)
{
}

} // namespace ps1::r300a::dma
