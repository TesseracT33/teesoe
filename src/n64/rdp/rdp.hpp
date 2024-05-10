#pragma once

#include "rdp_implementation.hpp"
#include "status.hpp"
#include "numtypes.hpp"

#include <memory>

namespace n64::rdp {

void Initialize();
Status MakeParallelRdp();
u32 ReadReg(u32 addr);
void WriteReg(u32 addr, u32 data);

inline std::unique_ptr<RDPImplementation> implementation;

} // namespace n64::rdp
