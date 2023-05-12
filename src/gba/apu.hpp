#pragma once

#include "serializer.hpp"
#include "types.hpp"

#include <concepts>

namespace gba::apu {

void Initialize();
template<std::integral Int> Int ReadReg(u32 addr);
void StreamState(Serializer& stream);
template<std::integral Int> void WriteReg(u32 addr, Int data);

} // namespace gba::apu
