#pragma once

#include "numtypes.hpp"
#include "rdp_implementation.hpp"
#include "status.hpp"

#include <memory>

struct SDL_Window;

namespace n64::rdp {

void Initialize();
u32 ReadReg(u32 addr);
void WriteReg(u32 addr, u32 data);

inline RdpImplementation* implementation;

} // namespace n64::rdp
