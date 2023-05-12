#pragma once

#include "types.hpp"

#include <concepts>

namespace gba::dma {

void AddCycles(u64 cycles, uint h);
void Initialize();
void OnHBlank();
void OnVBlank();
template<std::integral Int> Int ReadReg(u32 addr);
template<std::integral Int> void WriteReg(u32 addr, Int data);

} // namespace gba::dma
