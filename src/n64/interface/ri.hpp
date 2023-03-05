#pragma once

#include "types.hpp"

namespace n64::ri {

void Initialize();
s32 ReadReg(u32 addr);
void WriteReg(u32 addr, s32 data);

} // namespace n64::ri
