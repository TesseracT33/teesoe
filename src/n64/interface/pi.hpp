#pragma once

#include "types.hpp"

#include <optional>

namespace n64::pi {

void Initialize();
std::optional<u32> IoBusy();
u32 ReadReg(u32 addr);
template<size_t size> void Write(u32 addr, s64 value, u8* dst = nullptr);
void WriteReg(u32 addr, u32 data);

} // namespace n64::pi
