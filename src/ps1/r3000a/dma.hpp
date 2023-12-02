#pragma once

#include "types.hpp"

namespace ps1::r300a::dma {

void init();
s32 read(s32 addr);
void write(s32 addr, s32 data);

} // namespace ps1::r300a::dma
