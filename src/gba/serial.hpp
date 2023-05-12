#pragma once

#include "types.hpp"

#include <concepts>

namespace gba::serial {

void Initialize();
template<std::integral Int> Int ReadReg(u32 addr);
template<std::integral Int> void WriteReg(u32 addr, Int data);

} // namespace gba::serial
