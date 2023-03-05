#pragma once

#include "types.hpp"

namespace n64::ri {

void Initialize();
u32 ReadReg(u32 addr);
void WriteReg(u32 addr, u32 data);

} // namespace n64::ri
