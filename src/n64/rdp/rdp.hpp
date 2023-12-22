#pragma once

#include "rdp_implementation.hpp"
#include "status.hpp"
#include "types.hpp"

#include <memory>

class VulkanRenderContext;

namespace n64::rdp {

void Initialize();
Status MakeParallelRdp(std::shared_ptr<VulkanRenderContext> render_context);
u32 ReadReg(u32 addr);
void WriteReg(u32 addr, u32 data);

inline std::unique_ptr<RDPImplementation> implementation;

} // namespace n64::rdp
