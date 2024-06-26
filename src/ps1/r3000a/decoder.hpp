#pragma once

#include "ps1.hpp"
#include "numtypes.hpp"

namespace ps1::r3000a {

template<CpuImpl> void decode(u32 instr);

} // namespace ps1::r3000a
