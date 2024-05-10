#pragma once

#include "numtypes.hpp"

namespace n64::si {

void Initialize();
u32 ReadReg(u32 addr);
void WriteReg(u32 addr, u32 data);

} // namespace n64::si
