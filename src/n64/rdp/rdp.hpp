#pragma once

#include "rdp_implementation.hpp"
#include "status.hpp"
#include "types.hpp"

#include <memory>

namespace n64::rdp {

void Initialize();
Status MakeParallelRdp();
s32 ReadReg(u32 addr);
void WriteReg(u32 addr, s32 data);

inline std::unique_ptr<RDPImplementation> implementation;

} // namespace n64::rdp
