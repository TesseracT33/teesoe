#pragma once

#include "numtypes.hpp"
#include <concepts>

namespace n64::si {

void Initialize();
template<std::signed_integral Int> Int ReadMemory(u32 addr);
u32 ReadReg(u32 addr);
template<size_t access_size> void WriteMemory(u32 addr, s64 data);
void WriteReg(u32 addr, u32 data);

} // namespace n64::si
