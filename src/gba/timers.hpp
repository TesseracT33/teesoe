#pragma once

#include "serializer.hpp"
#include "numtypes.hpp"

#include <concepts>

namespace gba::timers {

void Initialize();
template<std::integral Int> Int ReadReg(u32 addr);
void StreamState(Serializer& stream);
template<std::integral Int> void WriteReg(u32 addr, Int data);

} // namespace gba::timers
